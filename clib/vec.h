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
#include <clib/vec_bootstrap.h>

/*
   CLIB vectors are ubiquitous dynamically resized arrays with by user
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

   Many macros have _a variants supporting alignment of vector data
   and _h variants supporting non zero length vector headers.
   The _ha variants support both.

   Standard programming error: memorize a pointer to the ith element 
   of a vector then expand it. Vectors expand by 3/2, so such code
   may appear to work for a period of time. Memorize vector indices
   which are invariant. 
 */

/* Low-level resize allocation function. */
void * vec_resize_allocate_memory (void * v,
				   word length_increment,
				   uword data_bytes,
				   uword header_bytes,
				   uword data_align);

/* Vector resize function.  Called as needed by various macros such as
   vec_add1() when we need to allocate memory. */
always_inline void *
_vec_resize (void * v,
	     word length_increment,
	     uword data_bytes,
	     uword header_bytes,
	     uword data_align)
{
  vec_header_t * vh = _vec_find (v);
  uword new_data_bytes, aligned_header_bytes;

  aligned_header_bytes = vec_header_bytes (header_bytes);

  new_data_bytes = data_bytes + aligned_header_bytes;

  if (PREDICT_TRUE (v != 0))
    {
      void * p = v - aligned_header_bytes;

      /* Vector header must start heap object. */
      ASSERT (clib_mem_is_heap_object (p));

      /* Typically we'll not need to resize. */
      if (new_data_bytes <= clib_mem_size (p))
	{
	  vh->len += length_increment;
	  return v;
	}
    }

  /* Slow path: call helper function. */
  return vec_resize_allocate_memory (v, length_increment, data_bytes, header_bytes,
				     clib_max (sizeof (vec_header_t), data_align));
}

uword clib_mem_is_vec_h (void * v, uword header_bytes);

always_inline uword clib_mem_is_vec (void * v)
{ return clib_mem_is_vec_h (v, 0); }

/* Local variable naming macro (prevents collisions with other macro naming). */
#define _v(var) _vec_##var

/* Resize a vector (general version).
   Add N elements to end of given vector V, return pointer to start of vector.
   Vector will have room for H header bytes and will have user's data aligned
   at alignment A (rounded to next power of 2). */
#define vec_resize_ha(V,N,H,A)							\
do {										\
  word _v(n) = (N);								\
  word _v(l) = vec_len (V);							\
  V = _vec_resize ((V), _v(n), (_v(l) + _v(n)) * sizeof ((V)[0]), (H), (A));	\
} while (0)

/* Resize a vector (unspecified alignment). */
#define vec_resize(V,N)     vec_resize_ha(V,N,0,0)
/* Resize a vector (aligned). */
#define vec_resize_aligned(V,N,A) vec_resize_ha(V,N,0,A)

/* Allocate space for N more elements but keep size the same (general version). */
#define vec_alloc_ha(V,N,H,A)			\
do {						\
    uword _v(l) = vec_len (V);			\
    vec_resize_ha (V, N, H, A);			\
    _vec_len (V) = _v(l);			\
} while (0)

/* Allocate space for N more elements but keep size the same (unspecified alignment) */
#define vec_alloc(V,N) vec_alloc_ha(V,N,0,0)
/* Allocate space for N more elements but keep size the same (alignment specified but no header) */
#define vec_alloc_aligned(V,N,A) vec_alloc_ha(V,N,0,A)

/* Create new vector of given type and length (general version). */
#define vec_new_ha(T,N,H,A)					\
({								\
  word _v(n) = (N);						\
  _vec_resize ((T *) 0, _v(n), _v(n) * sizeof (T), (H), (A));	\
})

/* Create new vector of given type and length (unspecified alignment, no header). */
#define vec_new(T,N)           vec_new_ha(T,N,0,0)
/* Create new vector of given type and length (alignment specified but no header). */
#define vec_new_aligned(T,N,A) vec_new_ha(T,N,0,A)

/* Free vector's memory (general version). */
#define vec_free_h(V,H)				\
do {						\
  if (V)					\
    {						\
      clib_mem_free (vec_header ((V), (H)));	\
      V = 0;					\
    }						\
} while (0)

/* Free vector's memory (unspecified alignment, no header). */
#define vec_free(V) vec_free_h(V,0)
/* Free vector user header */
#define vec_free_header(h) clib_mem_free (h)

/* Return copy of vector (general version). */
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

/* Return copy of vector (no alignment). */
#define vec_dup(V) vec_dup_ha(V,0,0)
/* Return copy of vector (alignment specified). */
#define vec_dup_aligned(V,A) vec_dup_ha(V,0,A)

/* Copy a vector */
#define vec_copy(DST,SRC) memcpy (DST, SRC, vec_len (DST) * sizeof ((DST)[0]))

/* Clone a vector

    Make a new vector with the same size as a given vector but
   possibly with a different type. */
#define vec_clone(NEW_V,OLD_V)							\
do {										\
  (NEW_V) = 0;									\
  (NEW_V) = _vec_resize ((NEW_V), vec_len (OLD_V),				\
			 vec_len (OLD_V) * sizeof ((NEW_V)[0]), (0), (0));	\
} while (0)

/* Make sure vector is long enough for given index (general version). */
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

/* Make sure vector is long enough for given index (unspecified alignment). */
#define vec_validate(V,I)           vec_validate_ha(V,I,0,0)
/* Make sure vector is long enough for given index (alignment specified but no header). */
#define vec_validate_aligned(V,I,A) vec_validate_ha(V,I,0,A)

/* Make sure vector is long enough for given index and initialize empty space (general version). */
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

/* Make sure vector is long enough for given index and initialize empty space (unspecified alignment). */
#define vec_validate_init_empty(V,I,INIT) \
  vec_validate_init_empty_ha(V,I,INIT,0,0)
/* Make sure vector is long enough for given index and initialize empty space (alignment specified). */
#define vec_validate_init_empty_aligned(V,I,INIT,A) \
  vec_validate_init_empty_ha(V,I,INIT,0,A)

/* Add 1 element to end of vector (general version). */
#define vec_add1_ha(V,E,H,A)						\
do {									\
  word _v(l) = vec_len (V);						\
  V = _vec_resize ((V), 1, (_v(l) + 1) * sizeof ((V)[0]), (H), (A));	\
  (V)[_v(l)] = (E);							\
} while (0)

/* Add 1 element to end of vector (unspecified alignment). */
#define vec_add1(V,E)           vec_add1_ha(V,E,0,0)
/* Add 1 element to end of vector (alignment specified). */
#define vec_add1_aligned(V,E,A) vec_add1_ha(V,E,0,A)

/* Add N elements to end of vector V, return pointer to new elements in P. (general version) */
#define vec_add2_ha(V,P,N,H,A)							\
do {										\
  word _v(n) = (N);								\
  word _v(l) = vec_len (V);							\
  V = _vec_resize ((V), _v(n), (_v(l) + _v(n)) * sizeof ((V)[0]), (H), (A));	\
  P = (V) + _v(l);								\
} while (0)

/* Add N elements to end of vector V, return pointer to new elements in P. (unspecified alignment) */
#define vec_add2(V,P,N)           vec_add2_ha(V,P,N,0,0)
/* Add N elements to end of vector V, return pointer to new elements in P. (alignment specified, no header) */
#define vec_add2_aligned(V,P,N,A) vec_add2_ha(V,P,N,0,A)

/* Add N elements to end of vector V (general version) */
#define vec_add_ha(V,E,N,H,A)							\
do {										\
  word _v(n) = (N);								\
  word _v(l) = vec_len (V);							\
  V = _vec_resize ((V), _v(n), (_v(l) + _v(n)) * sizeof ((V)[0]), (H), (A));	\
  memcpy ((V) + _v(l), (E), _v(n) * sizeof ((V)[0]));				\
} while (0)

/* Add N elements to end of vector V (unspecified alignment) */
#define vec_add(V,E,N)           vec_add_ha(V,E,N,0,0)
/* Add N elements to end of vector V (alignment specified) */
#define vec_add_aligned(V,E,N,A) vec_add_ha(V,E,N,0,A)

/* Returns last element of a vector and decrements its length */
#define vec_pop(V)				\
({						\
  uword _v(l) = vec_len (V);			\
  ASSERT (_v(l) > 0);				\
  _v(l) -= 1;					\
  _vec_len (V) = _v (l);			\
  (V)[_v(l)];					\
})

/* Set E to the last element of a vector, decrement vector length */
#define vec_pop2(V,E)				\
({						\
  uword _v(l) = vec_len (V);			\
  if (_v(l) > 0) (E) = vec_pop (V);		\
  _v(l) > 0;					\
})

/* Resize vector by N elements starting from element M, initialize new elements (general version). */
#define vec_insert_init_empty_ha(V,N,M,INIT,H,A)	\
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
  memset  ((V) + _v(m), INIT, _v(n) * sizeof ((V)[0]));	\
} while (0)

/* Resize vector by N elements starting from element M, initialize new elements to zero (general version). */
#define vec_insert_ha(V,N,M,H,A)    vec_insert_init_empty_ha(V,N,M,0,H,A)
/* Resize vector by N elements starting from element M, initialize new elements to zero (unspecified alignment, no header). */
#define vec_insert(V,N,M)           vec_insert_ha(V,N,M,0,0)
/* Resize vector by N elements starting from element M, initialize new elements to zero (alignment specified, no header). */
#define vec_insert_aligned(V,N,M,A) vec_insert_ha(V,N,M,0,A)

/* Resize vector by N elements starting from element M, initialize new elements to INIT (unspecified alignment, no header). */
#define vec_insert_init_empty(V,N,M,INIT) \
  vec_insert_init_empty_ha(V,N,M,INIT,0,0)
/* Resize vector by N elements starting from element M, initialize new elements to INIT (alignment specified, no header). */
#define vec_insert_init_empty_aligned(V,N,M,INIT,A) \
  vec_insert_init_empty_ha(V,N,M,INIT,0,A)

/* Resize vector by N elements starting from element M. Insert given elements (general version). */
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

/* Resize vector by N elements starting from element M. Insert given elements (unspecified alignment, no header). */
#define vec_insert_elts(V,E,N,M)           vec_insert_elts_ha(V,E,N,M,0,0)
/* Resize vector by N elements starting from element M. Insert given elements (alignment specified, no header). */
#define vec_insert_elts_aligned(V,E,N,M,A) vec_insert_elts_ha(V,E,N,M,0,A)

/* Delete N elements starting from element M */
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

/* Delete a single element at index I. */
#define vec_del1(v,i)				\
do {						\
  uword __vec_del_l = _vec_len (v) - 1;		\
  uword __vec_del_i = (i);			\
  if (_vec_del_i < __vec_del_l)			\
    (v)[_vec_del_i] = (v)[_vec_del_l];		\
  _vec_len (v) = __vec_del_l;			\
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

/* Sort a vector with qsort via user's comparison body

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

/* Sort a vector using the supplied element comparison function

    A simple qsort wrapper */
#define vec_sort_with_function(vec,f)				\
do {								\
  qsort (vec, vec_len (vec), sizeof (vec[0]), (void *) (f));	\
} while (0)

#endif /* included_vec_h */

