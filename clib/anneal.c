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

#include <clib/anneal.h>

/*
 * Optimize an objective function by simulated annealing
 * 
 * Here are a couple of short, easily-understood
 * descriptions of simulated annealing:
 *
 * http://www.cs.sandia.gov/opt/survey/sa.html
 * Numerical Recipes in C, 2nd ed., 444ff
 *
 * The description in the Wikipedia is not helpful.
 *
 * The algorithm tries to produce a decent answer to combinatorially
 * explosive optimization problems by analogy to slow cooling
 * of hot metal, aka annealing.
 *
 * There are (at least) three problem-dependent annealing parameters
 * to consider: 
 *
 * t0, the initial "temperature. Should be set so that the probability
 * of accepting a transition to a higher cost configuration is 
 * initially about 0.8.
 *
 * ntemps, the number of temperatures to use. Each successive temperature
 * is some fraction of the previous temperature.
 *
 * nmoves_per_temp, the number of configurations to try at each temperature
 *
 * It is a black art to set ntemps, nmoves_per_temp, and the rate
 * at which the temperature drops. Go too fast with too few iterations,
 * and the computation falls into a local minimum instead of the
 * (desired) global minimum.
 */

void clib_anneal (clib_anneal_param_t * p)
{
  f64 t;
  f64 cost, prev_cost, delta_cost, initial_cost;
  f64 random_accept, delta_cost_over_t;
  f64 total_increase=0.0, average_increase;
  u32 i, j;
  u32 number_of_increases = 0;
  u32 accepted_this_temperature;
    
  t = p->initial_temperature;
  initial_cost = prev_cost = p->anneal_metric (p->opaque);

  if (p->flags & CLIB_ANNEAL_VERBOSE)
    fformat(stdout, "Initial cost %.2f\n", initial_cost);

  for (i = 0; i < p->number_of_temperatures; i++) 
    {
      accepted_this_temperature = 0;
      
      for (j = 0; j < p->number_of_configurations_per_temperature; j++) 
        {
          p->anneal_new_configuration (p->opaque);

          cost = p->anneal_metric (p->opaque);

          delta_cost = cost - prev_cost;

          /* cost function looks better, accept this move */
          if ((delta_cost < 0.0 && (p->flags & CLIB_ANNEAL_MINIMIZE))
              || (delta_cost > 0.0 && (p->flags & CLIB_ANNEAL_MAXIMIZE)))
            {
              accepted_this_temperature++;
              prev_cost = cost;
              continue;
            }

          /* cost function worse, keep stats to suggest t0 */
          total_increase += (p->flags & CLIB_ANNEAL_MINIMIZE) ? 
            delta_cost : -delta_cost;

          number_of_increases++;

          /* 
           * Accept a higher cost with Pr { e^(-(delta_cost / T)) }, 
           * equivalent to rnd[0,1] < e^(-(delta_cost / T))
           * 
           * AKA, the Boltzmann factor.
           */
          random_accept = random_f64 (&p->random_seed);

          delta_cost_over_t = delta_cost / t;
          if (random_accept < exp (-delta_cost_over_t)) {
            prev_cost = cost;
            continue;
          }
          p->anneal_restore_configuration (p->opaque);
        }

      if (p->flags & CLIB_ANNEAL_VERBOSE)
        {
          fformat (stdout, "Temp %.2f, cost %.2f, accepted %d\n", t, 
                   prev_cost, accepted_this_temperature);
          fformat (stdout, "Improvement %.2f\n", initial_cost - prev_cost);
          fformat (stdout, "-------------\n");
        }
      
      t = t * p->temperature_step;
    }

  /* 
   * Empirically, one wants the probability of accepting a move
   * at the initial temperature to be about 0.8.
   */
  average_increase = total_increase / (f64) number_of_increases;
  p->suggested_initial_temperature = 
    average_increase / 0.22 ; /* 0.22 = -ln (0.8) */

  p->final_temperature = t;
  p->final_metric = p->anneal_metric (p->opaque);
  
  if (p->flags & CLIB_ANNEAL_VERBOSE)
    {
      fformat (stdout, "Average cost increase from a bad move: %.2f\n", 
               average_increase);
      fformat (stdout, "Suggested t0 = %.2f\n", 
               p->suggested_initial_temperature);
    }
}
