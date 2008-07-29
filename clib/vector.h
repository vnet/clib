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

#if defined (__MMX__) || defined (__IWMMXT__)
#define CLIB_HAVE_VEC64
#endif

#if defined (__SSE2__)
#define CLIB_HAVE_VEC128
#endif

/* 128 implies 64 */
#ifdef CLIB_HAVE_VEC128
#define CLIB_HAVE_VEC64
#endif

#if !(defined(CLIB_HAVE_VEC128) || defined(CLIB_HAVE_VEC64))
#error "vector types not supported."
#endif

#define _(n) __attribute__ ((vector_size (n)))

#ifdef CLIB_HAVE_VEC64
/* Signed 64 bit. */
typedef i8 i8x8 _ (8);
typedef i16 i16x4 _ (8);
typedef i32 i32x2 _ (8);

/* Unsigned 64 bit. */
typedef u8 u8x8 _ (8);
typedef u16 u16x4 _ (8);
typedef u32 u32x2 _ (8);

/* Floating point 64 bit. */
typedef f32 f32x2 _ (8);
#endif /* CLIB_HAVE_VEC64 */

#ifdef CLIB_HAVE_VEC128
/* Signed 128 bit. */
typedef i8 i8x16 _ (16);
typedef i16 i16x8 _ (16);
typedef i32 i32x4 _ (16);
typedef i64 i64x2 _ (16);

/* Unsigned 128 bit. */
typedef u8 u8x16 _ (16);
typedef u16 u16x8 _(16);
typedef u32 u32x4 _(16);
typedef u64 u64x2 _(16);

typedef f32 f32x4 _(16);
typedef f64 f64x2 _(16);
#endif /* CLIB_HAVE_VEC128 */

/* Vector word sized types. */
#ifdef CLIB_HAVE_VEC128
typedef  i8  i8x _ (16);
typedef i16 i16x _ (16);
typedef i32 i32x _ (16);
typedef i64 i64x _ (16);
typedef  u8  u8x _ (16);
typedef u16 u16x _ (16);
typedef u32 u32x _ (16);
typedef u64 u64x _ (16);
#else /* CLIB_HAVE_VEC64 */
typedef  i8  i8x _ (8);
typedef i16 i16x _ (8);
typedef i32 i32x _ (8);
typedef i64 i64x _ (8);
typedef  u8  u8x _ (8);
typedef u16 u16x _ (8);
typedef u32 u32x _ (8);
typedef u64 u64x _ (8);
#endif

#undef _

#define VECTOR_WORD_TYPE(t) t##x
#define VECTOR_WORD_TYPE_LEN(t) (sizeof (VECTOR_WORD_TYPE(t)) / sizeof (t))

#if defined (CLIB_HAVE_VEC128)

#if defined (__SSE2__) && __GNUC__ >= 4
#include <clib/vector_sse2.h>
#endif

#endif

#include <clib/vector_funcs.h>

#endif /* included_clib_vector_h */
