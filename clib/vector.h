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

#ifndef included_clib_vector_h
#define included_clib_vector_h

#include <clib/clib.h>

/* Vector types. */

/* Determine number of bits in vector register. */
#if defined (__AVX2__)

#define CLIB_VECTOR_WORD_BITS 256

/* AVX only supports 256 bit floating point and does not fully support integer operations. */ 
#elif defined (__SSE2__) || defined (__AVX__) || defined (__ALTIVEC__)

#define CLIB_VECTOR_WORD_BITS 128

#elif defined (__MMX__) || defined (__IWMMXT__)

#define CLIB_VECTOR_WORD_BITS 64

#else

/* Vectors not supported. */
#define CLIB_VECTOR_WORD_BITS 0

#endif

#define __(vt,t0,t1,n)							\
  typedef t1 vt __attribute__ ((vector_size (sizeof (t1) * (n))));	\
  typedef union { vt as_##vt; t0 as_##t0[n]; } vt##_union_t;
#define _(vt,t,n) __(vt,t,t,n)

#if CLIB_VECTOR_WORD_BITS >= 64
_ (i8x8, i8, 8); _ (i16x4, i16, 4); _ (i32x2, i32, 2);
_ (u8x8, u8, 8); _ (u16x4, u16, 4); _ (u32x2, u32, 2);
_ (f32x2, f32, 2);
#endif

#if CLIB_VECTOR_WORD_BITS >= 128
_ (i8x16, i8, 16); _ (i16x8, i16, 8); _ (i32x4, i32, 4); __ (i64x2, i64, signed long long, 2);
_ (u8x16, u8, 16); _ (u16x8, u16, 8); _ (u32x4, u32, 4); __ (u64x2, i64, unsigned long long, 2);
_ (f32x4, f32, 4); _ (f64x2, f64, 2);
#endif

#if CLIB_VECTOR_WORD_BITS >= 256
_ (i8x32, i8, 32); _ (i16x16, i16, 16); _ (i32x8, i32, 8); _ (i64x4, i64, 4);
_ (u8x32, u8, 32); _ (u16x16, u16, 16); _ (u32x8, u32, 8); _ (u64x4, u64, 4);
_ (f32x8, f32, 8); _ (f64x4, f64, 4);
#endif

#undef _
#undef __

/* Number of a given type that fits into a vector register of largest width. */
#define CLIB_VECTOR_WORD_LEN(t) (CLIB_VECTOR_WORD_BITS / (8 * sizeof (t)))

#if CLIB_VECTOR_WORD_BITS > 0

#define _(vt,t)								\
  typedef t vt __attribute__ ((vector_size (CLIB_VECTOR_WORD_BITS / BITS (u8)))); \
  typedef union { vt as_##vt; t as_##t[CLIB_VECTOR_WORD_LEN (t)]; } vt##_union_t;

_ (i8x, i8); _ (i16x, i16); _ (i32x, i32); _ (i64x, i64);
_ (u8x, u8); _ (u16x, u16); _ (u32x, u32); _ (u64x, u64);
_ (f32x, f32); _ (f64x, f64);

#undef _

#else /* CLIB_VECTOR_WORD_BITS > 0 */

/* When we don't have vector types, still define e.g. u32x4_union_t but as an array. */

#define _(t,n)					\
  typedef union {				\
    t as_##t[n];				\
  } t##x##n##_union_t;				\

_ (u8, 16);
_ (u16, 8);
_ (u32, 4);
_ (u64, 2);
_ (i8, 16);
_ (i16, 8);
_ (i32, 4);
_ (i64, 2);
_ (f32, 4);
_ (f64, 2);

#undef _

#endif

#if defined (__SSE2__) && __GNUC__ >= 4
#include <clib/vector_sse2.h>
#endif

#if defined (__ALTIVEC__)
#include <clib/vector_altivec.h>
#endif

#if defined (__IWMMXT__)
#include <clib/vector_iwmmxt.h>
#endif

#if (defined(CLIB_HAVE_VEC128) || defined(CLIB_HAVE_VEC64))
#include <clib/vector_funcs.h>
#endif

#endif /* included_clib_vector_h */
