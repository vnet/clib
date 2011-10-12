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

#include <clib/bitops.h>
#include <clib/hash.h>
#include <clib/format.h>
#include <clib/mheap.h>
#include <clib/os.h>

static void mheap_get_trace (void * v, uword offset, uword size);
static void mheap_put_trace (void * v, uword offset, uword size);
static int mheap_trace_sort (const void * t1, const void * t2);

always_inline void mheap_lock_init (mheap_t * h)
{
#ifdef MHEAP_LOCK_PTHREAD
  pthread_mutexattr_t attr;
  int error = 0;
  if (pthread_mutexattr_init (&attr))
    error = 1;
  if (pthread_mutexattr_setpshared (&attr, PTHREAD_PROCESS_SHARED))
    error = 1;
  pthread_mutex_init (&h->lock, error ? (pthread_mutexattr_t *) 0 : &attr);
#endif
}

always_inline void mheap_maybe_lock (void * v)
{
  mheap_t * h = mheap_header (v);
  if (! v || ! (h->flags & MHEAP_FLAG_THREAD_SAFE))
    return;

#ifdef MHEAP_LOCK_PTHREAD
  pthread_mutex_lock (&h->lock);
#endif
}

always_inline void mheap_maybe_unlock (void * v)
{
  mheap_t * h = mheap_header (v);
  if (! v || ! (h->flags & MHEAP_FLAG_THREAD_SAFE))
    return;

#ifdef MHEAP_LOCK_PTHREAD
  pthread_mutex_unlock (&h->lock);
#endif
}

/* Find bin for objects with size at least n_user_data_bytes. */
always_inline uword
user_data_size_to_bin_index (uword n_user_data_bytes)
{
  uword n_user_data_words;
  word small_bin, large_bin;

  /* User size must be at least big enough to hold free elt. */
  n_user_data_bytes = clib_max (n_user_data_bytes, MHEAP_MIN_USER_DATA_BYTES);

  /* Round to words. */
  n_user_data_words = (round_pow2 (n_user_data_bytes, MHEAP_USER_DATA_WORD_BYTES)
		       / MHEAP_USER_DATA_WORD_BYTES);

  ASSERT (n_user_data_words > 0);
  small_bin = n_user_data_words - (MHEAP_MIN_USER_DATA_BYTES / MHEAP_USER_DATA_WORD_BYTES);
  ASSERT (small_bin >= 0);

  large_bin = MHEAP_N_SMALL_OBJECT_BINS + max_log2 (n_user_data_bytes) - MHEAP_LOG2_N_SMALL_OBJECT_BINS;

  return small_bin < MHEAP_N_SMALL_OBJECT_BINS ? small_bin : large_bin;
}

always_inline uword
mheap_elt_size_to_user_n_bytes (uword n_bytes)
{
  ASSERT (n_bytes >= sizeof (mheap_elt_t));
  return (n_bytes - STRUCT_OFFSET_OF (mheap_elt_t, user_data));
}

always_inline uword
mheap_elt_size_to_user_n_words (uword n_bytes)
{
  ASSERT (n_bytes % MHEAP_USER_DATA_WORD_BYTES == 0);
  return mheap_elt_size_to_user_n_bytes (n_bytes) / MHEAP_USER_DATA_WORD_BYTES;
}

always_inline void
mheap_elt_set_size (void * v,
		    uword uoffset,
		    uword n_user_data_bytes,
		    uword is_free)
{
  mheap_elt_t * e, * n;

  e = mheap_elt_at_uoffset (v, uoffset);

  ASSERT (n_user_data_bytes % MHEAP_USER_DATA_WORD_BYTES == 0);
  e->n_user_data = n_user_data_bytes / MHEAP_USER_DATA_WORD_BYTES;
  e->is_free = is_free;
  ASSERT (e->prev_n_user_data * sizeof (e->user_data[0]) >= MHEAP_MIN_USER_DATA_BYTES);

  n = mheap_next_elt (e);
  n->prev_n_user_data = e->n_user_data;
  n->prev_is_free = is_free;
}

always_inline void set_first_free_elt_offset (mheap_t * h, uword bin, uword uoffset)
{
  uword i0, i1;

  h->first_free_elt_uoffset_by_bin[bin] = uoffset;

  i0 = bin / BITS (h->non_empty_free_elt_heads[0]);
  i1 = (uword) 1 << (uword) (bin % BITS (h->non_empty_free_elt_heads[0]));

  ASSERT (i0 < ARRAY_LEN (h->non_empty_free_elt_heads));
  if (h->first_free_elt_uoffset_by_bin[bin] == ~0)
    h->non_empty_free_elt_heads[i0] &= ~i1;
  else
    h->non_empty_free_elt_heads[i0] |= i1;
}

always_inline void
set_free_elt (void * v, uword uoffset, uword n_user_data_bytes)
{
  mheap_t * h = mheap_header (v);
  mheap_elt_t * e = mheap_elt_at_uoffset (v, uoffset);
  mheap_elt_t * n = mheap_next_elt (e);
  uword bin = user_data_size_to_bin_index (n_user_data_bytes);

  ASSERT (n->prev_is_free);
  ASSERT (e->is_free);

  e->free_elt.prev_uoffset = ~0;
  e->free_elt.next_uoffset = h->first_free_elt_uoffset_by_bin[bin];

  /* Fill in next free elt's previous pointer. */
  if (e->free_elt.next_uoffset != ~0)
    {
      mheap_elt_t * nf = mheap_elt_at_uoffset (v, e->free_elt.next_uoffset);
      ASSERT (nf->is_free);
      nf->free_elt.prev_uoffset = uoffset;
    }

  set_first_free_elt_offset (h, bin, uoffset);
}

always_inline void
new_free_elt (void * v, uword uoffset, uword n_user_data_bytes)
{
  mheap_t * h = mheap_header (v);
  mheap_elt_set_size (v, uoffset, n_user_data_bytes, /* is_free */ 1);
  set_free_elt (v, uoffset, n_user_data_bytes);
}

always_inline void
remove_free_elt (void * v, mheap_elt_t * e, uword bin)
{
  mheap_t * h = mheap_header (v);
  mheap_elt_t * p, * n;
  u32 no, po;

  no = e->free_elt.next_uoffset;
  n = no != ~0 ? mheap_elt_at_uoffset (v, no) : 0;
  po = e->free_elt.prev_uoffset;
  p = po != ~0 ? mheap_elt_at_uoffset (v, po) : 0;

  if (! p)
    set_first_free_elt_offset (h, bin, no);
  else
    p->free_elt.next_uoffset = no;

  if (n)
    n->free_elt.prev_uoffset = po;
}

always_inline void
remove_free_elt2 (void * v, mheap_elt_t * e)
{
  uword bin;
  bin = user_data_size_to_bin_index (mheap_elt_data_bytes (e));
  remove_free_elt (v, e, bin);
}

#define MHEAP_VM_MAP		(1 << 0)
#define MHEAP_VM_UNMAP		(1 << 1)
#define MHEAP_VM_NOMAP		(0 << 1)
#define MHEAP_VM_ROUND		(1 << 2)
#define MHEAP_VM_ROUND_UP	MHEAP_VM_ROUND
#define MHEAP_VM_ROUND_DOWN	(0 << 2)

static uword mheap_vm (void * v, uword flags,
		       clib_address_t start_addr, uword size);
static uword mheap_vm_elt (void * v, uword flags, uword offset);

static uword mheap_page_size;

static_always_inline uword mheap_page_round (uword addr)
{ return (addr + mheap_page_size - 1) &~ (mheap_page_size - 1); }

static_always_inline uword mheap_page_truncate (uword addr)
{ return addr &~ (mheap_page_size - 1); }

static uword
mheap_get_search_free_bin (void * v,
			   uword bin,
			   uword * n_user_data_bytes_arg,
			   uword align,
			   uword align_offset)
{
  mheap_t * h = mheap_header (v);
  mheap_elt_t * e;

  /* Free object is at offset f0 ... f1;
     Allocatted object is at offset o0 ... o1. */
  word o0, o1, f0, f1, search_n_user_data_bytes;
  word lo_free_usize, hi_free_usize;

  ASSERT (h->first_free_elt_uoffset_by_bin[bin] != ~0);
  e = mheap_elt_at_uoffset (v, h->first_free_elt_uoffset_by_bin[bin]);

  search_n_user_data_bytes = *n_user_data_bytes_arg;

  /* Silence compiler warning. */
  o0 = o1 = f0 = f1 = 0;

  /* Find an object that is large enough with correct alignment at given alignment offset. */
  while (1)
    {
      uword this_object_n_user_data_bytes = mheap_elt_data_bytes (e);

      ASSERT (e->is_free);

      if (this_object_n_user_data_bytes >= search_n_user_data_bytes)
	{
	  /* Bounds of free object: from f0 to f1. */
	  f0 = ((void *) e->user_data - v);
	  f1 = f0 + this_object_n_user_data_bytes;

	  /* Place candidate object at end of free block and align as requested. */
	  o0 = ((f1 - search_n_user_data_bytes) &~ (align - 1)) - align_offset;
	  while (o0 < f0)
	    o0 += align;

	  /* Make sure that first free fragment is either empty or
	     large enough to be valid. */
	  while (1)
	    {
	      lo_free_usize = o0 != f0 ? o0 - f0 - MHEAP_ELT_OVERHEAD_BYTES : 0;
	      if (o0 == f0 || lo_free_usize >= (word) MHEAP_MIN_USER_DATA_BYTES)
		break;
	      o0 += (sizeof (mheap_elt_t) + align - 1) &~ (align - 1);
	    }

	  o1 = o0 + search_n_user_data_bytes;

	  /* Does it fit? */
	  if (o0 >= f0 && o1 <= f1)
	    break;
	}

      /* Reached end of free list without finding large enough object. */
      if (e->free_elt.next_uoffset == ~0)
	return ~0;

      /* Otherwise keep searching for large enough object. */
      e = mheap_elt_at_uoffset (v, e->free_elt.next_uoffset);
    }

  /* Free fragment at end. */
  hi_free_usize = f1 != o1 ? f1 - o1 - MHEAP_ELT_OVERHEAD_BYTES : 0;

  /* If fragment at end is too small to be a new object,
     give user's object a bit more space than requested. */
  if (hi_free_usize < (word) MHEAP_MIN_USER_DATA_BYTES)
    {
      search_n_user_data_bytes += f1 - o1;
      o1 = f1;
      hi_free_usize = 0;
    }

  /* Need to make sure that relevant memory areas are mapped. */
  if (! (h->flags & MHEAP_FLAG_DISABLE_VM)
      && mheap_vm_elt (v, MHEAP_VM_NOMAP, f0))
    {
      mheap_elt_t * f0_elt = mheap_elt_at_uoffset (v, f0);
      mheap_elt_t * f1_elt = mheap_elt_at_uoffset (v, f1);
      mheap_elt_t * o0_elt = mheap_elt_at_uoffset (v, o0);
      mheap_elt_t * o1_elt = mheap_elt_at_uoffset (v, o1);

      uword f0_page_start, f0_page_end;
      uword o0_page_start, o0_page_end;

      /* Free elt is mapped.  Addresses after that may not be mapped. */
      f0_page_start = mheap_page_round (pointer_to_uword (f0_elt->user_data));
      f0_page_end   = mheap_page_truncate (pointer_to_uword (f1_elt));

      o0_page_start = mheap_page_truncate (pointer_to_uword (o0_elt));
      o0_page_end = mheap_page_round (pointer_to_uword (o1_elt->user_data));

      if (o0_page_start < f0_page_start)
	o0_page_start = f0_page_start;
      if (o0_page_end > f0_page_end)
	o0_page_end = f0_page_end;

      if (o0_page_end > o0_page_start)
	clib_mem_vm_map (uword_to_pointer (o0_page_start, void *),
			 o0_page_end - o0_page_start);
    }

  /* Remove free object from free list. */
  remove_free_elt (v, e, bin);

  /* Free fragment at begining. */
  if (o0 != f0)
    {
      ASSERT (lo_free_usize >= (word) MHEAP_MIN_USER_DATA_BYTES);
      mheap_elt_set_size (v, f0, lo_free_usize, /* is_free */ 1);
      new_free_elt (v, f0, lo_free_usize);
    }

  mheap_elt_set_size (v, o0, search_n_user_data_bytes, /* is_free */ 0);

  if (hi_free_usize > 0)
    {
      uword uo = o1 + MHEAP_ELT_OVERHEAD_BYTES;
      mheap_elt_set_size (v, uo, hi_free_usize, /* is_free */ 1);
      new_free_elt (v, uo, hi_free_usize);
    }

  /* Return actual size of block. */
  *n_user_data_bytes_arg = search_n_user_data_bytes;

  return o0;
}

/* Search free lists for object with given size and alignment. */
static uword
mheap_get_search_free_list (void * v,
			    uword * n_user_bytes_arg,
			    uword align,
			    uword align_offset)
{
  mheap_t * h = mheap_header (v);
  uword bin, n_user_bytes, i, bi, r;

  r = ~0;
  if (! v)
    return r;

  n_user_bytes = *n_user_bytes_arg;
  bin = user_data_size_to_bin_index (n_user_bytes);

  for (i = bin / BITS (uword); i < ARRAY_LEN (h->non_empty_free_elt_heads); i++)
    {
      uword possible_bin_mask = h->non_empty_free_elt_heads[i];

      /* No need to search smaller bins. */
      if (i == 0 && bin > 0)
	possible_bin_mask &= ~pow2_mask (bin - 1);

      /* Search each occupied free bin which is large enough. */
      foreach_set_bit (bi, possible_bin_mask, ({
	r = mheap_get_search_free_bin (v, bi + i * BITS (uword), n_user_bytes_arg, align, align_offset);
	if (r != ~0)
	  return r;
      }));
    }

  return r;
}

static void *
mheap_get_extend_vector (void * v,
			 uword n_user_data_bytes,
			 uword align,
			 uword align_offset,
			 uword * offset_return)
{
  /* Bounds of free and allocated objects (as above). */
  uword f0, f1, o0, o1;
  word free_size;
  mheap_t * h = mheap_header (v);
  mheap_elt_t * e;

  if (_vec_len (v) == 0)
    {
      _vec_len (v) = MHEAP_ELT_OVERHEAD_BYTES;

      /* Create first element of heap. */
      e = mheap_elt_at_uoffset (v, _vec_len (v));
      e->prev_n_user_data = MHEAP_N_USER_DATA_INVALID;
    }

  f0 = _vec_len (v);

  o0 = round_pow2 (f0, align) - align_offset;
  while (1)
    {
      free_size = o0 - f0 - MHEAP_ELT_OVERHEAD_BYTES;
      if (o0 == f0 || free_size >= (word) sizeof (mheap_elt_t))
	break;

      o0 += align;
    }

  o1 = o0 + n_user_data_bytes;
  f1 = o1 + MHEAP_ELT_OVERHEAD_BYTES;
  
  ASSERT (v != 0);
  h = mheap_header (v);

  /* Make sure we have space for object plus overhead. */
  if (f1 > h->max_size)
    {
      *offset_return = ~0;
      return v;
    }

  _vec_len (v) = f1;

  if (! (h->flags & MHEAP_FLAG_DISABLE_VM))
    {
      mheap_elt_t * f0_elt = mheap_elt_at_uoffset (v, f0);
      mheap_elt_t * f1_elt = mheap_elt_at_uoffset (v, f1);

      uword f0_page = mheap_page_round (pointer_to_uword (f0_elt->user_data));
      uword f1_page = mheap_page_round (pointer_to_uword (f1_elt->user_data));

      if (f1_page > f0_page)
	mheap_vm (v, MHEAP_VM_MAP, f0_page, f1_page - f0_page);
    }

  if (free_size > 0)
    new_free_elt (v, f0, free_size);

  mheap_elt_set_size (v, o0, n_user_data_bytes, /* is_free */ 0);

  /* Mark last element. */
  e = mheap_elt_at_uoffset (v, f1);
  e->n_user_data = MHEAP_N_USER_DATA_INVALID;

  *offset_return = o0;

  return v;
}

void * mheap_get_aligned (void * v,
			  uword n_user_data_bytes,
			  uword align,
			  uword align_offset,
			  uword * offset_return)
{
  mheap_t * h;
  uword offset;

  align = clib_max (align, STRUCT_SIZE_OF (mheap_elt_t, user_data[0]));
  align = max_pow2 (align);

  /* Correct align offset to be smaller than alignment. */
  align_offset &= (align - 1);

  /* Align offset must be multiple of minimum object size. */
  if (align_offset % STRUCT_SIZE_OF (mheap_elt_t, user_data[0]) != 0)
    {
      *offset_return = ~0;
      return v;
    }

  /* Round requested size. */
  n_user_data_bytes = clib_max (n_user_data_bytes, MHEAP_MIN_USER_DATA_BYTES);
  n_user_data_bytes = round_pow2 (n_user_data_bytes, STRUCT_SIZE_OF (mheap_elt_t, user_data[0]));

  if (! v)
    v = mheap_alloc (0, 64 << 20);

  mheap_maybe_lock (v);

  h = mheap_header (v);

  if (h->flags & MHEAP_FLAG_VALIDATE)
    mheap_validate (v);

  /* First search free lists for object. */
  offset = mheap_get_search_free_list (v, &n_user_data_bytes, align, align_offset);

  h = mheap_header (v);

  /* If that fails allocate object at end of heap by extending vector. */
  if (offset == ~0)
    {
      /* Always leave at least 4k bytes of room for free lists. */
      if (_vec_len (v) < h->max_size)
	{
	  v = mheap_get_extend_vector (v, n_user_data_bytes, align, align_offset, &offset);
	  h = mheap_header (v);
	}
    }

  *offset_return = offset;
  if (offset != ~0)
    {
      h->n_elts += 1;

      if ((h->flags & MHEAP_FLAG_TRACE))
	{
	  /* Recursion block for case when we are traceing main clib heap. */
	  h->flags &= ~MHEAP_FLAG_TRACE;

	  mheap_get_trace (v, offset, n_user_data_bytes);

	  h->flags |= MHEAP_FLAG_TRACE;
	}
    }

  if (h->flags & MHEAP_FLAG_VALIDATE)
    mheap_validate (v);

  mheap_maybe_unlock (v);

  return v;
}

static void free_last_elt (void * v, mheap_elt_t * e)
{
  mheap_t * h = mheap_header (v);

  /* Possibly delete preceeding free element also. */
  if (e->prev_is_free)
    {
      e = mheap_prev_elt (e);
      remove_free_elt2 (v, e);
    }

  if (e->prev_n_user_data == MHEAP_N_USER_DATA_INVALID)
    {
      if (! (h->flags & MHEAP_FLAG_DISABLE_VM))
	mheap_vm_elt (v, MHEAP_VM_UNMAP, mheap_elt_uoffset (v, e));
      _vec_len (v) = 0;
    }
  else
    {
      uword uo = mheap_elt_uoffset (v, e);
      if (! (h->flags & MHEAP_FLAG_DISABLE_VM))
	mheap_vm_elt (v, MHEAP_VM_UNMAP, uo);
      e->n_user_data = MHEAP_N_USER_DATA_INVALID;
      _vec_len (v) = uo;
    }
}

void mheap_put (void * v, uword uoffset)
{
  mheap_t * h;
  uword b, size, n_user_data_bytes;
  mheap_elt_t * e, * n, * p;

  h = mheap_header (v);

  mheap_maybe_lock (v);

  if (h->flags & MHEAP_FLAG_VALIDATE)
    mheap_validate (v);

  ASSERT (h->n_elts > 0);
  h->n_elts--;

  e = mheap_elt_at_uoffset (v, uoffset);
  n = mheap_next_elt (e);
  n_user_data_bytes = mheap_elt_data_bytes (e);

  /* Assert that forward and back pointers are equal. */
  if (e->n_user_data != n->prev_n_user_data)
    os_panic ();

  /* Forward and backwards is_free must agree. */
  if (e->is_free != n->prev_is_free)
    os_panic ();

  /* Object was already freed. */
  if (e->is_free)
    os_panic ();

  /* Special case: delete last element in heap. */
  if (n->n_user_data == MHEAP_N_USER_DATA_INVALID)
    free_last_elt (v, e);

  else
    {
      uword f0, f1, n_combine;

      f0 = uoffset;
      f1 = f0 + n_user_data_bytes;
      n_combine = 0;

      if (e->prev_is_free)
	{
	  mheap_elt_t * p = mheap_prev_elt (e);
	  f0 = mheap_elt_uoffset (v, p);
	  remove_free_elt2 (v, p);
	  n_combine++;
	}

      if (n->is_free)
	{
	  mheap_elt_t * m = mheap_next_elt (n);
	  f1 = (void *) m - v;
	  remove_free_elt2 (v, n);
	  n_combine++;
	}

      if (n_combine)
	mheap_elt_set_size (v, f0, f1 - f0, /* is_free */ 1);
      else
	e->is_free = n->prev_is_free = 1;
      set_free_elt (v, f0, f1 - f0);

      if (! (h->flags & MHEAP_FLAG_DISABLE_VM))
	mheap_vm_elt (v, MHEAP_VM_UNMAP, f0);
    }

  h = mheap_header (v);

  if (h->flags & MHEAP_FLAG_TRACE)
    {
      /* Recursion block for case when we are traceing main clib heap. */
      h->flags &= ~MHEAP_FLAG_TRACE;

      mheap_put_trace (v, uoffset, n_user_data_bytes);

      h->flags |= MHEAP_FLAG_TRACE;
    }

  if (h->flags & MHEAP_FLAG_VALIDATE)
    mheap_validate (v);

  mheap_maybe_unlock (v);
}

static uword mheap_vm (void * v,
		       uword flags,
		       clib_address_t start_addr,
		       uword size)
{
  mheap_t * h = mheap_header (v);
  clib_address_t start_page, end_page, end_addr;
  uword mapped_bytes;

  ASSERT (! (h->flags & MHEAP_FLAG_DISABLE_VM));

  end_addr = start_addr + size;

  /* Round start/end address up to page boundary. */
  start_page = mheap_page_round (start_addr);

  if ((flags & MHEAP_VM_ROUND) == MHEAP_VM_ROUND_UP)
    end_page = mheap_page_round (end_addr);
  else
    end_page = mheap_page_truncate (end_addr);

  mapped_bytes = 0;
  if (end_page > start_page)
    {
      mapped_bytes = end_page - start_page;
      if (flags & MHEAP_VM_MAP)
	clib_mem_vm_map ((void *) start_page, end_page - start_page);
      else if (flags & MHEAP_VM_UNMAP)
	clib_mem_vm_unmap ((void *) start_page, end_page - start_page);
    }

  return mapped_bytes;
}

static uword mheap_vm_elt (void * v, uword flags, uword offset)
{
  mheap_elt_t * e;
  clib_address_t start_addr, end_addr;

  e = mheap_elt_at_uoffset (v, offset);
  start_addr = (clib_address_t) ((void *) e->user_data);
  end_addr = (clib_address_t) mheap_next_elt (e);
  return mheap_vm (v, flags, start_addr, end_addr - start_addr);
}

always_inline uword
mheap_vm_alloc_size (void * v)
{
  mheap_t * h = mheap_header (v);
  return (u8 *) v + h->max_size - (u8 *) h;
}

void * mheap_alloc_with_flags (void * memory, uword size, uword flags)
{
  mheap_t * h;
  void * v;

  if (! mheap_page_size)
    mheap_page_size = clib_mem_get_page_size ();

  if (! memory)
    {
      /* No memory given, try to VM allocate some. */
      memory = clib_mem_vm_alloc (size);
      if (! memory)
	return 0;

      /* No memory region implies we have virtual memory. */
      flags &= ~MHEAP_FLAG_DISABLE_VM;
    }

  {
    uword n, m;

    /* Make sure that given memory is page aligned. */
    m = pointer_to_uword (memory);
    n = round_pow2 (m, mheap_page_size);
    if (n - sizeof (h[0]) - sizeof (_vec_len (v)) < m)
      n += mheap_page_size;
    n -= sizeof (h[0]) + sizeof (_vec_len (v));

    size -= n - m;
    memory = uword_to_pointer (n, void *);
  }

  /* VM map header so we can use memory. */
  h = memory;
  if (! (flags & MHEAP_FLAG_DISABLE_VM))
    clib_mem_vm_map (h, sizeof (h[0]));

  v = mheap_vector (h);

  /* Zero vector header: both heap header and vector length. */
  memset (h, 0, sizeof (h[0]));
  _vec_len (v) = 0;

  h->max_size = (memory + size - v);

  /* Set flags based on those given less builtin-flags. */
  h->flags |= (flags &~ MHEAP_FLAG_TRACE);

  if (h->flags & MHEAP_FLAG_THREAD_SAFE)
    mheap_lock_init (h);

  /* Unmap remainder of heap until we will be ready to use it. */
  if (! (h->flags & MHEAP_FLAG_DISABLE_VM))
    mheap_vm (v, MHEAP_VM_UNMAP | MHEAP_VM_ROUND_UP,
	      (clib_address_t) v, h->max_size);

  /* Initialize free list heads to empty. */
  memset (h->first_free_elt_uoffset_by_bin, ~0, sizeof (h->first_free_elt_uoffset_by_bin));

  ASSERT (size == mheap_vm_alloc_size (v));

  return v;
}

void * mheap_alloc (void * memory, uword size)
{
  return mheap_alloc_with_flags (memory, size,
				 /* flags */ memory != 0 ? MHEAP_FLAG_DISABLE_VM : 0);
}

void * _mheap_free (void * v)
{
  mheap_t * h = mheap_header (v);

  if (v)
    {
      /* No need to free free lists since they are allocated in heap itself. */
      clib_mem_vm_free (h, mheap_vm_alloc_size (v));
    }
  
  return 0;
}

/* Call user's function with each object in heap. */
void mheap_foreach (void * v,
		    uword (* func) (void * arg, void * v, void * elt_data, uword elt_size),
		    void * arg)
{
  mheap_t * h = mheap_header (v);
  mheap_elt_t * e;
  uword b;
  void * p;
  u8 * stack_heap, * clib_mem_mheap_save;
  u8 tmp_heap_memory[16*1024];

  mheap_maybe_lock (v);

  if (vec_len (v) == 0)
    goto done;

  clib_mem_mheap_save = 0;
  stack_heap = 0;

  /* Allocate a new temporary heap on the stack.
     This is so that our hash table & user's callback function can
     themselves allocate memory somewhere without getting in the way
     of the heap we are looking at. */
  if (v == clib_mem_get_heap ())
    {
      stack_heap = mheap_alloc (tmp_heap_memory, sizeof (tmp_heap_memory));
      clib_mem_mheap_save = v;
      clib_mem_set_heap (stack_heap);
    }

  for (e = v;
       e->n_user_data != MHEAP_N_USER_DATA_INVALID;
       e = mheap_next_elt (e))
    {
      void * p = mheap_elt_data (v, e);
      if (e->is_free)
	continue;
      if ((* func) (arg, v, p, mheap_elt_data_bytes (e)))
	break;
    }

  /* Restore main CLIB heap. */
  if (clib_mem_mheap_save)
    clib_mem_set_heap (clib_mem_mheap_save);

 done:
  mheap_maybe_unlock (v);
}

/* Bytes in mheap header overhead not including data bytes. */
always_inline uword
mheap_bytes_overhead (void * v)
{
  mheap_t * h = mheap_header (v);
  return v ? sizeof (h[0]) + h->n_elts * sizeof (mheap_elt_t) : 0;
}

/* Total number of bytes including both data and overhead. */
uword mheap_bytes (void * v)
{ return mheap_bytes_overhead (v) + vec_bytes (v); }

static void mheap_usage_no_lock (void * v, clib_mem_usage_t * usage)
{
  mheap_t * h = mheap_header (v);
  uword used = 0, free = 0, free_vm_unmapped = 0;

  if (vec_len (v) > 0)
    {
      mheap_elt_t * e;

      for (e = v;
	   e->n_user_data != MHEAP_N_USER_DATA_INVALID;
	   e = mheap_next_elt (e))
	{
	  uword size = mheap_elt_data_bytes (e);
	  if (e->is_free)
	    {
	      free += size;
	      if (! (h->flags & MHEAP_FLAG_DISABLE_VM))
		free_vm_unmapped +=
		  mheap_vm_elt (v, MHEAP_VM_NOMAP, mheap_elt_uoffset (v, e));
	    }
	  else
	    used += size;
	}
    }

  usage->object_count = mheap_elts (v);
  usage->bytes_total = mheap_bytes (v);
  usage->bytes_overhead = mheap_bytes_overhead (v);
  usage->bytes_max = mheap_max_size (v);
  usage->bytes_used = used;
  usage->bytes_free = free;
  usage->bytes_free_reclaimed = free_vm_unmapped;
}

void mheap_usage (void * v, clib_mem_usage_t * usage)
{
  mheap_maybe_lock (v);
  mheap_usage_no_lock (v, usage);
  mheap_maybe_unlock (v);
}

static u8 * format_mheap_byte_count (u8 * s, va_list * va)
{
  uword n_bytes = va_arg (*va, uword);
  if (n_bytes < 1024)
    return format (s, "%wd", n_bytes);
  else
    return format (s, "%wdk", n_bytes / 1024);
}

/* Returns first corrupt heap element. */
static mheap_elt_t * mheap_first_corrupt (void * v)
{
  mheap_elt_t * e, * n;

  if (vec_len (v) == 0)
    return 0;

  e = v;
  while (1)
    {
      if (e->n_user_data == MHEAP_N_USER_DATA_INVALID)
	break;

      n = mheap_next_elt (e);

      if (e->n_user_data != n->prev_n_user_data)
	return e;

      if (e->is_free != n->prev_is_free)
	return e;

      e = n;
    }

  return 0;
}

u8 * format_mheap (u8 * s, va_list * va)
{
  void * v = va_arg (*va, u8 *);
  int verbose = va_arg (*va, int);

  mheap_t * h;
  uword i, o, size;
  clib_mem_usage_t usage;
  mheap_elt_t * first_corrupt;

  mheap_maybe_lock (v);

  h = mheap_header (v);

  mheap_usage_no_lock (v, &usage);

  s = format (s, "%d objects, %U of %U used, %U free, %U reclaimed, %U overhead",
	      usage.object_count,
	      format_mheap_byte_count, usage.bytes_used,
	      format_mheap_byte_count, usage.bytes_total,
	      format_mheap_byte_count, usage.bytes_free,
	      format_mheap_byte_count, usage.bytes_free_reclaimed,
	      format_mheap_byte_count, usage.bytes_overhead);

  if (usage.bytes_max != ~0)
    s = format (s, ", %U capacity", format_mheap_byte_count, usage.bytes_max);

  if ((h->flags & MHEAP_FLAG_TRACE) && vec_len (h->trace_main.traces) > 0)
    {
      /* Make a copy of traces since we'll be sorting them. */
      mheap_trace_t * t, * traces_copy;
      uword indent, total_objects_traced;

      traces_copy = vec_dup (h->trace_main.traces);
      qsort (traces_copy, vec_len (traces_copy), sizeof (traces_copy[0]),
	     mheap_trace_sort);

      total_objects_traced = 0;
      s = format (s, "\n");
      vec_foreach (t, traces_copy) {
	/* Skip over free elements. */
	if (t->n_allocations == 0)
	  continue;

	total_objects_traced += t->n_allocations;

	/* When not verbose only report allocations of more than 1k. */
	if (! verbose && t->n_bytes < 1024)
	    continue;

	if (t == traces_copy)
	  s = format (s, "%=9s%=9s %=10s Traceback\n", "Bytes", "Count", 
            "Sample");
	s = format (s, "%9d%9d %p", t->n_bytes, t->n_allocations, 
                    t->offset + v);
	indent = format_get_indent (s);
	for (i = 0; i < ARRAY_LEN (t->callers) && t->callers[i]; i++)
	  {
	    if (i > 0)
	      s = format (s, "%U", format_white_space, indent);
	    s = format (s, " %p\n", t->callers[i]);
	  }
      }

      s = format (s, "%d total traced objects\n", total_objects_traced);

      vec_free (traces_copy);
  }

  first_corrupt = mheap_first_corrupt (v);
  if (first_corrupt)
    {
      size = mheap_elt_data_bytes (first_corrupt);
      s = format (s, "\n  first corrupt object: %p, size %wd\n  %U",
		  first_corrupt,
		  size,
		  format_hex_bytes, first_corrupt, size);
    }

  /* FIXME.  This output could be wrong in the unlikely case that format
     uses the same mheap as we are currently inspecting. */
  if (verbose > 1)
    {
      mheap_elt_t * e;
      uword i, o;

      s = format (s, "\n");

      e = mheap_elt_at_uoffset (v, 0);
      i = 0;
      while (1)
	{
	  if ((i % 8) == 0)
	    s = format (s, "%8d: ", i);

	  o = mheap_elt_uoffset (v, e);

	  if (e->is_free)
	    s = format (s, "(%8d) ", o);
	  else
	    s = format (s, " %8d  ", o);

	  if ((i % 8) == 7 || (i + 1) >= h->n_elts)
	    s = format (s, "\n");
	}
    }

  mheap_maybe_unlock (v);

  return s;
}

void dmh (void * v)
{ fformat (stderr, "%U", format_mheap, v, 1); }

static void mheap_validate_breakpoint ()
{ os_panic (); }

void mheap_validate (void * v)
{
  mheap_t * h = mheap_header (v);
  uword i, o, s;

  uword elt_count, elt_size;
  uword free_count_from_free_lists, free_size_from_free_lists;

  clib_error_t * error = 0;

#define CHECK(x) if (! (x)) { mheap_validate_breakpoint (); os_panic (); }

  if (vec_len (v) == 0)
    return;

  mheap_maybe_lock (v);

  /* Validate number of elements and size. */
  free_size_from_free_lists = free_count_from_free_lists = 0;
  for (i = 0; i < ARRAY_LEN (h->first_free_elt_uoffset_by_bin); i++)
    {
      mheap_elt_t * e, * n;
      uword is_first;

      CHECK ((h->first_free_elt_uoffset_by_bin[i] != ~0)
	     == ((h->non_empty_free_elt_heads[i / BITS (uword)] & ((uword) 1 << (uword) (i % BITS (uword)))) != 0));

      if (h->first_free_elt_uoffset_by_bin[i] == ~0)
	continue;

      e = mheap_elt_at_uoffset (v, h->first_free_elt_uoffset_by_bin[i]);
      is_first = 1;
      while (1)
	{
	  uword s;

	  n = mheap_next_elt (e);

	  /* Object must be marked free. */
	  CHECK (e->is_free);

	  /* Next object's previous free bit must also be set. */
	  CHECK (n->prev_is_free);

	  if (is_first)
	    CHECK (e->free_elt.prev_uoffset == ~0);
	  is_first = 0;

	  s = mheap_elt_data_bytes (e);
	  CHECK (user_data_size_to_bin_index (s) == i);

	  free_count_from_free_lists += 1;
	  free_size_from_free_lists += s;

	  if (e->free_elt.next_uoffset == ~0)
	    break;

	  n = mheap_elt_at_uoffset (v, e->free_elt.next_uoffset);

	  /* Check free element linkages. */
	  CHECK (n->free_elt.prev_uoffset == mheap_elt_uoffset (v, e));

	  e = n;
	}
    }

  {
    mheap_elt_t * e, * n;
    uword elt_free_size, elt_free_count;

    elt_count = elt_size = elt_free_size = elt_free_count = 0;
    for (e = v;
	 e->n_user_data != MHEAP_N_USER_DATA_INVALID;
	 e = n)
      {
	if (e->prev_n_user_data != MHEAP_N_USER_DATA_INVALID)
	  CHECK (e->prev_n_user_data * sizeof (e->user_data[0]) >= MHEAP_MIN_USER_DATA_BYTES);

	CHECK (e->n_user_data * sizeof (e->user_data[0]) >= MHEAP_MIN_USER_DATA_BYTES);

	n = mheap_next_elt (e);

	CHECK (e->is_free == n->prev_is_free);

	elt_count++;
	s = mheap_elt_data_bytes (e);
	elt_size += s;

	if (e->is_free)
	  {
	    elt_free_count++;
	    elt_free_size += s;
	  }

	/* Consecutive free objects should have been combined. */
	CHECK (! (e->prev_is_free && n->prev_is_free));
      }

    CHECK (free_count_from_free_lists == elt_free_count);
    CHECK (free_size_from_free_lists == elt_free_size);
    CHECK (elt_count == h->n_elts + elt_free_count);
    CHECK (elt_size + (elt_count + 1) * MHEAP_ELT_OVERHEAD_BYTES == vec_len (v));
  }

  {
    mheap_elt_t * e, * n;

    for (e = v;
	 e->n_user_data == MHEAP_N_USER_DATA_INVALID;
	 e = n)
      {
	n = mheap_next_elt (e);
	CHECK (e->n_user_data == n->prev_n_user_data);
      }
  }

#undef CHECK

  mheap_maybe_unlock (v);

  h->validate_serial += 1;
}

static void mheap_get_trace (void * v, uword offset, uword size)
{
  mheap_t * h;
  mheap_trace_main_t * tm;
  mheap_trace_t * t;
  uword i, n_callers, trace_index, * p;
  mheap_trace_t trace;

  n_callers = clib_backtrace (trace.callers, ARRAY_LEN (trace.callers),
			      /* Skip mheap_get_aligned's frame */ 1);
  if (n_callers == 0)
      return;

  for (i = n_callers; i < ARRAY_LEN (trace.callers); i++)
    trace.callers[i] = 0;

  h = mheap_header (v);
  tm = &h->trace_main;

  if (! tm->trace_by_callers)
    tm->trace_by_callers = hash_create_mem (0, sizeof (trace.callers), sizeof (uword));

  p = hash_get_mem (tm->trace_by_callers, &trace.callers);
  if (p)
    {
      trace_index = p[0];
      t = tm->traces + trace_index;
    }
  else
    {
      i = vec_len (tm->trace_free_list);
      if (i > 0)
	{
	  trace_index = tm->trace_free_list[i - 1];
	  _vec_len (tm->trace_free_list) = i - 1;
	}
      else
	{
	  mheap_trace_t * old_start = tm->traces;
	  mheap_trace_t * old_end = vec_end (tm->traces);

	  vec_add2 (tm->traces, t, 1);

	  if (tm->traces != old_start) {
	    hash_pair_t * p;
	    mheap_trace_t * q;
	    hash_foreach_pair (p, tm->trace_by_callers, ({
	      q = uword_to_pointer (p->key, mheap_trace_t *);
	      ASSERT (q >= old_start && q < old_end);
	      p->key = pointer_to_uword (tm->traces + (q - old_start));
	    }));
	  }
	  trace_index = t - tm->traces;
	}

      t = tm->traces + trace_index;
      t[0] = trace;
      t->n_allocations = 0;
      t->n_bytes = 0;
      hash_set_mem (tm->trace_by_callers, t->callers, trace_index);
    }

  t->n_allocations += 1;
  t->n_bytes += size;
  t->offset = offset;           /* keep a sample to autopsy */
  hash_set (tm->trace_index_by_offset, offset, t - tm->traces);
}

static void mheap_put_trace (void * v, uword offset, uword size)
{
  mheap_t * h;
  mheap_trace_main_t * tm;
  mheap_trace_t * t;
  uword trace_index, * p;

  h = mheap_header (v);
  tm = &h->trace_main;
  p = hash_get (tm->trace_index_by_offset, offset);
  if (! p)
    return;

  trace_index = p[0];
  hash_unset (tm->trace_index_by_offset, offset);
  ASSERT (trace_index < vec_len (tm->traces));

  t = tm->traces + trace_index;
  ASSERT (t->n_allocations > 0);
  ASSERT (t->n_bytes >= size);
  t->n_allocations -= 1;
  t->n_bytes -= size;
  if (t->n_allocations == 0)
    {
      hash_unset_mem (tm->trace_by_callers, t->callers);
      vec_add1 (tm->trace_free_list, trace_index);
      memset (t, 0, sizeof (t[0]));
    }
}

static int mheap_trace_sort (const void * _t1, const void * _t2)
{
  const mheap_trace_t * t1 = _t1;
  const mheap_trace_t * t2 = _t2;
  word cmp;

  cmp = (word) t2->n_bytes - (word) t1->n_bytes;
  if (! cmp)
    cmp = (word) t2->n_allocations - (word) t1->n_allocations;
  return cmp;
}

always_inline void
mheap_trace_main_free (mheap_trace_main_t * tm)
{
  vec_free (tm->traces);
  vec_free (tm->trace_free_list);
  hash_free (tm->trace_by_callers);
  hash_free (tm->trace_index_by_offset);
}

void mheap_trace (void * v, int enable)
{
  mheap_t * h;

  h = mheap_header (v);

  if (enable)
    {
      h->flags |= MHEAP_FLAG_TRACE;
    }
  else
    {
      mheap_trace_main_free (&h->trace_main);
      h->flags &= ~MHEAP_FLAG_TRACE;
    }
}
