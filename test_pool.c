/*
  Copyright (c) 2001, 2002, 2003 Eliot Dresselhaus

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

#include <clib/mem.h>
#include <clib/pool.h>

#ifdef __KERNEL__
# include <linux/unistd.h>
#else
# include <unistd.h>
#endif

int main (int argc, char * argv[])
{
  int i, n, seed;

  int * p = 0, * e, j, * o = 0;

  n = atoi (argv[1]);
  seed = getpid ();
  srandom (1);

  for (i = 0; i < n; i++)
    {
      if (vec_len (o) < 10 || (random () & 1))
	{
	  pool_get (p, e);
	  j = e - p;
	  *e = j;
	  vec_add1 (o, j);
	}
      else
	{
	  j = random () % vec_len (o);
	  e = p + j;
	  pool_put (p, e);
	  vec_delete (o, 1, j);
	}
    }
  p = pool_free (p);
  vec_free (o);
  return 0;
}
