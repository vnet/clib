/*
  Copyright (c) 2001, 2002, 2003, 2005 Eliot Dresselhaus

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

#include <clib/vec.h>
#include <clib/mem.h>
#include <clib/zvec.h>
#include <clib/error.h> /* for ASSERT */

/* Consider coding as bitmap, coding = 2^c_0 + 2^c_1 + ... + 2^c_n
   With c_0 < c_1 < ... < c_n.  coding == 0 represents c_n = BITS (uword).

   Unsigned integers i = 0 ... are represented as follows:

       0 <= i < 2^c_0       	(i << 1) | (1 << 0) binary:   i 1
   2^c_0 <= i < 2^c_0 + 2^c_1   (i << 2) | (1 << 1) binary: i 1 0
   ...                                              binary: i 0 ... 0

   Smaller numbers use less bits.  Coding is chosen so that encoding
   of given histogram of typical values gives smallest number of bits.
   The number and position of coding bits c_i are used to best fit the
   histogram of typical values.
*/

/* Compute number of bits needed to encode given histogram. */
static uword zvec_coding_bits (uword coding,
			       uword * histogram_counts,
			       uword min_bits)
{
  uword n_type_bits, n_bits;
  uword this_count, last_count, max_count_index;

  n_bits = 0;
  n_type_bits = 1;
  last_count = 0;
  max_count_index = vec_len (histogram_counts) - 1;

  while (coding != 0)
    {
      uword b, l, i;

      b = first_set (coding);
      l = min_log2 (b);
      i = b - 1;
      if (i > max_count_index)
	i = max_count_index;

      this_count = histogram_counts[i];

      if (this_count == last_count)
	break;

      if (coding == b)
	n_type_bits--;

      n_bits += (this_count - last_count) * (n_type_bits + l);

      /* This coding cannot be minimal: so return. */
      if (n_bits >= min_bits)
	return ~0;

      last_count = this_count;
      coding ^= b;
      n_type_bits++;
    }

  return n_bits;
}

typedef struct {
  u32 count;
} zvec_histogram_t;

static int sort_by_decreasing_count (const void * v1, const void * v2)
{
  const zvec_histogram_t * h1 = v1, * h2 = v2;
  return h2->count - h1->count;
}

uword
_zvec_coding_from_histogram (void * _histogram,
			     uword histogram_len,
			     uword histogram_elt_bytes,
			     uword max_coding_bits,
			     zvec_coding_t * coding_return)
{
  uword coding, min_coding, max_coding;
  uword min_coding_bits, coding_bits;
  uword i, total_count;
  uword * counts;
  zvec_histogram_t * h, * histogram = _histogram;

  if (histogram_len <= 1)
    {
      coding_return->coding = 0;
      coding_return->min_coding_bits = 0;
      coding_return->count = histogram_len == 1 ? histogram[0].count : 0;
      coding_return->n_codes = histogram_len;
      coding_return->ave_coding_length = 0;
      return histogram_len;
    }

  qsort (histogram, histogram_len, histogram_elt_bytes, sort_by_decreasing_count);

  h = histogram;
  total_count = 0;
  counts = vec_new (uword, histogram_len);
  for (i = 0; i < histogram_len; i++)
    {
      total_count += h->count;
      counts[i] = total_count;
      h = (zvec_histogram_t *) ((u8 *) h + histogram_elt_bytes);
    }

  min_coding = coding = vec_len (counts);
  max_coding = max_pow2 (2 * coding);
  min_coding_bits = ~0;
	      
  while (coding < max_coding)
    {
      uword max_n_bits = max_coding_bits;

      /* Coding must have less than requested maximum number of bits. */
      if (max_coding_bits != ~0)
	(void) zvec_encode1 (coding, vec_len (counts) - 1, &max_n_bits);

      if (max_n_bits <= max_coding_bits)
	{
	  coding_bits = zvec_coding_bits (coding, counts, min_coding_bits);
	  if (coding_bits < min_coding_bits)
	    {
	      min_coding_bits = coding_bits;
	      min_coding = coding;
	    }
	}

      coding++;
    }

  if (min_coding_bits == ~0)
    min_coding = 0;

  if (coding_return)
    {
      coding_return->coding = min_coding;
      coding_return->min_coding_bits = min_coding_bits;
      coding_return->count = total_count;
      coding_return->n_codes = vec_len (counts);
      coding_return->ave_coding_length = (f32) min_coding_bits / (f32) total_count;
    }

  vec_free (counts);

  return min_coding;
}
