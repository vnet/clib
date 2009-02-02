/*
  Copyright (c) 2008 Eliot Dresselhaus

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

#include <clib/elf.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef CLIB_UNIX
#error "unix only"
#endif

int main (int argc, char * argv[])
{
  int fd, l, n;
  u8 * v = 0;

  fd = open (argv[1], 0);
  if (fd < 0)
    abort ();
  {
    struct stat fs;
    if (fstat (fd, &fs) < 0)
      abort ();
    vec_resize (v, fs.st_size);
  }

  for (l = 0; l < vec_len (v); )
    {
      n = read (fd, v + l, vec_len (v) - l);
      if (n < 0)
	abort ();
      if (n == 0)
	break;
      l += n;
    }

  close (fd);

  if (l != vec_len (v))
    abort ();

  {
    elf_main_t em;

    elf_parse (&em, v, vec_len (v));
    vec_free (v);

    fformat (stdout, "%U", format_elf_main, &em);
  }

  return 0;
}
