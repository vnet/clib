/*
  Copyright (c) 2001, 2002, 2003, 2004 Eliot Dresselhaus

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

#ifndef included_pool_h
#define included_pool_h

#include <clib/bitmap.h>
#include <clib/error.h>
#include <clib/mheap.h>

/* Pool of objects of a fixed size. */

typedef struct {
  /* Bitmap of indices of free objects. */
  uword * free_bitmap;

  /* Vector of free indices.  One element for each set bit in bitmap. */
  u32 * free_indices;

  /* Pad so that sizeof (pool_header_t) + sizeof (vec_header_t)
     is a multiple of 8 bytes. */
  u8 pad[MHEAP_ALIGN_PAD_BYTES (sizeof(2*sizeof (void *) + sizeof (_VEC)))];
} pool_header_t;

static inline pool_header_t * pool_header (void * v)
{ return vec_header (v, sizeof (pool_header_t)); }

static inline void pool_validate (void * v)
{
  pool_header_t * p = pool_header (v);
  uword i, n_free_bitmap;

  if (! v)
    return;

  n_free_bitmap = clib_bitmap_count_set_bits (p->free_bitmap);
  ASSERT (n_free_bitmap == vec_len (p->free_indices));
  for (i = 0; i < vec_len (p->free_indices); i++)
    ASSERT (clib_bitmap_get (p->free_bitmap, p->free_indices[i]) == 1);
}

static inline uword pool_elts (void * v)
{
  uword ret = vec_len (v);
  if (v)
    ret -= vec_len (pool_header (v)->free_indices);
  return ret;
}

#define pool_len(p)	vec_len(p)
#define _pool_len(p)	_vec_len(p)

/* Memory usage of pool. */
static inline uword
pool_header_bytes (void * v)
{
  pool_header_t * p = pool_header (v);

  if (! v)
    return 0;

  return vec_bytes (p->free_bitmap) + vec_bytes (p->free_indices);
}

#define pool_bytes(P) (vec_bytes (P) + pool_header_bytes (P))

/* Local variable naming macro. */
#define _pool_var(v) _pool_##v

/* Queries whether pool has at least N_FREE free elements. */
static inline uword
pool_free_elts (void * v)
{
  pool_header_t * p = pool_header (v);
  uword n_free = 0;

  if (v) {
    n_free += vec_len (p->free_indices);

    /* Space left at end of vector? */
    n_free += vec_capacity (v, sizeof (p[0])) - vec_len (v);
  }

  return n_free;
}

/* Allocate an object E from a pool P.
   First search free list.  If nothing is free extend vector of objects. */
#define pool_get_aligned(P,E,A)						\
do {									\
  pool_header_t * _pool_var (p) = pool_header (P);				\
  uword _pool_var (l);							\
									\
  _pool_var (l) = 0;								\
  if (P)								\
    _pool_var (l) = vec_len (_pool_var (p)->free_indices);			\
									\
  if (_pool_var (l) > 0)							\
    {									\
      /* Return free element from free list. */				\
      uword _pool_var (i) = _pool_var (p)->free_indices[_pool_var (l) - 1];		\
      (E) = (P) + _pool_var (i);						\
      _pool_var (p)->free_bitmap =						\
	clib_bitmap_andnoti (_pool_var (p)->free_bitmap, _pool_var (i));		\
      _vec_len (_pool_var (p)->free_indices) = _pool_var (l) - 1;			\
    }									\
  else									\
    {									\
      /* Nothing on free list, make a new element and return it. */	\
      P = _vec_resize (P,						\
		       /* length_increment */ 1,			\
		       /* new size */ (vec_len (P) + 1) * sizeof (P[0]), \
		       /* header bytes */ sizeof (pool_header_t),	\
		       /* align */ (A));				\
      E = vec_end (P) - 1;						\
    }									\
} while (0)

#define pool_get(P,E) pool_get_aligned(P,E,0)

/* Use free bitmap to query whether given element is free. */
#define pool_is_free(P,E)								\
({											\
  pool_header_t * _pool_var (p) = pool_header (P);						\
  uword _pool_var (i) = (E) - (P);								\
  (_pool_var (i) < vec_len (P)) ? clib_bitmap_get (_pool_var (p)->free_bitmap, _pool_i) : 1;	\
})
  
#define pool_is_free_index(P,I) pool_is_free((P),(P)+(I))

/* Free an object E in pool P. */
#define pool_put(P,E)							\
do {									\
  pool_header_t * _pool_var (p) = pool_header (P);			\
  uword _pool_var (l) = (E) - (P);					\
  ASSERT (vec_is_member (P, E));					\
  ASSERT (! pool_is_free (P, E));					\
  memset ((E), 0, sizeof ((E)[0]));					\
									\
  /* Add element to free bitmap and to free list. */			\
  _pool_var (p)->free_bitmap =						\
    clib_bitmap_ori (_pool_var (p)->free_bitmap, _pool_var (l));	\
  vec_add1 (_pool_var (p)->free_indices, _pool_var (l));		\
} while (0)

/* Free element with given index. */		\
#define pool_put_index(p,i)			\
do {						\
  typeof (p) _e = (p) + (i);			\
  pool_put (p, _e);				\
} while (0)

/* Allocate N more free elements to pool. */
#define pool_alloc_aligned(P,N,A)					\
do {									\
  pool_header_t * _p;							\
  (P) = _vec_resize ((P), 0, (vec_len (P) + (N)) * sizeof (P[0]),	\
		     sizeof (pool_header_t), (A));			\
  _p = pool_header (P);							\
  vec_resize (_p->free_indices, (N));					\
  _vec_len (_p->free_indices) -= (N);					\
} while (0)

#define pool_alloc(P,N) pool_alloc_aligned(P,N,0)

static inline void * _pool_free_aligned (void * v, uword align)
{
  pool_header_t * p = pool_header (v);
  if (! v)
    return v;
  clib_bitmap_free (p->free_bitmap);
  vec_free (p->free_indices);
  vec_free_ha (v, sizeof (p[0]), align);
  return 0;
}

#define pool_free_aligned(p,a) (p) = _pool_free_aligned(p,a)
#define pool_free(p) (p) = _pool_free_aligned(p,0)

/* Use free bitmap to iterate through pool. */
#define pool_foreach_region(LO,HI,POOL,BODY)				\
do {									\
  uword _pool_var (i), _pool_var (lo), _pool_var (hi), _pool_var (len);	\
  uword _pool_var (bl), * _pool_var (b);				\
  pool_header_t * _pool_var (p);					\
									\
  _pool_var (p) = pool_header (POOL);					\
  _pool_var (b) = (POOL) ? _pool_var (p)->free_bitmap : 0;		\
  _pool_var (bl) = vec_len (_pool_var (b));				\
  _pool_var (len) = vec_len (POOL);					\
  _pool_var (lo) = 0;							\
									\
  for (_pool_var (i) = 0;						\
       _pool_var (i) <= _pool_var (bl);					\
       _pool_var (i)++)							\
    {									\
      uword _pool_var (m), _pool_var (f);				\
      _pool_var (m) = (_pool_var (i) < _pool_var (bl)			\
		       ? _pool_var (b) [_pool_var (i)]			\
		       : 1);						\
      while (_pool_var (m) != 0)					\
	{								\
	  _pool_var (f) = first_set (_pool_var (m));			\
	  _pool_var (hi) = (_pool_var (i) * BITS (_pool_var (b)[0])	\
			    + min_log2 (_pool_var (f)));		\
	  _pool_var (hi) = (_pool_var (i) < _pool_var (bl)		\
			    ? _pool_var (hi) : _pool_var (len));	\
	  _pool_var (m) ^= _pool_var (f);				\
	  if (_pool_var (hi) > _pool_var (lo))				\
	    {								\
	      (LO) = _pool_var (lo);					\
	      (HI) = _pool_var (hi);					\
	      do { BODY; } while (0);					\
	    }								\
	  _pool_var (lo) = _pool_var (hi) + 1;				\
	}								\
    }									\
} while (0)

#define pool_foreach(VAR,POOL,BODY)					\
do {									\
  uword _pool_foreach_lo, _pool_foreach_hi;				\
  pool_foreach_region (_pool_foreach_lo, _pool_foreach_hi, (POOL),	\
    ({									\
      for ((VAR) = (POOL) + _pool_foreach_lo;				\
	   (VAR) < (POOL) + _pool_foreach_hi;				\
	   (VAR)++)							\
	do { BODY; } while (0);						\
    }));								\
} while (0)

/* Returns pointer to element at given index. */
#define pool_elt_at_index(p,i)			\
({						\
  typeof (p) _e = (p) + (i);			\
  ASSERT (! pool_is_free (p, _e));		\
  _e;						\
})

#endif /* included_pool_h */
