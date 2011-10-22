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

#ifndef included_error_bootstrap_h
#define included_error_bootstrap_h

/* Bootstrap include so that #include <clib/mem.h> can include e.g.
   <clib/mheap.h> which depends on <clib/vec.h>. */

#include <clib/clib.h>		/* for uword */

enum {
  CLIB_ERROR_FATAL	= 1 << 0,
  CLIB_ERROR_ABORT	= 1 << 1,
  CLIB_ERROR_WARNING	= 1 << 2,
  CLIB_ERROR_ERRNO_VALID = 1 << 16,
  CLIB_ERROR_NO_RATE_LIMIT = 1 << 17,
};

/* Current function name.  Need (char *) cast to silence gcc4 pointer signedness warning. */
#define clib_error_function ((char *) __FUNCTION__)

#ifndef CLIB_ASSERT_ENABLE
#define CLIB_ASSERT_ENABLE (CLIB_DEBUG > 0)
#endif

/* Low level error reporting function.
   Code specifies whether to call exit, abort or nothing at
   all (for non-fatal warnings). */
extern void _clib_error (int code,
			 char * function_name,
			 uword line_number,
			 char * format, ...);

#define ASSERT(truth)					\
do {							\
  if (CLIB_ASSERT_ENABLE && ! (truth))			\
    {							\
      _clib_error (CLIB_ERROR_ABORT, 0, 0,		\
		   "%s:%d (%s) assertion `%s' fails",	\
		   __FILE__,				\
		   (uword) __LINE__,			\
		   clib_error_function,			\
		   # truth);				\
    }							\
} while (0)

#endif /* included_error_bootstrap_h */
