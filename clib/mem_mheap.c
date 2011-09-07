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
#include <clib/format.h>
#include <clib/os.h>
#include <clib/valgrind.h>
#include <clib/memcheck.h>

/* Per CPU heaps. */
static void * per_cpu_mheaps[32];

static inline void * get_heap (void)
{
  int cpu = os_get_cpu_number ();
  return per_cpu_mheaps[cpu];
}

static inline void * set_heap (u8 * new)
{
  int cpu = os_get_cpu_number ();
  void * old = per_cpu_mheaps[cpu];
  per_cpu_mheaps[cpu] = new;
  return old;
}

static void my_exit (void)
{
  u8 * heap = get_heap ();
  if (heap)
    mheap_free (heap);
  set_heap (0);
}

/* Initialize CLIB heap based on memory/size given by user.
   Set memory to 0 and CLIB will try to allocate its own heap. */
static void * my_init (void * memory, uword memory_size)
{
  u8 * heap;

  if (memory || memory_size)
    heap = mheap_alloc (memory, memory_size);
  else
    {
      /* Allocate lots of address space since this will limit
	 the amount of memory the program can allocate.
	 In the kernel we're more conservative since some architectures
	 (e.g. mips) have pretty small kernel virtual address spaces. */
#ifdef __KERNEL__
#define MAX_VM_MEG 64
#else
#define MAX_VM_MEG 1024
#endif

      uword alloc_size = MAX_VM_MEG << 20;
      uword tries = 16;

      while (1)
	{
	  heap = mheap_alloc (0, alloc_size);
	  if (heap)
	    break;
	  alloc_size = (alloc_size * 3) / 4;
	  tries--;
	  if (tries == 0)
	    break;
	}
    }

  set_heap (heap);

  return heap;
}

static void * my_alloc (uword size, uword align, uword align_offset)
{
  u8 * heap;
  uword offset, cpu;

  cpu = os_get_cpu_number ();
  heap = per_cpu_mheaps[cpu] =
    mheap_get_aligned (per_cpu_mheaps[cpu],
		       size, align, align_offset,
		       &offset);

  if (offset == ~0)
    {
      os_out_of_memory ();
      return 0;
    }
  else
    {
#if CLIB_DEBUG > 0
      VALGRIND_MALLOCLIKE_BLOCK(heap+offset, mheap_data_bytes(heap,offset),
                                0, 0);
#endif
      return heap + offset;
    }
}

static uword my_is_heap_object (void * p)
{
  void * heap = get_heap ();
  uword offset = p - heap;
  mheap_elt_t * e, * n;

  if (offset >= vec_len (heap))
    return 0;

  e = mheap_elt_at_offset (heap, offset);
  if (mheap_is_last (e) || mheap_is_first (e))
    return 1;

  n = mheap_next_elt (heap, e);
  
  /* Check that heap forward and reverse pointers agree. */
  if ((e->size &~ (sizeof (mheap_elt_t) - 1))
      != (n->prev_size &~ (sizeof (mheap_elt_t) - 1)))
    return 0;

  return 1;
}

static void my_free (void * p)
{
  u8 * heap = get_heap ();

  /* Make sure object is in the correct heap. */
  ASSERT (my_is_heap_object (p));

  mheap_put (heap, (u8 *) p - heap);
#if CLIB_DEBUG > 0
  VALGRIND_FREELIKE_BLOCK(p, 0);
#endif
}

static uword my_size (void * p)
{
  u8 * heap = get_heap ();

  return mheap_data_bytes (heap, (u8 *) p - heap);
}

#ifdef CLIB_LINUX_KERNEL
#include <asm/page.h>

static uword my_get_page_size (void)
{ return PAGE_SIZE; }
#endif

#ifdef CLIB_UNIX
static uword my_get_page_size (void)
{ return getpagesize (); }
#endif

/* Make a guess for standalone. */
#ifdef CLIB_STANDALONE
static uword my_get_page_size (void)
{ return 4096; }
#endif

static u8 * my_format_usage (u8 * s, va_list * va)
{
    int verbose = va_arg (*va, int);
    return format (s, "%U", format_mheap, get_heap (), verbose);
}

static void my_query_usage (clib_mem_usage_t * u)
{ mheap_usage (get_heap (), u); }

/* Call serial number for debugger breakpoints. */
uword clib_mem_validate_serial = 0;

static void my_validate (void)
{
  clib_error_t * error;
  error = mheap_validate (get_heap ());
  clib_error_report (error);
  clib_mem_validate_serial++;
}

static void my_trace (int enable)
{ mheap_trace (get_heap (), enable); }

static void * my_get_heap (void)
{ return get_heap (); }

static void * my_set_heap (void * heap)
{ return set_heap (heap); }

static clib_memfuncs_t m = {
  alloc: my_alloc,
  free: my_free,
  size: my_size,
  is_heap_object: my_is_heap_object,
  get_heap: my_get_heap,
  set_heap: my_set_heap,
  get_page_size: my_get_page_size,
  format_usage: my_format_usage,
  query_usage: my_query_usage,
  validate: my_validate,
  trace: my_trace,
  init: my_init,
  exit: my_exit,
};

clib_memfuncs_t * clib_memfuncs_mheap = &m;
clib_memfuncs_t * clib_memfuncs = &m;
