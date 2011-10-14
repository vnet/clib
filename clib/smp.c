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

/* Defaults. */
clib_smp_main_t clib_smp_main = {
  .n_cpus = 1,
  .log2_per_cpu_stack_size = 20,
  .log2_per_cpu_heap_size = 28,
};

void clib_smp_free (clib_smp_main_t * m)
{
  clib_mem_vm_free (uword_to_pointer (m->stack_base, void *),
		    m->stack_base_alloc_size);
  clib_mem_vm_free (uword_to_pointer (m->heap_base, void *),
		    m->heap_base_alloc_size);
}

static uword allocate_per_cpu_mheap (uword cpu)
{
  clib_smp_main_t * m = &clib_smp_main;
  void * heap;

  ASSERT (os_get_cpu_number () == cpu);
  heap = mheap_alloc (clib_smp_heap_for_cpu (m, cpu), (uword) 1 << m->log2_per_cpu_heap_size);
  clib_mem_set_heap (heap);

  if (cpu == 0)
    {
      /* Now that we have a heap, allocate main structure on cpu 0. */
      vec_resize (m->per_cpu_mains, m->n_cpus);

      /* Allocate shared global heap (thread safe). */
      m->global_heap = mheap_alloc_with_flags (clib_smp_heap_for_cpu (m, m->n_cpus),
					       /* size */ (uword) 1 << m->log2_per_cpu_heap_size,
					       /* flags */ MHEAP_FLAG_THREAD_SAFE);
    }

  m->per_cpu_mains[cpu].heap = heap;
  return 0;
}

void clib_smp_init_stacks_and_heaps (clib_smp_main_t * m)
{
  if (! m->n_cpus)
    m->n_cpus = 1;

  if (! m->log2_per_cpu_stack_size)
    m->log2_per_cpu_stack_size = 18;

  if (! m->log2_per_cpu_heap_size)
    m->log2_per_cpu_heap_size = 28;

  m->stack_base_alloc_size = (uword) m->n_cpus << m->log2_per_cpu_stack_size;
  m->heap_base_alloc_size = (uword) (1 + m->n_cpus) << m->log2_per_cpu_heap_size;

  m->stack_base = pointer_to_uword (clib_mem_vm_alloc (m->stack_base_alloc_size));
  if (! m->stack_base)
    clib_error ("error allocating stack");

  m->heap_base = pointer_to_uword (clib_mem_vm_alloc (m->heap_base_alloc_size));
  if (! m->heap_base)
    clib_error ("error allocating heap");

  {
    uword i;

    for (i = 0; i < m->n_cpus; i++)
      clib_calljmp (allocate_per_cpu_mheap, i,
		    clib_smp_stack_top_for_cpu (m, i));
  }
}

