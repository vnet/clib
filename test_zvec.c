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

#include <clib/zvec.h>
#include <clib/format.h>
#include <clib/random.h>

static int verbose;
#define if_verbose(format,args...) \
  if (verbose) { clib_warning(format, ## args); }

int test_zvec_main (unformat_input_t * input)
{
  uword n_iterations;
  uword i;
  u32 seed;
  
  n_iterations = 1024;
  seed = 0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (0 == unformat (input, "iter %d", &n_iterations)
	  && 0 == unformat (input, "seed %d", &seed))
	clib_error ("unknown input `%U'", format_unformat_error, input);
    }

  if_verbose   ("%d iterations, seed %d\n", n_iterations, seed);

  for (i = 0; i < n_iterations; i++)
    {
      uword coding, data, d[2], limit, n_zdata_bits;

      if (seed)
	coding = random_u32 (&seed);
      else
	coding = i;

      limit = coding - 1;
      if (limit > (1 << 16))
	limit = 1 << 16;
      for (data = 0; data <= limit; data++)
	{
	  d[0] = zvec_encode1 (coding, data, &n_zdata_bits);

	  if (coding != 0)
	    ASSERT ((d[0] >> n_zdata_bits) == 0);

	  d[1] = zvec_decode1 (coding, d[0]);
	  ASSERT (data == d[1]);
	}
    }

  return 0;
}

#ifdef CLIB_UNIX
int main (int argc, char * argv[])
{
  unformat_input_t i;
  int ret;

  verbose = (argc > 1);
  unformat_init_command_line (&i, argv);
  ret = test_zvec_main (&i);
  unformat_free (&i);

  return ret;
}
#endif /* CLIB_UNIX */

