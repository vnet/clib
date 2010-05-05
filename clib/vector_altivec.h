/*
  Copyright (c) 2009 Eliot Dresselhaus

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

#ifndef included_vector_altivec_h
#define included_vector_altivec_h

/* Unaligned loads/stores. */

#define _(t)						\
  always_inline void t##_store_unaligned (t x, t * a)	\
  { clib_mem_unaligned (a, t) = x; }			\
  always_inline t t##_load_unaligned (t * a)		\
  { return clib_mem_unaligned (a, t); }

_ (u8x16)
_ (u16x8)
_ (u32x4)
_ (u64x2)
_ (i8x16)
_ (i16x8)
_ (i32x4)
_ (i64x2)

#undef _

#endif /* included_vector_altivec_h */
