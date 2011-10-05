#ifndef included_clib_graph_h
#define included_clib_graph_h

#include <clib/format.h>
#include <clib/hash.h>
#include <clib/pool.h>

/* Generic graphs. */
typedef struct {
  /* Next node along this link. */
  u32 node_index;

  /* Other direction link index to reach back to current node. */
  u32 link_to_self_index;

  /* Distance to next node. */
  u32 distance;
} graph_link_t;

/* Direction on graph: either next or previous. */
typedef struct {
  /* Vector of links. */
  graph_link_t * links;

  /* Hash mapping node index to link which visits this node. */
  uword * link_index_by_node_index;
} graph_dir_t;

always_inline void
graph_dir_free (graph_dir_t * d)
{
  vec_free (d->links);
  hash_free (d->link_index_by_node_index);
}

always_inline graph_link_t *
graph_dir_get_link_to_node (graph_dir_t * d, u32 node_index)
{
  uword * p = hash_get (d->link_index_by_node_index, node_index);
  return p ? vec_elt_at_index (d->links, p[0]) : 0;
}

always_inline uword
graph_dir_add_link (graph_dir_t * d, u32 node_index, u32 distance)
{
  graph_link_t * l;
  ASSERT (! graph_dir_get_link_to_node (d, node_index));
  vec_add2 (d->links, l, 1);
  l->node_index = node_index;
  l->distance = distance;
  hash_set (d->link_index_by_node_index, node_index, l - d->links);
  return l - d->links;
}

always_inline void
graph_dir_del_link (graph_dir_t * d, u32 node_index)
{
  graph_link_t * l = graph_dir_get_link_to_node (d, node_index);
  uword li = l - d->links;
  uword n_links = vec_len (d->links);

  ASSERT (l != 0);
  hash_unset (d->link_index_by_node_index, node_index);
  n_links -= 1;
  if (li < n_links)
    d->links[li] = d->links[n_links];
  _vec_len (d->links) = n_links;
}

typedef struct {
  /* Nodes we are connected to plus distances. */
  graph_dir_t next, prev;
} graph_node_t;

typedef struct {
  /* Pool of nodes. */
  graph_node_t * nodes;
} graph_t;

/* Set link distance, creating link if not found. */
u32 graph_set_link (graph_t * g, u32 src, u32 dst, u32 distance);
void graph_del_link (graph_t * g, u32 src, u32 dst);
uword graph_del_node (graph_t * g, u32 src);

unformat_function_t unformat_graph;
format_function_t format_graph;

/* Fibonacci Heaps Fredman, M. L.; Tarjan (1987).
   "Fibonacci heaps and their uses in improved network optimization algorithms" */

typedef struct {
  /* Node index of parent. */
  u32 parent;

  /* Node index of first child. */
  u32 first_child;

  /* Next and previous nodes in doubly linked list of siblings. */
  u32 next_sibling, prev_sibling;

  /* Key (distance) for this node.  Parent always has key
     <= than keys of children. */
  u32 key;

  /* Number of children (as opposed to descendents). */
  u32 rank;

  u32 is_marked;
} fheap_node_t;

#define foreach_fheap_node_sibling(f,ni,first_ni,body)			\
do {									\
  u32 __fheap_foreach_first_ni = (first_ni);				\
  u32 __fheap_foreach_ni = __fheap_foreach_first_ni;			\
  u32 __fheap_foreach_next_ni;						\
  fheap_node_t * __fheap_foreach_n;					\
  if (__fheap_foreach_ni != ~0)						\
    while (1)								\
      {									\
	__fheap_foreach_n = fheap_get_node ((f), __fheap_foreach_ni);	\
	__fheap_foreach_next_ni = __fheap_foreach_n -> next_sibling;	\
	(ni) = __fheap_foreach_ni;					\
									\
	body;								\
									\
	/* End of circular list? */					\
	if (__fheap_foreach_next_ni == __fheap_foreach_first_ni)	\
	  break;							\
									\
	__fheap_foreach_ni = __fheap_foreach_next_ni;			\
									\
      }									\
} while (0)

typedef struct {
  u32 min_root;

  /* Vector of nodes. */
  fheap_node_t * nodes;

  u32 * root_list_by_rank;

  u32 enable_validate;

  u32 validate_serial;
} fheap_t;

/* Initialize empty heap. */
always_inline void
fheap_init (fheap_t * f, u32 n_nodes)
{
  fheap_node_t * save_nodes = f->nodes;
  u32 * save_root_list = f->root_list_by_rank;

  memset (f, 0, sizeof (f[0]));

  f->nodes = save_nodes;
  f->root_list_by_rank = save_root_list;

  vec_validate (f->nodes, n_nodes - 1);
  vec_reset_length (f->root_list_by_rank);

  f->min_root = ~0;
}

always_inline void
fheap_free (fheap_t * f)
{
  vec_free (f->nodes);
  vec_free (f->root_list_by_rank);
}

always_inline u32
fheap_find_min (fheap_t * f)
{ return f->min_root; }

always_inline u32
fheap_is_empty (fheap_t * f)
{ return f->min_root == ~0; }

void fheap_add_item (fheap_t * f, u32 ni, u32 key);
void fheap_del_item (fheap_t * f, u32 ni);

u32 fheap_del_min (fheap_t * f, u32 * min_key);

void fheap_decrease_key (fheap_t * f, u32 ni, u32 new_key);

#endif /* included_clib_graph_h */
