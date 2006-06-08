/*
  Copyright (c) 2005 Eliot Dresselhaus

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

#include <clib/format.h>
#include <clib/longjmp.h>
#include <clib/os.h>

#include <linux/init.h>
#include <linux/kernel.h>	/* for printk */

typedef enum {
  CLIB_KERNEL_LONGJMP_NONE,
  CLIB_KERNEL_LONGJMP_PANIC,
  CLIB_KERNEL_LONGJMP_OUT_OF_MEMORY,
  CLIB_KERNEL_LONGJMP_EXIT,
} clib_kernel_longjmp_type_t;

typedef struct {
  /* Module main function and input string. */
  int (* main_function) (unformat_input_t *);
  char * input_string;

  /* CLIB heap size. */
  int heap_size;

  clib_longjmp_t longjmp;

  int show_heap_usage;
  int heap_trace;
} clib_kernel_main_t;

/* Global copy for os_panic, etc. */
static clib_kernel_main_t kernel_main;

/* Wrapper to parse generic clib input and call main function. */
int __init clib_kernel_init (int (* main_function) (unformat_input_t * i),
			     char * input_string,
			     int kernel_thread_flags)
{
  clib_kernel_main_t * cm = &kernel_main;
  clib_kernel_longjmp_type_t longjmp_type;
  unformat_input_t i;
  int result, initial_heap_size;

  /* Default heap is 1 Meg. */
  memset (cm, 0, sizeof (cm[0]));
  cm->input_string = input_string;
  cm->main_function = main_function;
  cm->heap_size = initial_heap_size = 1 << 20;
  cm->show_heap_usage = 0;
  cm->heap_trace = 0;

  /* Create initial CLIB heap using vmalloc'ed memory. */
  if (! clib_mem_init (0, cm->heap_size))
    return -ENOMEM;

  unformat_init_string (&i, cm->input_string, strlen (cm->input_string));
  while (unformat_check_input (&i) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (&i, "heap-size %U", unformat_memory_size, &cm->heap_size))
	;
      else if (unformat (&i, "heap-trace"))
	cm->heap_trace = 1;
      else if (unformat (&i, "heap-usage"))
	cm->show_heap_usage = 1;
      else
	break;
    }

  /* Advance input by amount parsed. */
  cm->input_string += i.index;

  /* User could have asked for a larger heap. */
  if (initial_heap_size != cm->heap_size) {
    /* Free CLIB heap. */
    clib_mem_exit ();

    /* Allocate new CLIB heap. */
    if (! clib_mem_init (0, cm->heap_size))
      return -ENOMEM;
  }

  /* Re-initialize input on possibly new heap. */
  unformat_init_string (&i, cm->input_string, strlen (cm->input_string));

  if (cm->show_heap_usage)
    fformat (stdout, "%U\n", format_clib_mem_usage);

  /* Turn on heap object tracing. */
  if (cm->heap_trace)
    clib_mem_trace (1);

  longjmp_type = clib_setjmp (&cm->longjmp,
			      CLIB_KERNEL_LONGJMP_NONE);
  switch (longjmp_type)
    {
    case CLIB_KERNEL_LONGJMP_NONE:
      /* Not called from long jump: just call main function. */
      result = cm->main_function (&i);

      if (cm->show_heap_usage)
	fformat (stdout, "%U\n", format_clib_mem_usage);
      break;

    case CLIB_KERNEL_LONGJMP_OUT_OF_MEMORY:
      /* Longjmp from os_out_of_memory (). */
      result = -ENOMEM;
      break;

    case CLIB_KERNEL_LONGJMP_PANIC:
    default:
      /* Lonjmp from os_panic (). */
      result = -EFAULT;
      break;

    case CLIB_KERNEL_LONGJMP_EXIT:
      result = 0;
      break;
    }

  /* Free CLIB heap. */
  if (result != 0)
    clib_mem_exit ();

  return result;
}

void __exit clib_kernel_exit (void)
{
  /* Free CLIB heap. */
  clib_mem_exit ();
}

void os_panic ()
{ clib_longjmp (&kernel_main.longjmp, CLIB_KERNEL_LONGJMP_PANIC); }

void os_exit (int code)
{ clib_longjmp (&kernel_main.longjmp, CLIB_KERNEL_LONGJMP_EXIT); }

void os_out_of_memory (void)
{ clib_longjmp (&kernel_main.longjmp, CLIB_KERNEL_LONGJMP_OUT_OF_MEMORY); }

int os_get_cpu_number (void)
{ return 0; }

void os_puts (u8 * s)
{ printk ("<1>%s\n", s); }
