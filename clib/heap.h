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

/* Heaps of objects of type T (e.g. int, struct foo, ...).

   Usage.  To declare a null heap:

     T * heap = 0;
   
   To allocate:

     offset = heap_alloc (heap, size, handle);

   New object is heap[offset] ... heap[offset + size]
   Handle is used to free/query object.

   To free object:

     heap_dealloc (heap, handle);

   To query the size of an object:

     heap_size (heap, handle)

*/

#ifndef included_heap_h
#define included_heap_h

#include <clib/clib.h>
#include <clib/cache.h>
#include <clib/hash.h>
#include <clib/format.h>
#include <clib/bitmap.h>

/* Doubly linked list of elements. */
typedef struct {
  /* Offset of this element (plus free bit).
     If element is free, data at offset contains pointer to free list. */
  u32 offset;

  /* Index of next and previous elements relative to current element. */
  i32 next, prev;
} heap_elt_t;

/* Use high bit of offset as free bit. */
#define HEAP_ELT_FREE_BIT	(1 << 31)

static always_inline uword heap_is_free (heap_elt_t * e)
{ return (e->offset & HEAP_ELT_FREE_BIT) != 0; }

static always_inline uword heap_offset (heap_elt_t * e)
{ return e->offset &~ HEAP_ELT_FREE_BIT; }

static always_inline heap_elt_t * heap_next (heap_elt_t * e)
{ return e + e->next; }

static always_inline heap_elt_t * heap_prev (heap_elt_t * e)
{ return e + e->prev; }

static always_inline uword heap_elt_size (void * v, heap_elt_t * e)
{
  heap_elt_t * n = heap_next (e);
  uword next_offset = n != e ? heap_offset (n) : vec_len (v);
  return next_offset - heap_offset (e);
}

/* Sizes are binned.  Sizes 1 to 2^log2_small_bins have their
   own free lists.  Larger sizes are grouped in powers of two. */
#define HEAP_LOG2_SMALL_BINS	(5)
#define HEAP_SMALL_BINS		(1 << HEAP_LOG2_SMALL_BINS)
#define HEAP_N_BINS		(2 * HEAP_SMALL_BINS)

/* Header for heaps. */
typedef struct {
  /* Vector of used and free elements. */
  heap_elt_t * elts;

  /* Vector of free indices of elts array. */
  u32 * free_elts;

  /* First and last element of doubly linked chain of elements. */
  uword head, tail;

  u32 ** free_lists;

  u32 used_count, max_len;

  u32 elt_bytes;

  uword * used_elt_bitmap;

  format_function_t * format_elt;

  u32 flags;
  /* Static heaps are made from external memory given to
     us by user. */
#define HEAP_IS_STATIC (1)
} heap_t;

/* Start of heap elements is always cache aligned. */
#define HEAP_DATA_ALIGN (CLIB_CACHE_LINE_BYTES)

static always_inline heap_t * heap_header (void * v)
{ return vec_header_ha (v, sizeof (heap_t), HEAP_DATA_ALIGN); }

static always_inline uword heap_header_bytes ()
{ return vec_header_bytes_ha (sizeof (heap_t), HEAP_DATA_ALIGN); }

static always_inline void heap_dup_header (heap_t * old, heap_t * new)
{
  uword i;

  new[0] = old[0];
  new->elts = vec_dup (new->elts);
  new->free_elts = vec_dup (new->free_elts);
  new->free_lists = vec_dup (new->free_lists);
  for (i = 0; i < vec_len (new->free_lists); i++)
    new->free_lists[i] = vec_dup (new->free_lists[i]);
  new->used_elt_bitmap = clib_bitmap_dup (new->used_elt_bitmap);
}

/* Make a duplicate copy of a heap. */
#define heap_dup(v) _heap_dup(v, vec_len (v) * sizeof (v[0]))

static always_inline void * _heap_dup (void * v_old, uword v_bytes)
{
  heap_t * h_old, * h_new;
  void * v_new;

  h_old = heap_header (v_old);

  if (! v_old)
    return v_old;

  v_new = 0;
  v_new = _vec_resize (v_new, _vec_len (v_old), v_bytes, sizeof (heap_t), 0);
  h_new = heap_header (v_new);
  heap_dup_header (h_old, h_new);
  memcpy (v_new, v_old, v_bytes);
  return v_new;
}

static always_inline uword heap_elts (void * v)
{
  heap_t * h = heap_header (v);
  return h->used_count;
}

uword heap_bytes (void * v);

static always_inline void * heap_set_format (void * v, format_function_t * format_elt)
{
  if (! v)
    v = _vec_resize (v, 0, 0, sizeof (heap_t), 0);
  heap_header (v)->format_elt = format_elt;
  return v;
}

static always_inline void * heap_set_max_len (void * v, uword max_len)
{
  if (! v)
    v = _vec_resize (v, 0, 0, sizeof (heap_t), 0);
  heap_header (v)->max_len = max_len;
  return v;
}

static always_inline uword heap_get_max_len (void * v)
{ return v ? heap_header (v)->max_len : 0; }

/* Create fixed size heap with given block of memory. */
static always_inline void *
heap_create_from_memory (void * memory, uword max_len, uword elt_bytes)
{
  heap_t * h;
  _VEC * vec_header;

  if (max_len * elt_bytes < sizeof (h[0]))
    return 0;

  h = memory;
  memset (h, 0, sizeof (h[0]));
  h->max_len = max_len;
  h->elt_bytes = elt_bytes;
  h->flags = HEAP_IS_STATIC;

  vec_header = (void *) (h + 1);
  vec_header->len = 0;
  return (void *) (vec_header + 1);
}

/* Execute BODY for each allocated heap element. */
#define heap_foreach(var,heap,body)			\
do {							\
  if (vec_len (heap) > 0)				\
    {							\
      heap_t * _h = heap_header (heap);			\
      heap_elt_t * _e   = _h->elts + _h->head;		\
      heap_elt_t * _end = _h->elts + _h->tail;		\
      while (1)						\
	{						\
	  if (! heap_is_free (_e))			\
	    {						\
	      (var) = (heap) + heap_offset (_e);	\
	      do { body; } while (0);			\
	    }						\
	  if (_e == _end)				\
	    break;					\
	  _e = heap_next (_e);				\
	}						\
    }							\
} while (0)

#define heap_elt_at_index(v,index) vec_elt_at_index(v,index)

#define heap_elt_with_handle(v,handle)				\
({								\
  heap_t * _h = heap_header (v);				\
  heap_elt_t * _e = vec_elt_at_index (_h->elts, (handle));	\
  ASSERT (! heap_is_free (_e));					\
  (v) + heap_offset (_e);					\
})

static always_inline uword
heap_is_free_handle (void * v, uword heap_handle)
{
  heap_t * h = heap_header (v);
  heap_elt_t * e = vec_elt_at_index (h->elts, heap_handle);
  return heap_is_free (e);
}

extern uword heap_len (void * v, word handle);

/* Low level allocation call. */
extern void * _heap_alloc (void * v, uword size, uword alignment,
			   uword elt_bytes,
			   uword * offset, uword * handle);

#define heap_alloc_aligned(v,size,align,handle)			\
({								\
  uword _o, _h;							\
  uword _a = (align);						\
  uword _s = (size);						\
  (v) = _heap_alloc ((v), _s, _a, sizeof ((v)[0]), &_o, &_h);	\
  (handle) = _h;						\
  _o;								\
})

#define heap_alloc(v,size,handle) heap_alloc_aligned((v),(size),0,(handle))

extern void heap_dealloc (void * v, uword handle);
extern void heap_validate (void * v);

/* Format heap internal data structures as string. */
extern u8 * format_heap (u8 * s, va_list * va);

void * _heap_free (void * v);

#define heap_free(v) (v)=_heap_free(v)

#endif /* included_heap_h */
