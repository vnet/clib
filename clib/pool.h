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

/** \file 

    Pools are built from clib vectors and bitmaps. Use pools when
    repeatedly allocating and freeing fixed-size data. Pools are
    fast, and avoid memory fragmentation.
*/


/** \brief Pool of objects of a fixed size. 
*/

typedef struct {
  /** Bitmap of indices of free objects. */
  uword * free_bitmap;

  /** Vector of free indices.  One element for each set bit in bitmap. */
  u32 * free_indices;

  /** Pad so that sizeof (pool_header_t) + sizeof (vec_header_t)
     is a multiple of 8 bytes. */
  u8 pad[MHEAP_ALIGN_PAD_BYTES (sizeof(2*sizeof (void *) + sizeof (vec_header_t)))];
} pool_header_t;

/** \brief Get pool header from user pool pointer */
always_inline pool_header_t * pool_header (void * v)
{ return vec_header (v, sizeof (pool_header_t)); }

/** \brief Validate a pool */
always_inline void pool_validate (void * v)
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

always_inline void pool_header_validate_index (void * v, uword index)
{
  pool_header_t * p = pool_header (v);

  if (v)
    vec_validate (p->free_bitmap, index / BITS (uword));
}

#define pool_validate_index(v,i)				\
do {								\
  uword __pool_validate_index = (i);				\
  vec_validate_ha ((v), __pool_validate_index,			\
		   sizeof (pool_header_t), /* align */ 0);	\
  pool_header_validate_index ((v), __pool_validate_index);	\
} while (0)

/** \brief Number of active elements in a pool */
always_inline uword pool_elts (void * v)
{
  uword ret = vec_len (v);
  if (v)
    ret -= vec_len (pool_header (v)->free_indices);
  return ret;
}

/** \brief Number of elements in pool vector

    Note: you probably want to call pool_elts() instead
*/
#define pool_len(p)	vec_len(p)
/** \brief Number of elements in pool vector (usable as an lvalue)

    Note: you probably don't want to use this macro
*/
#define _pool_len(p)	_vec_len(p)

/**\brief Memory usage of pool header */
always_inline uword
pool_header_bytes (void * v)
{
  pool_header_t * p = pool_header (v);

  if (! v)
    return 0;

  return vec_bytes (p->free_bitmap) + vec_bytes (p->free_indices);
}

/**\brief Memory usage of pool */
#define pool_bytes(P) (vec_bytes (P) + pool_header_bytes (P))

/**\brief Local variable naming macro. */
#define _pool_var(v) _pool_##v

/**\brief Queries whether pool has at least N_FREE free elements. */
always_inline uword
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

/** \brief Allocate an object E from a pool P (general version)

   First search free list.  If nothing is free extend vector of objects
*/
#define pool_get_aligned(P,E,A)						\
do {									\
  pool_header_t * _pool_var (p) = pool_header (P);			\
  uword _pool_var (l);							\
									\
  _pool_var (l) = 0;							\
  if (P)								\
    _pool_var (l) = vec_len (_pool_var (p)->free_indices);		\
									\
  if (_pool_var (l) > 0)						\
    {									\
      /* Return free element from free list. */				\
      uword _pool_var (i) = _pool_var (p)->free_indices[_pool_var (l) - 1]; \
      (E) = (P) + _pool_var (i);					\
      _pool_var (p)->free_bitmap =					\
	clib_bitmap_andnoti (_pool_var (p)->free_bitmap, _pool_var (i)); \
      _vec_len (_pool_var (p)->free_indices) = _pool_var (l) - 1;	\
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

/** \brief Allocate an object E from a pool P (unspecified alignment) */
#define pool_get(P,E) pool_get_aligned(P,E,0)

/** \brief Use free bitmap to query whether given element is free */
#define pool_is_free(P,E)						\
({									\
  pool_header_t * _pool_var (p) = pool_header (P);			\
  uword _pool_var (i) = (E) - (P);					\
  (_pool_var (i) < vec_len (P)) ? clib_bitmap_get (_pool_var (p)->free_bitmap, _pool_i) : 1; \
})
  
/** \brief Use free bitmap to query whether given index is free */
#define pool_is_free_index(P,I) pool_is_free((P),(P)+(I))

/** \brief Free an object E in pool P */
#define pool_put(P,E)							\
do {									\
  pool_header_t * _pool_var (p) = pool_header (P);			\
  uword _pool_var (l) = (E) - (P);					\
  ASSERT (vec_is_member (P, E));					\
  ASSERT (! pool_is_free (P, E));					\
									\
  /* Add element to free bitmap and to free list. */			\
  _pool_var (p)->free_bitmap =						\
    clib_bitmap_ori (_pool_var (p)->free_bitmap, _pool_var (l));	\
  vec_add1 (_pool_var (p)->free_indices, _pool_var (l));		\
} while (0)

/** \brief Free pool element with given index. */
#define pool_put_index(p,i)			\
do {						\
  typeof (p) _e = (p) + (i);			\
  pool_put (p, _e);				\
} while (0)

/** \brief Allocate N more free elements to pool (general version) */
#define pool_alloc_aligned(P,N,A)					\
do {									\
  pool_header_t * _p;							\
  (P) = _vec_resize ((P), 0, (vec_len (P) + (N)) * sizeof (P[0]),	\
		     sizeof (pool_header_t), (A));			\
  _p = pool_header (P);							\
  vec_resize (_p->free_indices, (N));					\
  _vec_len (_p->free_indices) -= (N);					\
} while (0)

/** \brief Allocate N more free elements to pool (unspecified alignment) */
#define pool_alloc(P,N) pool_alloc_aligned(P,N,0)

/** \brief low-level free pool operator (do not call directly) */
always_inline void * _pool_free (void * v)
{
  pool_header_t * p = pool_header (v);
  if (! v)
    return v;
  clib_bitmap_free (p->free_bitmap);
  vec_free (p->free_indices);
  vec_free_h (v, sizeof (p[0]));
  return 0;
}

/** \brief Free a pool. */
#define pool_free(p) (p) = _pool_free(p)

/** \brief Optimized iteration through pool 

    @param LO pointer to first element in chunk
    @param HI pointer to last element in chunk
    @param POOL pool to iterate across
    @param BODY operation to perform

    Optimized version which assumes that BODY is smart enough to 
    process multiple (LOW,HI) chunks. See also pool_foreach().
 */
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

/** \brief Iterate through pool 

    @param VAR variable of same type as pool vector
    @param POOL pool to iterate across
    @param BODY operation to perform. See the example below.

    call BODY with each active pool element.

    Example:
    proc_t *procs;   // a pool of processes <br>
    proc_t *proc;    // pointer to one process <br>

    pool_foreach (proc, procs, ({
    <br>
    &nbsp;&nbsp;if (proc->state != PROC_STATE_RUNNING)<br>
    &nbsp;&nbsp;&nbsp;&nbsp;continue;
    <br>
    &nbsp;&nbsp;<i>check a running proc in some way</i><br>
    &nbsp;&nbsp;}));

    It is a bad idea to allocate or free pool element from within
    pool_foreach. Build a vector of indices and dispose of them later.

    Because pool_foreach is a macro, syntax errors can be difficult to
    find inside BODY, let alone actual code bugs. One can temporarily
    split a complex pool_foreach into a trivial pool_foreach which
    builds a vector of active indices, and a vec_foreach() (or plain
    for-loop) to walk the active index vector.
 */
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

/** \brief Returns pointer to element at given index

    ASSERTs that the supplied index is valid. Even though
    one can write correct code of the form "p = pool_base + index",
    use of pool_elt_at_index is strongly suggested. 
 */
#define pool_elt_at_index(p,i)			\
({						\
  typeof (p) _e = (p) + (i);			\
  ASSERT (! pool_is_free (p, _e));		\
  _e;						\
})

/** \brief Return next occupied pool index after i, useful for safe iteration */
#define pool_next_index(P,I)                                            \
({                                                                      \
  pool_header_t * _pool_var (p) = pool_header (P);                      \
  uword _pool_var (rv) = (I) + 1;                                       \
                                                                        \
  _pool_var(rv) =                                                       \
    (_pool_var (rv) < vec_len (P) ?                                     \
     clib_bitmap_next_clear (_pool_var (p)->free_bitmap, _pool_var(rv)) \
     : ~0);                                                             \
  _pool_var(rv);                                                        \
})

#endif /* included_pool_h */
