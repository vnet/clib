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

#include <clib/longjmp.h>
#include <clib/mheap.h>
#include <clib/os.h>

void clib_smp_free (clib_smp_main_t * m)
{
  clib_mem_vm_free (m->vm_base, (uword) ((1 + m->n_cpus) << m->log2_n_per_cpu_vm_bytes));
}

static uword allocate_per_cpu_mheap (uword cpu)
{
  clib_smp_main_t * m = &clib_smp_main;
  void * heap;
  uword vm_size, stack_size;

  ASSERT (os_get_cpu_number () == cpu);

  vm_size = (uword) 1 << m->log2_n_per_cpu_vm_bytes;
  stack_size = (uword) 1 << m->log2_n_per_cpu_stack_bytes;

  /* Heap extends up to start of stack. */
  heap = mheap_alloc_with_flags (clib_smp_vm_base_for_cpu (m, cpu),
				 vm_size - stack_size,
				 /* flags */ 0);
  clib_mem_set_heap (heap);

  if (cpu == 0)
    {
      /* Now that we have a heap, allocate main structure on cpu 0. */
      vec_resize (m->per_cpu_mains, m->n_cpus);

      /* Allocate shared global heap (thread safe). */
      m->global_heap =
	mheap_alloc_with_flags (clib_smp_vm_base_for_cpu (m, cpu + m->n_cpus),
				vm_size,
				/* flags */ MHEAP_FLAG_THREAD_SAFE);
    }

  m->per_cpu_mains[cpu].heap = heap;
  return 0;
}

void clib_smp_init (void)
{
  clib_smp_main_t * m = &clib_smp_main;
  uword cpu;

  m->vm_base = clib_mem_vm_alloc ((uword) (m->n_cpus + 1) << m->log2_n_per_cpu_vm_bytes);
  if (! m->vm_base)
    clib_error ("error allocating virtual memory");

  for (cpu = 0; cpu < m->n_cpus; cpu++)
    clib_calljmp (allocate_per_cpu_mheap, cpu,
		  clib_smp_stack_top_for_cpu (m, cpu));
}

#if defined (i386) || defined (__x86_64__)
#define clib_smp_pause() do { asm volatile ("pause"); } while (0)
#endif

#ifndef clib_smp_pause
#define clib_smp_pause() do { asm volatile ("nop"); } while (0)
#endif

void clib_smp_lock_init (clib_smp_lock_t ** pl)
{
  clib_smp_lock_t * l;
  uword n_cpus = clib_smp_main.n_cpus;

  /* No locking necessary if n_cpus <= 1.
     Null means no locking is necessary. */
  if (n_cpus < 2)
    {
      *pl = 0;
      return;
    }

  l = clib_mem_alloc_aligned (n_cpus * sizeof (l[0]), CLIB_CACHE_LINE_BYTES);

  memset (l, 0, n_cpus * sizeof (l[0]));

  /* Unlock it. */
  l->header = clib_smp_lock_header_unlock (l->header);

  *pl = l;
}

void clib_smp_lock_free (clib_smp_lock_t ** pl)
{
  if (*pl)
    clib_mem_free (*pl);
  *pl = 0;
}

void clib_smp_lock_slow_path (clib_smp_lock_t * l, uword my_cpu, clib_smp_lock_header_t h0)
{
  clib_smp_lock_header_t h1, h2, h3;
  uword n_cpus = clib_smp_main.n_cpus;
  uword my_tail;

  /* Atomically advance waiting FIFO tail pointer; my_tail will point
     to entry where we can insert ourselves to wait for lock to be granted. */
  while (1)
    {
      h1 = h0;
      my_tail = h1.waiting_fifo.tail_index;
      h1.waiting_fifo.tail_index = my_tail == n_cpus - 1 ? 0 : my_tail + 1;
      h1.request_cpu = my_cpu;

      /* FIFO should never be full since there is always one cpu that has the lock. */
      ASSERT_AND_PANIC (h1.waiting_fifo.tail_index != h1.waiting_fifo.head_index);

      h2 = clib_smp_lock_set_header (l, h1, h0);

      /* Tail successfully advanced? */
      if (clib_smp_lock_header_is_equal (h0, h2))
	break;

      /* It is possible that if head and tail are both zero, CPU with lock would have unlocked lock. */
      else if (! clib_smp_lock_header_is_locked (h2))
	{
	  do {
	    h0 = h2;
	    h0.request_cpu = my_cpu;

	    /* Try to set head and tail to zero and thereby get the lock. */
	    h3 = clib_smp_lock_set_header (l, h0, h2);

	    /* Got it? */
	    if (clib_smp_lock_header_is_equal (h2, h3))
	      return;

	    h2 = h3;
	  } while (! clib_smp_lock_header_is_locked (h2));
	}

      /* Try to advance tail again. */
      h0 = h2;
    }

  /* Wait until CPU holding the lock grants us the lock. */
  while (! l[my_tail].lock_granted)
    clib_smp_pause ();

  /* Clear it for next time. */
  l[my_tail].lock_granted = 0;
}

void clib_smp_unlock_slow_path (clib_smp_lock_t * l, uword my_cpu, clib_smp_lock_header_t h0)
{
  clib_smp_lock_header_t h1, h2;
  uword n_cpus = clib_smp_main.n_cpus;
  uword my_head;
  
  /* Some other cpu is on waiting fifo. */
  while (1)
    {
      h1 = h0;
      my_head = h1.waiting_fifo.head_index;
      h1.waiting_fifo.head_index = my_head == n_cpus - 1 ? 0 : my_head + 1;
      h2 = clib_smp_lock_set_header (l, h1, h0);
      if (clib_smp_lock_header_is_equal (h2, h0))
	break;
      h0 = h2;
    }

  /* Shift lock to first thread waiting in fifo. */
  l[my_head].lock_granted = 1;
}
