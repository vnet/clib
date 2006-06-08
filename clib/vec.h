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

#ifndef included_vec_h
#define included_vec_h

#include <clib/clib.h>          /* word, etc */
#include <clib/mem.h>           /* clib_mem_free */
#include <clib/string.h>	/* memcpy, memmove */

/* CLIB vectors are ubiquitous dynamically resized arrays with by user
   defined "headers".  Many CLIB data structures (e.g. hash, heap,
   pool) are vectors with various different headers.

   The memory layout looks like this:

                      user header (aligned to uword boundary)
                      vector length: number of elements
user's pointer ->     vector element #1
                      vector element #2
                      ...

   A user is returned a pointer to element # 1.
   Null pointer vectors are valid and mean a zero length vector.
   You can also have an allocated non-null zero length vector by just
   setting the vector length field to zero (e.g. _vec_len (v) = 0).

   Typically, the header is not present.  Headers allow for other
   data structures to be built atop CLIB vectors.

   Users may specify the alignment for data elements via the
   vec_*_aligned macros.

   Vectors elements can be any C type e.g. (int, double, struct bar).
   This is also true for data types built atop vectors (e.g. heap,
   pool, etc.).
 */

/* Bookeeping header preceding vector elements in memory.
   User header information may preceed standard vec header. */
typedef struct {
  uword len;
} _VEC;

#define _vec_find(v)	((_VEC *) (v) - 1)

#define _vec_round_size(s) \
  (((s) + sizeof (uword) - 1) &~ (sizeof (uword) - 1))

ALWAYS_INLINE (static inline uword
	       vec_header_bytes_ha (uword header_bytes,
				    uword align_bytes))
{
    header_bytes = _vec_round_size (header_bytes) + sizeof (_VEC);
    if (align_bytes > 0 && header_bytes < 8)
	header_bytes = 8;
    return header_bytes;
}

ALWAYS_INLINE (static inline void *
	       vec_header_ha (void * v, uword header_bytes,
			      uword align_bytes))
{ return v - vec_header_bytes_ha (header_bytes, align_bytes); }

static inline void *
vec_header_end_ha (void * v, uword header_bytes, uword align_bytes)
{ return v + vec_header_bytes_ha (header_bytes, align_bytes); }

#define vec_header(v,header_bytes)     vec_header_ha(v,header_bytes,0)
#define vec_header_end(v,header_bytes) vec_header_end_ha(v,header_bytes,0)

/* Number of elements in vector.
   0 is always the null zero-element vector.
   _vec_len (v) does not check for null, but can be used as a lvalue
   (e.g. _vec_len (v) = 99). */
#define _vec_len(v)	(_vec_find(v)->len)
#define vec_len(v)	((v) ? _vec_len(v) : 0)

/* Number of data bytes in vector. */
#define vec_bytes(v) (vec_len (v) * sizeof (v[0]))

/* Total number of bytes that can fit in vector with current allocation. */
#define vec_capacity(v,b)							\
({										\
  void * _vec_capacity_v = (void *) (v);					\
  uword _vec_capacity_b = (b);							\
  _vec_capacity_b = sizeof (_VEC) + _vec_round_size (_vec_capacity_b);		\
  _vec_capacity_v ? clib_mem_size (_vec_capacity_v - _vec_capacity_b) : 0;	\
})

/* Total number of elements that can fit into vector. */
#define vec_max_len(v) (vec_capacity(v,0) / sizeof (v[0]))

/* End (last data address) of vector. */
#define vec_end(v)	((v) + vec_len (v))

/* True if given pointer is within given vector. */
#define vec_is_member(v,e) ((e) >= (v) && (e) < vec_end (v))

#define vec_elt_at_index(v,i)			\
({						\
  ASSERT ((i) < vec_len (v));			\
  (v) + (i);					\
})

#define vec_elt(v,i) (vec_elt_at_index(v,i))[0]

/* Vector iterator. */ 
#define vec_foreach(var,vec) for (var = (vec); var < vec_end (vec); var++)

#define vec_foreach_backwards(var,vec) \
for (var = vec_end (vec) - 1; var >= (vec); var--)

/* Low-level resize function. */
extern void *
_vec_resize (void * _v,
	     word length_increment,
	     uword data_bytes,
	     uword header_bytes,
	     uword data_align);

/* Local variable naming macro (prevents collisions with other macro naming). */
#define _v(var) _vec_##var

/* Many macros have _a variants supporting alignment of vector data
   and _h variants supporting non zero length vector headers.
   The _ha variants support both. */

/* Add N elements to end of given vector V, return pointer to start of vector.
   Vector will have room for H header bytes and will have user's data aligned
   at alignment A (rounded to next power of 2). */
#define vec_resize_ha(V,N,H,A)							\
do {										\
  word _v(n) = (N);								\
  word _v(l) = vec_len (V);							\
  V = _vec_resize ((V), _v(n), (_v(l) + _v(n)) * sizeof ((V)[0]), (H), (A));	\
} while (0)

#define vec_resize(V,N)     vec_resize_ha(V,N,0,0)
#define vec_resize_aligned(V,N,A) vec_resize_ha(V,N,0,A)

/* Create new vector of given type and length. */
#define vec_new_ha(T,N,H,A)					\
({								\
  word _v(n) = (N);						\
  _vec_resize ((T *) 0, _v(n), _v(n) * sizeof (T), (H), (A));	\
})

#define vec_new(T,N)           vec_new_ha(T,N,0,0)
#define vec_new_aligned(T,N,A) vec_new_ha(T,N,0,A)

/* Free vector's memory. */
#define vec_free_ha(V,H,A)				\
do {							\
  if (V)						\
    {							\
      clib_mem_free (vec_header_ha ((V), (H), (A)));	\
      V = 0;						\
    }							\
} while (0)

#define vec_free(V) vec_free_ha(V,0,0)
#define vec_free_aligned(V,A) vec_free_ha(V,0,A)
#define vec_free_h(V,H) vec_free_ha(V,H,0)
#define vec_free_header(h) clib_mem_free (h)

/* Return copy of vector. */
#define vec_dup_ha(V,H,A)				\
({							\
  __typeof__ ((V)[0]) * _v(v) = 0;			\
  uword _v(l) = vec_len (V);				\
  if (_v(l) > 0)					\
    {							\
      vec_resize_ha (_v(v), _v(l), (H), (A));		\
      memcpy (_v(v), (V), _v(l) * sizeof ((V)[0]));	\
    }							\
  _v(v);						\
})

#define vec_dup(V) vec_dup_ha(V,0,0)
#define vec_dup_aligned(V,A) vec_dup_ha(V,0,A)

#define vec_copy(DST,SRC) memcpy (DST, SRC, vec_len (DST) * sizeof ((DST)[0]))

/* Make a new vector with the same size as a given vector but
   possibly with a different type. */
#define vec_clone(NEW_V,OLD_V)							\
do {										\
  (NEW_V) = 0;									\
  (NEW_V) = _vec_resize ((NEW_V), vec_len (OLD_V),				\
			 vec_len (OLD_V) * sizeof ((NEW_V)[0]), (0), (0));	\
} while (0)

/* Make sure vector is long enough for given index. */
#define vec_validate_ha(V,I,H,A)					\
do {									\
  word _v(i) = (I);							\
  word _v(l) = vec_len (V);						\
  if (_v(i) >= _v(l))							\
    {									\
      vec_resize_ha ((V), 1 + (_v(i) - _v(l)), (H), (A));		\
      /* Must zero new space since user may have previously		\
	 used e.g. _vec_len (v) -= 10 */				\
      memset ((V) + _v(l), 0, (1 + (_v(i) - _v(l))) * sizeof ((V)[0]));	\
    }									\
} while (0)

#define vec_validate(V,I)           vec_validate_ha(V,I,0,0)
#define vec_validate_aligned(V,I,A) vec_validate_ha(V,I,0,A)

/* As above but initialize empty space with given value. */
#define vec_validate_init_empty_ha(V,I,INIT,H,A)		\
do {								\
  word _v(i) = (I);						\
  word _v(l) = vec_len (V);					\
  if (_v(i) >= _v(l))						\
    {								\
      vec_resize_ha ((V), 1 + (_v(i) - _v(l)), (H), (A));	\
      while (_v(l) <= _v(i))					\
	{							\
	  (V)[_v(l)] = (INIT);					\
	  _v(l)++;						\
	}							\
    }								\
} while (0)

#define vec_validate_init_empty(V,I,INIT) \
  vec_validate_init_empty_ha(V,I,INIT,0,0)
#define vec_validate_init_empty_aligned(V,I,A) \
  vec_validate_init_empty_ha(V,I,INIT,0,A)

/* Add 1 element to end of vector. */
#define vec_add1_ha(V,E,H,A)						\
do {									\
  word _v(l) = vec_len (V);						\
  V = _vec_resize ((V), 1, (_v(l) + 1) * sizeof ((V)[0]), (H), (A));	\
  (V)[_v(l)] = (E);							\
} while (0)

#define vec_add1(V,E)           vec_add1_ha(V,E,0,0)
#define vec_add1_aligned(V,E,A) vec_add1_ha(V,E,0,A)

/* Add N elements to end of vector V, return pointer to new elements in P. */
#define vec_add2_ha(V,P,N,H,A)							\
do {										\
  word _v(n) = (N);								\
  word _v(l) = vec_len (V);							\
  V = _vec_resize ((V), _v(n), (_v(l) + _v(n)) * sizeof ((V)[0]), (H), (A));	\
  P = (V) + _v(l);								\
} while (0)

#define vec_add2(V,P,N)           vec_add2_ha(V,P,N,0,0)
#define vec_add2_aligned(V,P,N,A) vec_add2_ha(V,P,N,0,A)

/* Add N elements E to end of vector V. */
#define vec_add_ha(V,E,N,H,A)							\
do {										\
  word _v(n) = (N);								\
  word _v(l) = vec_len (V);							\
  V = _vec_resize ((V), _v(n), (_v(l) + _v(n)) * sizeof ((V)[0]), (H), (A));	\
  memcpy ((V) + _v(l), (E), _v(n) * sizeof ((V)[0]));				\
} while (0)

#define vec_add(V,E,N)           vec_add_ha(V,E,N,0,0)
#define vec_add_aligned(V,E,N,A) vec_add_ha(V,E,N,0,A)

/* Resize vector by N elements starting from element M.
   Zero new elements. */
#define vec_insert_ha(V,N,M,H,A)			\
do {							\
  word _v(l) = vec_len (V);				\
  word _v(n) = (N);					\
  word _v(m) = (M);					\
  V = _vec_resize ((V),					\
		   _v(n),				\
		   (_v(l) + _v(n))*sizeof((V)[0]),	\
		   (H), (A));				\
  ASSERT (_v(m) <= _v(l));				\
  memmove ((V) + _v(m) + _v(n),				\
	   (V) + _v(m),					\
	   (_v(l) - _v(m)) * sizeof ((V)[0]));		\
  memset  ((V) + _v(m), 0, _v(n) * sizeof ((V)[0]));	\
} while (0)

#define vec_insert(V,N,M)           vec_insert_ha(V,N,M,0,0)
#define vec_insert_aligned(V,N,M,A) vec_insert_ha(V,N,M,0,A)

/* Resize vector by N elements starting from element M.
   Insert given elements. */
#define vec_insert_elts_ha(V,E,N,M,H,A)			\
do {							\
  word _v(l) = vec_len (V);				\
  word _v(n) = (N);					\
  word _v(m) = (M);					\
  V = _vec_resize ((V),					\
		   _v(n),				\
		   (_v(l) + _v(n))*sizeof((V)[0]),	\
		   (H), (A));				\
  ASSERT (_v(m) <= _v(l));				\
  memmove ((V) + _v(m) + _v(n),				\
	   (V) + _v(m),					\
	   (_v(l) - _v(m)) * sizeof ((V)[0]));		\
  memcpy  ((V) + _v(m), (E), _v(n) * sizeof ((V)[0]));	\
} while (0)

#define vec_insert_elts(V,E,N,M)           vec_insert_elts_ha(V,E,N,M,0,0)
#define vec_insert_elts_aligned(V,E,N,M,A) vec_insert_elts_ha(V,E,N,M,0,A)

/* Delete N elements starting from element M. */
#define vec_delete(V,N,M)					\
do {								\
  word _v(l) = vec_len (V);					\
  word _v(n) = (N);						\
  word _v(m) = (M);						\
  /* Copy over deleted elements. */				\
  if (_v(l) - _v(n) - _v(m) > 0)				\
    memmove ((V) + _v(m), (V) + _v(m) + _v(n),			\
	     (_v(l) - _v(n) - _v(m)) * sizeof ((V)[0]));	\
  /* Zero empty space at end (for future re-allocation). */	\
  if (_v(n) > 0)						\
    memset ((V) + _v(l) - _v(n), 0, _v(n) * sizeof ((V)[0]));	\
  _vec_len (V) -= _v(n);					\
} while (0)

/* Appends v2 after v1. Result in v1. */
#define vec_append(v1,v2)						\
do {									\
  uword _v(l1) = vec_len (v1);						\
  uword _v(l2) = vec_len (v2);						\
									\
  v1 = _vec_resize ((v1), _v(l2),					\
		    (_v(l1) + _v(l2)) * sizeof ((v1)[0]), 0, 0);	\
  memcpy ((v1) + _v(l1), (v2), _v(l2) * sizeof ((v2)[0]));		\
} while (0)

/* Prepends v2 in front of v1. Result in v1. */
#define vec_prepend(v1,v2)					\
do {								\
  uword _v(l1) = vec_len (v1);					\
  uword _v(l2) = vec_len (v2);					\
								\
  v1 = _vec_resize ((v1), _v(l2),				\
		    (_v(l1) + _v(l2)) * sizeof ((v1)[0]), 0, 0);	\
  memmove ((v1) + _v(l2), (v1), _v(l1) * sizeof ((v1)[0]));	\
  memcpy ((v1), (v2), _v(l2) * sizeof ((v2)[0]));		\
} while (0)

/* Zero all elements. */
#define vec_zero(var)						\
do {								\
  if (var)							\
    memset ((var), 0, vec_len (var) * sizeof ((var)[0]));	\
} while (0)

/* Set all elements to given value. */
#define vec_set(v,val)				\
do {						\
  word _v(i);					\
  __typeof__ ((v)[0]) _val = (val);		\
  for (_v(i) = 0; _v(i) < vec_len (v); _v(i)++)	\
    (v)[_v(i)] = _val;				\
} while (0)

#ifdef CLIB_UNIX
#include <stdlib.h>		/* for qsort */
#endif

/* Compare two vectors. */
#define vec_is_equal(v1,v2) \
  (vec_len (v1) == vec_len (v2) && ! memcmp ((v1), (v2), vec_len (v1) * sizeof ((v1)[0])))

/* Compare two vectors (only applicable to vectors of signed numbers).
   Used in qsort compare functions. */
#define vec_cmp(v1,v2)					\
({							\
  word _v(i), _v(cmp), _v(l);				\
  _v(l) = clib_min (vec_len (v1), vec_len (v2));	\
  _v(cmp) = 0;						\
  for (_v(i) = 0; _v(i) < _v(l); _v(i)++) {		\
    _v(cmp) = (v1)[_v(i)] - (v2)[_v(i)];		\
    if (_v(cmp))					\
      break;						\
  }							\
  if (_v(cmp) == 0 && _v(l) > 0)			\
    _v(cmp) = vec_len(v1) - vec_len(v2);		\
  (_v(cmp) < 0 ? -1 : (_v(cmp) > 0 ? +1 : 0));		\
})

/* Sort a vector with qsort via user's comparison body.
   Example to sort an integer vector:
     int * int_vec = ...;
     vec_sort (int_vec, i0, i1, i0[0] - i1[0]);
*/
#define vec_sort(vec,v0,v1,body)					\
do {									\
  int _vec_sort_compare (const void * _v0,				\
			 const void * _v1)				\
  {									\
    __typeof__ (vec) v0 = (__typeof__ (vec)) _v0;			\
    __typeof__ (vec) v1 = (__typeof__ (vec)) _v1;			\
    return (int) (body);						\
  }									\
  qsort (vec, vec_len (vec), sizeof (vec[0]), _vec_sort_compare);	\
} while (0)

#endif /* included_vec_h */

