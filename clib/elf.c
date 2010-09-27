#include <clib/byte_order.h>
#include <clib/error.h>
#include <clib/hash.h>
#include <clib/vec.h>
#include <clib/elf.h>

always_inline void
elf_swap_first_header (elf_main_t * em, elf_first_header_t * h)
{
  h->architecture = elf_swap_u16 (em, h->architecture);
  h->file_type = elf_swap_u16 (em, h->file_type);
  h->file_version = elf_swap_u32 (em, h->file_version);
}

clib_error_t *
elf_get_section_by_name (elf_main_t * em, char * section_name, elf_section_t ** result)
{
  elf_section_t * s;
  uword * p;

  p = hash_get_mem (em->section_by_name, section_name);
  if (! p)
    return clib_error_return (0, "no such section `%s'", section_name);

  *result = vec_elt_at_index (em->sections, p[0]);
  return 0;
}

clib_error_t *
elf_get_section_by_start_address (elf_main_t * em, uword start_address, elf_section_t ** result)
{
  elf_section_t * s;
  uword * p;

  p = hash_get (em->section_by_start_address, start_address);
  if (! p)
    return clib_error_return (0, "no section with address 0x%wx", start_address);

  *result = vec_elt_at_index (em->sections, p[0]);
  return 0;
}

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
  elf_section_t * es = va_arg (*args, elf_section_t *);
  elf64_section_header_t * h = &es->header;

  if (! h)
    return format (s, "%=40s%=10s%=20s%=8s%=16s%=16s",
		   "Name", "Index", "Type", "Size", "Address", "File offset");

  s = format (s, "%-40s%10d%=20U%8Lx%16Lx %Lx-%Lx",
	      elf_section_name (em, es),
	      es->index,
	      format_elf_section_type, h->type,
	      h->file_size,
	      h->exec_address,
	      h->file_offset, h->file_offset + h->file_size);

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
  elf_segment_t * es = va_arg (*args, elf_segment_t *);
  elf64_segment_header_t * h = &es->header;

  if (! h)
    return format (s, "%=16s%=16s%=16s%=16s",
		   "Type", "Virt. Address", "Phys. Address", "Size");

  s = format (s, "%=16U%16Lx%16Lx%16Lx%16Lx",
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

  switch (em->first_header.architecture)
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

  if (em->first_header.file_class == ELF_64BIT)
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
      elf_section_t * es;
      es = vec_elt_at_index (em->sections, sym->section_index);
      s = format (s, " {%s}", elf_section_name (em, es));
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

static u8 *
format_elf_dynamic_entry_type (u8 * s, va_list * args)
{
  u32 type = va_arg (*args, u32);
  char * t = 0;
  switch (type)
    {
#define _(f,n) case n: t = #f; break;
      foreach_elf_dynamic_entry_type;
#undef _
    default: break;
    }
  if (t)
    return format (s, "%s", t);
  else
    return format (s, "unknown 0x%x", type);
}

static u8 *
format_elf_dynamic_entry (u8 * s, va_list * args)
{
  elf_main_t * em = va_arg (*args, elf_main_t *);
  elf64_dynamic_entry_t * e = va_arg (*args, elf64_dynamic_entry_t *);

  if (! e)
    return format (s, "%=40s%=16s", "Type", "Data");

  s = format (s, "%=40U",
	      format_elf_dynamic_entry_type, (u32) e->type);
  switch (e->type)
    {
    case ELF_DYNAMIC_ENTRY_NEEDED_LIBRARY:
    case ELF_DYNAMIC_ENTRY_RPATH:
    case ELF_DYNAMIC_ENTRY_RUN_PATH:
      s = format (s, "%s", em->dynamic_string_table + e->data);
      break;

    default:
      s = format (s, "0x%Lx", e->data);
      break;
    }
}

static u8 * format_elf_architecture (u8 * s, va_list * args)
{
  int a = va_arg (*args, int);
  char * t;

  switch (a)
    {
#define _(f,n) case n: t = #f; break;
      foreach_elf_architecture;
#undef _
    default:
      return format (s, "unknown 0x%x", a);
    }

  return format (s, "%s", t);
}

static u8 * format_elf_abi (u8 * s, va_list * args)
{
  int a = va_arg (*args, int);
  char * t;

  switch (a)
    {
#define _(f,n) case n: t = #f; break;
      foreach_elf_abi;
#undef _
    default:
      return format (s, "unknown 0x%x", a);
    }

  return format (s, "%s", t);
}

static u8 * format_elf_file_class (u8 * s, va_list * args)
{
  int a = va_arg (*args, int);
  char * t;

  switch (a)
    {
#define _(f) case ELF_##f: t = #f; break;
      foreach_elf_file_class;
#undef _
    default:
      return format (s, "unknown 0x%x", a);
    }

  return format (s, "%s", t);
}

static u8 * format_elf_file_type (u8 * s, va_list * args)
{
  int a = va_arg (*args, int);
  char * t;

  if (a >= ELF_ARCH_SPECIFIC_LO && a <= ELF_ARCH_SPECIFIC_HI)
    return format (s, "arch-specific 0x%x", a - ELF_ARCH_SPECIFIC_LO);

  if (a >= ELF_OS_SPECIFIC_LO && a <= ELF_OS_SPECIFIC_HI)
    return format (s, "os-specific 0x%x", a - ELF_OS_SPECIFIC_LO);

  switch (a)
    {
#define _(f,n) case n: t = #f; break;
      foreach_elf_file_type;
#undef _
    default:
      return format (s, "unknown 0x%x", a);
    }

  return format (s, "%s", t);
}

static u8 * format_elf_data_encoding (u8 * s, va_list * args)
{
  int a = va_arg (*args, int);
  char * t;

  switch (a)
    {
#define _(f) case ELF_##f: t = #f; break;
      foreach_elf_data_encoding;
#undef _
    default:
      return format (s, "unknown 0x%x", a);
    }

  return format (s, "%s", t);
}

u8 *
format_elf_main (u8 * s, va_list * args)
{
  elf_main_t * em = va_arg (*args, elf_main_t *);
  elf64_file_header_t * fh = &em->file_header;

  s = format (s, "File header: machine: %U, file type/class %U/%U, data-encoding: %U, abi: %U version %d\n",
	      format_elf_architecture, em->first_header.architecture,
	      format_elf_file_type, em->first_header.file_type,
	      format_elf_file_class, em->first_header.file_class,
	      format_elf_data_encoding, em->first_header.data_encoding,
	      format_elf_abi, em->first_header.abi, em->first_header.abi_version);

  s = format (s, "  entry 0x%Lx, arch-flags 0x%x",
	      em->file_header.entry_point, em->file_header.flags);

  if (em->interpreter)
    s = format (s, "\n  interpreter: %s", em->interpreter);

  {
    elf_section_t * h, * copy;

    copy = 0;
    vec_foreach (h, em->sections)
      if (h->header.type != ~0)
	vec_add1 (copy, h[0]);

    if (0)
      {
	/* Sort sections by name. */
	vec_sort (copy, s1, s2,
		  strcmp ((char *) elf_section_name (em, s1),
			  (char *) elf_section_name (em, s2)));
      }
    else
      {
	vec_sort (copy, s1, s2,
		  (i64) s1->header.file_offset - (i64) s2->header.file_offset);
      }

    s = format (s, "\nSections %d at file offset 0x%Lx:\n",
		em->file_header.section_header_count, em->file_header.section_header_file_offset);
    s = format (s, "%U\n", format_elf_section, em, 0);
    vec_foreach (h, copy)
      s = format (s, "%U\n", format_elf_section, em, h);

    vec_free (copy);
  }

  {
    elf_segment_t * h, * copy;

    copy = 0;
    vec_foreach (h, em->segments)
      if (h->header.type != ELF_SEGMENT_UNUSED && h->header.type != ~0)
	vec_add1 (copy, h[0]);

    /* Sort segments by address. */
    vec_sort (copy, s1, s2,
	      (i64) s1->header.virtual_address - (i64) s2->header.virtual_address);

    s = format (s, "\nSegments: %d at file offset 0x%Lx-0x%Lx:\n",
		fh->segment_header_count,
		fh->segment_header_file_offset,
		fh->segment_header_file_offset + fh->segment_header_count * fh->segment_header_size);
		
    s = format (s, "%U\n", format_elf_segment, 0);
    vec_foreach (h, copy)
      s = format (s, "%U\n", format_elf_segment, h);

    vec_free (copy);
  }

  if (vec_len (em->relocation_tables) > 0)
    {
      elf_relocation_table_t * t;
      elf64_relocation_t * r;
      elf_section_t * es;
      elf64_section_header_t * h;
      uword i;

      vec_foreach (t, em->relocation_tables)
	{
	  es = vec_elt_at_index (em->sections, t->section_index);
	  h = &es->header;
	  r = t->relocations;
	  s = format (s, "\nRelocations for section %s:\n",
		      elf_section_name (em, es));

	  s = format (s, "%U\n", format_elf_relocation, em, 0, 0);
	  for (i = 0; i < vec_len (t->relocations); i++)
	    {
	      s = format (s, "%U\n",
			  format_elf_relocation, em, r, h->type);
	      r = elf_relocation_next (r, h->type);
	    }
	}
    }

  if (vec_len (em->dynamic_entries) > 0)
    {
      elf64_dynamic_entry_t * es, * e;
      s = format (s, "\nDynamic linker information:\n");
      es = vec_dup (em->dynamic_entries);
      s = format (s, "%U\n", format_elf_dynamic_entry, em, 0);
      vec_foreach (e, es)
	s = format (s, "%U\n", format_elf_dynamic_entry, em, e);
    }

  return s;
}

static void
elf_parse_segments (elf_main_t * em, void * data)
{
  void * d = data + em->file_header.segment_header_file_offset;
  uword n = em->file_header.segment_header_count;
  uword i;

  vec_resize (em->segments, n);

  for (i = 0; i < n; i++)
    {
      if (em->first_header.file_class == ELF_64BIT)
	{
	  elf64_segment_header_t * h = d;
#define _(t,f) em->segments[i].header.f = elf_swap_##t (em, h->f);
	  foreach_elf64_segment_header
#undef _
	  d = (h + 1);
	}
      else
	{
	  elf32_segment_header_t * h = d;
#define _(t,f) em->segments[i].header.f = elf_swap_##t (em, h->f);
	  foreach_elf32_segment_header
#undef _
	  d = (h + 1);
	}
    }
}

static void
elf_parse_sections (elf_main_t * em, void * data)
{
  elf64_file_header_t * fh = &em->file_header;
  elf_section_t * s;
  void * d = data + fh->section_header_file_offset;
  uword n = fh->section_header_count;
  uword i;

  vec_resize (em->sections, n);

  for (i = 0; i < n; i++)
    {
      s = em->sections + i;

      s->index = i;

      if (em->first_header.file_class == ELF_64BIT)
	{
	  elf64_section_header_t * h = d;
#define _(t,f) em->sections[i].header.f = elf_swap_##t (em, h->f);
	  foreach_elf64_section_header
#undef _
	  d = (h + 1);
	}
      else
	{
	  elf32_section_header_t * h = d;
#define _(t,f) em->sections[i].header.f = elf_swap_##t (em, h->f);
	  foreach_elf32_section_header
#undef _
	  d = (h + 1);
	}

      if (s->header.type != ELF_SECTION_NO_BITS)
	vec_add (s->contents, data + s->header.file_offset, s->header.file_size);
    }

  s = vec_elt_at_index (em->sections, fh->section_header_string_table_index);

  em->section_by_name
    = hash_create_string (/* # elts */ vec_len (em->sections),
			  /* sizeof of value */ sizeof (uword));

  vec_foreach (s, em->sections)
    {
      hash_set_mem (em->section_by_name,
		    elf_section_name (em, s),
		    s - em->sections);
      hash_set (em->section_by_start_address,
		s->header.exec_address,
		s - em->sections);
    }
}

static void
add_symbol_table (elf_main_t * em, elf_section_t * s)
{
  elf_symbol_table_t * tab;
  elf32_symbol_t * sym32;
  elf64_symbol_t * sym64;
  uword i;

  vec_add2 (em->symbol_tables, tab, 1);

  if (em->first_header.file_class == ELF_64BIT)
    {
      tab->symbols = elf_section_contents (em, s - em->sections, sizeof (tab->symbols[0]));
      for (i = 0; i < vec_len (tab->symbols); i++)
	{
#define _(t,f) tab->symbols[i].f = elf_swap_##t (em, tab->symbols[i].f);
	  foreach_elf64_symbol_header;
#undef _
	}
    }
  else
    {
      sym32 = elf_section_contents (em, s - em->sections, sizeof (sym32[0]));
      vec_clone (tab->symbols, sym32);
      for (i = 0; i < vec_len (tab->symbols); i++)
	{
#define _(t,f) tab->symbols[i].f = elf_swap_##t (em, sym32[i].f);
	  foreach_elf32_symbol_header;
#undef _
	}
    }

  if (s->header.link == 0)
    return;

  tab->string_table =
    elf_section_contents (em, s->header.link, sizeof (tab->string_table[0]));
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
add_relocation_table (elf_main_t * em, elf_section_t * s)
{
  uword has_addend = s->header.type == ELF_SECTION_RELOCATION_ADD;
  elf_relocation_table_t * t;
  uword i;

  vec_add2 (em->relocation_tables, t, 1);
  t->section_index = s - em->sections;

  if (em->first_header.file_class == ELF_64BIT)
    {
      elf64_relocation_t * r, * rs;

      rs = elf_section_contents (em, t->section_index, 
				 sizeof (rs[0]) + has_addend * sizeof (rs->addend[0]));

      if (em->need_byte_swap)
	{
	  r = rs;
	  for (i = 0; i < vec_len (r); i++)
	    {
	      r->address = elf_swap_u64 (em, r->address);
	      r->symbol_and_type = elf_swap_u32 (em, r->symbol_and_type);
	      if (has_addend)
		r->addend[0] = elf_swap_u64 (em, r->addend[0]);
	      r = elf_relocation_next (r, s->header.type);
	    }
	}

      t->relocations = rs;
    }
  else
    {
      elf64_relocation_t * r64;
      elf32_relocation_t * r32, * r32s;

      r32s = elf_section_contents (em, t->section_index, 
				   sizeof (r32s[0]) + has_addend * sizeof (r32s->addend[0]));
      vec_clone (t->relocations, r32s);

      if (em->need_byte_swap)
	{
	  r64 = t->relocations;
	  r32 = r32s;
	  for (i = 0; i < vec_len (r32s); i++)
	    {
	      r64->address = elf_swap_u32 (em, r32->address);
	      r64->symbol_and_type = elf_swap_u32 (em, r32->symbol_and_type);
	      if (has_addend)
		r64->addend[0] = elf_swap_u32 (em, r32->addend[0]);
	      r32 = elf_relocation_next (r32, s->header.type);
	      r64 = elf_relocation_next (r64, s->header.type);
	    }
	}

      vec_free (r32s);
    }
}

static void elf_parse_symbols (elf_main_t * em)
{
  elf_section_t * s;

  /* No need to parse symbols twice. */
  if (em->parsed_symbols)
    return;
  em->parsed_symbols = 1;

  vec_foreach (s, em->sections)
    {
      switch (s->header.type)
	{
	case ELF_SECTION_SYMBOL_TABLE:
	case ELF_SECTION_DYNAMIC_SYMBOL_TABLE:
	  add_symbol_table (em, s);
	  break;

	case ELF_SECTION_RELOCATION_ADD:
	case ELF_SECTION_RELOCATION:
	  add_relocation_table (em, s);
	  break;

	default:
	  break;
	}
    }
}

static void
add_dynamic_entries (elf_main_t * em, elf_section_t * s)
{
  uword i;

  if (em->first_header.file_class == ELF_64BIT)
    {
      elf64_dynamic_entry_t * e;

      e = elf_section_contents (em, s - em->sections, sizeof (e[0]));
      if (em->need_byte_swap)
	for (i = 0; i < vec_len (e); i++)
	  {
	    e[i].type = elf_swap_u64 (em, e[i].type);
	    e[i].data = elf_swap_u64 (em, e[i].data);
	  }

      em->dynamic_entries = e;
    }
  else
    {
      elf32_dynamic_entry_t * e;

      e = elf_section_contents (em, s - em->sections, sizeof (e[0]));
      vec_clone (em->dynamic_entries, e);
      if (em->need_byte_swap)
	for (i = 0; i < vec_len (e); i++)
	  {
	    em->dynamic_entries[i].type = elf_swap_u32 (em, e[i].type);
	    em->dynamic_entries[i].data = elf_swap_u32 (em, e[i].data);
	  }

      vec_free (e);
    }
}

static void elf_parse_dynamic (elf_main_t * em)
{
  elf_section_t * s;
  elf64_dynamic_entry_t * e;

  vec_foreach (s, em->sections)
    {
      switch (s->header.type)
	{
	case ELF_SECTION_DYNAMIC:
	  add_dynamic_entries (em, s);
	  break;

	default:
	  break;
	}
    }

  em->dynamic_string_table_section_index = ~0;
  em->dynamic_string_table = 0;

  vec_foreach (e, em->dynamic_entries)
    {
      if (e->type == ELF_DYNAMIC_ENTRY_STRING_TABLE)
	{
	  elf_section_t * s;
	  clib_error_t * error;

	  error = elf_get_section_by_start_address (em, e->data, &s);
	  if (error)
	    {
	      clib_error_report (error);
	      return;
	    }

	  ASSERT (vec_len (em->dynamic_string_table) == 0);
	  em->dynamic_string_table_section_index = s - em->sections;
	  em->dynamic_string_table
	    = elf_section_contents (em, s - em->sections, sizeof (u8));
	}
    }
}

static char *
elf_find_interpreter (elf_main_t * em, void * data)
{
  elf_segment_t * g;
  elf_section_t * s;
  clib_error_t * error;
  uword * p;

  vec_foreach (g, em->segments)
    {
      if (g->header.type == ELF_SEGMENT_INTERP)
	break;
    }

  if (g >= vec_end (em->segments))
    return 0;

  p = hash_get (em->section_by_start_address, g->header.virtual_address);
  if (! p)
    return 0;

  s = vec_elt_at_index (em->sections, p[0]);
  return vec_dup (s->contents);
}

static clib_error_t *
elf_parse (elf_main_t * em,
	   void * data,
	   uword data_bytes)
{
  elf_first_header_t * h = data;
  elf64_file_header_t * fh = &em->file_header;
  clib_error_t * error = 0;

  memset (em, 0, sizeof (em[0]));

  em->first_header = h[0];
  em->need_byte_swap = 
      CLIB_ARCH_IS_BIG_ENDIAN != (h->data_encoding == ELF_TWOS_COMPLEMENT_BIG_ENDIAN);
  elf_swap_first_header (em, &em->first_header);

  if (! (h->magic[0] == 0x7f
	 && h->magic[1] == 'E'
	 && h->magic[2] == 'L'
	 && h->magic[3] == 'F'))
    return clib_error_return (0, "bad magic");

  if (h->file_class == ELF_64BIT)
    {
      elf64_file_header_t * h64 = (void *) (h + 1);
#define _(t,f) fh->f = elf_swap_##t (em, h64->f);
      foreach_elf64_file_header
#undef _
    }
  else
    {
      elf32_file_header_t * h32 = (void *) (h + 1);

#define _(t,f) fh->f = elf_swap_##t (em, h32->f);
      foreach_elf32_file_header
#undef _
    }

  elf_parse_segments (em, data);
  elf_parse_sections (em, data);

  return error;
}

#ifdef CLIB_UNIX

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

clib_error_t * elf_read_file (elf_main_t * em, char * file_name)
{
  int fd;
  struct stat fd_stat;
  uword mmap_length = 0;
  void * data = 0;
  clib_error_t * error = 0;

  elf_main_init (em);

  fd = open (file_name, 0);
  if (fd < 0)
    {
      error = clib_error_return_unix (0, "open `%s'", file_name);
      goto done;
    }

  if (fstat (fd, &fd_stat) < 0)
    {
      error = clib_error_return_unix (0, "fstat `%s'", file_name);
      goto done;
    }
  mmap_length = fd_stat.st_size;

  data = mmap (0, mmap_length, PROT_READ, MAP_SHARED, fd, /* offset */ 0);
  if (~pointer_to_uword (data) == 0)
    {
      error = clib_error_return_unix (0, "mmap `%s'", file_name);
      goto done;
    }

  error = elf_parse (em, data, mmap_length);
  if (error)
    goto done;

  elf_parse_symbols (em);
  elf_parse_dynamic (em);

  em->interpreter = elf_find_interpreter (em, data);

  munmap (data, mmap_length);
  close (fd);

  return /* no error */ 0;

 done:
  elf_main_free (em);
  if (fd >= 0)
    close (fd);
  if (data)
    munmap (data, mmap_length);
  return error;
}

clib_error_t * elf_write_file (elf_main_t * em, char * file_name)
{
  int fd;
  FILE * f;
  u64 section_max_file_offset;
  clib_error_t * error = 0;

  fd = open (file_name, O_CREAT | O_RDWR | O_TRUNC, 0755);
  if (fd < 0)
    return clib_error_return_unix (0, "open `%s'", file_name);

  f = fdopen (fd, "w");

  /* Write section contents. */
  {
    elf_section_t * s;

    section_max_file_offset = 0;

    vec_foreach (s, em->sections)
      {
	if (s->header.file_size == 0)
	  continue;

	switch (s->header.type)
	  {
	  case ~0:
	  case ELF_SECTION_SYMBOL_TABLE:
	    break;

	  case ELF_SECTION_STRING_TABLE:
	    if (s->index != em->dynamic_string_table_section_index)
	      continue;
	    else
	      break;

	  default:
	    break;
	  }

	section_max_file_offset = clib_max (s->header.file_offset + s->header.file_size,
					    section_max_file_offset);

	if (fseek (f, s->header.file_offset, SEEK_SET) < 0)
	  return clib_error_return_unix (0, "fseek 0x%Lx", s->header.file_offset);

	if (s->header.type == ELF_SECTION_NO_BITS)
	  /* don't write for .bss sections */;
	else if (fwrite (s->contents, vec_len (s->contents), 1, f) != 1)
	  {
	    error = clib_error_return_unix (0, "write %s section contents", elf_section_name (em, s));
	    goto error;
	  }
      }
  }

  /* Re-build section string table and write it out. */
  {
    elf_section_t * s;
    u8 * st = 0;

    vec_foreach (s, em->sections)
      {
	u8 * name = elf_section_name (em, s);
	s->header.name = vec_len (st);
	vec_add (st, name, strlen ((char *) name) + 1);
      }

    s = vec_elt_at_index (em->sections, em->file_header.section_header_string_table_index);

    s->header.file_offset = section_max_file_offset;
    s->header.file_size = vec_len (st);

    if (fseek (f, section_max_file_offset, SEEK_SET) < 0)
      return clib_error_return_unix (0, "fseek 0x%Lx", s->header.file_offset);

    if (fwrite (st, vec_len (st), 1, f) != 1)
      {
	error = clib_error_return_unix (0, "write section contents %s", elf_section_name (em, s));
	goto error;
      }

    section_max_file_offset += vec_len (st);
    vec_free (st);

    /* Round up to multiple of 16 bytes. */
    section_max_file_offset = round_pow2 (section_max_file_offset, 16);
  }

  /* Next write file headers. */
  fseek (f, 0, SEEK_SET);

  /* Write first header. */
  {
    elf_first_header_t h = em->first_header;

    elf_swap_first_header (em, &h);
    if (fwrite (&h, sizeof (h), 1, f) != 1)
      {
	error = clib_error_return_unix (0, "write first header");
	goto error;
      }
  }

  /* Write file header. */
  {
    elf64_file_header_t h = em->file_header;

    /* Segment headers are after first header. */
    h.segment_header_file_offset = sizeof (elf_first_header_t);
    if (em->first_header.file_class == ELF_64BIT)
      h.segment_header_file_offset += sizeof (elf64_file_header_t);
    else
      h.segment_header_file_offset += sizeof (elf32_file_header_t);

    h.section_header_file_offset = section_max_file_offset;

    if (em->first_header.file_class == ELF_64BIT)
      {
#define _(t,field) h.field = elf_swap_##t (em, h.field);
	foreach_elf64_file_header;
#undef _

      if (fwrite (&h, sizeof (h), 1, f) != 1)
	{
	  error = clib_error_return_unix (0, "write file header");
	  goto error;
	}
    }
  else
    {
      elf32_file_header_t h32;

#define _(t,field) h32.field = elf_swap_##t (em, h.field);
      foreach_elf32_file_header;
#undef _

      if (fwrite (&h32, sizeof (h32), 1, f) != 1)
	{
	  error = clib_error_return_unix (0, "write file header");
	  goto error;
	}
    }
  }

  /* Write segment headers. */
  {
    elf_segment_t * s;

    vec_foreach (s, em->segments)
      {
	elf64_segment_header_t h;

	if (s->header.type == ~0)
	  continue;

	h = s->header;

	if (em->first_header.file_class == ELF_64BIT)
	  {
#define _(t,field) h.field = elf_swap_##t (em, h.field);
	    foreach_elf64_segment_header;
#undef _
	    
	    if (fwrite (&h, sizeof (h), 1, f) != 1)
	      {
		error = clib_error_return_unix (0, "write segment header %U", format_elf_segment, em, s);
		goto error;
	      }
	  }
	else
	  {
	    elf32_segment_header_t h32;

#define _(t,field) h32.field = elf_swap_##t (em, h.field);
	    foreach_elf32_segment_header;
#undef _

	    if (fwrite (&h32, sizeof (h32), 1, f) != 1)
	      {
		error = clib_error_return_unix (0, "write segment header %U", format_elf_segment, em, s);
		goto error;
	      }
	  }
      }
  }

  {
    elf_section_t * s;
    u64 section_header_file_offset;

    section_header_file_offset = section_max_file_offset;

    /* Round up to multiple of 16 bytes. */
    section_max_file_offset = round_pow2 (section_max_file_offset, 16);

    if (fseek (f, section_max_file_offset, SEEK_SET) < 0)
      return clib_error_return_unix (0, "fseek 0x%Lx", section_max_file_offset);

    /* Write symbol/string tables. */
    vec_foreach (s, em->sections)
      {
	switch (s->header.type)
	  {
	  case ELF_SECTION_SYMBOL_TABLE:
	  case ELF_SECTION_STRING_TABLE:
	    break;

	  default:
	    continue;
	  }

	/* We've already written the section header string table. */
	if (em->file_header.section_header_string_table_index == s->index)
	  continue;
	if (em->dynamic_string_table_section_index == s->index)
	  continue;

	/* Correct file offsets/sizes in headers. */
	s->header.file_offset = ftell (f);
	s->header.file_size = vec_len (s->contents);

	if (fwrite (s->contents, vec_len (s->contents), 1, f) != 1)
	  {
	    error = clib_error_return_unix (0, "write section contents %s", elf_section_name (em, s));
	    goto error;
	  }
      }

    /* Finally write section headers. */
    if (fseek (f, section_header_file_offset, SEEK_SET) < 0)
      return clib_error_return_unix (0, "fseek 0x%Lx", section_max_file_offset);

    vec_foreach (s, em->sections)
      {
	elf64_section_header_t h;

	if (s->header.type == ~0)
	  continue;

	h = s->header;

	if (em->first_header.file_class == ELF_64BIT)
	  {
#define _(t,field) h.field = elf_swap_##t (em, h.field);
	    foreach_elf64_section_header;
#undef _
	    
	    if (fwrite (&h, sizeof (h), 1, f) != 1)
	      {
		error = clib_error_return_unix (0, "write %s section header", elf_section_name (em, s));
		goto error;
	      }
	  }
	else
	  {
	    elf32_section_header_t h32;

#define _(t,field) h32.field = elf_swap_##t (em, h.field);
	    foreach_elf32_section_header;
#undef _

	    if (fwrite (&h32, sizeof (h32), 1, f) != 1)
	      {
		error = clib_error_return_unix (0, "write %s section header", elf_section_name (em, s));
		goto error;
	      }
	  }
      }
  }

 error:
  fclose (f);
  return error;
}

clib_error_t * elf_delete_named_section (elf_main_t * em, char * section_name)
{
  elf_section_t * s;
  clib_error_t * error;

  error = elf_get_section_by_name (em, section_name, &s);
  if (error)
    return error;
  
  s->header.type = ~0;

  return 0;
}

void
elf_create_section_with_contents (elf_main_t * em,
				  char * section_name,
				  elf64_section_header_t * header,
				  void * contents,
				  uword n_content_bytes)
{
  elf_section_t * s, * sts;
  u8 * st, * c;
  uword * p, is_new_section;

  /* See if section already exists with given name.
     If so, just replace contents. */
  is_new_section = 0;
  if ((p = hash_get_mem (em->section_by_name, section_name)))
    {
      s = vec_elt_at_index (em->sections, p[0]);
      _vec_len (s->contents) = 0;
      c = s->contents;
    }
  else
    {
      vec_add2 (em->sections, s, 1);
      is_new_section = 1;
      c = 0;
    }

  sts = vec_elt_at_index (em->sections, em->file_header.section_header_string_table_index);
  st = sts->contents;

  s->header = header[0];

  s->header.file_offset = ~0;
  s->header.file_size = n_content_bytes;

  /* Add name to string table. */
  s->header.name = vec_len (st);
  vec_add (st, section_name, strlen (section_name));
  vec_add1 (st, 0);
  sts->contents = st;

  vec_resize (c, n_content_bytes);
  memcpy (c, contents, n_content_bytes);
  s->contents = c;

  em->file_header.section_header_count += is_new_section && s->header.type != ~0;
}

uword elf_delete_segment_with_type (elf_main_t * em, elf_segment_type_t segment_type)
{
  uword n_deleted = 0;
  elf_segment_t * s;

  vec_foreach (s, em->segments)
    if (s->header.type == segment_type)
      {
	s->header.type = ~0;
	n_deleted += 1;
      }

  ASSERT (em->file_header.segment_header_count >= n_deleted);
  em->file_header.segment_header_count -= n_deleted;

  return n_deleted;
}

#endif /* CLIB_UNIX */
