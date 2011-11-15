/*
  Copyright (c) 2011 by Cisco Systems, Inc.

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

#ifndef __included_anneal_h__
#define __included_anneal_h__

#include <clib/clib.h>
#include <clib/format.h>
#include <clib/random.h>
#include <math.h>

typedef struct {
  /* Initial temperature */
  f64 initial_temperature;

  /* Temperature fraction at each step, 0.95 is reasonable */
  f64 temperature_step;

  /* Number of temperatures used */
  u32 number_of_temperatures;

  /* Number of configurations tried at each temperature */
  u32 number_of_configurations_per_temperature;

  u32 flags;
#define CLIB_ANNEAL_VERBOSE (1<<0)
#define CLIB_ANNEAL_MINIMIZE (1<<1) /* mutually exclusive */
#define CLIB_ANNEAL_MAXIMIZE (1<<2) /* mutually exclusive */

  /* Random number seed, set to ensure repeatable results */
  u32 random_seed;

  /* Opaque data passed to callbacks */
  void * opaque;

  /* Final temperature (output) */
  f64 final_temperature;

  /* Final metric (output) */
  f64 final_metric;

  /* Suggested initial temperature (output) */
  f64 suggested_initial_temperature;


  /*--- Callbacks ---*/
  
  /* objective function to minimize */
  f64 (*anneal_metric)(void * opaque); 

  /* Generate a new configuration */
  void (*anneal_new_configuration)(void * opaque);

  /* Restore the previous configuration */
  void (*anneal_restore_configuration)(void * opaque);
} clib_anneal_param_t;

void clib_anneal (clib_anneal_param_t * p);

#endif /* __included_anneal_h__ */
