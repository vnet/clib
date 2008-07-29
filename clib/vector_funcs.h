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

#ifndef included_vector_funcs_h
#define included_vector_funcs_h

#include <clib/byte_order.h>

#define _vector_interleave(a,b,t)		\
do {						\
  t _tmp_lo = t##_interleave_lo (a, b);		\
  t _tmp_hi = t##_interleave_hi (a, b);		\
  if (CLIB_ARCH_IS_LITTLE_ENDIAN)		\
    (a) = _tmp_lo, (b) = _tmp_hi;		\
  else						\
    (a) = _tmp_hi, (b) = _tmp_lo;		\
} while (0)

#define u8x16_interleave(a,b) _vector_interleave(a,b,u8x16)
#define i8x16_interleave(a,b) _vector_interleave(a,b,i8x16)
#define u16x8_interleave(a,b) _vector_interleave(a,b,u16x8)
#define i16x8_interleave(a,b) _vector_interleave(a,b,i16x8)
#define u32x4_interleave(a,b) _vector_interleave(a,b,u32x4)
#define i32x4_interleave(a,b) _vector_interleave(a,b,i32x4)
#define u64x2_interleave(a,b) _vector_interleave(a,b,u64x2)
#define i64x2_interleave(a,b) _vector_interleave(a,b,i64x2)

#endif /* included_vector_funcs_h */
