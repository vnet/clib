/*
  Copyright (c) 2011 Eliot Dresselhaus

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

#include <thread_db.h>
#include <malloc.h>
#include <string.h>
#include <clib/os.h>

#include "clib_thread_db.h"
#include "clib_thread_db_proc_service.h"

static int thread_db_debug = 0;

#define LOG(args...) \
  if (thread_db_debug) { printf ("%s: ", __FUNCTION__); printf (args); printf ("\n"); }

#define foreach_symbol				\
  _ (clib_smp_main)				\
  _ (thread_db_event)				\
  _ (thread_db_breakpoint)

typedef enum {
#define _(f) SYM_##f,
  foreach_symbol
#undef _
  N_SYM,
} clib_sym_t;

struct td_thragent
{
  clib_smp_main_t clib_smp_main;
  void * proc_handle;
  td_thrhandle_t msg_th;

  psaddr_t addrs[N_SYM];
};


td_err_e td_init (void)
{
  LOG ("here");
  return TD_OK;
}

td_err_e td_ta_new (struct ps_prochandle * ps, td_thragent_t ** ta_result)
{
  td_thragent_t * ta;

  ta = calloc (1, sizeof (ta[0]));
  ta->proc_handle = ps;
  *ta_result = ta;
  
  {
    char * t[] = {
#define _(f) #f,
      foreach_symbol
#undef _
    };
    int i;
    for (i = 0; i < N_SYM; i++)
      {
	if (ps_pglobal_lookup (ta->proc_handle, 0, t[i], &ta->addrs[i]) != PS_OK)
	  return TD_ERR;
	LOG ("%s at 0x%lx", t[i], (long) ta->addrs[i]);
      }
  }

  return TD_OK;
}

td_err_e
td_ta_map_id2thr (const td_thragent_t *ta, pthread_t pt, td_thrhandle_t *th)
{
  /* Create the `td_thrhandle_t' object.  */
  th->th_ta_p = (td_thragent_t *) ta;
  th->th_unique = (void *) pt;
  LOG ("uniq %lx", (long) pt);
  return TD_OK;
}

td_err_e
td_ta_map_lwp2thr (const td_thragent_t *ta,
		   lwpid_t lwpid, td_thrhandle_t *th)
{
  th->th_unique = (void *) (long) lwpid;
  LOG ("lwpid %ld uniq %ld", (long) lwpid, (long) th->th_unique);
  return TD_OK;
}

td_err_e
td_ta_thr_iter (const td_thragent_t *ta_arg, td_thr_iter_f *callback,
		void *cbdata_p, td_thr_state_e state, int ti_pri,
		sigset_t *ti_sigmask_p, unsigned int ti_user_flags)
{
  td_thragent_t * const ta = (td_thragent_t *) ta_arg;
  clib_smp_main_t * m = &ta->clib_smp_main;
  td_thrhandle_t th;
  int i;

  if (ps_pdread (ta->proc_handle, ta->addrs[SYM_clib_smp_main],
		 m, sizeof (m[0])) != PS_OK)
    return TD_ERR;

  LOG ("n cpus %d log2 vm bytes %d", m->n_cpus, m->log2_n_per_cpu_vm_bytes);

  th.th_ta_p = (td_thragent_t *) ta;

  th.th_unique = uword_to_pointer (ps_getpid (ta->proc_handle), void *);
  if (callback (&th, cbdata_p) != 0)
    return TD_DBERR;

  for (i = 1; i < m->n_cpus; i++)
    {
      clib_smp_per_cpu_main_t pm;

      if (ps_pdread (ta->proc_handle, &m->per_cpu_mains[i], &pm, sizeof (pm)) != PS_OK)
	return TD_ERR;

      LOG ("cpu %d tid %d", i, pm.thread_id);

      th.th_unique = uword_to_pointer (pm.thread_id, void *);
      if (callback (&th, cbdata_p) != 0)
	return TD_DBERR;
    }

  return TD_OK;
}

td_err_e
td_thr_validate (const td_thrhandle_t *th)
{
  LOG ("here");
  return TD_OK;
}

td_err_e
td_thr_get_info (const td_thrhandle_t *th, td_thrinfo_t * i)
{
  td_thragent_t * ta = th->th_ta_p;

  memset (i, 0, sizeof (i[0]));

  i->ti_ta_p = ta;
  i->ti_type = TD_THR_USER;
  i->ti_state = TD_THR_ACTIVE;
  i->ti_tid = (thread_t) th->th_unique;
  i->ti_lid = pointer_to_uword (th->th_unique);

  LOG ("uniq %ld lid %d", (long) th->th_unique, i->ti_lid);

  return TD_OK;
}

/* Get event address for EVENT.  */
td_err_e td_ta_event_addr (const td_thragent_t * ta,
			   td_event_e e,
			   td_notify_t * n)
{
  LOG ("event %d", e);
  switch (e)
    {
    case TD_CREATE:
    case TD_DEATH:
      n->type = NOTIFY_BPT;
      n->u.bptaddr = ta->addrs[SYM_thread_db_breakpoint];
      return TD_OK;

    default:
      return TD_ERR;
    }
}

/* Enable generation of events given in mask.  */
td_err_e
td_ta_set_event (const td_thragent_t * ta, td_thr_events_t * e)
{
  LOG ("0x%x 0x%x", e->event_bits[0], e->event_bits[1]);
  return TD_OK;
}

/* Disable generation of events given in mask.  */
td_err_e
td_ta_clear_event (const td_thragent_t * ta, td_thr_events_t * e)
{
  LOG ("0x%x 0x%x", e->event_bits[0], e->event_bits[1]);
  return TD_OK;
}

/* Return information about last event.  */
td_err_e td_ta_event_getmsg (const td_thragent_t * ta_arg, td_event_msg_t * m)
{
  td_thragent_t *const ta = (td_thragent_t *) ta_arg;
  td_thrhandle_t * th = &ta->msg_th;
  clib_thread_db_event_t e, e_invalid;

  memset (&e_invalid, ~0, sizeof (e_invalid));
  ps_pdread (ta->proc_handle, ta->addrs[SYM_thread_db_event], &e, sizeof (e));

  if (! memcmp (&e, &e_invalid, sizeof (e)))
    return TD_NOMSG;

  ps_pdwrite (ta->proc_handle, ta->addrs[SYM_thread_db_event], &e_invalid, sizeof (e_invalid));

  LOG ("%d %d", e.event, (int) e.data);

  m->event = e.event;
  m->th_p = th;
  m->msg.data = 0;
  th->th_ta_p = ta;
  th->th_unique = (void *) e.data;
  return TD_OK;
}

/* Enable reporting for EVENT for thread TH.  */
td_err_e td_thr_event_enable (const td_thrhandle_t * th, int e)
{
  LOG ("%ld %d", (long) th->th_unique, e);
  return TD_OK;
}

/* Get address of thread local variable.  */
td_err_e _td_thr_tls_get_addr (const td_thrhandle_t *__th,
			      psaddr_t __map_address, size_t __offset,
			      psaddr_t *__address)
{
  LOG ("here");
  return TD_OK;
}
