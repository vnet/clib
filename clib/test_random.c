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

#include <clib/format.h>
#include <clib/bitmap.h>

int test_random_main (unformat_input_t * input)
{
  uword n_iterations;
  uword i, repeat_count;
  uword * bitmap = 0;
  uword print;
  u32 seed;
  
  n_iterations = 1000;
  seed = 0;
  print = 1 << 24;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (0 == unformat (input, "iter %d", &n_iterations)
	  && 0 == unformat (input, "print %d", &print)
	  && 0 == unformat (input, "seed %d", &seed))
	clib_error ("unknown input `%U'", format_unformat_error, input);
    }

  if (! seed)
    seed = random_default_seed ();

  if (n_iterations == 0)
    n_iterations = random_u32_max ();

  clib_warning ("%d iterations, seed %d\n", n_iterations, seed);

  repeat_count = 0;
  for (i = 0; i < n_iterations; i++)
    {
      uword r = random_u32 (&seed);
      uword b, ri, rj;

      ri = r / BITS (bitmap[0]);
      rj = (uword) 1 << (r % BITS (bitmap[0]));

      vec_validate (bitmap, ri);
      b = bitmap[ri];

      if (b & rj)
	goto repeat;
      b |= rj;
      bitmap[ri] = b;

      if (0 == (i & (print - 1)))
	fformat (stderr, "0x%08x iterations %d repeats\n", i, repeat_count);
      continue;

    repeat:
      fformat (stderr, "repeat found at iteration  %d/%d\n", i, n_iterations);
      repeat_count++;
      continue;
    }

  return 0;
}

#ifdef CLIB_UNIX
int main (int argc, char * argv[])
{
  unformat_input_t i;
  int ret;

  unformat_init_command_line (&i, argv);
  ret = test_random_main (&i);
  unformat_free (&i);

  return ret;
}
#endif /* CLIB_UNIX */

