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

#ifndef included_error_h
#define included_error_h

#include <clib/clib.h> /* for CLIB_LINUX_KERNEL */

#ifdef CLIB_UNIX
#include <errno.h>
#endif

#ifdef CLIB_LINUX_KERNEL
#include <linux/errno.h>
#endif

#include <stdarg.h>
#include <clib/vec.h>

enum {
  ERROR_FATAL	= 1 << 0,
  ERROR_ABORT	= 1 << 1,
  ERROR_WARNING	= 1 << 2,
  ERROR_ERRNO_VALID = 1 << 16,
  ERROR_NO_RATE_LIMIT = 1 << 17,
};

/* Callback functions for error reporting. */
typedef void clib_error_handler_func_t (void * arg, u8 * msg, int msg_len);
void clib_error_register_handler (clib_error_handler_func_t func, void * arg);

/* Low level error reporting function.
   Code specifies whether to call exit, abort or nothing at
   all (for non-fatal warnings). */
extern void _clib_error (int code,
			 char * function_name,
			 uword line_number,
			 char * format, ...);

/* Current function name.  Need (char *) cast to silence gcc4 pointer signedness warning. */
#define clib_error_function ((char *) __FUNCTION__)

#define clib_warning(format,args...) \
  _clib_error (ERROR_WARNING, clib_error_function, __LINE__, format, ## args)

#define clib_error(format,args...) \
  _clib_error (ERROR_FATAL, clib_error_function, __LINE__, format, ## args)

#define clib_unix_error(format,args...) \
  _clib_error (ERROR_FATAL | ERROR_ERRNO_VALID, clib_error_function, __LINE__, format, ## args)

#define clib_unix_warning(format,args...) \
  _clib_error (ERROR_WARNING | ERROR_ERRNO_VALID, clib_error_function, __LINE__, format, ## args)

/* For programming errors and assert. */
#define clib_panic(format,args...) \
  _clib_error (ERROR_ABORT, (char *) clib_error_function, __LINE__, format, ## args)

typedef struct {
  /* Error message. */
  u8 * what;

  /* Where error occurred (e.g. __FUNCTION__ __LINE__) */
  const u8 * where;

  uword flags;

  /* Error code (e.g. errno for Unix errors). */
  any code;
} clib_error_t;

#define clib_error_get_code(err) ((err) ? (err)->code : 0)
#define clib_error_set_code(err, c)		\
do {						\
  if (err)					\
    (err)->code = (c);				\
} while (0)

extern void * clib_error_free_vector (clib_error_t * errors);

#define clib_error_free(e) e = clib_error_free_vector(e)

extern clib_error_t *
_clib_error_return (clib_error_t * errors,
		    any code,
		    uword flags,
		    char * where,
		    char * fmt, ...);

#define clib_error_return_code(e,code,flags,args...) \
  _clib_error_return((e),(code),(flags),(char *)clib_error_function,args)

#define clib_error_create(args...) \
  clib_error_return_code(0,0,0,args)

#define clib_error_return(e,args...) \
  clib_error_return_code(e,0,0,args)

#define clib_error_return_unix(e,args...) \
  clib_error_return_code(e,errno,ERROR_ERRNO_VALID,args)

#define clib_error_return_fatal(e,args...) \
  clib_error_return_code(e,0,ERROR_FATAL,args)

#define clib_error_return_unix_fatal(e,args...) \
  clib_error_return_code(e,errno,ERROR_ERRNO_VALID|ERROR_FATAL,args)

extern clib_error_t * _clib_error_report (clib_error_t * errors);

#define clib_error_report(e) do { (e) = _clib_error_report (e); } while (0)

u8 * format_clib_error (u8 * s, va_list * va);

static inline word unix_error_is_fatal (word error)
{
#ifdef CLIB_UNIX
  switch (error)
    {
    case EWOULDBLOCK:
    case EINTR:
      return 0;
    }
#endif
  return 1;
}

#define IF_ERROR_IS_FATAL_RETURN_ELSE_FREE(e)			\
do {								\
  if (e)							\
    {								\
      if (unix_error_is_fatal (clib_error_get_code (e)))	\
	return (e);						\
      else							\
	clib_error_free (e);					\
    }								\
} while (0)

#define ERROR_RETURN_IF(x)				\
do {							\
  clib_error_t * _error_return_if = (x);		\
  if (_error_return_if)					\
    return clib_error_return (_error_return_if, 0);	\
} while (0)

#define ASSERT(truth)					\
do {							\
  if (DEBUG > 0 && ! (truth))				\
    {							\
      _clib_error (ERROR_ABORT, 0, 0,			\
		   "%s:%d (%s) assertion `%s' fails",	\
		   __FILE__,				\
		   (uword) __LINE__,			\
		   clib_error_function,			\
		   # truth);				\
    }							\
} while (0)

#define ERROR_ASSERT(truth)			\
({						\
  clib_error_t * _error_assert = 0;		\
  if (DEBUG > 0 && ! (truth))			\
    {						\
      _error_assert = clib_error_return_fatal	\
        (0, "%s:%d (%s) assertion `%s' fails",	\
	 __FILE__,				\
	 (uword) __LINE__,			\
	 clib_error_function,			\
	 # truth);				\
    }						\
  _error_assert;				\
})

/* Assert to remain even if DEBUG is set to 0. */
#define CLIB_ERROR_ASSERT(truth)		\
({						\
  clib_error_t * _error_assert = 0;		\
  if (! (truth))				\
    {						\
      _error_assert =				\
        clib_error_return_fatal			\
        (0, "%s:%d (%s) assertion `%s' fails",	\
         __FILE__,				\
	 (uword) __LINE__,			\
	 clib_error_function,			\
	 # truth);				\
    }						\
  _error_assert;				\
})

#endif /* included_error_h */
