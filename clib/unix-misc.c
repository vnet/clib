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

#include <sys/types.h>
#include <sys/stat.h>
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

  if (m->n_cpus > 1)
    {
      sprintf (buf, "%d: ", cpu);
      (void) write (fd, buf, strlen (buf));
    }
  (void) write (fd, string, string_length);
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
  linux_clone_bootstrap_args_t * a = arg;
  linux_bind_to_current_cpu ();
  exit (a->bootstrap_function (a->bootstrap_function_arg));
}

uword os_smp_bootstrap (uword n_cpus,
			void * bootstrap_function,
			uword bootstrap_function_arg)
{
  clib_smp_main_t * m = &clib_smp_main;
  int i;

  m->n_cpus = n_cpus ? n_cpus : linux_guess_n_cpus ();

  clib_smp_init_stacks_and_heaps (m);

  for (i = 1; i < m->n_cpus; i++)
    {
      linux_clone_bootstrap_args_t a;
      a.bootstrap_function = bootstrap_function;
      a.bootstrap_function_arg = bootstrap_function_arg;
      if (clone (linux_clone_bootstrap,
		 clib_smp_stack_top_for_cpu (m, i),
		 /* flags */ 0,
		 &a) < 0)
	clib_error ("clone");
    }

  return clib_calljmp (bootstrap_function,
		       bootstrap_function_arg,
		       clib_smp_stack_top_for_cpu (m, 0));
}
