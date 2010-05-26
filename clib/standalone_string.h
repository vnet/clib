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

#ifndef included_standalone_string_h
#define included_standalone_string_h

#include <clib/clib.h>

void * memset64 (void * s, i64 c, uword count);
void * memset32 (void * s, i32 c, uword count);
void * memset16 (void * s, i16 c, uword count);
void * memset8 (void * s, int c, uword count);

void * memcpy8 (void * dest, void * src, uword count);
void * memmove8 (void * dest, void * src, uword count);
word memcmp8 (void * dest, void * src, uword count);

uword strlen8 (u8 * str);
u8 * strcpy8 (u8 * dest, u8 * src);
u8 * strncpy8 (u8 * dest, u8 * src, uword size);
word strcmp8 (u8 * s1, u8 * s2);
word strncmp8 (u8 * s1, u8 * s2, uword size);

#ifndef CLIB_UNIX
#include <stddef.h>		/* for size_t */

void * memset (void * s, int c, size_t count);
void * memcpy (void * dest, const void * src, size_t count);
void * memmove (void * dest, const void * src, size_t count);
int memcmp (const void * dest, const void * src, size_t count);

size_t strlen (const i8 * str);
i8 * strcpy (i8 * dest, const i8 * src);
i8 * strncpy (i8 * dest, const i8 * src, size_t size);
int strcmp (const i8 * s1, const i8 * s2);
int strncmp (const i8 * s1, const i8 * s2, size_t size);
char * strstr (const char * haystack, const char * needle);
char * strchr (const char * s, int c);
word atoi (u8 * s);

#endif

/* True iff (dest[i] & mask[i]) == src[i] for each i. */
always_inline int
memmatch (void * dest, void * src, void * mask, uword n_bytes)
{
    u8 * d = dest, * m = mask, * s = src;
    u8 * m_end = m + n_bytes;

    while (m < m_end) {
	if (m[0]) {
	    if ((d[0] & m[0]) != (s[0] & m[0]))
		return 1;
	}
	m++; d++; s++;
    }

    return 0;
}

#endif /* included_standalone_string_h */
