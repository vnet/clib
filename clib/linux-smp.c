/*
  Copyright (c) 2005 Eliot Dresselhaus

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#define _GNU_SOURCE
#include <sched.h>
#include <sys/syscall.h>	/* __NR_exit */

#include <clib/error.h>
#include <clib/longjmp.h>
#include <clib/types.h>
#include "clib_thread_db.h"

#if defined (__x86_64__)
#define clib_thread_exit(val) \
  asm volatile ("syscall" :: "a" (__NR_exit), "D" (val))
#endif

#if defined (i386)
#define clib_thread_exit(val)						\
  while (1) {								\
    if (__builtin_constant_p (val) && (val) == 0)			\
      asm volatile ("xorl %%ebx, %%ebx; int $0x80" :: "a" (__NR_exit));	\
    else								\
      asm volatile ("movl %1, %%ebx; int $0x80"				\
		    :: "a" (__NR_exit), "r" (val));			\
  }
#endif

#ifndef clib_thread_exit
#define clib_thread_exit(x) exit(x)
#endif

/* I couldn't quite stomach doing it with /sys file system reads. */
static uword linux_guess_n_cpus ()
{
  int i;
  cpu_set_t s, s_save;

  if (sched_getaffinity (/* pid */ 0, sizeof (s_save), &s_save) < 0)
    clib_unix_error ("sched_getaffinity");

  for (i = 0; 1; i++)
    {
      memset (&s, 0, sizeof (s));
      s.__bits[i / BITS (s.__bits[0])] = (uword) 1 << (i % BITS (s.__bits[0]));
      if (sched_setaffinity (/* pid */ 0, sizeof (s), &s) < 0)
	break;
    }

  if (sched_setaffinity (/* pid */ 0, sizeof (s_save), &s_save) < 0)
    clib_unix_error ("sched_setaffinity");

  return i;
}

static void linux_bind_to_cpu (uword cpu)
{
  cpu_set_t s;
  memset (&s, 0, sizeof (s));
  s.__bits[cpu / BITS (s.__bits[0])] = (uword) 1 << (cpu % BITS (s.__bits[0]));
  if (sched_setaffinity (/* pid */ 0, sizeof (s), &s) < 0)
    clib_unix_warning ("sched_setaffinity (cpu %d)", cpu);
}

typedef struct {
  uword (* bootstrap_function) (uword);
  uword bootstrap_function_arg;
  pid_t tid;
} linux_clone_bootstrap_args_t;

static clib_thread_db_event_t thread_db_event;

static never_inline void
thread_db_breakpoint (void)
{
}

static int linux_clone_bootstrap (linux_clone_bootstrap_args_t * a)
{
  clib_smp_main_t * m = &clib_smp_main;
  uword result, my_cpu;

  my_cpu = os_get_cpu_number ();

  thread_db_event.event = TD_CREATE;
  thread_db_event.data = a->tid;
  thread_db_breakpoint ();

  linux_bind_to_cpu (my_cpu);

  result = a->bootstrap_function (a->bootstrap_function_arg);

  thread_db_event.event = TD_DEATH;
  thread_db_event.data = a->tid;
  thread_db_breakpoint ();

  clib_smp_atomic_add (&m->n_cpus_exited, 1);

  clib_thread_exit (result);

  return 0;
}

uword os_smp_bootstrap (uword n_cpus,
			void * bootstrap_function,
			uword bootstrap_function_arg)
{
  clib_smp_main_t * m = &clib_smp_main;
  clib_smp_per_cpu_main_t * cm;
  uword cpu, bootstrap_result;
  void * stack_top_for_cpu0 = 0;

  m->n_cpus = n_cpus ? n_cpus : linux_guess_n_cpus ();

  clib_smp_init ();

  for (cpu = 0; cpu < m->n_cpus; cpu++)
    {
      void * tls;

      /* Allocate TLS at top of stack. */
      tls = clib_smp_stack_top_for_cpu (m, cpu);
      tls -= m->n_tls_4k_pages << 12;
      memset (tls, 0, m->n_tls_4k_pages << 12);

      cm = vec_elt_at_index (m->per_cpu_mains, cpu);

      if (cpu != 0)
	{
	  linux_clone_bootstrap_args_t a;

	  a.bootstrap_function = bootstrap_function;
	  a.bootstrap_function_arg = bootstrap_function_arg;

	  if (clone ((void *) linux_clone_bootstrap,
		     /* Stack top ends with TLS and extends towards lower addresses. */
		     (void *) tls,
		     (CLONE_VM | CLONE_FS | CLONE_FILES
		      | CLONE_SIGHAND | CLONE_SYSVSEM | CLONE_THREAD
		      | CLONE_SETTLS | CLONE_PARENT_SETTID),
		     &a,
		     &a.tid, tls, &a.tid) < 0)
	    os_panic ();

	  cm->thread_id = a.tid;
	}
      else
	{
	  linux_bind_to_cpu (cpu);

	  stack_top_for_cpu0 = tls;

	  cm->thread_id = getpid ();
	}
    }

  /* For CPU 0, no need for new thread. */
  bootstrap_result
    = clib_calljmp (bootstrap_function,
		    bootstrap_function_arg,
		    stack_top_for_cpu0);

  /* Wait for all CPUs to exit. */
  clib_smp_atomic_add (&m->n_cpus_exited, 1);
  while (m->n_cpus_exited < m->n_cpus)
    sched_yield ();

  /* This effectively switches to normal stack. */
  m->n_cpus = 0;
  m->n_cpus_exited = 0;

  return bootstrap_result;
}
