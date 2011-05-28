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

#ifndef _included_clib_mem_h
#define _included_clib_mem_h

#include <stdarg.h>
#include <clib/clib.h>          /* uword, etc */
#include <clib/string.h>        /* memcpy, memset */
#include <clib/error.h>		/* clib_panic */

typedef struct {
  /* Total number of objects allocated. */
  uword object_count;

  /* Total allocated bytes.  Bytes used and free.
     used + free = total */
  uword bytes_total, bytes_used, bytes_free;

  /* Number of bytes used by mheap data structure overhead
     (e.g. free lists, mheap header). */
  uword bytes_overhead;

  /* Amount of free space returned to operating system. */
  uword bytes_free_reclaimed;
  
  /* For malloc which puts small objects in sbrk region and
     large objects in mmap'ed regions. */
  uword bytes_used_sbrk;
  uword bytes_used_mmap;

  /* Max. number of bytes in this heap. */
  uword bytes_max;
} clib_mem_usage_t;

typedef struct {
  /* Allocate memory with specified alignment. */
  void * (* alloc) (uword size, uword align, uword align_offset);

  /* Re-alloc: does not handle alignment. */
  void * (* realloc) (void * p, uword new_size, uword old_size);

  /* Free it. */
  void (* free) (void * p);

  /* Query how many data bytes user can use (i.e. excluding overhead). */
  uword (* size) (void * p);

  /* Is it a CLIB object or not? */
  uword (* is_heap_object) (void * p);

  /* Return pointer to current heap. */
  void * (* get_heap) (void);

  /* Sets current heap; returns previous value. */
  void * (* set_heap) (void *);

  /* Format function for heap usage. */
  u8 * (* format_usage) (u8 * s, va_list * va);

  void (* query_usage) (clib_mem_usage_t * usage);

  uword (* get_page_size) (void);

  /* Validates internal consistency of heap data structures. */
  void (* validate) (void);

  /* Startup and tear down. */
  void * (* init) (void * heap, uword size);
  void (* exit) (void);

  /* Enable/disable tracing. */
  void (* trace) (int enable);

  /* Hook functions: called before/after alloc/free calls. */
  void * (* pre_alloc_hook) (uword size, uword align, uword align_offset);
  void (* post_alloc_hook) (uword size, uword align, uword align_offset, void * p);

  int (* pre_free_hook) (void * p);
  void (* post_free_hook) (void * p);
} clib_memfuncs_t;

extern clib_memfuncs_t * clib_memfuncs;
extern clib_memfuncs_t * clib_memfuncs_malloc;
extern clib_memfuncs_t * clib_memfuncs_mheap;

/* Memory allocator which returns null when it fails. */
always_inline void *
clib_mem_alloc_aligned_at_offset (uword size,
				  uword align,
				  uword align_offset)
{
  void * p;
  clib_memfuncs_t * mf = clib_memfuncs;

  if (align_offset > align)
    {
      if (align > 0)
	align_offset %= align;
      else
	align_offset = align;
    }

  if (mf->pre_alloc_hook)
    {
      p = mf->pre_alloc_hook (size, align, align_offset);
      if (p)
        return p;
    }

  p = clib_memfuncs->alloc (size, align, align_offset);

  if (mf->post_alloc_hook)
    mf->post_alloc_hook (size, align, align_offset, p);

  return p;
}

/* Memory allocator which returns null when it fails. */
always_inline void *
clib_mem_alloc (uword size)
{ return clib_mem_alloc_aligned_at_offset (size, 1, 0); }

always_inline void *
clib_mem_alloc_aligned (uword size, uword align)
{ return clib_mem_alloc_aligned_at_offset (size, align, 0); }

/* Memory allocator which panics when it fails.
   Use macro so that clib_panic macro can expand __FUNCTION__ and __LINE__. */
#define clib_mem_alloc_aligned_no_fail(size,align)				\
({										\
  uword _clib_mem_alloc_size = (size);						\
  void * _clib_mem_alloc_p;							\
  _clib_mem_alloc_p = clib_mem_alloc_aligned (_clib_mem_alloc_size, (align));	\
  if (! _clib_mem_alloc_p)							\
    clib_panic ("failed to allocate %d bytes", _clib_mem_alloc_size);		\
  _clib_mem_alloc_p;								\
})

#define clib_mem_alloc_no_fail(size) clib_mem_alloc_aligned_no_fail(size,1)

/* Alias to stack allocator for naming consistency. */
#define clib_mem_alloc_stack(bytes) __builtin_alloca(bytes)

always_inline void clib_mem_free (void * p)
{
  clib_memfuncs_t * mf = clib_memfuncs;

  if (mf->pre_free_hook)
    {
      if (mf->pre_free_hook (p))
        return;
    }

  clib_memfuncs->free (p);

  if (mf->post_free_hook)
    mf->post_free_hook (p);
}

always_inline void * clib_mem_realloc (void * p, uword new_size, uword old_size)
{
  if (clib_memfuncs->realloc)
    return clib_memfuncs->realloc (p, new_size, old_size);
  else
    {
      /* By default use alloc, copy and free to emulate realloc. */
      void * q = clib_mem_alloc (new_size);
      if (q)
	{
	  uword copy_size;
	  if (old_size < new_size)
	    copy_size = old_size;
	  else
	    copy_size = new_size;
	  memcpy (q, p, copy_size);
	  clib_mem_free (p);
	}
      return q;
    }
}

/* Hook functions: called before/after alloc/free calls. */
always_inline void *
clib_mem_set_pre_alloc_hook (void * (* pre_alloc_hook)
			     (uword size, uword align, uword align_offset))
{
  clib_memfuncs_t * mf = clib_memfuncs;
  void * old = (void *) mf->pre_alloc_hook;
  mf->pre_alloc_hook = pre_alloc_hook;
  return old;
}

always_inline void *
clib_mem_set_post_alloc_hook (void (* post_alloc_hook)
			      (uword size, uword align, uword align_offset, void * p))
{
  clib_memfuncs_t * mf = clib_memfuncs;
  void * old = (void *) mf->post_alloc_hook;
  mf->post_alloc_hook = post_alloc_hook;
  return old;
}

always_inline void *
clib_mem_set_pre_free_hook (int (* pre_free_hook) (void * p))
{
  clib_memfuncs_t * mf = clib_memfuncs;
  void * old = (void *) mf->pre_free_hook;
  mf->pre_free_hook = pre_free_hook;
  return old;
}

always_inline void *
clib_mem_set_post_free_hook (void (* post_free_hook) (void * p))
{
  clib_memfuncs_t * mf = clib_memfuncs;
  void * old = (void *) mf->post_free_hook;
  mf->post_free_hook = post_free_hook;
  return old;
}

always_inline uword clib_mem_size (void * p)
{ return clib_memfuncs->size (p); }

always_inline uword clib_mem_is_heap_object (void * p)
{
  if (clib_memfuncs->is_heap_object)
    return clib_memfuncs->is_heap_object (p);
  return 1;
}

always_inline void * clib_mem_get_heap (void)
{
  if (clib_memfuncs->get_heap)
    return clib_memfuncs->get_heap ();
  return 0;
}

always_inline void * clib_mem_set_heap (void * heap)
{
  return clib_memfuncs->set_heap
    ? clib_memfuncs->set_heap (heap)
    : 0;
}

always_inline void * clib_mem_init (void * heap, uword size)
{
  return clib_memfuncs->init
    ? clib_memfuncs->init (heap, size)
    : 0;
}

always_inline void clib_mem_exit (void)
{
  if (clib_memfuncs->exit)
    clib_memfuncs->exit ();
}

always_inline uword clib_mem_get_page_size (void)
{ return clib_memfuncs->get_page_size (); }

always_inline void clib_mem_validate (void)
{ 
  if (clib_memfuncs->validate)
    clib_memfuncs->validate ();
}

always_inline void clib_mem_trace (int enable)
{
  if (clib_memfuncs->trace)
    clib_memfuncs->trace (enable);
}

#define format_clib_mem_usage clib_memfuncs->format_usage

always_inline void clib_mem_usage (clib_mem_usage_t * usage)
{
  if (clib_memfuncs->query_usage)
    clib_memfuncs->query_usage (usage);
}

always_inline void * clib_mem_set_funcs (clib_memfuncs_t * new_param)
{
  clib_memfuncs_t * old = clib_memfuncs;
  clib_memfuncs = new_param;
  return old;
}

/* Include appropriate VM functions depending on whether
   we are compiling for linux kernel, for Unix or standalone. */
#ifdef CLIB_LINUX_KERNEL
#include <clib/vm_linux_kernel.h>
#endif

#ifdef CLIB_UNIX
#include <clib/vm_unix.h>
#endif

#ifdef CLIB_STANDALONE
#include <clib/vm_standalone.h>
#endif

#endif /* _included_clib_mem_h */
