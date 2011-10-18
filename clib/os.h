/*
  Copyright (c) 2001-2005 Eliot Dresselhaus

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

#ifndef included_os_h
#define included_os_h

#include <clib/clib.h>
#include <clib/types.h>

/* External panic function. */
void os_panic (void);

/* External exit function analagous to unix exit. */
void os_exit (int code);

/* External function to print a line. */
void os_puts (u8 * string, uword length, uword is_error);

/* External function to handle out of memory. */
void os_out_of_memory (void);

/* Estimate, measure or divine CPU timestamp clock frequency. */
f64 os_cpu_clock_frequency (void);

/* Per-CPU state. */
typedef struct {
  /* Per-cpu local heap. */
  void * heap;

  u32 thread_id;
} clib_smp_per_cpu_main_t;

typedef struct {
  /* Number of CPUs used to model current computer. */
  u32 n_cpus;

  /* Log2 stack and heap size. */
  u32 log2_per_cpu_stack_size, log2_per_cpu_heap_size;

  uword stack_base, heap_base;
  uword stack_base_alloc_size, heap_base_alloc_size;

  clib_smp_per_cpu_main_t * per_cpu_mains;

  /* Thread-safe global heap.  Objects here can be allocated/freed by
     any cpu. */
  void * global_heap;

  /* Number of cpus that are done and have exited. */
  u32 n_cpus_exited;
} clib_smp_main_t;

extern clib_smp_main_t clib_smp_main;

#define clib_smp_compare_and_swap(addr,new,old) __sync_val_compare_and_swap(addr,old,new)
#define clib_smp_swap(addr,new) __sync_lock_test_and_set(addr,new)
#define clib_smp_atomic_add(addr,increment) __sync_fetch_and_add(addr,increment)

typedef struct {
  u32 is_locked;
} clib_smp_lock_t;

always_inline void
clib_smp_lock (clib_smp_lock_t * l)
{
  while (clib_smp_compare_and_swap (&l->is_locked, /* new */ 1, /* old */ 0) != 0)
    ;
}

always_inline void
clib_smp_unlock (clib_smp_lock_t * l)
{
  clib_smp_swap (&l->is_locked, 0);
}

uword os_smp_bootstrap (uword n_cpus,
			void * bootstrap_function,
			uword bootstrap_function_arg);

void clib_smp_init_stacks_and_heaps (clib_smp_main_t * m);

always_inline uword
os_get_cpu_number ()
{
  clib_smp_main_t * m = &clib_smp_main;
  uword sp, n;

  /* Get any old stack address. */
  sp = pointer_to_uword (&sp);

  n = (sp - m->stack_base) >> m->log2_per_cpu_stack_size;

  if (CLIB_DEBUG && m->n_cpus > 0 && n >= m->n_cpus)
    os_panic ();

  return n < m->n_cpus ? n : 0;
}

always_inline void *
clib_smp_heap_for_cpu (clib_smp_main_t * m, uword cpu)
{
  uword a = m->heap_base;

  a += cpu << m->log2_per_cpu_heap_size;

  return uword_to_pointer (a, void *);
}

always_inline void *
clib_smp_stack_top_for_cpu (clib_smp_main_t * m, uword cpu)
{
  uword a = m->stack_base;

  a += (cpu + 1) << m->log2_per_cpu_stack_size;

  return uword_to_pointer (a, void *);
}

#endif /* included_os_h */
