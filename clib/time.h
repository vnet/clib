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

#ifndef included_time_h
#define included_time_h

#include <clib/clib.h>

typedef struct {
  /* Total run time in clock cycles
     since clib_time_init call. */
  u64 total_cpu_time;

  /* Last recorded time stamp. */
  u64 last_cpu_time;

  /* CPU clock frequency. */
  f64 clocks_per_second;

  /* 1 / cpu clock frequency: conversion factor
     from clock cycles into seconds. */
  f64 seconds_per_clock;
} clib_time_t;

/* Return CPU time stamp as 64bit number. */
#if defined(__x86_64__) || defined(i386)
static always_inline u64 clib_cpu_time_now (void)
{
  u32 a, d;
  asm volatile ("rdtsc"
		: "=a" (a), "=d" (d));
  return (u64) a + ((u64) d << (u64) 32);
}

#elif defined (__powerpc64__)

static always_inline u64 clib_cpu_time_now (void)
{
  u64 t;
  asm volatile ("mftb %0" : "=r" (t));
  return t;
}

#elif defined (__SPU__)

static always_inline u64 clib_cpu_time_now (void)
{
#ifdef _XLC
  return spu_rdch (0x8);
#else
  return 0 /* __builtin_si_rdch (0x8) FIXME */;
#endif
}

#elif defined (__powerpc__)

static always_inline u64 clib_cpu_time_now (void)
{
  u32 hi, lo;
  asm volatile ("mftbu %[hi]\n"
		"mftb  %[lo]\n"
		: [hi] "=r" (hi), [lo] "=r" (lo));
  return (u64) lo + ((u64) hi << (u64) 32);
}

#else

#error "don't know how to read CPU time stamp"

#endif

static always_inline f64
clib_time_now (clib_time_t * c)
{
  u64 n = clib_cpu_time_now ();
  u64 l = c->last_cpu_time;
  u64 t = c->total_cpu_time;
  t += n - l;
  c->total_cpu_time = t;
  c->last_cpu_time = n;
  return t * c->seconds_per_clock;
}

static always_inline void clib_cpu_time_wait (u64 dt)
{
  u64 t_end = clib_cpu_time_now () + dt;
  while (clib_cpu_time_now () < t_end)
    ;
}

void clib_time_init (clib_time_t * c);

#ifdef CLIB_UNIX

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

/* Use 64bit floating point to represent time offset from epoch. */
static inline f64 unix_time_now (void)
{
  struct timeval tv;
  gettimeofday (&tv, 0);
  return tv.tv_sec + 1e-6*tv.tv_usec;
}

static inline f64 unix_usage_now (void)
{
  struct rusage u;
  getrusage (RUSAGE_SELF, &u);
  return u.ru_utime.tv_sec + 1e-6*u.ru_utime.tv_usec
    + u.ru_stime.tv_sec + 1e-6*u.ru_stime.tv_usec;
}

#else  /* ! CLIB_UNIX */

static inline f64 unix_time_now (void)
{ return 0; }

static inline f64 unix_usage_now (void)
{ return 0; }

#endif

#endif /* included_time_h */
