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

#include <clib/error.h>
#include <clib/longjmp.h>
#include <clib/os.h>
#include <clib/unix.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>		/* writev */
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

clib_error_t * unix_file_n_bytes (char * file, uword * result)
{
  struct stat s;

  if (stat (file, &s) < 0)
    return clib_error_return_unix (0, "stat `%s'", file);

  if (S_ISREG (s.st_mode))
    *result = s.st_size;
  else
    *result = 0;

  return /* no error */ 0;
}

clib_error_t * unix_file_read_contents (char * file, u8 * result, uword n_bytes)
{
  int fd = -1;
  uword n_done, n_left;
  clib_error_t * error = 0;
  u8 * v = result;

  if ((fd = open (file, 0)) < 0)
    {
      error = clib_error_return_unix (0, "open `%s'", file);
      goto done;
    }

  n_left = n_bytes;
  n_done = 0;
  while (n_left > 0)
    {
      int n_read;
      if ((n_read = read (fd, v + n_done, n_left)) < 0)
	{
	  error = clib_error_return_unix (0, "open `%s'", file);
	  goto done;
	}

      /* End of file. */
      if (n_read == 0)
	break;

      n_left -= n_read;
      n_done += n_read;
    }

  if (n_left > 0)
    {
      error = clib_error_return (0, " `%s' expected to read %wd bytes; read only %wd",
				 file, n_bytes, n_bytes - n_left);
      goto done;
    }

 done:
  close (fd);
  return error;
}

clib_error_t * unix_file_contents (char * file, u8 ** result)
{
  uword n_bytes;
  clib_error_t * error = 0;
  u8 * v;

  if ((error = unix_file_n_bytes (file, &n_bytes)))
    return error;

  v = 0;
  vec_resize (v, n_bytes);

  error = unix_file_read_contents (file, v, n_bytes);

  if (error)
    vec_free (v);
  else
    *result = v;

  return error;
}

clib_error_t * unix_proc_file_contents (char * file, u8 ** result)
{
  u8 *rv = 0;
  uword pos;
  int bytes, fd;

  /* Unfortunately, stat(/proc/XXX) returns zero... */
  fd = open (file, O_RDONLY);

  if (fd < 0)
    return clib_error_return_unix (0, "open `%s'", file);

  vec_validate(rv, 4095);
  pos = 0;
  while (1) 
    {
      bytes = read(fd, rv+pos, 4096);
      if (bytes < 0) 
        {
          close (fd);
          vec_free (rv);
          return clib_error_return_unix (0, "read '%s'", file);
        }

      if (bytes == 0) 
        {
          _vec_len(rv) = pos;
          break;
        }
      pos += bytes;
      vec_validate(rv, pos+4095);
    }
  *result = rv;
  close (fd);
  return 0;
}

void os_panic (void)
{ abort (); }

void os_exit (int code)
{ exit (code); }

void os_puts (u8 * string, uword string_length, uword is_error)
  __attribute__ ((weak));

void os_puts (u8 * string, uword string_length, uword is_error)
{
  clib_smp_main_t * m = &clib_smp_main;
  int cpu = os_get_cpu_number ();
  char buf[64];
  int fd = is_error ? 2 : 1;
  struct iovec iovs[2];
  int n_iovs = 0;

  if (m->n_cpus > 1)
    {
      sprintf (buf, "%d: ", cpu);

      iovs[n_iovs].iov_base = buf;
      iovs[n_iovs].iov_len = strlen (buf);
      n_iovs++;
    }

  iovs[n_iovs].iov_base = string;
  iovs[n_iovs].iov_len = string_length;
  n_iovs++;

  if (writev (fd, iovs, n_iovs) < 0)
    ;
}

void os_out_of_memory (void) __attribute__ ((weak));
void os_out_of_memory (void)
{ os_panic (); }

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

static void linux_bind_to_current_cpu (void)
{
  cpu_set_t s;
  uword cpu = os_get_cpu_number ();

  memset (&s, 0, sizeof (s));
  s.__bits[cpu / BITS (s.__bits[0])] = (uword) 1 << (cpu % BITS (s.__bits[0]));
  if (sched_setaffinity (/* pid */ 0, sizeof (s), &s) < 0)
    clib_unix_warning ("sched_setaffinity (cpu %d)", cpu);
}

typedef struct {
  uword (* bootstrap_function) (uword);
  uword bootstrap_function_arg;
} linux_clone_bootstrap_args_t;

static int linux_clone_bootstrap (void * arg)
{
  clib_smp_main_t * m = &clib_smp_main;
  linux_clone_bootstrap_args_t * a = arg;
  uword result;

  linux_bind_to_current_cpu ();
  result = a->bootstrap_function (a->bootstrap_function_arg);
  clib_smp_atomic_add (&m->n_cpus_exited, 1);
  return result;
}

typedef struct
{
  u8 unused[256 * 3];
} linux_thread_local_store_t;

uword os_smp_bootstrap (uword n_cpus,
			void * bootstrap_function,
			uword bootstrap_function_arg)
{
  clib_smp_main_t * m = &clib_smp_main;
  clib_smp_per_cpu_main_t * cm;
  linux_thread_local_store_t * tls = 0;
  uword cpu, bootstrap_result;

  m->n_cpus = n_cpus ? n_cpus : linux_guess_n_cpus ();

  clib_smp_init_stacks_and_heaps (m);

  vec_validate (tls, m->n_cpus - 1);

  /* CPUs my_cpu > 0. */
  for (cpu = 1; cpu < m->n_cpus; cpu++)
    {
      linux_clone_bootstrap_args_t a;
      pid_t tid;

      a.bootstrap_function = bootstrap_function;
      a.bootstrap_function_arg = bootstrap_function_arg;

      if (clone (linux_clone_bootstrap,
		 clib_smp_stack_top_for_cpu (m, cpu),
		 /* flags */
		 (CLONE_VM | CLONE_FS | CLONE_FILES
		  | CLONE_SIGHAND | CLONE_SYSVSEM
		  | CLONE_SETTLS | CLONE_PARENT_SETTID),
		 &a,
		 &tid, &tls[cpu - 1], &tid) < 0)
	os_panic ();

      cm = vec_elt_at_index (m->per_cpu_mains, cpu);
      cm->thread_id = tid;
    }

  /* CPU 0: the master. */
  m->per_cpu_mains[0].thread_id = getpid ();

  linux_bind_to_current_cpu ();

  bootstrap_result
    = clib_calljmp (bootstrap_function,
		    bootstrap_function_arg,
		    clib_smp_stack_top_for_cpu (m, 0));

  /* Wait for all CPUs to exit. */
  clib_smp_atomic_add (&m->n_cpus_exited, 1);
  while (m->n_cpus_exited < m->n_cpus)
    sched_yield ();

  /* This effectively switches to normal stack. */
  m->n_cpus = 0;

  vec_free (tls);

  return bootstrap_result;
}
