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

#include <clib/random.h>

/* Default random seed for standalone version of library.
   Value can be overridden by platform code from e.g.
   machine's clock count register. */
u32 standalone_random_default_seed = 1;

/* Simple random number generator with period 2^31 - 1. */
u32 random_u32 (u32 * seed_return)
{
  /* Unlikely mask value to XOR into seed.
     Otherwise small seed values would give
     non-random seeming smallish numbers. */
  const u32 mask = 0x12345678;
  u32 seed, a, b, result;

  seed = *seed_return;
  seed ^= mask;

  a = seed / 127773;
  b = seed % 127773;
  seed = 16807 * b - 2836 * a;

  if ((i32) seed < 0)
    seed += ((u32) 1 << 31) - 1;

  result = seed;

  /* Random number is only 31 bits. */
  ASSERT ((result >> 31) == 0);

  *seed_return = seed ^ mask;

  return result;
}
