/*
  Copyright (c) 2011 by Cisco Systems, Inc.

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
  int i;
  uword next;
  u32 *tp = 0;
  u32 *junk;

  for (i = 0; i < 70; i++)
    pool_get (tp, junk);
  
  pool_put_index (tp, 1);
  pool_put_index (tp, 65);

  next = ~0;
  do {
    next = pool_next_index (tp, next);
    fformat (stdout, "next index %d\n", next);
  } while (next != ~0);

  return 0;
}
