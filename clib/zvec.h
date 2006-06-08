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

#ifndef included_zvec_h
#define included_zvec_h

#include <clib/clib.h>
#include <clib/error.h>		/* for ASSERT */

static inline uword
zvec_decode1 (uword coding, uword zdata)
{
  uword c, d, result;
  uword explicit_end, implicit_end, end;

  result = 0;
  do {
    c = first_set (coding);
    implicit_end = c == coding;
    explicit_end = (zdata & 1) &~ implicit_end;
    end = explicit_end | implicit_end;
    d = (zdata >> explicit_end) & (c - 1);
    result += end ? d : c;
    coding ^= c;
    zdata >>= 1;
  } while (! end);

  return result;
}

static inline uword
zvec_encode1 (uword coding,
	      uword data,
	      uword * n_result_bits)
{
  uword c, shift, result;
  uword end, explicit_end, implicit_end;

  /* Data must be in range.  Note special coding == 0
     would break for data - 1 <= coding. */
  ASSERT (data <= coding - 1);

  shift = 0;
  while (1)
    {
      c = first_set (coding);
      implicit_end = c == coding;
      explicit_end = ((data & (c - 1)) == data) &~ implicit_end;
      end = explicit_end | implicit_end;
      if (end)
	{
	  result = ((data << explicit_end) | explicit_end) << shift;
	  if (n_result_bits)
	    *n_result_bits =
	      /* data bits */ (c == 0 ? BITS (uword) : min_log2 (c))
	      /* shift bits */ + shift + explicit_end;
	  return result;
	}
      data -= c;
      coding ^= c;
      shift++;
    }

  /* Never reached. */
  ASSERT (0);
  return ~0;
}

typedef struct {
  u32 coding;
  u32 count;
  u32 n_codes;
  u32 min_coding_bits;
  f32 ave_coding_length;
} zvec_coding_t;

#define zvec_coding_from_histogram(h,l,max_bits,zc) \
  _zvec_coding_from_histogram ((h), (l), sizeof (h[0]), (max_bits), (zc))

uword
_zvec_coding_from_histogram (void * _histogram,
			     uword histogram_len,
			     uword histogram_elt_bytes,
			     uword max_coding_bits,
			     zvec_coding_t * coding_return);

typedef struct {
  u32 n_elts, n_bits;
} zvec_header_t;

#define zvec_header(z) ((zvec_header_t *) vec_header (z, sizeof (zvec_header_t)))
#define zvec_elts(z)   ((z) ? zvec_header(z)->n_elts : 0)
#define zvec_n_bits(z) ((z) ? zvec_header(z)->n_bits : 0)

#endif /* included_zvec_h */
