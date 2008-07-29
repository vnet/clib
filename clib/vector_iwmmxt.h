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

#ifndef included_vector_iwmmxt_h
#define included_vector_iwmmxt_h

#include <clib/error.h>		/* for ASSERT */

/* 64 bit interleaves. */
static always_inline u8x8 u8x8_interleave_hi (u8x8 a, u8x8 b)
{ return __builtin_arm_wunpckihb (a, b); }

static always_inline u8x8 u8x8_interleave_lo (u8x8 a, u8x8 b)
{ return __builtin_arm_wunpckilb (a, b); }

static always_inline u16x4 u16x4_interleave_hi (u16x4 a, u16x4 b)
{ return __builtin_arm_wunpckihh (a, b); }

static always_inline u16x4 u16x4_interleave_lo (u16x4 a, u16x4 b)
{ return __builtin_arm_wunpckilh (a, b); }

static always_inline u32x2 u32x2_interleave_hi (u32x2 a, u32x2 b)
{ return __builtin_arm_wunpckihw (a, b); }

static always_inline u32x2 u32x2_interleave_lo (u32x2 a, u32x2 b)
{ return __builtin_arm_wunpckilw (a, b); }

static always_inline u32x2 u32x2_splat (u32 a)
{
  u32x2 x = {a};
  x = u32x2_interleave_lo (x, x);
  return x;
 }

static always_inline u16x4 u16x4_splat (u16 a)
{
  u32 t = (u32) a | ((u32) a << 16);
  return u32x2_splat (t);
}

static always_inline u8x8 u8x8_splat (u8 a)
{
  u32 t = (u32) a | ((u32) a << 8);
  t |= t << 16;
  return u32x2_splat (t);
}

#define i32x2_splat u32x2_splat
#define i16x4_splat u16x4_splat
#define i8x8_splat u8x8_splat

/* 64 bit shifts. */

/* As of July 2008 the __builtin_arm shifts cause gcc-4.3.1 to crash
   so we use asm versions. */
#define _(t,lr,f)				\
  static always_inline t			\
  t##_shift_##lr (t x, int i)			\
  {						\
    i16x4 y;					\
    asm (#f " %[y], %[x], %[shift]"		\
	 : [y] "=y" (y)				\
	 : [x] "y" (x), [shift] "i" (i));	\
    return y;					\
  }

_ (u16x4, left, wsllhi)
_ (u32x2, left, wsllwi)
_ (u16x4, right, wsrlhi)
_ (u32x2, right, wsrlwi)
_ (i16x4, left, wsllhi)
_ (i32x2, left, wsllwi)
_ (i16x4, right, wsrahi)
_ (i32x2, right, wsrawi)

#undef _

#endif /* included_vector_iwmmxt_h */
