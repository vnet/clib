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

#ifndef included_clib_vec_bootstrap_h
#define included_clib_vec_bootstrap_h

/* Bootstrap include so that #include <clib/mem.h> can include e.g.
   <clib/mheap.h> which depends on <clib/vec.h>. */
/* Bookeeping header preceding vector elements in memory.
   User header information may preceed standard vec header. */
typedef struct {
  /* Number of elements in vector (NOT its allocated length). */
  u32 len;

  /* Vector data follows. */
  u8 vector_data[0];
} vec_header_t;

/* Given the user's pointer to a vector, find the corresponding vector header. */
#define _vec_find(v)	((vec_header_t *) (v) - 1)

#define _vec_round_size(s) \
  (((s) + sizeof (uword) - 1) &~ (sizeof (uword) - 1))

always_inline uword
vec_header_bytes (uword header_bytes)
{ return round_pow2 (header_bytes + sizeof (vec_header_t), sizeof (vec_header_t)); }

always_inline void *
vec_header (void * v, uword header_bytes)
{ return v - vec_header_bytes (header_bytes); }

always_inline void *
vec_header_end (void * v, uword header_bytes)
{ return v + vec_header_bytes (header_bytes); }

always_inline uword
vec_aligned_header_bytes (uword header_bytes, uword align)
{
  return round_pow2 (header_bytes + sizeof (vec_header_t), align);
}

always_inline void *
vec_aligned_header (void * v, uword header_bytes, uword align)
{ return v - vec_aligned_header_bytes (header_bytes, align); }

always_inline void *
vec_aligned_header_end (void * v, uword header_bytes, uword align)
{ return v + vec_aligned_header_bytes (header_bytes, align); }

/* Finds the user header of a vector with unspecified alignment given
   the user pointer to the vector. */
    
/* Number of elements in vector (lvalue-capable).
   _vec_len (v) does not check for null, but can be used as a lvalue
   (e.g. _vec_len (v) = 99). */
#define _vec_len(v)	(_vec_find(v)->len)

/* Number of elements in vector (rvalue-only, NULL tolerant)
   vec_len (v) checks for NULL, but cannot be used as an lvalue.
   If in doubt, use vec_len... */
#define vec_len(v)	((v) ? _vec_len(v) : 0)

/* Reset vector length to zero. */
#define vec_reset_length(v) do { if (v) _vec_len (v) = 0; } while (0)

/* Number of data bytes in vector. */
#define vec_bytes(v) (vec_len (v) * sizeof (v[0]))

/* Total number of bytes that can fit in vector with current allocation. */
#define vec_capacity(v,b)							\
({										\
  void * _vec_capacity_v = (void *) (v);					\
  uword _vec_capacity_b = (b);							\
  _vec_capacity_b = sizeof (vec_header_t) + _vec_round_size (_vec_capacity_b);	\
  _vec_capacity_v ? clib_mem_size (_vec_capacity_v - _vec_capacity_b) : 0;	\
})

/* Total number of elements that can fit into vector. */
#define vec_max_len(v) (vec_capacity(v,0) / sizeof (v[0]))

/* End (last data address) of vector. */
#define vec_end(v)	((v) + vec_len (v))

/* True if given pointer is within given vector. */
#define vec_is_member(v,e) ((e) >= (v) && (e) < vec_end (v))

/* Get vector element at index i checking that i is in bounds. */
#define vec_elt_at_index(v,i)			\
({						\
  ASSERT ((i) < vec_len (v));			\
  (v) + (i);					\
})

/* Get vector value at index i */
#define vec_elt(v,i) (vec_elt_at_index(v,i))[0]

/* Vector iterator */
#define vec_foreach(var,vec) for (var = (vec); var < vec_end (vec); var++)

/* Vector iterator (reverse) */
#define vec_foreach_backwards(var,vec) \
for (var = vec_end (vec) - 1; var >= (vec); var--)

/* Iterate over vector indices. */
#define vec_foreach_index(var,v) for ((var) = 0; (var) < vec_len (v); (var)++)

#endif /* included_clib_vec_bootstrap_h */
