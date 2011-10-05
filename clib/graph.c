#include <clib/graph.h>

/* Set link distance, creating link if not found. */
u32 graph_set_link (graph_t * g, u32 src, u32 dst, u32 distance)
{
  graph_node_t * src_node, * dst_node;
  graph_link_t * l;
  u32 old_distance;
  
  /* The following validate will not work if src or dst are on the
     pool free list. */
  if (src < vec_len (g->nodes))
    ASSERT (! pool_is_free_index (g->nodes, src));
  if (dst < vec_len (g->nodes))
    ASSERT (! pool_is_free_index (g->nodes, dst));

  /* Make new (empty) nodes to make src and dst valid. */
  pool_validate_index (g->nodes, clib_max (src, dst));

  src_node = pool_elt_at_index (g->nodes, src);
  dst_node = pool_elt_at_index (g->nodes, dst);

  l = graph_dir_get_link_to_node (&src_node->next, dst);
  if (l)
    {
      old_distance = l->distance;
      l->distance = distance;

      l = graph_dir_get_link_to_node (&dst_node->prev, src);
      l->distance = distance;
    }
  else
    {
      uword li_next, li_prev;

      old_distance = ~0;

      li_next = graph_dir_add_link (&src_node->next, dst, distance);
      li_prev = graph_dir_add_link (&dst_node->prev, src, distance);

      l = vec_elt_at_index (src_node->next.links, li_next);
      l->link_to_self_index = li_prev;

      l = vec_elt_at_index (dst_node->prev.links, li_prev);
      l->link_to_self_index = li_next;
    }

  return old_distance;
}

void graph_del_link (graph_t * g, u32 src, u32 dst)
{
  graph_node_t * src_node, * dst_node;
  
  src_node = pool_elt_at_index (g->nodes, src);
  dst_node = pool_elt_at_index (g->nodes, dst);

  graph_dir_del_link (&src_node->next, dst);
  graph_dir_del_link (&dst_node->next, src);
}

/* Delete source node and all links from other nodes from/to source. */
uword graph_del_node (graph_t * g, u32 src)
{
  graph_node_t * src_node, * n;
  uword index;
  graph_link_t * l;

  src_node = pool_elt_at_index (g->nodes, src);

  vec_foreach (l, src_node->next.links)
    {
      n = pool_elt_at_index (g->nodes, l->node_index);
      graph_dir_del_link (&n->prev, src);
    }

  vec_foreach (l, src_node->prev.links)
    {
      n = pool_elt_at_index (g->nodes, l->node_index);
      graph_dir_del_link (&n->next, src);
    }

  graph_dir_free (&src_node->next);
  graph_dir_free (&src_node->prev);

  index = n - g->nodes;
  pool_put (g->nodes, n);
  memset (n, ~0, sizeof (n[0]));

  return index;
}

uword unformat_graph (unformat_input_t * input, va_list * args)
{
  graph_t * g = va_arg (*args, graph_t *);
  typedef struct {
    u32 src, dst, distance;
  } T;
  T * links = 0, * l;
  uword result;

  while (1)
    {
      vec_add2 (links, l, 1);
      if (! unformat (input, "%d%d%d", &l->src, &l->dst, &l->distance))
	break;
    }
  _vec_len (links) -= 1;
  result = vec_len (links) > 0;
  vec_foreach (l, links)
    {
      graph_set_link (g, l->src, l->dst, l->distance);
      graph_set_link (g, l->dst, l->src, l->distance);
    }

  vec_free (links);
  return result;
}

u8 * format_graph (u8 * s, va_list * args)
{
  graph_t * g = va_arg (*args, graph_t *);
  graph_node_t * n;
  graph_link_t * l;
  uword indent = format_get_indent (s);

  s = format (s, "graph %d nodes", pool_elts (g->nodes));
  pool_foreach (n, g->nodes, ({
    s = format (s, "\n%U", format_white_space, indent + 2);
    s = format (s, "%d -> ", n - g->nodes);
    vec_foreach (l, n->next.links)
      s = format (s, "%d (%d), ", l->node_index, l->distance);
  }));

  return s;
}

/* Fibonacci heaps. */
always_inline fheap_node_t *
fheap_get_node (fheap_t * f, u32 i)
{ return i != ~0 ? vec_elt_at_index (f->nodes, i) : 0; }

always_inline fheap_node_t *
fheap_get_root (fheap_t * f)
{ return fheap_get_node (f, f->min_root); }

static void fheap_validate (fheap_t * f)
{
  fheap_node_t * n, * m;
  uword ni, si;

  if (! CLIB_DEBUG || ! f->enable_validate)
    return;

  vec_foreach_index (ni, f->nodes)
    {
      n = vec_elt_at_index (f->nodes, ni);

      /* Min root must have minimal key. */
      m = vec_elt_at_index (f->nodes, f->min_root);
      ASSERT (n->key >= m->key);

      /* Min root must have no parent. */
      if (ni == f->min_root)
	ASSERT (n->parent == ~0);

      /* Check sibling linkages. */
      if (n->next_sibling == ~0)
	ASSERT (n->prev_sibling == ~0);
      else if (n->prev_sibling == ~0)
	ASSERT (n->next_sibling == ~0);
      else
	{
	  fheap_node_t * prev, * next;
	  u32 si = n->next_sibling, si_start = si;
	  do {
	    m = vec_elt_at_index (f->nodes, si);
	    prev = vec_elt_at_index (f->nodes, m->prev_sibling);
	    next = vec_elt_at_index (f->nodes, m->next_sibling);
	    ASSERT (prev->next_sibling == si);
	    ASSERT (next->prev_sibling == si);
	    si = m->next_sibling;
	  } while (si != si_start);
	}

      /* Loop through all siblings. */
      {
	u32 n_siblings = 0;

	foreach_fheap_node_sibling (f, si, n->next_sibling, ({
	  m = vec_elt_at_index (f->nodes, si);

	  /* All siblings must have same parent. */
	  ASSERT (m->parent == n->parent);

	  n_siblings += 1;
	}));

	/* Either parent is non-empty or there are siblings present. */
	if (n->parent == ~0 && ni != f->min_root)
	  ASSERT (n_siblings > 0);
      }

      /* Loop through all children. */
      {
	u32 found_first_child = n->first_child == ~0;
	u32 n_children = 0;

	foreach_fheap_node_sibling (f, si, n->first_child, ({
	  m = vec_elt_at_index (f->nodes, si);

	  /* Children must have larger keys than their parent. */
	  ASSERT (m->key >= n->key);

	  if (! found_first_child)
	    found_first_child = si == n->first_child;

	  n_children += 1;
	}));

	/* Check that first child is present on list. */
	ASSERT (found_first_child);

	/* Make sure rank is correct. */
	ASSERT (n->rank == n_children);
      }
    }

  /* Increment serial number for each successful validate.
     Failure can be used as condition for gdb breakpoints. */
  f->validate_serial++;
}

static void
fheap_node_add_sibling (fheap_t * f, u32 ni, u32 ni_to_add)
{
  fheap_node_t * n = vec_elt_at_index (f->nodes, ni);
  fheap_node_t * n_to_add = vec_elt_at_index (f->nodes, ni_to_add);
  fheap_node_t * n_next = fheap_get_node (f, n->next_sibling);
  fheap_node_t * parent;

  /* Empty list? */
  if (n->next_sibling == ~0)
    {
      ASSERT (n->prev_sibling == ~0);
      n->next_sibling = n->prev_sibling = ni_to_add;
      n_to_add->next_sibling = n_to_add->prev_sibling = ni;
    }
  else
    {
      /* Add node after existing node. */
      n_to_add->prev_sibling = ni;
      n_to_add->next_sibling = n->next_sibling;

      n->next_sibling = ni_to_add;
      n_next->prev_sibling = ni_to_add;
    }

  n_to_add->parent = n->parent;
  parent = fheap_get_node (f, n->parent);
  if (parent)
    parent->rank += 1;
}

void fheap_add_item (fheap_t * f, u32 ni, u32 key)
{
  fheap_node_t * r, * n;
  u32 ri;

  n = vec_elt_at_index (f->nodes, ni);

  memset (n, 0, sizeof (n[0]));
  n->parent = n->first_child = n->next_sibling = n->prev_sibling = ~0;
  n->key = key;

  r = fheap_get_root (f);
  ri = f->min_root;
  if (! r)
    {
      /* No root?  Add node as new root. */
      f->min_root = ni;
    }
  else
    {
      /* Add node as sibling of current root. */
      fheap_node_add_sibling (f, ri, ni);

      /* New node may become new root. */
      if (r->key > n->key)
	f->min_root = ni;
    }

  fheap_validate (f);
}

static u32
fheap_node_remove (fheap_t * f, u32 ni)
{
  fheap_node_t * n = vec_elt_at_index (f->nodes, ni);
  u32 prev_ni = n->prev_sibling;
  u32 next_ni = n->next_sibling;
  u32 list_has_single_element = prev_ni == ni;
  fheap_node_t * prev = fheap_get_node (f, prev_ni);
  fheap_node_t * next = fheap_get_node (f, next_ni);
  fheap_node_t * p = fheap_get_node (f, n->parent);

  if (p)
    {
      ASSERT (p->rank > 0);
      p->rank -= 1;
      p->first_child = list_has_single_element ? ~0 : next_ni;
    }

  if (prev)
    {
      ASSERT (prev->next_sibling == ni);
      prev->next_sibling = next_ni;
    }
  if (next)
    {
      ASSERT (next->prev_sibling == ni);
      next->prev_sibling = prev_ni;
    }

  n->prev_sibling = n->next_sibling = ni;
  n->parent = ~0;

  return list_has_single_element ? ~0 : next_ni;
}

static void fheap_link_root (fheap_t * f, u32 ni)
{
  fheap_node_t * n = vec_elt_at_index (f->nodes, ni);
  fheap_node_t * r, * lo, * hi;
  u32 ri, lo_i, hi_i, k;

  while (1)
    {
      k = n->rank;
      vec_validate_init_empty (f->root_list_by_rank, k, ~0);
      ri = f->root_list_by_rank[k];
      r = fheap_get_node (f, ri);
      if (! r)
	{
	  f->root_list_by_rank[k] = ni;
	  return;
	}

      f->root_list_by_rank[k] = ~0;

      /* Sort n/r into lo/hi by their keys. */
      lo = r, lo_i = ri;
      hi = n, hi_i = ni;
      if (hi->key < lo->key)
	{
	  u32 ti;
	  fheap_node_t * tn;
	  ti = lo_i, tn = lo;
	  lo = hi, lo_i = hi_i;
	  hi = tn, hi_i = ti;
	}

      /* Remove larger key. */
      fheap_node_remove (f, hi_i);

      /* Add larger key as child of smaller one. */
      if (lo->first_child == ~0)
	{
	  hi->parent = lo_i;
	  lo->first_child = hi_i;
	  lo->rank = 1;
	}
      else
	fheap_node_add_sibling (f, lo->first_child, hi_i);

      /* Following Fredman & Trajan: "When making a root node X a child of another node in a linking step,
	 we unmark X". */
      hi->is_marked = 0;

      ni = lo_i;
      n = lo;
    }
}

u32 fheap_del_min (fheap_t * f, u32 * min_key)
{
  fheap_node_t * r = fheap_get_root (f);
  u32 to_delete_min_ri = f->min_root;
  u32 ri, ni;

  /* Empty heap? */
  if (! r)
    return ~0;

  /* Root's children become siblings. */
  foreach_fheap_node_sibling (f, ni, r->first_child, ({
    fheap_node_remove (f, ni);
    fheap_node_add_sibling (f, to_delete_min_ri, ni);
  }));

  /* Remove min root. */
  ri = fheap_node_remove (f, to_delete_min_ri);

  /* Find new min root from among siblings including the ones we've just added. */
  f->min_root = ~0;
  if (ri != ~0)
    {
      u32 ri_last, ri_next, i, min_ds;

      ASSERT (f->nodes[ri].parent == ~0);

      r = fheap_get_node (f, ri);
      ri_last = r->prev_sibling;
      while (1)
	{
	  ri_next = r->next_sibling;
	  fheap_link_root (f, ri);
	  if (ri == ri_last)
	    break;
	  ri = ri_next;
	  r = fheap_get_node (f, ri);
	}

      min_ds = ~0;
      vec_foreach_index (i, f->root_list_by_rank)
	{
	  ni = f->root_list_by_rank[i];
	  if (ni == ~0)
	    continue;
	  f->root_list_by_rank[i] = ~0;
	  r = fheap_get_node (f, ni);
	  if (r->key < min_ds)
	    {
	      f->min_root = ni;
	      min_ds = r->key;
	      ASSERT (r->parent == ~0);
	    }
	}
    }

  /* Return deleted min root. */
  r = vec_elt_at_index (f->nodes, to_delete_min_ri);
  if (min_key)
    *min_key = r->key;

  fheap_validate (f);

  return to_delete_min_ri;
}

static void fheap_mark_parent (fheap_t * f, u32 pi)
{
  fheap_node_t * p = vec_elt_at_index (f->nodes, pi);

  /* Parent is a root: do nothing. */
  if (p->parent == ~0)
    return;

  /* If not marked, mark it. */
  if (! p->is_marked)
    {
      p->is_marked = 1;
      return;
    }

  /* Its a previously marked, non-root parent.
     Cut edge to its parent and add to root list. */
  fheap_node_remove (f, pi);
  fheap_node_add_sibling (f, f->min_root, pi);

  /* Unmark it since its now a root node. */
  p->is_marked = 0;

  /* "Cascading cuts": check parent. */
  if (p->parent != ~0)
    fheap_mark_parent (f, p->parent);
}

/* Set key to new smaller value. */
void fheap_decrease_key (fheap_t * f, u32 ni, u32 new_key)
{
  fheap_node_t * n = vec_elt_at_index (f->nodes, ni);
  fheap_node_t * r = fheap_get_root (f);

  n->key = new_key;

  if (n->parent != ~0)
    {
      fheap_mark_parent (f, n->parent);

      /* Remove node and add to root list. */
      fheap_node_remove (f, ni);
      fheap_node_add_sibling (f, f->min_root, ni);
    }

  if (n->key < r->key)
    f->min_root = ni;

  fheap_validate (f);
}

void fheap_del_item (fheap_t * f, u32 ni)
{
  fheap_node_t * n;

  n = vec_elt_at_index (f->nodes, ni);

  if (n->parent == ~0)
    {
      ASSERT (ni == f->min_root);
      fheap_del_min (f, 0);
    }
  else
    {
      u32 ci;

      fheap_mark_parent (f, n->parent);

      /* Add children to root list. */
      foreach_fheap_node_sibling (f, ci, n->first_child, ({
	fheap_node_remove (f, ci);
	fheap_node_add_sibling (f, f->min_root, ci);
      }));

      fheap_node_remove (f, ni);
      pool_put_index (f->nodes, ni);
    }

  fheap_validate (f);
}
