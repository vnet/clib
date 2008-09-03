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

/* Addition/subtraction. */
#if CLIB_VECTOR_WORD_BITS == 128
#define u8x_add u8x16_add
#define u16x_add u16x8_add
#define u32x_add u32x4_add
#define u64x_add u64x2_add
#define i8x_add i8x16_add
#define i16x_add i16x8_add
#define i32x_add i32x4_add
#define i64x_add i64x2_add
#define u8x_sub u8x16_sub
#define u16x_sub u16x8_sub
#define u32x_sub u32x4_sub
#define u64x_sub u64x2_sub
#define i8x_sub i8x16_sub
#define i16x_sub i16x8_sub
#define i32x_sub i32x4_sub
#define i64x_sub i64x2_sub
#endif

#if CLIB_VECTOR_WORD_BITS == 64
#define u8x_add u8x8_add
#define u16x_add u16x4_add
#define u32x_add u32x2_add
#define i8x_add i8x8_add
#define i16x_add i16x4_add
#define i32x_add i32x2_add
#define u8x_sub u8x8_sub
#define u16x_sub u16x4_sub
#define u32x_sub u32x2_sub
#define i8x_sub i8x8_sub
#define i16x_sub i16x4_sub
#define i32x_sub i32x2_sub
#endif

/* Saturating addition/subtraction. */
#if CLIB_VECTOR_WORD_BITS == 128
#define u8x_add_saturate u8x16_add_saturate
#define u16x_add_saturate u16x8_add_saturate
#define i8x_add_saturate i8x16_add_saturate
#define i16x_add_saturate i16x8_add_saturate
#define u8x_sub_saturate u8x16_sub_saturate
#define u16x_sub_saturate u16x8_sub_saturate
#define i8x_sub_saturate i8x16_sub_saturate
#define i16x_sub_saturate i16x8_sub_saturate
#endif

#if CLIB_VECTOR_WORD_BITS == 64
#define u8x_add_saturate u8x8_add_saturate
#define u16x_add_saturate u16x4_add_saturate
#define i8x_add_saturate i8x8_add_saturate
#define i16x_add_saturate i16x4_add_saturate
#define u8x_sub_saturate u8x8_sub_saturate
#define u16x_sub_saturate u16x4_sub_saturate
#define i8x_sub_saturate i8x8_sub_saturate
#define i16x_sub_saturate i16x4_sub_saturate
#endif

#define _vector_interleave(a,b,t)		\
do {						\
  t _tmp_lo = t##_interleave_lo (a, b);		\
  t _tmp_hi = t##_interleave_hi (a, b);		\
  if (CLIB_ARCH_IS_LITTLE_ENDIAN)		\
    (a) = _tmp_lo, (b) = _tmp_hi;		\
  else						\
    (a) = _tmp_hi, (b) = _tmp_lo;		\
} while (0)

/* 128 bit interleaves. */
#define u8x16_interleave(a,b) _vector_interleave(a,b,u8x16)
#define i8x16_interleave(a,b) _vector_interleave(a,b,i8x16)
#define u16x8_interleave(a,b) _vector_interleave(a,b,u16x8)
#define i16x8_interleave(a,b) _vector_interleave(a,b,i16x8)
#define u32x4_interleave(a,b) _vector_interleave(a,b,u32x4)
#define i32x4_interleave(a,b) _vector_interleave(a,b,i32x4)
#define u64x2_interleave(a,b) _vector_interleave(a,b,u64x2)
#define i64x2_interleave(a,b) _vector_interleave(a,b,i64x2)

/* 64 bit interleaves. */
#define u8x8_interleave(a,b) _vector_interleave(a,b,u8x8)
#define i8x8_interleave(a,b) _vector_interleave(a,b,i8x8)
#define u16x4_interleave(a,b) _vector_interleave(a,b,u16x4)
#define i16x4_interleave(a,b) _vector_interleave(a,b,i16x4)
#define u32x2_interleave(a,b) _vector_interleave(a,b,u32x2)
#define i32x2_interleave(a,b) _vector_interleave(a,b,i32x2)

/* Word sized interleaves. */
#if CLIB_VECTOR_WORD_BITS == 128
#define u8x_interleave u8x16_interleave
#define u16x_interleave u16x8_interleave
#define u32x_interleave u32x4_interleave
#define u64x_interleave u64x2_interleave
#endif

#if CLIB_VECTOR_WORD_BITS == 64
#define u8x_interleave u8x8_interleave
#define u16x_interleave u16x4_interleave
#define u32x_interleave u32x2_interleave
#define u64x_interleave(a,b) /* do nothing */
#endif

/* Vector word sized shifts. */
#if CLIB_VECTOR_WORD_BITS == 128
#define u8x_shift_left u8x16_shift_left
#define i8x_shift_left i8x16_shift_left
#define u16x_shift_left u16x8_shift_left
#define i16x_shift_left i16x8_shift_left
#define u32x_shift_left u32x4_shift_left
#define i32x_shift_left i32x4_shift_left
#define u64x_shift_left u64x2_shift_left
#define i64x_shift_left i64x2_shift_left
#define u8x_shift_right u8x16_shift_right
#define i8x_shift_right i8x16_shift_right
#define u16x_shift_right u16x8_shift_right
#define i16x_shift_right i16x8_shift_right
#define u32x_shift_right u32x4_shift_right
#define i32x_shift_right i32x4_shift_right
#define u64x_shift_right u64x2_shift_right
#define i64x_shift_right i64x2_shift_right
#endif

#if CLIB_VECTOR_WORD_BITS == 64
#define u8x_shift_left u8x8_shift_left
#define i8x_shift_left i8x8_shift_left
#define u16x_shift_left u16x4_shift_left
#define i16x_shift_left i16x4_shift_left
#define u32x_shift_left u32x2_shift_left
#define i32x_shift_left i32x2_shift_left
#define u8x_shift_right u8x8_shift_right
#define i8x_shift_right i8x8_shift_right
#define u16x_shift_right u16x4_shift_right
#define i16x_shift_right i16x4_shift_right
#define u32x_shift_right u32x2_shift_right
#define i32x_shift_right i32x2_shift_right
#endif

#if CLIB_VECTOR_WORD_BITS == 128
#define u8x_splat u8x16_splat
#define i8x_splat i8x16_splat
#define u16x_splat u16x8_splat
#define i16x_splat i16x8_splat
#define u32x_splat u32x4_splat
#define i32x_splat i32x4_splat
#define u64x_splat u64x2_splat
#define i64x_splat i64x2_splat
#endif

#if CLIB_VECTOR_WORD_BITS == 64
#define u8x_splat u8x8_splat
#define i8x_splat i8x8_splat
#define u16x_splat u16x4_splat
#define i16x_splat i16x4_splat
#define u32x_splat u32x2_splat
#define i32x_splat i32x2_splat
#endif

#endif /* included_vector_funcs_h */
