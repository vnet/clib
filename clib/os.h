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

#include <clib/types.h>

#ifndef CLIB_UNIX
/* External panic function. */
void os_panic (void);

/* External exit function analagous to unix exit. */
void os_exit (int code);

/* External function to print a line. */
void os_puts (u8 * string);

/* External function to fetch cpu number. */
int os_get_cpu_number (void);

/* External function to handle out of memory. */
void os_out_of_memory (void);

/* Estimate, measure or divine CPU timestamp clock frequency. */
f64 os_cpu_clock_frequency (void);

#else

#include <unistd.h>

static inline void os_panic (void)
{ abort (); }

static inline void os_exit (int code)
{ exit (code); }

static inline void os_puts (u8 * string)
{ write (1, string, strlen ((char *) string)); }

/* FIXME */
static inline int os_get_cpu_number (void)
{ return 0; }

static inline void os_out_of_memory (void)
{ }

/* Estimate, measure or divine CPU timestamp clock frequency. */
f64 os_cpu_clock_frequency (void);
#endif

#endif /* included_os_h */
