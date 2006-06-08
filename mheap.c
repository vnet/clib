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

#include <clib/mheap.h>
#include <clib/hash.h>
#include <clib/format.h>
#include <clib/os.h>

static void mheap_get_trace (u8 * v, uword offset, uword size);
static void mheap_put_trace (u8 * v, uword offset, uword size);
static int mheap_trace_sort (const void * t1, const void * t2);

static inline uword
mheap_prev_is_free (mheap_elt_t * e)
{ return (e->prev_size & MHEAP_PREV_IS_FREE) != 0; }

static inline uword
mheap_is_free (u8 * v, mheap_elt_t * e)
{
  /* Final element is never free since it would have been
     removed from end of vector. */
  if (mheap_is_last (e))
    return 0;
  else
    return mheap_prev_is_free (mheap_next_elt (v, e));
}

static inline void
mheap_elt_set_size (u8 * v, uword offset, uword size, uword flags)
{
  mheap_elt_t * e = mheap_elt_at_offset (v, offset);
  mheap_elt_t * n;

  e->size = size | (offset == 0 ? MHEAP_IS_FIRST : 0);
  n = mheap_next_elt (v, e);
  n->prev_size = size | flags;
}

/* Minimum sized objects includes one header and enough data
   to write a free element header. */
#define MHEAP_MIN_SIZE (sizeof (mheap_elt_t) + mheap_round_size (1))

static inline uword
size_to_bin (uword size)
{
  uword bin;

  ASSERT (size > 0);
  ASSERT (0 == (size & (sizeof (mheap_size_t) - 1)));

  size = (size - sizeof (mheap_elt_t)) / sizeof (mheap_size_t);

  if (size <= MHEAP_SMALL_BINS)
    {
      bin = size - 1;
      if (size == 0)
	bin = 0;
    }
  else
    {
      bin = MHEAP_SMALL_BINS + max_log2 (size) - (MHEAP_LOG2_SMALL_BINS + 1);
      if (bin >= MHEAP_N_BINS)
	bin = MHEAP_N_BINS - 1;
    }

  return bin;
}

static inline uword
bin_to_size (uword bin)
{
  uword size;

  if (bin <= MHEAP_SMALL_BINS - 1)
    size = bin + 1;
  else
    size = 1 << ((bin - MHEAP_SMALL_BINS) + MHEAP_LOG2_SMALL_BINS + 1);

  size = (size * sizeof (mheap_size_t) + sizeof (mheap_elt_t));

  return size;
}

/* Vector add operations may allocate memory.  The CLIB memory
   allocator may be using the same mheap object that we are operating
   on.  So, we need to be careful to avoid recursion problems free
   list adds.

   We accomplish this by hand allocating the free lists.  When a heap
   is first referenced each free list is allocated to a small fixed
   size.  Subsequently, we track when a bin will soon need resizing
   and resize them then.  This way we ensure that mheap_{get,put}
   operations are not recursive. */

static inline uword
free_list_length_needs_resize (uword current_len, uword max_len)
{
  ASSERT (max_len >= current_len);
  return (max_len - current_len) <= 2;
}

/* Returns offset of free list vector for given bin. */
static inline uword
free_list_offset (void * v, uword i)
{
  mheap_t * h = mheap_header (v);
  void * f = h->free_lists[i];
  
  ASSERT (f != 0);
  return (void *) &_vec_len (f) - v;
}

/* Max number of free heap elements we can store in given
   free list bin. */
static inline uword
free_list_max_len (void * v, uword i)
{
  mheap_t * h = mheap_header (v);
  mheap_free_elt_t * f = h->free_lists[i];
  uword l;

  l = 0;
  if (f != 0)
    {
      l = free_list_offset (v, i);
      l = mheap_data_bytes (v, l) - sizeof (_vec_len (f));
      l /= sizeof (f[0]);
    }

  return l;
}

static void *
free_list_resize (void * v)
{
  mheap_t * h = mheap_header (v);
  u32 old_offsets[ARRAY_LEN (h->free_lists)];
  uword i;

  if (! (h->flags & MHEAP_FLAG_FREE_LISTS_NEED_RESIZE))
    goto done;
  h->flags &= ~MHEAP_FLAG_FREE_LISTS_NEED_RESIZE;
  h->flags |= MHEAP_FLAG_INHIBIT_FREE_LIST_SEARCH;

  for (i = 0; i < ARRAY_LEN (h->free_lists); i++)
    {
      mheap_free_elt_t * f = h->free_lists[i];
      uword l = vec_len (f);
      uword l_max = free_list_max_len (v, i);

      old_offsets[i] = 0;
      if (free_list_length_needs_resize (l, l_max))
	{
	  uword new_len = l_max < 8 ? 8 : 2*l_max;
	  mheap_free_elt_t * f_new;
	  uword new_offset;

	  v = mheap_get_aligned (v,
				 new_len * sizeof (f_new[0]) + sizeof (_vec_len (f_new)),
				 1, 0,
				 &new_offset);
	  if (new_offset == ~0)
	    clib_panic ("ran out of memory resizing free list");
				       
	  f_new = (void *) (v + new_offset) + sizeof (_vec_len (v));
	  _vec_len (f_new) = l;
	  if (l > 0)
	    {
	      memcpy (f_new, f, l * sizeof (f[0]));
	      old_offsets[i] = free_list_offset (v, i);
	    }

	  h->free_lists[i] = f_new;

	  ASSERT (free_list_max_len (v, i) == new_len);
	}
    }

  h->flags &= ~MHEAP_FLAG_INHIBIT_FREE_LIST_SEARCH;

  for (i = 0; i < ARRAY_LEN (h->free_lists); i++)
    if (old_offsets[i])
      mheap_put (v, old_offsets[i]);

 done:
  return v;
}

static inline mheap_free_elt_t *
add_to_free_list (void * v, uword bin)
{
  mheap_t * h = mheap_header (v);
  mheap_free_elt_t * f = h->free_lists[bin];
  uword l = _vec_len (f);
  uword l_max = free_list_max_len (v, bin);

  ASSERT (l < l_max);

  _vec_len (f) = l + 1;

  /* Record globally whether any free list bin needs to be resized. */
  if (free_list_length_needs_resize (l + 1, l_max))
    h->flags |= MHEAP_FLAG_FREE_LISTS_NEED_RESIZE;

  return f + l;
}

static inline void
set_free_elt2 (u8 * v, mheap_elt_t * e, mheap_elt_t * n, uword fi)
{
  *mheap_elt_data (v, e) = fi;
  n->prev_size |= MHEAP_PREV_IS_FREE;
}

static inline void
set_free_elt (u8 * v, uword i, uword fi)
{
  mheap_elt_t * e = mheap_elt_at_offset (v, i);
  mheap_elt_t * n = mheap_next_elt (v, e);
  set_free_elt2 (v, e, n, fi);
}

#define get_free_elt(v,e,fb,fi)				\
do {							\
  mheap_elt_t * _e = (e);				\
  ASSERT (mheap_is_free (v, _e));			\
  fb = size_to_bin (mheap_trunc_size (_e->size));	\
  fi = *mheap_elt_data (v, e);				\
} while (0)

static inline void
add_free_elt (u8 * v, uword offset, uword size)
{
  mheap_t * h = mheap_header (v);
  mheap_free_elt_t * f;
  uword bin;

  bin = size_to_bin (size);
  f = add_to_free_list (v, bin);
  f->offset = offset;
  f->size = size;
  mheap_elt_set_size (v, offset, size, MHEAP_PREV_IS_FREE);
  set_free_elt (v, f->offset, f - h->free_lists[bin]);
}

static inline void
remove_free_elt (u8 * v, uword b, uword i)
{
  mheap_t * h = mheap_header (v);
  uword l;
  mheap_free_elt_t * f;

  l = vec_len (h->free_lists[b]);

  ASSERT (b < MHEAP_N_BINS);
  ASSERT (i < l);

  if (i < l - 1)
    {
      /* Move last free list to index I. */
      f = h->free_lists[b] + l - 1;
      h->free_lists[b][i] = f[0];
      set_free_elt (v, f->offset, i);
    }
  _vec_len (h->free_lists[b]) = l - 1;
}

#define MHEAP_VM_MAP		1
#define MHEAP_VM_UNMAP		2
#define MHEAP_VM_NOMAP		0
#define MHEAP_VM_ROUND		4
#define MHEAP_VM_ROUND_UP	MHEAP_VM_ROUND
#define MHEAP_VM_ROUND_DOWN	0

static uword mheap_vm (u8 * v, uword flags,
		       clib_address_t start_addr, uword size);
static uword mheap_vm_elt (u8 * v, uword flags, uword offset);

static uword mheap_page_size;

static inline uword mheap_page_round (uword addr)
{ return (addr + mheap_page_size - 1) &~ (mheap_page_size - 1); }

static inline uword mheap_page_truncate (uword addr)
{ return addr &~ (mheap_page_size - 1); }

/* Search free lists for object with given size and alignment. */
static uword mheap_get_search_free_list (u8 * v,
					 uword * size_arg,
					 uword align,
					 uword align_offset)
{
  mheap_t * h = mheap_header (v);
  uword bin, bin_len, size;

  if (! v || (h->flags & MHEAP_FLAG_INHIBIT_FREE_LIST_SEARCH))
    return ~0;

  size = *size_arg;

  /* Search free lists for bins >= given size. */
  for (bin = size_to_bin (size); bin < ARRAY_LEN (h->free_lists); bin++)
    {
      if ((bin_len = vec_len (h->free_lists[bin])) > 0)
	{
	  word free_index;

	  /* Free object is at offset f0 ... f1;
	     Allocatted object is at offset o0 ... o1. */
	  word o0, o1, f0, f1, frag_size;

	  /* Silence compiler warning. */
	  o0 = o1 = f0 = f1 = 0;

	  /* Find an object that is large enough.
	     Search list in reverse so that more recently freed objects will be
	     allocated again sooner. */
	  for (free_index = bin_len - 1; free_index >= 0; free_index--)
	    {
	      mheap_free_elt_t * f = h->free_lists[bin] + free_index;

	      if (f->size < size)
		continue;

	      /* Bounds of free object. */
	      f0 = f->offset;
	      f1 = f0 + f->size;

	      /* Place candidate object at end of free block. */
	      o0 = ((f1 - size) &~ (align - 1)) - align_offset;
	      while (o0 < f0)
		o0 += align;

	      /* Make sure object is aligned and that first free
		 fragment is large enough to be valid. */
	      frag_size = o0 - f0;
	      if (frag_size > 0 && frag_size < MHEAP_MIN_SIZE)
		{
		  o0 += (MHEAP_MIN_SIZE + align - 1) &~ (align - 1);
		  frag_size = o0 - f0;
		  ASSERT (frag_size >= MHEAP_MIN_SIZE);
		}

	      o1 = o0 + size;

	      /* Does it fit? */
	      if (o0 >= f0 && o1 <= f1)
		break;
	    }

	  /* If we fail to find a large enough object,
	     try the next larger sized bin. */
	  if (free_index < 0)
	    continue;

	  /* Need to make sure that relevant memory areas are mapped. */
	  if (! (h->flags & MHEAP_FLAG_NO_VM)
	      && mheap_vm_elt (v, MHEAP_VM_NOMAP, f0))
	    {
	      mheap_elt_t * f0_elt = mheap_elt_at_offset (v, f0);
	      mheap_elt_t * f1_elt = mheap_elt_at_offset (v, f1);
	      mheap_elt_t * o0_elt = mheap_elt_at_offset (v, o0);
	      mheap_elt_t * o1_elt = mheap_elt_at_offset (v, o1);

	      uword f0_page_start, f0_page_end;
	      uword o0_page_start, o0_page_end;

	      /* Free elt and index of free list entry are mapped.  Addresses after
		 that may not be mapped. */
	      f0_page_start = pointer_to_uword (mheap_elt_data (v, f0_elt)) + sizeof (mheap_size_t);
	      f0_page_start = mheap_page_round (f0_page_start);
	      f0_page_end   = mheap_page_truncate (pointer_to_uword (f1_elt));

	      o0_page_start = pointer_to_uword (o0_elt);
	      o0_page_start = mheap_page_truncate (o0_page_start);
	      o0_page_end = pointer_to_uword (o1_elt) + MHEAP_MIN_SIZE;
	      o0_page_end = mheap_page_round (o0_page_end);

	      if (o0_page_start < f0_page_start)
		o0_page_start = f0_page_start;
	      if (o0_page_end > f0_page_end)
		o0_page_end = f0_page_end;

	      if (o0_page_end > o0_page_start)
		clib_mem_vm_map (uword_to_pointer (o0_page_start, void *),
				 o0_page_end - o0_page_start);
	    }

	  /* Free fragment at end. */
	  frag_size = f1 - o1;
	  ASSERT (frag_size >= 0);
	  if (frag_size < MHEAP_MIN_SIZE)
	    {
	      /* If its too small give user's object a bit more space
		 than requested. */
	      size += frag_size;
	      o1 = f1;
	      frag_size = 0;
	    }
	  else
	    add_free_elt (v, o1, frag_size);

	  /* Correct size of free fragment at begining. */
	  {
	    uword new_bin;
	    mheap_free_elt_t * f = h->free_lists[bin] + free_index;

	    frag_size = o0 - f0;
	    ASSERT (frag_size == 0 || frag_size >= MHEAP_MIN_SIZE);
	    f->size = frag_size;

	    if (frag_size == 0)
	      new_bin = MHEAP_N_BINS;
	    else
	      {
		mheap_elt_set_size (v, f0, frag_size, MHEAP_PREV_IS_FREE);
		new_bin = size_to_bin (frag_size);
	      }

	    if (new_bin != bin)
	      {
		if (new_bin < MHEAP_N_BINS)
		  {
		    mheap_free_elt_t * g;
		    g = add_to_free_list (v, new_bin);
		    g[0] = f[0];
		    set_free_elt (v, g->offset, g - h->free_lists[new_bin]);
		  }

		remove_free_elt (v, bin, f - h->free_lists[bin]);
	      }
	  }

	  *size_arg = size;
	  return o0;
	}
    }

  return ~0;
}

static u8 * mheap_get_extend_vector (u8 * v,
				     uword size,
				     uword align,
				     uword align_offset,
				     uword * offset_return)
{
  /* Bounds of free and allocated objects (as above). */
  uword f0, f1, o0, o1;
  word free_size;
  mheap_t * h = mheap_header (v);

  f0 = vec_len (v);
  o0 = (f0 + align - 1) &~ (align - 1);

  while (1)
    {
      free_size = o0 - f0 - align_offset;

      if (o0 >= f0 + align_offset
	  && (free_size == 0 || free_size >= MHEAP_MIN_SIZE))
	break;
      o0 += align;
    }

  o0 -= align_offset;
  o1 = o0 + size;
  f1 = o1;
  
  /* Create new heap. */
  ASSERT (v != 0);
  h = mheap_header (v);

  if (f1 > h->max_size)
    {
      *offset_return = ~0;
      return v;
    }

  _vec_len (v) = f1;

  if (! (h->flags & MHEAP_FLAG_NO_VM))
    {
      uword f0_page = mheap_page_round (pointer_to_uword (v + f0));
      uword f1_page = mheap_page_round (pointer_to_uword (v + f1));

      if (f1_page > f0_page)
	mheap_vm (v, MHEAP_VM_MAP, f0_page, f1_page - f0_page);
    }

  if (free_size > 0)
    add_free_elt (v, f0, free_size);

  /* Mark last element. */
  {
    mheap_elt_t * e = mheap_elt_at_offset (v, f1);
    e->size = MHEAP_IS_LAST;
  }

  *offset_return = o0;

  return v;
}

u8 * mheap_get_aligned (u8 * v, uword size,
			uword align,
			uword align_offset,
			uword * offset_return)
{
  mheap_t * h;
  uword offset, trace_enabled;

  /* Include header overhead in size. */
  if (size == 0)
    size = 1;
  size = (mheap_round_size (size) + sizeof (mheap_elt_t));

  /* Round up alignment to power of 2. */
  if (align <= 1)
    align = 1;
  else
    {
      if (align < sizeof (mheap_elt_t))
	align = sizeof (mheap_elt_t);
      align = max_pow2 (align);
    }

  /* Correct align offset to be smaller than alignment. */
  align_offset &= (align - 1);

  /* Align offset must be multiple of minimum object size. */
  if (align_offset != mheap_round_size (align_offset))
    {
      *offset_return = ~0;
      return v;
    }

  if (! v)
    v = mheap_alloc (0, 64 << 20);

  h = mheap_header (v);

  trace_enabled = (h->flags & MHEAP_FLAG_TRACE) != 0;
  h->flags &= ~MHEAP_FLAG_TRACE;

  v = free_list_resize (v);

  offset = mheap_get_search_free_list (v, &size, align, align_offset);
  if (offset >= vec_len (v))
    {
      mheap_t * h = mheap_header (v);
      offset = ~0;

      /* Always leave at least 4k bytes of room for free lists. */
      if ((h->flags & MHEAP_FLAG_INHIBIT_FREE_LIST_SEARCH)
	  || h->max_size == ~0
	  || h->max_size - vec_len (v) > 4*1024)
	v = mheap_get_extend_vector (v, size, align, align_offset, &offset);
    }

  h = mheap_header (v);

  if (offset < vec_len (v))
    {
      mheap_elt_set_size (v, offset, size, 0);
      h->n_elts++;
    }

  if (trace_enabled && offset != ~0)
    {
      mheap_get_trace (v, offset, mheap_data_bytes (v, offset));
      h->flags |= MHEAP_FLAG_TRACE;
    }

  *offset_return = offset;

  return v;
}

static void free_last_elt (u8 * v,
			   mheap_elt_t * e);
static void combine_free_elts (u8 * v,
			       mheap_elt_t * e0,
			       mheap_elt_t * e1);

void mheap_put (u8 * v, uword offset)
{
  mheap_t * h;
  uword b, size, data_size, trace_enabled;
  mheap_elt_t * e, * n, * p;
  mheap_free_elt_t * f;

  h = mheap_header (v);

  ASSERT (offset < vec_len (v));
  ASSERT (h->n_elts > 0);

  h->n_elts--;
  trace_enabled = (h->flags & MHEAP_FLAG_TRACE) != 0;
  h->flags &= ~MHEAP_FLAG_TRACE;

  v = free_list_resize (v);

  e = mheap_elt_at_offset (v, offset);
  n = mheap_next_elt (v, e);
  data_size = mheap_data_bytes (v, offset);

  /* Assert that forward and back pointers are equal. */
  if (((e->size &~ (sizeof (mheap_elt_t) - 1))
       != (n->prev_size &~ (sizeof (mheap_elt_t) - 1)))
      || mheap_prev_is_free (n))
    os_panic ();

  /* Special case: delete last element in heap. */
  if (mheap_is_last (n))
    free_last_elt (v, e);
  else
    {
      size = mheap_trunc_size (e->size);
      b = size_to_bin (size);
      f = add_to_free_list (v, b);
      f->size = size;
      f->offset = offset;
      set_free_elt2 (v, e, n, f - h->free_lists[b]);

      /* See if we can combine the elt we just freed with neighboring free elts. */
      if (! mheap_is_first (e) && mheap_prev_is_free (e))
	p = mheap_prev_elt (v, e);
      else
	p = e;

      if (! mheap_is_free (v, n))
	n = e;

      if (p < n)
	combine_free_elts (v, p, n);
    }

  h = mheap_header (v);

  if (trace_enabled)
    {
      mheap_put_trace (v, offset, data_size);
      h->flags |= MHEAP_FLAG_TRACE;
    }
}

static uword mheap_vm (u8 * v,
		       uword flags,
		       clib_address_t start_addr,
		       uword size)
{
  mheap_t * h = mheap_header (v);
  clib_address_t start_page, end_page, end_addr;
  uword mapped_bytes;

  ASSERT (! (h->flags & MHEAP_FLAG_NO_VM));

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

static uword mheap_vm_elt (u8 * v, uword flags, uword offset)
{
  mheap_elt_t * e;
  clib_address_t start_addr, end_addr;

  e = mheap_elt_at_offset (v, offset);
  if (mheap_is_first (e))
    start_addr = (clib_address_t) v;
  else
    start_addr = (clib_address_t) ((u8 *) e + MHEAP_MIN_SIZE);

  if (mheap_is_last (e))
    end_addr = (clib_address_t) vec_end (v);
  else
    end_addr = (clib_address_t) mheap_next_elt (v, e);

  return mheap_vm (v, flags, start_addr, end_addr - start_addr);
}

static void free_last_elt (u8 * v, mheap_elt_t * e)
{
  mheap_t * h = mheap_header (v);

  /* Possibly delete preceeding free element also. */
  if (mheap_prev_is_free (e))
    {
      uword fb, fi;
      mheap_free_elt_t * f;

      e = mheap_prev_elt (v, e);
      get_free_elt (v, e, fb, fi);
      f = h->free_lists[fb] + fi;
      
      remove_free_elt (v, fb, fi);
    }

  if (mheap_is_first (e))
    {
      if (! (h->flags & MHEAP_FLAG_NO_VM))
	mheap_vm_elt (v, MHEAP_VM_UNMAP, 0);
      e->size = MHEAP_IS_FIRST | MHEAP_IS_LAST;
      _vec_len (v) = 0;
    }
  else
    {
      if (! (h->flags & MHEAP_FLAG_NO_VM))
	mheap_vm_elt (v, MHEAP_VM_UNMAP, mheap_elt_offset (v, e));
      e->size = MHEAP_IS_LAST;
      _vec_len (v) = mheap_elt_offset (v, e);
    }
}

/* While freeing objects at INDEX we noticed free elts i0 <= index and
   i1 >= index.  We combine these two or three elts into one big free elt. */
static void combine_free_elts (u8 * v, mheap_elt_t * e0, mheap_elt_t * e1)
{
  mheap_t * h = mheap_header (v);
  uword total_size, i, b;
  mheap_elt_t * e;

  uword fi[3], fb[3];
  mheap_free_elt_t * f[3], * g;

  /* Compute total size of free objects i0 through i1. */
  total_size = 0;
  for (i = 0, e = e0; e <= e1; e = mheap_next_elt (v, e), i++)
    {
      get_free_elt (v, e, fb[i], fi[i]);
      f[i] = h->free_lists[fb[i]] + fi[i];
      total_size += f[i]->size;
    }

  /* Compute combined bin.  See if all objects can be
     combined into existing bin. */
  b = size_to_bin (total_size);
  g = 0;
  for (i = 0, e = e0; e <= e1; e = mheap_next_elt (v, e), i++)
    if (b == fb[i])
      g = f[i];

  /* Make sure we found a bin. */
  if (! g)
    g = add_to_free_list (v, b);

  g->offset = f[0]->offset;
  g->size = total_size;

  /* Delete unused bins. */
  for (i = 0, e = e0; e <= e1; e = mheap_next_elt (v, e), i++)
    if (g != f[i])
      {
	uword bb, ii;
	get_free_elt (v, e, bb, ii);
	remove_free_elt (v, bb, ii);
      }

  mheap_elt_set_size (v, g->offset, total_size, MHEAP_PREV_IS_FREE);
  set_free_elt (v, g->offset, g - h->free_lists[b]);
  if (! (h->flags & MHEAP_FLAG_NO_VM))
    mheap_vm_elt (v, MHEAP_VM_UNMAP, g->offset);
}

static inline uword
mheap_vm_alloc_size (void * v)
{
  mheap_t * h = mheap_header (v);
  return (u8 *) v + h->max_size - (u8 *) h;
}

u8 * mheap_alloc (void * memory, uword size)
{
  mheap_t * h;
  void * v;
  uword memory_given;

  if (! mheap_page_size)
    mheap_page_size = clib_mem_get_page_size ();

  memory_given = memory != 0;
  if (! memory_given)
    {
      memory = clib_mem_vm_alloc (size);
      if (! memory)
	return 0;
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

  h = memory;
  if (! memory_given)
    clib_mem_vm_map (h, sizeof (h[0]));

  v = mheap_vector (h);

  /* Zero vector header: both heap header and vector length. */
  memset (h, 0, sizeof (h[0]));
  _vec_len (v) = 0;

  h->max_size = (memory + size - v);

  /* Force free list resize before first memory allocation from heap. */
  h->flags |= MHEAP_FLAG_FREE_LISTS_NEED_RESIZE;

  if (memory_given)
    h->flags |= MHEAP_FLAG_NO_VM;

  if (! (h->flags & MHEAP_FLAG_NO_VM))
    mheap_vm (v, MHEAP_VM_UNMAP | MHEAP_VM_ROUND_UP,
	      (clib_address_t) v, h->max_size);

  ASSERT (size == mheap_vm_alloc_size (v));

  return v;
}

u8 * _mheap_free (u8 * v)
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
void mheap_foreach (u8 * v,
		    uword (* func) (void * arg, u8 * v, void * elt_data, uword elt_size),
		    void * arg)
{
  mheap_t * h = mheap_header (v);
  mheap_elt_t * e;
  uword b, * free_list_objects;
  void * p;
  u8 * stack_heap, * clib_mem_mheap_save;

  if (vec_len (v) == 0)
    return;

  clib_mem_mheap_save = 0;
  stack_heap = 0;

  /* Allocate a new temporary heap on the stack.
     This is so that our hash table & user's callback function can
     themselves allocate memory somewhere without getting in the way
     of the heap we are looking at. */
  if (v == clib_mem_get_heap ())
    {
      static u8 buffer[16*1024];
      stack_heap = mheap_alloc (buffer, sizeof (buffer));

      clib_mem_mheap_save = v;
      clib_mem_set_heap (stack_heap);
    }

  /* Use hash table to avoid calling user's function with free list
     objects which may well reside in heap. */
  free_list_objects = hash_create_uword (0, 0);
  for (b = 0; b < ARRAY_LEN (h->free_lists); b++)
    {
      if (! h->free_lists[b])
	continue;

      p = vec_header (h->free_lists[b], 0);
      hash_set1_mem (free_list_objects, p);
    }

  for (e = mheap_elt_at_offset (v, 0); ! mheap_is_last (e); e = mheap_next_elt (v, e))
    {
      void * p = mheap_elt_data (v, e);

      if (! mheap_is_free (v, e)
	  && ! hash_get_mem (free_list_objects, p))
	if ((* func) (arg, v, p, mheap_trunc_size (e->size)))
	  break;
    }

  hash_free (free_list_objects);

  /* Restore main CLIB heap. */
  if (clib_mem_mheap_save)
    clib_mem_set_heap (clib_mem_mheap_save);
}

/* Bytes in mheap header overhead not including data bytes. */
static inline uword
mheap_bytes_overhead (u8 * v)
{
  mheap_t * h = mheap_header (v);
  uword bytes = 0;

  if (v)
    {
      uword b;

      bytes += sizeof (h[0]);
      for (b = 0; b < ARRAY_LEN (h->free_lists); b++)
	if (h->free_lists[b])
	  {
	    u8 * t = vec_header (h->free_lists[b], 0);
	    bytes += mheap_data_bytes (v, t - v);
	  }
    }

  return bytes;
}

/* Total number of bytes including both data and overhead. */
uword mheap_bytes (u8 * v)
{ return mheap_bytes_overhead (v) + vec_bytes (v); }

void mheap_usage (u8 * v, clib_mem_usage_t * usage)
{
  mheap_t * h = mheap_header (v);
  uword used = 0, free = 0, free_vm_unmapped = 0;

  if (vec_len (v) > 0)
    {
      mheap_elt_t * e;

      for (e = mheap_elt_at_offset (v, 0);
	   ! mheap_is_last (e);
	   e = mheap_next_elt (v, e))
	{
	  uword size = mheap_trunc_size (e->size);
	  if (mheap_is_free (v, e))
	    {
	      free += size;
	      if (! (h->flags & MHEAP_FLAG_NO_VM))
		free_vm_unmapped +=
		  mheap_vm_elt (v, MHEAP_VM_NOMAP, mheap_elt_offset (v, e));
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

static u8 * format_mheap_byte_count (u8 * s, va_list * va)
{
  uword n_bytes = va_arg (*va, uword);
  if (n_bytes < 1024)
    return format (s, "%wd", n_bytes);
  else
    return format (s, "%wdk", n_bytes / 1024);
}

/* Returns first corrupt heap element. */
static mheap_elt_t * mheap_first_corrupt (u8 * v)
{
  uword i, o, s;

  if (! v)
    return 0;

  for (i = o = 0; 1; i++)
    {
      mheap_elt_t * e = mheap_elt_at_offset (v, o);
      mheap_elt_t * n;

      if (mheap_is_last (e))
	break;

      n = mheap_next_elt (v, e);
      s = mheap_trunc_size (e->size);

      if ((e->size &~ MHEAP_IS_FIRST) != (n->prev_size &~ MHEAP_PREV_IS_FREE))
	return e;

      o += s;
      e = n;
    }

  return 0;
}

u8 * format_mheap (u8 * s, va_list * va)
{
  u8 * v = va_arg (*va, u8 *);
  int verbose = va_arg (*va, int);

  mheap_t * h = mheap_header (v);
  uword i, o, size;
  uword n_elts = mheap_elts (v);
  clib_mem_usage_t usage;
  mheap_elt_t * first_corrupt;
  int trace_enabled;

  trace_enabled = 0;
  if (v && (h->flags & MHEAP_FLAG_TRACE))
    {
      h->flags &= ~MHEAP_FLAG_TRACE;
      trace_enabled = 1;
    }

  mheap_usage (v, &usage);

  s = format (s, "%6d objects, %U of %U used, %U free, %U reclaimed, %U overhead",
	      usage.object_count,
	      format_mheap_byte_count, usage.bytes_used,
	      format_mheap_byte_count, usage.bytes_total,
	      format_mheap_byte_count, usage.bytes_free,
	      format_mheap_byte_count, usage.bytes_free_reclaimed,
	      format_mheap_byte_count, usage.bytes_overhead);

  if (usage.bytes_max != ~0)
    s = format (s, ", %U capacity", format_mheap_byte_count, usage.bytes_max);

  if (trace_enabled && vec_len (h->trace_main.traces) > 0)
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
	  s = format (s, "%=9s%=9s Traceback\n", "Bytes", "Count");
	s = format (s, "%9d%9d", t->n_bytes, t->n_allocations);
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
      size = mheap_trunc_size (first_corrupt->size);
      s = format (s, "\n  first corrupt object: %p, size %wd\n  %U",
		  first_corrupt,
		  size,
		  format_hex_bytes, first_corrupt, size);
    }

  /* FIXME.  This output could be wrong in the unlikely case that format
     uses the same mheap as we are currently inspecting. */
  if (verbose > 1)
    {
      s = format (s, "\n");

      for (i = o = 0; i < n_elts; i++)
	{
	  mheap_elt_t * e = mheap_elt_at_offset (v, o);

	  size = mheap_trunc_size (e->size);

	  if ((i % 8) == 0)
	    s = format (s, "%8d: ", i);

	  if (mheap_is_free (v, e))
	    {
	      uword fb, fi;
	      get_free_elt (v, e, fb, fi);
	      size = h->free_lists[fb][fi].size;
	      s = format (s, "(%8d) ", o);
	    }
	  else
	    s = format (s, " %8d  ", o);

	  if ((i % 8) == 7 || (i + 1) >= h->n_elts)
	    s = format (s, "\n");

	  o += size;
	}
    }

  /* Re-enable traceing. */
  if (trace_enabled)
    h->flags |= MHEAP_FLAG_TRACE;

  return s;
}

void dmh (u8 * v)
{ fformat (stderr, "%U", format_mheap, v, 1); }

static void mheap_validate_breakpoint ()
{ os_panic(); }

clib_error_t * mheap_validate (u8 * v)
{
  mheap_t * h = mheap_header (v);
  uword i, o, s;

  uword elt_count, elt_size;
  uword free_count, free_size;

  clib_error_t * error = 0;

#define CHECK(x) if ((error = ERROR_ASSERT (x))) { mheap_validate_breakpoint (); goto done; }

  if (! v)
    return 0;

  /* Validate number of elements and size. */
  free_size = free_count = 0;
  for (i = 0; i < ARRAY_LEN (h->free_lists); i++)
    {
      free_count += vec_len (h->free_lists[i]);
      for (o = 0; o < vec_len (h->free_lists[i]); o++)
	{
	  mheap_free_elt_t * f = h->free_lists[i] + o;
	  CHECK (size_to_bin (f->size) == i);
	  CHECK (mheap_is_free (v, mheap_elt_at_offset (v, f->offset)));
	  free_size += f->size;
	}
    }

  {
    mheap_elt_t * e, * n;
    uword elt_free_size, elt_free_count;

    elt_count = elt_size = elt_free_size = elt_free_count = 0;
    for (e = mheap_elt_at_offset (v, 0); ! mheap_is_last (e); e = n)
      {
	CHECK (e->size > 0);
	elt_count++;
	s = mheap_trunc_size (e->size);
	elt_size += s;
	if (mheap_is_free (v, e))
	  {
	    elt_free_count++;
	    elt_free_size += s;
	  }
	n = mheap_next_elt (v, e);
	CHECK (! (mheap_prev_is_free (e) && mheap_prev_is_free (n)));
      }

    CHECK (free_count == elt_free_count);
    CHECK (free_size == elt_free_size);
    CHECK (elt_count == h->n_elts + free_count);
    CHECK (elt_size == vec_len (v));
  }

  for (i = o = 0; 1; i++)
    {
      mheap_elt_t * e = mheap_elt_at_offset (v, o);
      mheap_elt_t * n;

      if (mheap_is_last (e))
	break;

      n = mheap_next_elt (v, e);
      s = mheap_trunc_size (e->size);

      CHECK ((e->size &~ MHEAP_IS_FIRST) == (n->prev_size &~ MHEAP_PREV_IS_FREE));

      if (mheap_is_free (v, e))
	{
	  uword fb, fi;
	  get_free_elt (v, e, fb, fi);

	  CHECK (fb < MHEAP_N_BINS);
	  CHECK (fi < vec_len (h->free_lists[fb]));
	  CHECK (h->free_lists[fb][fi].offset == o);

	  s = h->free_lists[fb][fi].size;
	}
      o += s;
      e = n;
    }
#undef CHECK

 done:
  return error;
}

static void mheap_get_trace (u8 * v, uword offset, uword size)
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
  hash_set (tm->trace_index_by_offset, offset, t - tm->traces);
}

static void mheap_put_trace (u8 * v, uword offset, uword size)
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

static inline void
mheap_trace_main_free (mheap_trace_main_t * tm)
{
  vec_free (tm->traces);
  vec_free (tm->trace_free_list);
  hash_free (tm->trace_by_callers);
  hash_free (tm->trace_index_by_offset);
}

void mheap_trace (u8 * v, int enable)
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
