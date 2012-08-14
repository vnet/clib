/*
  Copyright (c) 2012 Eliot Dresselhaus

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

#include <clib/smp_fifo.h>
#include <clib/random.h>

typedef struct {
  u32 seed, verbose, n_cpu;

  u32 write_max_size;

  f64 n_write;
  f64 print_every;

  clib_smp_fifo_t ** smp_fifos;
} test_smp_main_t;

typedef struct {
  u32 tx_cpu;
  u32 serial;
} test_smp_fifo_elt_t;

#include <clib/time.h>
#include <clib/math.h>

typedef struct {
  /* CPU time stamp after barrier sync is done. */
  u64 cpu_time_stamp_history[1024];

  uword history_index;
} clib_smp_barrier_sync_per_cpu_history_t;

typedef struct {
  clib_smp_barrier_sync_per_cpu_history_t * per_cpu;

  clib_time_t time;

  uword history_count;
} clib_smp_barrier_sync_history_t;

always_inline void
clib_smp_barrier_sync_done_for_cpu (clib_smp_barrier_sync_history_t * h, uword my_cpu)
{
  clib_smp_barrier_sync_per_cpu_history_t * c;

  c = vec_elt_at_index (h->per_cpu, my_cpu);

  if (c->history_index < ARRAY_LEN (c->cpu_time_stamp_history))
    {
      c->cpu_time_stamp_history[c->history_index] = clib_cpu_time_now ();
      c->history_index++;
      clib_smp_atomic_add (&h->history_count, 1);
    }
}

static clib_smp_barrier_sync_history_t clib_smp_barrier_sync_history;

void clib_smp_barrier_sync_history_init (uword n_cpu)
{
  clib_smp_barrier_sync_history_t * h = &clib_smp_barrier_sync_history;

  vec_validate (h->per_cpu, n_cpu - 1);
  clib_time_init (&h->time);
}

u8 * format_clib_smp_barrier_sync_history (u8 * s, va_list * va)
{
  clib_smp_barrier_sync_history_t * h = &clib_smp_barrier_sync_history;
  f64 ave, rms, min, max;
  uword i, j, n_ave;

  ave = rms = 0;
  min = 1e100; max = 0;
  n_ave = 0;
  for (i = 1; i < vec_len (h->per_cpu); i++)
    {
      clib_smp_barrier_sync_per_cpu_history_t * c0, * ci;

      c0 = vec_elt_at_index (h->per_cpu, 0);
      ci = vec_elt_at_index (h->per_cpu, i);

      for (j = 0; j < ci->history_index; j++)
	{
	  i64 idt = ci->cpu_time_stamp_history[j] - c0->cpu_time_stamp_history[j];
	  f64 dt;

	  dt = (idt < 0 ? -idt : idt) * h->time.seconds_per_clock;
	  ave += dt;
	  rms += dt*dt;
	  if (dt < min) min = dt;
	  if (dt > max) max = dt;
	  n_ave++;
	}
    }

  ave /= n_ave;
  rms = sqrt (rms / n_ave - ave*ave) / n_ave;

  s = format (s, "count %d, ave %.4e +- %.4e, min %.4e max %.4e",
	      n_ave, ave, rms, min, max);

  return s;
}

void clib_smp_barrier_sync (uword n_cpus)
{
  static volatile struct {
    u32 wait_enable;
    u8 pad0[CLIB_CACHE_LINE_BYTES - sizeof (u32)];
    u32 n_slaves_at_barrier;
    u8 pad1[CLIB_CACHE_LINE_BYTES - sizeof (u32)];
  } work;

  u32 my_cpu = os_get_cpu_number ();

  if (my_cpu == 0)
    {
      work.wait_enable = 1;
      work.n_slaves_at_barrier = 0;
      while (work.n_slaves_at_barrier < n_cpus - 1)
	;
      work.wait_enable = 0;
    }
  else
    {
      clib_smp_atomic_add (&work.n_slaves_at_barrier, 1);
      while (work.wait_enable)
	;
    }

  clib_smp_barrier_sync_done_for_cpu (&clib_smp_barrier_sync_history, my_cpu);
}

static uword test_smp_per_cpu_main (test_smp_main_t * m)
{
  uword my_cpu = os_get_cpu_number ();
  uword j, n, to_cpu;
  u32 my_seed = m->seed + my_cpu;
  uword * write_serial_per_cpu = 0;
  uword * read_serial_per_cpu = 0;
  clib_smp_fifo_t * my_fifo, * to_fifo;
  uword n_left_to_write, n_left_to_read, n_write;
  test_smp_fifo_elt_t * e;

  if (1)
    {
      uword i, j, n;
      for (i = 0; i < 1023; i++)
	{
	  n = random_u32 (&my_seed) % (1 << 20);
	  for (j = 0; j < n; j++)
	    asm volatile ("nop");
	  clib_smp_barrier_sync (m->n_cpu);
	}
      clib_smp_barrier_sync (m->n_cpu);
      return 0;
    }

  if (my_cpu == 0)
    {
      uword i;

      vec_resize (m->smp_fifos, m->n_cpu);
      for (i = 0; i < m->n_cpu; i++)
	m->smp_fifos[i] = clib_smp_fifo_init (/* max elts */ 2 * m->n_cpu * m->write_max_size,
					      sizeof (test_smp_fifo_elt_t));
    }

  clib_smp_barrier_sync (m->n_cpu);

  my_fifo = vec_elt (m->smp_fifos, my_cpu);
  vec_resize (write_serial_per_cpu, m->n_cpu);
  vec_resize (read_serial_per_cpu, m->n_cpu);

  n_left_to_write = m->n_cpu * m->n_write;
  if (m->n_cpu > 1)
    n_left_to_write -= m->n_write;
  n_left_to_read = n_left_to_write;
  n_write = 0;
  while (n_left_to_read > 0 || n_left_to_write > 0)
    {
      /* Choose a random fifo destination to write to. */
      switch (m->n_cpu)
	{
	case 1: to_cpu = my_cpu; break;
	case 2: to_cpu = my_cpu ^ 1; break;
	default:
	  to_cpu = my_cpu + 1 + ((random_u32 (&my_seed) >> 0) % (m->n_cpu - 1));
	  to_cpu = to_cpu % m->n_cpu;
	  break;
	}

      to_fifo = vec_elt (m->smp_fifos, to_cpu);

      /* Write to other fifo as long as there is space. */
      n = 1 + ((random_u32 (&my_seed) >> 0) % m->write_max_size);
      for (j = 0; j < n && write_serial_per_cpu[to_cpu] < m->n_write; j++)
	{
	  e = clib_smp_fifo_write_alloc (to_fifo, sizeof (e[0]));
	  if (! e)
	    break;
	  e->tx_cpu = my_cpu;
	  e->serial = write_serial_per_cpu[to_cpu]++;
	  clib_smp_fifo_write_done (to_fifo, e, sizeof (e[0]));

	  n_left_to_write--;
	  n_write++;
	  if (m->print_every != 0 && (n_write % (u32) m->print_every) == 0)
	    {
	      clib_warning ("%wd written, write %U read %U",
			    n_write,
			    format_vec_uword, write_serial_per_cpu, "%wd",
			    format_vec_uword, read_serial_per_cpu, "%wd");
	    }
	}

      /* Read from our fifo. */
      n = m->write_max_size;
      while (n > 0)
	{
	  e = clib_smp_fifo_read_fetch (my_fifo, sizeof (e[0]));
	  if (! e)
	    break;
	  if (e->serial != read_serial_per_cpu[e->tx_cpu])
	    os_panic ();
	  read_serial_per_cpu[e->tx_cpu] += 1;
	  clib_smp_fifo_read_done (my_fifo, e, sizeof (e[0]));
	  n_left_to_read--;
	  n--;
	}
    }

  clib_warning ("%f written, write %U read %U",
		m->n_write,
		format_vec_uword, write_serial_per_cpu, "%wd",
		format_vec_uword, read_serial_per_cpu, "%wd");

  return 0;
}

int test_smp_main (unformat_input_t * input)
{
  test_smp_main_t _m = {0}, * m = &_m;

  m->n_write = 1e3;
  m->seed = random_default_seed ();
  m->verbose = 0;
  m->n_cpu = 1;
  m->write_max_size = 1;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "write %f", &m->n_write))
	;
      else if (unformat (input, "seed %d", &m->seed))
	;
      else if (unformat (input, "n-cpu %d", &m->n_cpu))
	;
      else if (unformat (input, "max-write-size %d", &m->write_max_size))
	;
      else if (unformat (input, "verbose %=", &m->verbose, 1))
	;
      else if (unformat (input, "print %f", &m->print_every))
	;
      else
	{
	  clib_warning ("unknown input `%U'\n",
			format_unformat_error, input);
	  return 1;
	}
    }

  if (! m->seed)
    m->seed = random_default_seed ();

  clib_warning ("%d cpu, write %f seed %d", m->n_cpu, m->n_write, m->seed);

  {
    uword r;

    clib_smp_barrier_sync_history_init (m->n_cpu);

    r = os_smp_bootstrap (m->n_cpu, test_smp_per_cpu_main, pointer_to_uword (m));

    clib_warning ("%U", format_clib_smp_barrier_sync_history);

    return r;
  }
}

#ifdef CLIB_UNIX
int main (int argc, char * argv [])
{
  unformat_input_t i;
  unformat_init_command_line (&i, argv);
  return test_smp_main (&i);
}
#endif
