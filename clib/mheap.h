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

#ifndef included_mheap_h
#define included_mheap_h

#include <clib/vec.h>
#include <clib/error.h>         /* clib_error_t */
#include <clib/mem.h>           /* clib_mem_usage_t */
#include <clib/format.h>	/* for unformat_input_t */

#ifdef CLIB_UNIX
#include <pthread.h>		/* for pthread_mutex_t */
#define MHEAP_LOCK_PTHREAD
#endif

typedef u32 mheap_size_t;

/* Each element in heap is immediately followed by this struct. */
typedef struct {
  /* Number of bytes in previous block.
     Low bit is used to mark previous block is free. */
  mheap_size_t prev_size;
#define MHEAP_PREV_IS_FREE (1)

  /* Size of this object (including mheap_elt_t header).
     Low bit is used to mark this element as the first in the heap
     (i.e. its the head of the doubly linked chain of heap elements). */
  mheap_size_t size;
#define MHEAP_IS_FIRST (1)
#define MHEAP_IS_LAST  (2)
} mheap_elt_t;

/* Size and offset of free heap element.  Used only on free lists. */
typedef struct {
  mheap_size_t size;
  mheap_size_t offset;
} mheap_free_elt_t;

#define MHEAP_LOG2_SMALL_BINS	(5)
#define MHEAP_SMALL_BINS	(1 << MHEAP_LOG2_SMALL_BINS)
#define MHEAP_N_BINS		(2 * MHEAP_SMALL_BINS)

typedef struct {
  /* Address of callers: outer first, inner last. */
  uword callers[12];

  /* Count of allocations with this traceback. */
  u32 n_allocations;

  /* Count of bytes allocated with this traceback. */
  u32 n_bytes;

  /* Offset of this item */
  uword offset;    

} mheap_trace_t;

typedef struct {
  mheap_trace_t * traces;

  /* Indices of free traces. */
  u32 * trace_free_list;

  /* Hash table mapping callers to trace index. */
  uword * trace_by_callers;

  /* Hash table mapping mheap offset to trace index. */
  uword * trace_index_by_offset;
} mheap_trace_main_t;

/* Vec header for heaps. */
typedef struct {
  /* Vectors of free elements in each bin. */
  mheap_free_elt_t * free_lists[MHEAP_N_BINS];

  uword flags;

  /* Set if one of the free list vectors needs to be grown. */
#define MHEAP_FLAG_FREE_LISTS_NEED_RESIZE	(1 << 0)
#define MHEAP_FLAG_INHIBIT_FREE_LIST_SEARCH	(1 << 1)
#define MHEAP_FLAG_TRACE			(1 << 2)
#define MHEAP_FLAG_DISABLE_VM			(1 << 3)
#define MHEAP_FLAG_THREAD_SAFE			(1 << 4)

#define MHEAP_FLAG_DISABLE_FOR_RECURSIVE_CALLS \
  (MHEAP_FLAG_TRACE)

  /* Number of allocated objects. */
  uword n_elts;

  /* First element of heap.  All other mheap_elt_t's are stored
     inline with user's data. */
  mheap_elt_t first;

  /* Maximum size (in bytes) this heap is allowed to grow to.
     Set to ~0 to grow heap (via vec_resize) arbitrarily. */
  uword max_size;

#ifdef MHEAP_LOCK_PTHREAD
  /* Global per-heap lock for thread-safe operation. */
  pthread_mutex_t lock;
#endif

  mheap_trace_main_t trace_main;
} mheap_t;

static inline mheap_t * mheap_header (u8 * v)
{ return vec_header (v, sizeof (mheap_t)); }

static inline u8 * mheap_vector (mheap_t * h)
{ return vec_header_end (h, sizeof (mheap_t)); }

/* Round all sizes to multiples of 8 bytes.  So, the minimum sized
   objects is 8 bytes of overhead plus 8 bytes of data. */
static inline uword mheap_round_size (uword size)
{ return (size + sizeof (mheap_elt_t) - 1) &~ (sizeof (mheap_elt_t) - 1); }

/* Round size down to minimum size multiple. */
static inline uword mheap_trunc_size (uword size)
{ return size &~ (sizeof (mheap_elt_t) - 1); }

#define MHEAP_ALIGN_PAD_BYTES(size)				\
  (((size) % sizeof (mheap_elt_t)) == 0				\
   ? 0								\
   : sizeof (mheap_elt_t) - ((size) % sizeof (mheap_elt_t)))

static inline uword mheap_is_first (mheap_elt_t * e)
{ return e->size & MHEAP_IS_FIRST; }

static inline uword mheap_is_last (mheap_elt_t * e)
{ return e->size & MHEAP_IS_LAST; }

static inline uword mheap_elt_offset (u8 * v, mheap_elt_t * e)
{ return (u8 *) (e + 1) - v; }

static inline mheap_elt_t * _mheap_elt_at_offset (u8 * v, uword offset)
{ return (mheap_elt_t *) (v + offset) - 1; }

static inline mheap_elt_t * mheap_elt_at_offset (u8 * v, uword i)
{
  /* First element is special. */
  if (i == 0)
    return &mheap_header (v)->first;
  else
    return _mheap_elt_at_offset (v, i);
}

static inline mheap_elt_t * mheap_next_elt (u8 * v, mheap_elt_t * e)
{
  ASSERT (! mheap_is_last (e));

  if (mheap_is_first (e))
    return _mheap_elt_at_offset (v, mheap_trunc_size (e->size));
  else
    {
      u8 * u = (u8 *) e + e->size;
      ASSERT (u < vec_end (v));
      return (mheap_elt_t *) u;
    }
}

static inline mheap_elt_t * mheap_prev_elt (u8 * v, mheap_elt_t * e)
{
  mheap_elt_t * p = (mheap_elt_t *) ((u8 *) e - mheap_trunc_size (e->prev_size));

  return (u8 *) p > v ? p : &mheap_header(v)->first;
}

static inline mheap_size_t * mheap_elt_data (u8 * v, mheap_elt_t * e)
{ return (mheap_size_t *) (v + (mheap_is_first (e) ? 0 : mheap_elt_offset (v, e))); }

/* Exported operations. */

static inline uword mheap_elts (u8 * v)
{ return v ? mheap_header (v)->n_elts : 0; }

static inline uword mheap_max_size (u8 * v)
{ return v ? mheap_header (v)->max_size : ~0; }

/* For debugging we keep track of offsets for valid objects.
   We make sure user is not trying to free object with invalid offset. */
static inline uword mheap_offset_is_valid (u8 * v, uword offset)
{ return offset < vec_len (v); }

static inline void * mheap_data (u8 * v, uword i)
{
  mheap_elt_t * e;
  ASSERT (mheap_offset_is_valid (v, i));
  e = mheap_elt_at_offset (v, i);
  return mheap_elt_data (v, e);
}

static inline uword mheap_data_bytes (u8 * v, uword i)
{
  mheap_elt_t * e;
  ASSERT (mheap_offset_is_valid (v, i));
  e = mheap_elt_at_offset (v, i);
  /* Size includes header overhead. */
  return mheap_trunc_size (e->size) - sizeof (e[0]);
}

#define mheap_len(v,d) (mheap_data_bytes((v),(u8 *) (d) - (v)) / sizeof ((d)[0]))

u8 * mheap_get_aligned (u8 * v, uword size, uword align, uword align_offset,
			uword * offset_return);

/* Allocate size bytes.  New heap and offset are returned.
   offset == ~0 means allocation failed. */
static inline u8 * mheap_get (u8 * v, uword size, uword * offset_return)
{ return mheap_get_aligned (v, size, 0, 0, offset_return); }

/* Free previously allocated offset. */
void mheap_put (u8 * v, uword offset);

/* Create allocation heap of given size. */
u8 * mheap_alloc (void * memory, uword memory_bytes);
u8 * mheap_alloc_with_flags (void * memory, uword memory_bytes, uword flags);

#define mheap_free(v) (v) = _mheap_free(v)
u8 * _mheap_free (u8 * v);

void mheap_foreach (u8 * v,
		    uword (* func) (void * arg, u8 * v, void * elt_data, uword elt_size),
		    void * arg);

/* Format mheap data structures as string. */
u8 * format_mheap (u8 * s, va_list * va);

/* Validate internal consistency. */
clib_error_t * mheap_validate (u8 * h);

/* Query bytes used. */
uword mheap_bytes (u8 * v);

void mheap_usage (u8 * v, clib_mem_usage_t * usage);

/* Enable disable traceing. */
void mheap_trace (u8 * v, int enable);

/* Test routine. */
int test_mheap_main (unformat_input_t * input);

#endif /* included_mheap_h */
