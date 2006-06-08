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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

clib_error_t * unix_file_contents (char * file, u8 ** result)
{
  int fd;
  struct stat s;
  clib_error_t * error = 0;
  u8 * v;

  fd = -1;
  v = 0;
  if (stat (file, &s) < 0)
    {
      error = clib_error_return_unix (0, "stat `%s'", file);
      goto done;
    }

  if ((fd = open (file, 0)) < 0)
    {
      error = clib_error_return_unix (0, "open `%s'", file);
      goto done;
    }

  vec_resize (v, s.st_size);
  if (read (fd, v, vec_len (v)) != vec_len (v))
    {
      error = clib_error_return_unix (0, "open `%s'", file);
      goto done;
    }

 done:
  close (fd);
  if (error)
    vec_free (v);
  else
    *result = v;

  return error;
}

