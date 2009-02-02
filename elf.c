#include <clib/byte_order.h>
#include <clib/error.h>
#include <clib/hash.h>
#include <clib/vec.h>
#include <clib/elf.h>

static u8 *
format_elf_section_type (u8 * s, va_list * args)
{
  elf_section_type_t type = va_arg (*args, elf_section_type_t);
  char * t = 0;

  switch (type)
    {
#define _(f,i) case ELF_SECTION_##f: t = #f; break;
      foreach_elf_section_type
#undef _
    }

  if (! t)
    s = format (s, "unknown 0x%x", type);
  else
    s = format (s, "%s", t);
  return s;
}

static u8 *
format_elf_section (u8 * s, va_list * args)
{
  elf_main_t * em = va_arg (*args, elf_main_t *);
  elf64_section_header_t * h = va_arg (*args, elf64_section_header_t *);

  if (! h)
    return format (s, "%=24s%=16s%=8s%=16s",
		   "Name", "Type", "Size", "Address");

  s = format (s, "%-24s%=16U%8Ld%16Lx%16Ld",
	      elf_section_name (em, h),
	      format_elf_section_type, h->type,
	      h->size_bytes,
	      h->exec_address,
	      h->file_offset);

  if (h->flags != 0)
    {
#define _(f,i) \
  if (h->flags & ELF_SECTION_FLAG_##f) s = format (s, " %s", #f);
      foreach_elf_section_flag;
#undef _
    }

  return s;
}

static u8 *
format_elf_segment_type (u8 * s, va_list * args)
{
  elf_segment_type_t type = va_arg (*args, elf_segment_type_t);
  char * t = 0;

  switch (type)
    {
#define _(f,i) case ELF_SEGMENT_##f: t = #f; break;
      foreach_elf_segment_type
#undef _
    }

  if (! t)
    s = format (s, "unknown 0x%x", type);
  else
    s = format (s, "%s", t);
  return s;
}

static u8 *
format_elf_segment (u8 * s, va_list * args)
{
  elf64_segment_header_t * h = va_arg (*args, elf64_segment_header_t *);

  if (! h)
    return format (s, "%=16s%=16s%=16s%=16s",
		   "Type", "Virt. Address", "Phys. Address", "Size");

  s = format (s, "%=16U%16Lx%16Lx%16Ld%16Ld",
	      format_elf_segment_type, h->type,
	      h->virtual_address,
	      h->physical_address,
	      h->memory_size,
	      h->file_offset);

  if (h->flags != 0)
    {
#define _(f,i) \
  if (h->flags & ELF_SEGMENT_FLAG_##f) s = format (s, " %s", #f);
      foreach_elf_segment_flag;
#undef _
    }

  return s;
}

static u8 *
format_elf_relocation_type (u8 * s, va_list * args)
{
  elf_main_t * em = va_arg (*args, elf_main_t *);
  int type = va_arg (*args, int);
  char * t = 0;

  switch (em->architecture)
    {
#define _(f,i) [i] = #f,

    case ELF_ARCH_X86_64:
      {
	static char * tab[] = {
	  foreach_elf_x86_64_relocation_type
	};

#undef _
	if (type < ARRAY_LEN (tab))
	  t = tab[type];
	break;
      }

    default:
      break;
    }

  if (! t)
    s = format (s, "0x%02x", type);
  else
    s = format (s, "%s", t);

  return s;
}

static u8 *
format_elf_relocation (u8 * s, va_list * args)
{
  elf_main_t * em = va_arg (*args, elf_main_t *);
  void * p = va_arg (*args, void *);
  elf_section_type_t type = va_arg (*args, elf_section_type_t);
  elf_symbol_table_t * t;
  elf64_relocation_t * r64, r64_tmp[2];
  elf64_symbol_t * sym;

  if (! p)
    return format (s, "%=16s%=16s%=16s", "Address", "Type", "Symbol");

  if (em->file_class == ELF_64BIT)
    r64 = p;
  else
    {
      elf32_relocation_t * r32 = p;
      r64 = &r64_tmp[0];
      r64->address = r32->address;
      r64->symbol_and_type = (u64) (r32->symbol_and_type >> 8) << 32;
      r64->symbol_and_type |= r32->symbol_and_type & 0xff;
      if (type == ELF_SECTION_RELOCATION_ADD)
	r64->addend[0] = r32->addend[0];
    }

  t = vec_elt_at_index (em->symbol_tables, 0);
  sym = vec_elt_at_index (t->symbols, r64->symbol_and_type >> 32);

  s = format (s, "%16Lx%16U",
	      r64->address,
	      format_elf_relocation_type, em, r64->symbol_and_type & 0xff);

  if (sym->section_index != 0)
    {
      elf64_section_header_t * h;
      h = vec_elt_at_index (em->sections, sym->section_index);
      s = format (s, " {%s}", elf_section_name (em, h));
    }

  if (sym->name != 0)
    s = format (s, " %s", elf_symbol_name (t, sym));

  if (type == ELF_SECTION_RELOCATION_ADD)
    {
      i64 a = r64->addend[0];
      if (a != 0)
	s = format (s, " %c 0x%Lx",
		    a > 0 ? '+' : '-',
		    a > 0 ? a : -a);
    }

  return s;
}

u8 *
format_elf_main (u8 * s, va_list * args)
{
  elf_main_t * em = va_arg (*args, elf_main_t *);

  {
    elf64_section_header_t * h, * copy;

    copy = 0;
    vec_foreach (h, em->sections)
      if (h->type != ELF_SECTION_UNUSED)
	vec_add1 (copy, h[0]);

    /* Sort sections by name. */
    vec_sort (copy, s1, s2,
	      strcmp ((char *) elf_section_name (em, s1),
		      (char *) elf_section_name (em, s2)));

    s = format (s, "\nSections:\n");
    s = format (s, "%U\n", format_elf_section, em, 0);
    vec_foreach (h, copy)
      s = format (s, "%U\n", format_elf_section, em, h);

    vec_free (copy);
  }

  {
    elf64_segment_header_t * h, * copy;

    copy = 0;
    vec_foreach (h, em->segments)
      if (h->type != ELF_SEGMENT_UNUSED)
	vec_add1 (copy, h[0]);

    /* Sort segments by address. */
    vec_sort (copy, s1, s2,
	      (i64) s1->virtual_address - (i64) s2->virtual_address);

    s = format (s, "\nSegments:\n");
    s = format (s, "%U\n", format_elf_segment, 0);
    vec_foreach (h, copy)
      s = format (s, "%U\n", format_elf_segment, h);

    vec_free (copy);
  }

  if (vec_len (em->relocation_tables) > 0)
    {
      elf_relocation_table_t * t;
      elf64_relocation_t * r;
      elf64_section_header_t * h;
      uword i;

      vec_foreach (t, em->relocation_tables)
	{
	  h = vec_elt_at_index (em->sections, t->section_index);
	  r = t->relocations;
	  s = format (s, "\nRelocations for section %s:\n",
		      elf_section_name (em, h));

	  s = format (s, "%U\n", format_elf_relocation, em, 0, 0);
	  for (i = 0; i < vec_len (t->relocations); i++)
	    {
	      s = format (s, "%U\n",
			  format_elf_relocation, em, r, h->type);
	      r = elf_relocation_next (r, h->type);
	    }
	}
    }

  return s;
}

static void
elf_parse_segment_header (elf_main_t * em, void * d, uword n)
{
  uword i;

  vec_resize (em->segments, n);

  for (i = 0; i < n; i++)
    {
      if (em->file_class == ELF_64BIT)
	{
	  elf64_segment_header_t * h = d;
#define _(t,f) h->f = elf_swap_##t (em, h->f);
	  foreach_elf64_segment_header
#undef _
	  em->segments[i] = h[0];
	  d = (h + 1);
	}
      else
	{
	  elf32_segment_header_t * h = d;
#define _(t,f) em->segments[i].f = elf_swap_##t (em, h->f);
	  foreach_elf32_segment_header
#undef _
	  d = (h + 1);
	}
    }
}

static void
elf_parse_section_header (elf_main_t * em, void * d, uword n)
{
  uword i;

  vec_resize (em->sections, n);

  for (i = 0; i < n; i++)
    {
      if (em->file_class == ELF_64BIT)
	{
	  elf64_section_header_t * h = d;
#define _(t,f) h->f = elf_swap_##t (em, h->f);
	  foreach_elf64_section_header
#undef _
	  em->sections[i] = h[0];
	  d = (h + 1);
	}
      else
	{
	  elf32_section_header_t * h = d;
#define _(t,f) em->sections[i].f = elf_swap_##t (em, h->f);
	  foreach_elf32_section_header
#undef _
	  d = (h + 1);
	}
    }
}

static void *
elf_contents (void * data, uword size)
{
  u8 * result = 0;
  vec_add (result, data, size);
  return result;
}

static void *
elf_section_contents (elf_main_t * em,
		      void * data,
		      uword section_index,
		      uword elt_size)
{
  elf64_section_header_t * s;
  u8 * v;

  s = vec_elt_at_index (em->sections, section_index);
  v = elf_contents (data + s->file_offset, s->size_bytes);
  ASSERT (vec_len (v) % elt_size == 0);
  _vec_len (v) /= elt_size;
  return v;
}

static void
add_symbol_table (elf_main_t * em, void * data, elf64_section_header_t * s)
{
  elf_symbol_table_t * tab;
  elf32_symbol_t * sym32;
  elf64_symbol_t * sym64;
  uword i;

  vec_add2 (em->symbol_tables, tab, 1);

  if (em->file_class == ELF_64BIT)
    {
      tab->symbols = elf_section_contents (em, data, s - em->sections,
					   sizeof (tab->symbols[0]));
      for (i = 0; i < vec_len (tab->symbols); i++)
	{
#define _(t,f) tab->symbols[i].f = elf_swap_##t (em, tab->symbols[i].f);
	  foreach_elf64_symbol_header;
#undef _
	}
    }
  else
    {
      sym32 = elf_section_contents (em, data, s - em->sections,
				    sizeof (sym32[0]));
      vec_clone (tab->symbols, sym32);
      for (i = 0; i < vec_len (tab->symbols); i++)
	{
#define _(t,f) tab->symbols[i].f = elf_swap_##t (em, sym32[i].f);
	  foreach_elf32_symbol_header;
#undef _
	}
    }

  if (s->link == 0)
    return;

  tab->string_table =
    elf_section_contents (em, data, s->link,
			  sizeof (tab->string_table[0]));
  tab->symbol_by_name
    = hash_create_string (/* # elts */ vec_len (tab->symbols),
			  /* sizeof of value */ sizeof (uword));

  vec_foreach (sym64, tab->symbols)
    {
      if (sym64->name != 0)
	hash_set_mem (tab->symbol_by_name,
		      tab->string_table + sym64->name,
		      sym64 - tab->symbols);
    }
}

static void
add_relocation_table (elf_main_t * em, void * data, elf64_section_header_t * s)
{
  uword has_addend = s->type == ELF_SECTION_RELOCATION_ADD;
  elf_relocation_table_t * t;
  uword i;

  vec_add2 (em->relocation_tables, t, 1);
  t->section_index = s - em->sections;

  if (em->file_class == ELF_64BIT)
    {
      elf64_relocation_t * r;

      r = elf_section_contents (em, data, t->section_index, 
				sizeof (r[0]) + has_addend * sizeof (r->addend[0]));
      if (em->need_byte_swap)
	for (i = 0; i < vec_len (r); i++)
	  {
	    r[i].address = elf_swap_u64 (em, r[i].address);
	    r[i].symbol_and_type = elf_swap_u32 (em, r[i].symbol_and_type);
	    if (has_addend)
	      r[i].addend[0] = elf_swap_u64 (em, r[i].addend[0]);
	  }
      t->relocations = r;
    }
  else
    {
      elf32_relocation_t * r32;

      r32 = elf_section_contents (em, data, t->section_index, 
				  sizeof (r32[0]) + has_addend * sizeof (r32->addend[0]));
      vec_clone (t->relocations, r32);
      if (em->need_byte_swap)
	for (i = 0; i < vec_len (r32); i++)
	  {
	    t->relocations[i].address = elf_swap_u32 (em, r32[i].address);
	    t->relocations[i].symbol_and_type = elf_swap_u32 (em, r32[i].symbol_and_type);
	    if (has_addend)
	      t->relocations[i].addend[0] = elf_swap_u32 (em, r32[i].addend[0]);
	  }
    }
}

clib_error_t *
elf_parse (elf_main_t * em,
	   void * data,
	   uword data_bytes)
{
  elf_file_header_t * h = data;
  clib_error_t * error = 0;
  elf64_file_header_t * h64, h64_32;

  if (! (h->magic[0] == 0x7f
	 && h->magic[1] == 'E'
	 && h->magic[2] == 'L'
	 && h->magic[3] == 'F'))
    return clib_error_return (0, "bad magic");

  memset (em, 0, sizeof (em[0]));

  em->need_byte_swap = 
      CLIB_ARCH_IS_BIG_ENDIAN != (h->data_encoding == ELF_TWOS_COMPLEMENT_BIG_ENDIAN);

  em->architecture = elf_swap_u16 (em, h->architecture);
  em->file_class = h->file_class;
  em->abi = h->abi;
  em->abi_version = h->abi_version;

  if (h->file_class == ELF_64BIT)
    {
      h64 = (void *) (h + 1);
#define _(t,f) h64->f = elf_swap_##t (em, h64->f);
      foreach_elf64_file_header
#undef _
    }
  else
    {
      elf32_file_header_t * h32 = (void *) (h + 1);

      h64 = &h64_32;

#define _(t,f) h64->f = elf_swap_##t (em, h32->f);
      foreach_elf32_file_header
#undef _
    }

  em->entry_point = h64->entry_point;

  elf_parse_segment_header (em,
			    data + h64->segment_header_file_offset,
			    h64->segment_header_count);
  elf_parse_section_header (em,
			    data + h64->section_header_file_offset,
			    h64->section_header_count);

  em->section_string_table = elf_section_contents
    (em, data, h64->section_header_string_table_index,
     sizeof (em->section_string_table[0]));

  {
    elf64_section_header_t * s;

    em->section_by_name
      = hash_create_string (/* # elts */ vec_len (em->sections),
			    /* sizeof of value */ sizeof (uword));

    vec_foreach (s, em->sections)
      {
	hash_set_mem (em->section_by_name,
		      elf_section_name (em, s),
		      s - em->sections);

	switch (s->type)
	  {
	  case ELF_SECTION_SYMBOL_TABLE:
	  case ELF_SECTION_DYNAMIC_SYMBOL_TABLE:
	    add_symbol_table (em, data, s);
	    break;

	  case ELF_SECTION_RELOCATION_ADD:
	  case ELF_SECTION_RELOCATION:
	    add_relocation_table (em, data, s);
	    break;

	  default:
	    break;
	  }
      }
  }

  return error;
}
