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

#ifndef included_clib_longjmp_h
#define included_clib_longjmp_h

#include <clib/types.h>

#if defined(__x86_64__)
/* rbx, rbp, r12, r13, r14, r15, eip, rsp */
#define CLIB_ARCH_LONGJMP_REGS 8

#elif defined(i386)
/* ebx, ebp, esi, edi, eip, rsp */
#define CLIB_ARCH_LONGJMP_REGS 6

#elif defined(__powerpc64__)
/* r1 r2 link condition+vsave regs 14-31 fp regs 14-31 vector regs 20-31 */
#define CLIB_ARCH_LONGJMP_REGS (4 + 2*(31 - 14 + 1) + 2*(31 - 20 + 1))

#elif defined(__powerpc__)
/* r1 lr cr regs 14-31 */
#define CLIB_ARCH_LONGJMP_REGS (3 + (31 - 14 + 1))

#elif defined(__SPU__)
/* FIXME */
#define CLIB_ARCH_LONGJMP_REGS (10)

#else
#error "unknown machine"
#endif

typedef struct {
  uword regs[CLIB_ARCH_LONGJMP_REGS];
} clib_longjmp_t __attribute__ ((aligned (16)));

/* Return given value to saved context. */
void clib_longjmp (clib_longjmp_t * save, uword return_value);

/* Save context.  Returns given value if jump is not taken;
   otherwise returns value from clib_longjmp if long jump is taken. */
uword clib_setjmp (clib_longjmp_t * save, uword return_value_not_taken);

/* Call function on given stack. */
uword clib_calljmp (uword (* func) (uword func_arg),
		    uword func_arg,
		    void * stack);

#endif /* included_clib_longjmp_h */
