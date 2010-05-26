#include <clib/byte_order.h>
#include <clib/error.h>
#include <clib/hash.h>
#include <clib/vec.h>
#include <clib/elf.h>

always_inline u8
elf_swap_u8 (elf_main_t * em, u8 x)
{ return x; }

always_inline u16
elf_swap_u16 (elf_main_t * em, u16 x)
{ return em->need_byte_swap ? clib_byte_swap_u16 (x) : x; }

always_inline u32
elf_swap_u32 (elf_main_t * em, u32 x)
{ return em->need_byte_swap ? clib_byte_swap_u32 (x) : x; }

always_inline u64
elf_swap_u64 (elf_main_t * em, u64 x)
{ return em->need_byte_swap ? clib_byte_swap_u64 (x) : x; }

always_inline void
elf_swap_first_header (elf_main_t * em, elf_first_header_t * h)
{
  h->architecture = elf_swap_u16 (em, h->architecture);
  h->file_type = elf_swap_u16 (em, h->file_type);
  h->file_version = elf_swap_u32 (em, h->file_version);
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
    return format (s, "%=24s%=16s%=8s%=16s",
		   "Name", "Type", "Size", "Address");

  s = format (s, "%-24s%=16U%8Lx%16Lx%16Lx",
	      elf_section_name (em, es),
	      format_elf_section_type, h->type,
	      h->file_size,
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

  s = format (s, "machine: %U, file type/class %U/%U, data-encoding: %U, abi: %U version %d\n",
	      format_elf_architecture, em->first_header.architecture,
	      format_elf_file_type, em->first_header.file_type,
	      format_elf_file_class, em->first_header.file_class,
	      format_elf_data_encoding, em->first_header.data_encoding,
	      format_elf_abi, em->first_header.abi, em->first_header.abi_version);

  s = format (s, "  entry 0x%Lx, arch-flags 0x%x",
	      em->file_header.entry_point, em->file_header.flags);

  {
    elf_section_t * h, * copy;

    copy = 0;
    vec_foreach (h, em->sections)
      if (h->header.type != ELF_SECTION_UNUSED && h->header.type != ~0)
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

    s = format (s, "\nSegments %d at file offset 0x%Lx:\n",
		em->file_header.segment_header_count, em->file_header.segment_header_file_offset);
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

  return s;
}

static void
elf_parse_segment_header (elf_main_t * em, void * d, uword n)
{
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
elf_parse_section_header (elf_main_t * em, void * d, uword n)
{
  uword i;

  vec_resize (em->sections, n);

  for (i = 0; i < n; i++)
    {
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
    }
}

static void
add_symbol_table (elf_main_t * em, void * data, elf_section_t * s)
{
  elf_symbol_table_t * tab;
  elf32_symbol_t * sym32;
  elf64_symbol_t * sym64;
  uword i;

  vec_add2 (em->symbol_tables, tab, 1);

  if (em->first_header.file_class == ELF_64BIT)
    {
      tab->symbols = elf_section_contents (em, data, s - em->sections,
					   sizeof (tab->symbols[0]));
      tab->symbols = vec_dup (tab->symbols);
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

  if (s->header.link == 0)
    return;

  tab->string_table =
    elf_section_contents (em, data, s->header.link,
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
add_relocation_table (elf_main_t * em, void * data, elf_section_t * s)
{
  uword has_addend = s->header.type == ELF_SECTION_RELOCATION_ADD;
  elf_relocation_table_t * t;
  uword i;

  vec_add2 (em->relocation_tables, t, 1);
  t->section_index = s - em->sections;

  if (em->first_header.file_class == ELF_64BIT)
    {
      elf64_relocation_t * r;

      r = elf_section_contents (em, data, t->section_index, 
				sizeof (r[0]) + has_addend * sizeof (r->addend[0]));
      r = vec_dup (r);

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

void elf_parse_symbols (elf_main_t * em, void * data)
{
  elf_section_t * s;

  vec_foreach (s, em->sections)
    {
      switch (s->header.type)
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

clib_error_t *
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

  elf_parse_segment_header (em,
			    data + fh->segment_header_file_offset,
			    fh->segment_header_count);
  elf_parse_section_header (em,
			    data + fh->section_header_file_offset,
			    fh->section_header_count);

  /* Read in section string table. */
  elf_section_contents (em, data, fh->section_header_string_table_index,
			sizeof (u8));

  {
    elf_section_t * s;

    em->section_by_name
      = hash_create_string (/* # elts */ vec_len (em->sections),
			    /* sizeof of value */ sizeof (uword));

    vec_foreach (s, em->sections)
      {
	hash_set_mem (em->section_by_name,
		      elf_section_name (em, s),
		      s - em->sections);
      }
  }

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

  /* Read in all sections. */
  {
    elf_section_t * s;

    vec_foreach (s, em->sections)
      {
	if (s->header.type == ELF_SECTION_NO_BITS)
	  continue;
	elf_section_contents (em, data, s - em->sections,
			      sizeof (u8));
      }
  }

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
	switch (s->header.type)
	  {
	  case ~0:
	  case ELF_SECTION_UNUSED:
	  case ELF_SECTION_SYMBOL_TABLE:
	  case ELF_SECTION_STRING_TABLE:
	  case ELF_SECTION_NO_BITS:
	    continue;

	  default:
	    break;
	  }

	section_max_file_offset = clib_max (s->header.file_offset + s->header.file_size,
					    section_max_file_offset);

	if (fseek (f, s->header.file_offset, SEEK_SET) < 0)
	  return clib_error_return_unix (0, "fseek 0x%Lx", s->header.file_offset);

	if (fwrite (s->contents, vec_len (s->contents), 1, f) != 1)
	  {
	    error = clib_error_return_unix (0, "write %s section contents", elf_section_name (em, s));
	    goto error;
	  }
      }
  }

  /* Re-build section string and write it out. */
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
	if (em->file_header.section_header_string_table_index == s - em->sections)
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
  uword * p;
  elf_section_t * s;

  p = hash_get_mem (em->section_by_name, section_name);
  if (! p[0])
    return clib_error_return (0, "no such section `%s'", section_name);
  
  s = vec_elt_at_index (em->sections, p[0]);
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
