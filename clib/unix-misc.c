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

#include <clib/error.h>
#include <clib/unix.h>
#include <clib/os.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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
  int fd = -1;
  uword n_bytes, n_done, n_left;
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

void os_panic (void)
{ abort (); }

void os_exit (int code)
{ exit (code); }

void os_puts (u8 * string, uword string_length, uword is_error)
  __attribute__ ((weak));
void os_puts (u8 * string, uword string_length, uword is_error)
{ (void) write (is_error ? 2 : 1, string, string_length); }

int os_get_cpu_number () __attribute__ ((weak));
int os_get_cpu_number (void)
{ return 0; }

void os_out_of_memory (void)
{ os_exit (1); }