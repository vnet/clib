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

#ifndef included_clib_bitops_h
#define included_clib_bitops_h

/* Population count from Hacker's Delight. */
ALWAYS_INLINE (static inline uword count_set_bits (uword x))
{
#if uword_bits == 64
  const uword c1 = 0x5555555555555555;
  const uword c2 = 0x3333333333333333;
  const uword c3 = 0x0f0f0f0f0f0f0f0f;
#else
  const uword c1 = 0x55555555;
  const uword c2 = 0x33333333;
  const uword c3 = 0x0f0f0f0f;
#endif

  /* Sum 1 bit at a time. */
  x = x - ((x >> (uword) 1) & c1);

  /* 2 bits at a time. */
  x = (x & c2) + ((x >> (uword) 2) & c2);

  /* 4 bits at a time. */
  x = (x + (x >> (uword) 4)) & c3;

  /* 8, 16, 32 bits at a time. */
  x = x + (x >> (uword) 8);
  x = x + (x >> (uword) 16);
#if uword_bits == 64
  x = x + (x >> (uword) 32);
#endif

  return x & (BITS (uword) - 1);
}

/* Based on "Hacker's Delight" code from GLS. */
typedef struct {
  uword masks[1 + log2_uword_bits];
} compress_main_t;

static inline void
compress_init (compress_main_t * cm, uword mask)
{
  uword q, m, zm, n, i;

  m = ~mask;
  zm = mask;

  cm->masks[0] = mask;
  for (i = 0; i < log2_uword_bits; i++)
    {
      q = m;
      m ^= m << 1;
      m ^= m << 2;
      m ^= m << 4;
      m ^= m << 8;
      m ^= m << 16;
#if uword_bits > 32
      m ^= m << (uword) 32;
#endif
      cm->masks[1 + i] = n = (m << 1) & zm;
      m = q & ~m;
      q = zm & n;
      zm = zm ^ q ^ (q >> (1 << i));
    }
}

ALWAYS_INLINE (static inline uword
	       compress_bits (compress_main_t * cm, uword x))
{
  uword q, r;

  r = x & cm->masks[0];
  q = r & cm->masks[1]; r ^= q ^ (q >> 1);
  q = r & cm->masks[2]; r ^= q ^ (q >> 2);
  q = r & cm->masks[3]; r ^= q ^ (q >> 4);
  q = r & cm->masks[4]; r ^= q ^ (q >> 8);
  q = r & cm->masks[5]; r ^= q ^ (q >> 16);
#if uword_bits > 32
  q = r & cm->masks[6]; r ^= q ^ (q >> (uword) 32);
#endif

  return r;
}

#endif /* included_clib_bitops_h */
