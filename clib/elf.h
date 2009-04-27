#ifndef included_clib_elf_h
#define included_clib_elf_h

#include <clib/format.h>
#include <clib/hash.h>
#include <clib/vec.h>

#define foreach_elf_file_class \
  _ (CLASS_NONE) _ (32BIT) _ (64BIT)

#define foreach_elf_data_encoding		\
  _ (ENCODING_NONE)				\
  _ (TWOS_COMPLEMENT_LITTLE_ENDIAN)		\
  _ (TWOS_COMPLEMENT_BIG_ENDIAN)

#define ELF_VERSION_NONE (0)
#define ELF_VERSION_CURRENT (1)

#define foreach_elf_abi				\
  _ (SYSV, 0)					\
  _ (HPUX, 1)					\
  _ (NETBSD, 2)					\
  _ (LINUX, 3)					\
  _ (SOLARIS, 6)				\
  _ (AIX, 7)					\
  _ (IRIX, 8)					\
  _ (FREEBSD, 9)				\
  _ (COMPAQ_TRU64, 10)				\
  _ (MODESTO, 11)				\
  _ (OPENBSD, 12)				\
  _ (ARM, 97)					\
  _ (STANDALONE, 255)

/* Legal values for type (object file type).  */
#define foreach_elf_file_type			\
  _ (NONE, 0)					\
  _ (RELOC, 1)					\
  _ (EXEC, 2)					\
  _ (SHARED, 3)					\
  _ (CORE, 4)					\
  _ (OS_SPECIFIC_LO, 0xfe00)			\
  _ (OS_SPECIFIC_HI, 0xfeff)			\
  _ (ARCH_SPECIFIC_LO, 0xff00)			\
  _ (ARCH_SPECIFIC_HI, 0xffff)

/* Legal values for architecture.  */
#define foreach_elf_architecture					\
  _ (NONE, 0)			/* No machine */			\
  _ (M32, 1)			/* AT&T WE 32100 */			\
  _ (SPARC, 2)			/* SUN SPARC */				\
  _ (386, 3)			/* Intel 80386 */			\
  _ (68K, 4)			/* Motorola m68k family */		\
  _ (88K, 5)			/* Motorola m88k family */		\
  _ (860, 7)			/* Intel 80860 */			\
  _ (MIPS, 8)			/* MIPS R3000 big-endian */		\
  _ (S370, 9)			/* IBM System/370 */			\
  _ (MIPS_RS3_LE, 10)		/* MIPS R3000 little-endian */		\
  _ (PARISC, 15)		/* HPPA */				\
  _ (VPP500, 17)		/* Fujitsu VPP500 */			\
  _ (SPARC32PLUS, 18)		/* Sun's "v8plus" */			\
  _ (960, 19)			/* Intel 80960 */			\
  _ (PPC, 20)			/* PowerPC */				\
  _ (PPC64, 21)			/* PowerPC 64-bit */			\
  _ (S390, 22)			/* IBM S390 */				\
  _ (V800, 36)			/* NEC V800 series */			\
  _ (FR20, 37)			/* Fujitsu FR20 */			\
  _ (RH32, 38)			/* TRW RH-32 */				\
  _ (RCE, 39)			/* Motorola RCE */			\
  _ (ARM, 40)			/* ARM */				\
  _ (FAKE_ALPHA, 41)		/* Digital Alpha */			\
  _ (SH, 42)			/* Hitachi SH */			\
  _ (SPARCV9, 43)		/* SPARC v9 64-bit */			\
  _ (TRICORE, 44)		/* Siemens Tricore */			\
  _ (ARC, 45)			/* Argonaut RISC Core */		\
  _ (H8_300, 46)		/* Hitachi H8/300 */			\
  _ (H8_300H, 47)		/* Hitachi H8/300H */			\
  _ (H8S, 48)			/* Hitachi H8S */			\
  _ (H8_500, 49)		/* Hitachi H8/500 */			\
  _ (IA_64, 50)			/* Intel Merced */			\
  _ (MIPS_X, 51)		/* Stanford MIPS-X */			\
  _ (COLDFIRE, 52)		/* Motorola Coldfire */			\
  _ (68HC12, 53)		/* Motorola M68HC12 */			\
  _ (MMA, 54)			/* Fujitsu MMA Multimedia Accel. */	\
  _ (PCP, 55)			/* Siemens PCP */			\
  _ (NCPU, 56)			/* Sony nCPU embeeded RISC */		\
  _ (NDR1, 57)			/* Denso NDR1 microprocessor */		\
  _ (STARCORE, 58)		/* Motorola Start*Core processor */	\
  _ (ME16, 59)			/* Toyota ME16 processor */		\
  _ (ST100, 60)			/* STMicroelectronic ST100  */		\
  _ (TINYJ, 61)			/* Advanced Logic Corp. Tinyj */	\
  _ (X86_64, 62)		/* AMD x86-64 architecture */		\
  _ (PDSP, 63)			/* Sony DSP Processor */		\
  _ (FX66, 66)			/* Siemens FX66 microcontroller */	\
  _ (ST9PLUS, 67)		/* STMicroelectronics ST9+ 8/16 mc */	\
  _ (ST7, 68)			/* STmicroelectronics ST7 8 bit mc */	\
  _ (68HC16, 69)		/* Motorola MC68HC16 */			\
  _ (68HC11, 70)		/* Motorola MC68HC11 */			\
  _ (68HC08, 71)		/* Motorola MC68HC08 */			\
  _ (68HC05, 72)		/* Motorola MC68HC05 */			\
  _ (SVX, 73)			/* Silicon Graphics SVx */		\
  _ (ST19, 74)			/* STMicroelectronics ST19 8 bit mc */	\
  _ (VAX, 75)			/* Digital VAX */			\
  _ (CRIS, 76)			/* Axis 32-bit embedded proc. */	\
  _ (JAVELIN, 77)		/* Infineon 32-bit embedded proc. */	\
  _ (FIREPATH, 78)		/* Element 14 64-bit DSP Processor */	\
  _ (ZSP, 79)			/* LSI Logic 16-bit DSP Processor */	\
  _ (MMIX, 80)			/* Knuth's 64-bit processor */		\
  _ (HUANY, 81)			/* Harvard machine-independent */	\
  _ (PRISM, 82)			/* SiTera Prism */			\
  _ (AVR, 83)			/* Atmel AVR 8-bit microcontroller */	\
  _ (FR30, 84)			/* Fujitsu FR30 */			\
  _ (D10V, 85)			/* Mitsubishi D10V */			\
  _ (D30V, 86)			/* Mitsubishi D30V */			\
  _ (V850, 87)			/* NEC v850 */				\
  _ (M32R, 88)			/* Mitsubishi M32R */			\
  _ (MN10300, 89)		/* Matsushita MN10300 */		\
  _ (MN10200, 90)		/* Matsushita MN10200 */		\
  _ (PJ, 91)			/* picoJava */				\
  _ (OPENRISC, 92)		/* OpenRISC 32-bit processor */		\
  _ (ARC_A5, 93)		/* ARC Cores Tangent-A5 */		\
  _ (XTENSA, 94)		/* Tensilica Xtensa Architecture */	\
  _ (ALPHA, 0x9026)

#define _(f) ELF_##f,

typedef enum {
  foreach_elf_file_class
  ELF_N_FILE_CLASS,
} elf_file_class_t;

typedef enum {
  foreach_elf_data_encoding
  ELF_N_DATA_ENCODING,
} elf_data_encoding_t;

#undef _

#define _(f,i) ELF_##f = i,

typedef enum {
  foreach_elf_abi
} elf_abi_t;

typedef enum {
  foreach_elf_file_type
} elf_file_type_t;

#undef _

typedef enum {
#define _(f,i) ELF_ARCH_##f = i,
  foreach_elf_architecture
#undef _
} elf_architecture_t;

typedef struct {
  /* 0x7f ELF */
  u8 magic[4];

  elf_file_class_t file_class : 8;
  elf_data_encoding_t data_encoding : 8;
  u8 file_version_ident;
  elf_abi_t abi : 8;
  u8 abi_version;

  u8 pad[7];

  elf_file_type_t file_type : 16;
  elf_architecture_t architecture : 16;

  u32 file_version;
} elf_first_header_t;

/* 32/64 bit file header following basic file header. */
#define foreach_elf32_file_header		\
  _ (u32, entry_point)				\
  _ (u32, segment_header_file_offset)		\
  _ (u32, section_header_file_offset)		\
  _ (u32, flags)				\
  _ (u16, n_bytes_this_header)			\
  _ (u16, segment_header_size)			\
  _ (u16, segment_header_count)			\
  _ (u16, section_header_size)			\
  _ (u16, section_header_count)			\
  _ (u16, section_header_string_table_index)

#define foreach_elf64_file_header		\
  _ (u64, entry_point)				\
  _ (u64, segment_header_file_offset)		\
  _ (u64, section_header_file_offset)		\
  _ (u32, flags)				\
  _ (u16, n_bytes_this_header)			\
  _ (u16, segment_header_size)			\
  _ (u16, segment_header_count)			\
  _ (u16, section_header_size)			\
  _ (u16, section_header_count)			\
  _ (u16, section_header_string_table_index)

/* Section header.  */
#define foreach_elf32_section_header		\
  _ (u32, name)					\
  _ (u32, type)					\
  _ (u32, flags)				\
  _ (u32, exec_address)				\
  _ (u32, file_offset)				\
  _ (u32, file_size)				\
  _ (u32, link)					\
  _ (u32, additional_info)			\
  _ (u32, align)				\
  _ (u32, entry_size)

#define foreach_elf64_section_header		\
  _ (u32, name)					\
  _ (u32, type)					\
  _ (u64, flags)				\
  _ (u64, exec_address)				\
  _ (u64, file_offset)				\
  _ (u64, file_size)				\
  _ (u32, link)					\
  _ (u32, additional_info)			\
  _ (u64, align)				\
  _ (u64, entry_size)

/* Program segment header.  */
#define foreach_elf32_segment_header		\
  _ (u32, type)					\
  _ (u32, file_offset)				\
  _ (u32, virtual_address)			\
  _ (u32, physical_address)			\
  _ (u32, file_size)				\
  _ (u32, memory_size)				\
  _ (u32, flags)				\
  _ (u32, align)

#define foreach_elf64_segment_header		\
  _ (u32, type)					\
  _ (u32, flags)				\
  _ (u64, file_offset)				\
  _ (u64, virtual_address)			\
  _ (u64, physical_address)			\
  _ (u64, file_size)				\
  _ (u64, memory_size)				\
  _ (u64, align)

/* Symbol table.  */
#define foreach_elf32_symbol_header		\
  _ (u32, name)					\
  _ (u32, value)				\
  _ (u32, size)					\
  /* binding upper 4 bits; type lower 4 bits */	\
  _ (u8, binding_and_type)			\
  _ (u8, visibility)				\
  _ (u16, section_index)

#define foreach_elf64_symbol_header		\
  _ (u32, name)					\
  _ (u8, binding_and_type)			\
  _ (u8, visibility)				\
  _ (u16, section_index)			\
  _ (u64, value)				\
  _ (u64, size)

#define _(t,f) t f;

typedef struct {
  foreach_elf32_file_header
} elf32_file_header_t;

typedef struct {
  foreach_elf64_file_header
} elf64_file_header_t;

typedef struct {
  foreach_elf32_section_header
} elf32_section_header_t;

typedef struct {
  foreach_elf64_section_header
} elf64_section_header_t;

typedef struct {
  foreach_elf32_segment_header
} elf32_segment_header_t;

typedef struct {
  foreach_elf64_segment_header
} elf64_segment_header_t;

typedef struct {
  foreach_elf32_symbol_header
} elf32_symbol_t;

typedef struct {
  foreach_elf64_symbol_header
} elf64_symbol_t;
#undef _

/* Special section names.  */
#define foreach_elf_special_section_name				\
  _ (UNDEFINED, 0)		/* Undefined section */			\
  _ (RESERVED_LO, 0xff00)	/* Start of reserved indices */		\
  _ (ARCH_SPECIFIC_LO, 0xff00)						\
  _ (ARCH_SPECIFIC_HI, 0xff1f)						\
  _ (OS_SPECIFIC_LO, 0xff20)						\
  _ (OS_SPECIFIC_HI, 0xff3f)						\
  _ (ABSOLUTE, 0xfff1)		/* Associated symbol is absolute */	\
  _ (COMMON, 0xfff2)		/* Associated symbol is common */	\
  _ (XINDEX, 0xffff)		/* Index is in extra table.  */		\
  _ (RESERVED_HI, 0xffff)	/* End of reserved indices */

/* Section types. */
#define foreach_elf_section_type					\
  _ (UNUSED, 0)								\
  _ (PROGRAM_DATA, 1)							\
  _ (SYMBOL_TABLE, 2)							\
  _ (STRING_TABLE, 3)							\
  _ (RELOCATION_ADD, 4)							\
  _ (SYMBOL_TABLE_HASH, 5)						\
  _ (DYNAMIC, 6)		/* Dynamic linking information */	\
  _ (NOTE, 7)			/* Notes */				\
  _ (NO_BITS, 8)		/* Program space with no data (bss) */	\
  _ (RELOCATION, 9)		/* Relocation entries, no addends */	\
  _ (DYNAMIC_SYMBOL_TABLE, 11)	/* Dynamic linker symbol table */	\
  _ (INIT_ARRAY, 14)		/* Array of constructors */		\
  _ (FINI_ARRAY, 15)		/* Array of destructors */		\
  _ (PREINIT_ARRAY, 16)		/* Array of pre-constructors */		\
  _ (GROUP, 17)			/* Section group */			\
  _ (SYMTAB_SHNDX, 18)		/* Extended section indices */		\
  _ (OS_SPECIFIC_LO, 0x60000000) /* Start OS-specific */		\
  _ (GNU_LIBLIST, 0x6ffffff7)	/* Prelink library list */		\
  _ (CHECKSUM, 0x6ffffff8)	/* Checksum for DSO content.  */	\
  _ (SUNW_MOVE, 0x6ffffffa)						\
  _ (SUNW_COMDAT, 0x6ffffffb)						\
  _ (SUNW_SYMINFO, 0x6ffffffc)						\
  _ (GNU_VERDEF, 0x6ffffffd)	/* Version definition section.  */	\
  _ (GNU_VERNEED, 0x6ffffffe) /* Version needs section.  */		\
  _ (GNU_VERSYM, 0x6fffffff)	/* Version symbol table.  */		\
  _ (ARCH_SPECIFIC_LO, 0x70000000) /* Start of processor-specific */	\
  _ (ARCH_SPECIFIC_HI, 0x7fffffff) /* End of processor-specific */	\
  _ (APP_SPECIFIC_LO, 0x80000000) /* Start of application-specific */	\
  _ (APP_SPECIFIC_HI, 0x8fffffff) /* End of application-specific */

/* Section flags. */
#define foreach_elf_section_flag		\
  _ (WRITE, 0)					\
  _ (ALLOC, 1)					\
  _ (EXEC, 2)					\
  _ (MERGE, 3)					\
  _ (STRING_TABLE, 5)				\
  _ (INFO_LINK, 6)				\
  _ (PRESERVE_LINK_ORDER, 7)			\
  _ (OS_NON_CONFORMING, 8)			\
  _ (GROUP, 9)					\
  _ (TLS, 10)					\
  _ (OS_SPECIFIC_LO, 20)			\
  _ (OS_SPECIFIC_HI, 27)			\
  _ (ARCH_SPECIFIC_LO, 28)			\
  _ (ARCH_SPECIFIC_HI, 31)

typedef enum {
#define _(f,i) ELF_SECTION_##f = i,
  foreach_elf_section_type
#undef _
  ELF_SECTION_OS_SPECIFIC_HI = 0x6fffffff,
} elf_section_type_t;

typedef enum {
#define _(f,i) ELF_SECTION_FLAG_BIT_##f = i,
  foreach_elf_section_flag
#undef _
} elf_section_flag_bit_t;

typedef enum {
#define _(f,i) ELF_SECTION_FLAG_##f = 1 << ELF_SECTION_FLAG_BIT_##f,
  foreach_elf_section_flag
#undef _
} elf_section_flag_t;

/* Symbol bindings (upper 4 bits of binding_and_type). */
#define foreach_elf_symbol_binding					\
  _ (LOCAL, 0)			/* Local symbol */			\
  _ (GLOBAL, 1)			/* Global symbol */			\
  _ (WEAK, 2)			/* Weak symbol */			\
  _ (OS_SPECIFIC_LO, 10)	/* Start of OS-specific */		\
  _ (OS_SPECIFIC_HI, 12)	/* End of OS-specific */		\
  _ (ARCH_SPECIFIC_LO, 13)	/* Start of processor-specific */	\
  _ (ARCH_SPECIFIC_HI, 15)	/* End of processor-specific */

/* Symbol types (lower 4 bits of binding_and_type). */
#define foreach_elf_symbol_type						\
  _ (NONE, 0)								\
  _ (DATA, 1)			/* Symbol is a data object */		\
  _ (CODE, 2)			/* Symbol is a code object */		\
  _ (SECTION, 3)		/* Symbol associated with a section */	\
  _ (FILE, 4)			/* Symbol's name is file name */	\
  _ (COMMON, 5)			/* Symbol is a common data object */	\
  _ (TLS, 6)			/* Symbol is thread-local data */	\
  _ (OS_SPECIFIC_LO, 10)	/* Start of OS-specific */		\
  _ (OS_SPECIFIC_HI, 12)	/* End of OS-specific */		\
  _ (ARCH_SPECIFIC_LO, 13)	/* Start of processor-specific */	\
  _ (ARCH_SPECIFIC_HI, 15)	/* End of processor-specific */

/* Symbol visibility. */
#define foreach_elf_symbol_visibility					\
  _ (DEFAULT, 0)		/* Default symbol visibility rules */	\
  _ (INTERNAL, 1)		/* Processor specific hidden class */	\
  _ (HIDDEN, 2)			/* Unavailable in other modules */	\
  _ (PROTECTED, 3)		/* Not preemptible, not exported */

/* The syminfo section if available contains additional
   information about every dynamic symbol.  */
typedef struct {
  u16 bound_to;
  u16 flags;
} elf_symbol_info_t;

/* Possible values for bound_to.  */
#define foreach_elf_symbol_info_bound_to			\
  _ (SELF, 0xffff)		/* Symbol bound to self */	\
  _ (PARENT, 0xfffe)		/* Symbol bound to parent */	\
  _ (RESERVED_LO, 0xff00)					\
  _ (RESERVED_HI, 0xffff)

/* Symbol info flags. */
#define foreach_elf_symbol_info_flags					\
  _ (DIRECT)			/* Direct bound symbol */		\
  _ (PASS_THRU)			/* Pass-thru symbol for translator */	\
  _ (COPY)			/* Symbol is a copy-reloc */		\
  _ (LAZY_LOAD)			/* Symbol bound to object to be lazy loaded */

/* Relocation table entry with/without addend. */
typedef struct {
  u32 address;
  u32 symbol_and_type;		/* high 24 symbol, low 8 type. */
  i32 addend[0];
} elf32_relocation_t;

typedef struct {
  u64 address;
  u64 symbol_and_type;		/* high 32 symbol, low 32 type. */
  i64 addend[0];
} elf64_relocation_t;

#define elf_relocation_next(r,type)					\
  ((void *) ((r) + 1)							\
   + ((type) == ELF_SECTION_RELOCATION_ADD ? sizeof ((r)->addend[0]) : 0))

/* Segment type. */
#define foreach_elf_segment_type					\
  _ (UNUSED, 0)								\
  _ (LOAD, 1)			/* Loadable program segment */		\
  _ (DYNAMIC, 2)		/* Dynamic linking information */	\
  _ (INTERP, 3)			/* Program interpreter */		\
  _ (NOTE, 4)			/* Auxiliary information */		\
  _ (SEGMENT_TABLE, 6)		/* Entry for header table itself */	\
  _ (TLS, 7)			/* Thread-local storage segment */	\
  _ (OS_SPECIFIC_LO, 0x60000000) /* Start of OS-specific */		\
  _ (GNU_EH_FRAME, 0x6474e550)	/* GCC .eh_frame_hdr segment */		\
  _ (GNU_STACK, 0x6474e551)	/* Indicates stack executability */	\
  _ (GNU_RELRO, 0x6474e552)	/* Read-only after relocation */	\
  _ (SUNW_BSS, 0x6ffffffa)	/* Sun specific BSS */			\
  _ (SUNW_STACK, 0x6ffffffb)	/* Sun specific stack */		\
  _ (OS_SPECIFIC_HI, 0x6fffffff) /* End of OS-specific */		\
  _ (ARCH_SPECIFIC_LO, 0x70000000) /* Start of processor-specific */	\
  _ (ARCH_SPECIFIC_HI, 0x7fffffff) /* End of processor-specific */

/* Segment flags. */
#define foreach_elf_segment_flag		\
  _ (EXEC, 0)					\
  _ (WRITE, 1)					\
  _ (READ, 2)					\
  _ (OS_SPECIFIC_LO, 20)			\
  _ (OS_SPECIFIC_HI, 27)			\
  _ (ARCH_SPECIFIC_LO, 28)			\
  _ (ARCH_SPECIFIC_HI, 31)

typedef enum {
#define _(f,i) ELF_SEGMENT_##f = i,
  foreach_elf_segment_type
#undef _
} elf_segment_type_t;

typedef enum {
#define _(f,i) ELF_SEGMENT_FLAG_BIT_##f = i,
  foreach_elf_segment_flag
#undef _
} elf_segment_flag_bit_t;

typedef enum {
#define _(f,i) ELF_SEGMENT_FLAG_##f = 1 << ELF_SEGMENT_FLAG_BIT_##f,
  foreach_elf_segment_flag
#undef _
} elf_segment_flag_t;

#define foreach_elf32_dynamic_entry_header	\
  _ (u32, type)					\
  _ (u32, data)

#define foreach_elf64_dynamic_entry_header	\
  _ (u64, type)					\
  _ (u64, data)
  
#define _(t,f) t f;

typedef struct {
  foreach_elf32_dynamic_entry_header
} elf32_dynamic_entry_t;

typedef struct {
  foreach_elf64_dynamic_entry_header
} elf64_dynamic_entry_t;

#undef _

#define foreach_elf_dynamic_entry_type					\
  _ (END, 0)			/* Marks end of dynamic section */	\
  _ (NEEDED, 1)			/* Name of needed library */		\
  _ (PLTRELSZ, 2)		/* Size in bytes of PLT relocs */	\
  _ (PLTGOT, 3)			/* Processor defined value */		\
  _ (HASH, 4)			/* Address of symbol hash table */	\
  _ (STRTAB, 5)			/* Address of string table */		\
  _ (SYMTAB, 6)			/* Address of symbol table */		\
  _ (RELA, 7)			/* Address of Rela relocs */		\
  _ (RELASZ, 8)			/* Total size of Rela relocs */		\
  _ (RELAENT, 9)		/* Size of one Rela reloc */		\
  _ (STRSZ, 10)			/* Size of string table */		\
  _ (SYMENT, 11)		/* Size of one symbol table entry */	\
  _ (INIT, 12)			/* Address of init function */		\
  _ (FINI, 13)			/* Address of termination function */	\
  _ (SONAME, 14)		/* Name of shared object */		\
  _ (RPATH, 15)			/* Library search path (deprecated) */	\
  _ (SYMBOLIC, 16)		/* Start symbol search here */		\
  _ (REL, 17)			/* Address of Rel relocs */		\
  _ (RELSZ, 18)			/* Total size of Rel relocs */		\
  _ (RELENT, 19)		/* Size of one Rel reloc */		\
  _ (PLTREL, 20)		/* Type of reloc in PLT */		\
  _ (DEBUG, 21)			/* For debugging; unspecified */	\
  _ (TEXTREL, 22)		/* Reloc might modify .text */		\
  _ (JMPREL, 23)		/* Address of PLT relocs */		\
  _ (BIND_NOW, 24)		/* Process relocations of object */	\
  _ (INIT_ARRAY, 25)		/* Array with addresses of init fct */	\
  _ (FINI_ARRAY, 26)		/* Array with addresses of fini fct */	\
  _ (INIT_ARRAYSZ, 27)		/* Size in bytes of DT_INIT_ARRAY */	\
  _ (FINI_ARRAYSZ, 28)		/* Size in bytes of DT_FINI_ARRAY */	\
  _ (RUNPATH, 29)		/* Library search path */		\
  _ (FLAGS, 30)			/* Flags for object being loaded */	\
  _ (ENCODING, 32)		/* Start of encoded range */		\
  _ (PREINIT_ARRAY, 32)		/* Array with addresses of fns */	\
  _ (PREINIT_ARRAY_SIZE, 33)	/* Size of PREINIT_ARRAY in bytes. */	\
  _ (OS_SPECIFIC_LO, 0x60000000)					\
  _ (OS_SPECIFIC_HI, 0x6fffffff)					\
  _ (ARCH_SPECIFIC_LO, 0x70000000)					\
  _ (ARCH_SPECIFIC_HI, 0x7fffffff)

#define DT_GNU_PRELINKED 0x6ffffdf5	/* Prelinking timestamp */
#define DT_GNU_CONFLICTSZ 0x6ffffdf6	/* Size of conflict section */
#define DT_GNU_LIBLISTSZ 0x6ffffdf7	/* Size of library list */
#define DT_CHECKSUM	0x6ffffdf8
#define DT_PLTPADSZ	0x6ffffdf9
#define DT_MOVEENT	0x6ffffdfa
#define DT_MOVESZ	0x6ffffdfb
#define DT_FEATURE_1	0x6ffffdfc	/* Feature selection (DTF_*).  */
#define DT_POSFLAG_1	0x6ffffdfd	/* Flags for following entries.  */
#define DT_SYMINSZ	0x6ffffdfe	/* Size of syminfo table (in bytes) */
#define DT_SYMINENT	0x6ffffdff	/* Entry size of syminfo */

#define DT_GNU_CONFLICT	0x6ffffef8	/* Start of conflict section */
#define DT_GNU_LIBLIST	0x6ffffef9	/* Library list */
#define DT_CONFIG	0x6ffffefa	/* Configuration information.  */
#define DT_DEPAUDIT	0x6ffffefb	/* Dependency auditing.  */
#define DT_AUDIT	0x6ffffefc	/* Object auditing.  */
#define	DT_PLTPAD	0x6ffffefd	/* PLT padding.  */
#define	DT_MOVETAB	0x6ffffefe	/* Move table.  */
#define DT_SYMINFO	0x6ffffeff	/* Syminfo table.  */

/* The versioning entry types.  The next are defined as part of the
   GNU extension.  */
#define DT_VERSYM	0x6ffffff0

#define DT_RELACOUNT	0x6ffffff9
#define DT_RELCOUNT	0x6ffffffa

/* These were chosen by Sun.  */
#define DT_FLAGS_1	0x6ffffffb	/* State flags, see DF_1_* below.  */
#define	DT_VERDEF	0x6ffffffc	/* Address of version definition
					   table */
#define	DT_VERDEFNUM	0x6ffffffd	/* Number of version definitions */
#define	DT_VERNEED	0x6ffffffe	/* Address of table with needed
					   versions */
#define	DT_VERNEEDNUM	0x6fffffff	/* Number of needed versions */

/* Sun added these machine-independent extensions in the "processor-specific"
   range.  Be compatible.  */
#define DT_AUXILIARY    0x7ffffffd      /* Shared object to load before self */
#define DT_FILTER       0x7fffffff      /* Shared object to get values from */

/* Values of `d_un.d_val' in the DT_FLAGS entry.  */
#define DF_ORIGIN	0x00000001	/* Object may use DF_ORIGIN */
#define DF_SYMBOLIC	0x00000002	/* Symbol resolutions starts here */
#define DF_TEXTREL	0x00000004	/* Object contains text relocations */
#define DF_BIND_NOW	0x00000008	/* No lazy binding for this object */
#define DF_STATIC_TLS	0x00000010	/* Module uses the static TLS model */

/* State flags selectable in the `d_un.d_val' element of the DT_FLAGS_1
   entry in the dynamic section.  */
#define DF_1_NOW	0x00000001	/* Set RTLD_NOW for this object.  */
#define DF_1_GLOBAL	0x00000002	/* Set RTLD_GLOBAL for this object.  */
#define DF_1_GROUP	0x00000004	/* Set RTLD_GROUP for this object.  */
#define DF_1_NODELETE	0x00000008	/* Set RTLD_NODELETE for this object.*/
#define DF_1_LOADFLTR	0x00000010	/* Trigger filtee loading at runtime.*/
#define DF_1_INITFIRST	0x00000020	/* Set RTLD_INITFIRST for this object*/
#define DF_1_NOOPEN	0x00000040	/* Set RTLD_NOOPEN for this object.  */
#define DF_1_ORIGIN	0x00000080	/* $ORIGIN must be handled.  */
#define DF_1_DIRECT	0x00000100	/* Direct binding enabled.  */
#define DF_1_TRANS	0x00000200
#define DF_1_INTERPOSE	0x00000400	/* Object is used to interpose.  */
#define DF_1_NODEFLIB	0x00000800	/* Ignore default lib search path.  */
#define DF_1_NODUMP	0x00001000	/* Object can't be dldump'ed.  */
#define DF_1_CONFALT	0x00002000	/* Configuration alternative created.*/
#define DF_1_ENDFILTEE	0x00004000	/* Filtee terminates filters search. */
#define	DF_1_DISPRELDNE	0x00008000	/* Disp reloc applied at build time. */
#define	DF_1_DISPRELPND	0x00010000	/* Disp reloc applied at run-time.  */

/* Flags for the feature selection in DT_FEATURE_1.  */
#define DTF_1_PARINIT	0x00000001
#define DTF_1_CONFEXP	0x00000002

/* Flags in the DT_POSFLAG_1 entry effecting only the next DT_* entry.  */
#define DF_P1_LAZYLOAD	0x00000001	/* Lazyload following object.  */
#define DF_P1_GROUPPERM	0x00000002	/* Symbols from next object are not
					   generally available.  */

/* Version definition sections.  */

typedef struct
{
  u16 version;
  u16 flags;
  u16 version_index;
  u16 aux_count;
  u32 name_hash;
  u32 aux_offset;
  u32 next_offset;
} elf_version_t;

typedef struct {
  u32 name;
  u32 next_offset;
} elf_version_aux_t;

/* Version definition flags. */
#define ELF_VERSION_FILE (1 << 0) /* Version definition of file itself */
#define ELF_VERSION_WEAK (1 << 1) /* Weak version identifier */

/* Version symbol index. */
#define	ELF_VERSION_LOCAL 0	/* Symbol is local.  */
#define	ELF_VERSION_GLOBAL 1	/* Symbol is global.  */
#define	ELF_VERSION_RESERVED_LO	0xff00 /* Beginning of reserved entries.  */
#define	ELF_VERSION_ELIMINATE	0xff01	/* Symbol is to be eliminated.  */

/* Version dependency section.  */

typedef struct
{
  u16 version;
  u16 dep_aux_count;
  u32 file_name_offset;
  u32 dep_aux_offset;
  u32 next_offset;
} elf_version_dependency_t;

typedef struct {
  u32 hash;
  u16 flags;
  u16 unused;
  u32 name;
  u32 next_offset;
} elf_version_dependency_aux_t;

/* Note section contents.  Each entry in the note section begins with
   a header of a fixed form.  */

typedef struct
{
  u32 name_size;
  u32 descriptor_size;
  u32 type;
} elf_note_t;

/* Known names of notes.  */

/* Solaris entries in the note section have this name.  */
#define ELF_NOTE_SOLARIS	"SUNW Solaris"

/* Note entries for GNU systems have this name.  */
#define ELF_NOTE_GNU		"GNU"


/* Defined types of notes for Solaris.  */

/* Value of descriptor (one word) is desired pagesize for the binary.  */
#define ELF_NOTE_PAGESIZE_HINT	1


/* Defined note types for GNU systems.  */

/* ABI information.  The descriptor consists of words:
   word 0: OS descriptor
   word 1: major version of the ABI
   word 2: minor version of the ABI
   word 3: subminor version of the ABI
*/
#ifndef ELF_NOTE_ABI
#define ELF_NOTE_ABI		1
#endif

/* Known OSes.  These value can appear in word 0 of an ELF_NOTE_ABI
   note section entry.  */
#define ELF_NOTE_OS_LINUX	0
#define ELF_NOTE_OS_GNU		1
#define ELF_NOTE_OS_SOLARIS2	2
#define ELF_NOTE_OS_FREEBSD	3

/* AMD x86-64 relocations.  */
#define foreach_elf_x86_64_relocation_type				\
  _ (NONE, 0)			/* No reloc */				\
  _ (DIRECT_64, 1)		/* Direct 64 bit  */			\
  _ (PC_REL_I32, 2)	        /* PC relative 32 bit signed */		\
  _ (GOT_REL_32, 3)		/* 32 bit GOT entry */			\
  _ (PLT_REL_32, 4)		/* 32 bit PLT address */		\
  _ (COPY, 5)			/* Copy symbol at runtime */		\
  _ (CREATE_GOT, 6)		/* Create GOT entry */			\
  _ (CREATE_PLT, 7)		/* Create PLT entry */			\
  _ (RELATIVE, 8)		/* Adjust by program base */		\
  _ (PC_REL_I32_GOT, 9)	/* 32 bit PC relative offset to GOT */		\
  _ (DIRECT_U32, 10)		/* Direct 32 bit zero extended */	\
  _ (DIRECT_I32, 11)		/* Direct 32 bit sign extended */	\
  _ (DIRECT_U16, 12)		/* Direct 16 bit zero extended */	\
  _ (PC_REL_I16, 13)	/* 16 bit sign extended pc relative */		\
  _ (DIRECT_I8, 14)		/* Direct 8 bit sign extended  */	\
  _ (PC_REL_I8, 15)	/* 8 bit sign extended pc relative */		\
  _ (DTPMOD64, 16)		/* ID of module containing symbol */	\
  _ (DTPOFF64, 17)		/* Offset in module's TLS block */	\
  _ (TPOFF64, 18)		/* Offset in initial TLS block */	\
  _ (TLSGD, 19)	/* 32 bit signed PC relative offset to two GOT entries for GD symbol */	\
  _ (TLSLD, 20) /* 32 bit signed PC relative offset to two GOT entries for LD symbol */	\
  _ (DTPOFF32, 21)		/* Offset in TLS block */		\
  _ (GOTTPOFF, 22) /* 32 bit signed PC relative offset to GOT entry for IE symbol */ \
  _ (TPOFF32, 23)		/* Offset in initial TLS, block) */

typedef struct {
  elf64_symbol_t * symbols;

  u8 * string_table;

  uword * symbol_by_name;
} elf_symbol_table_t;

static always_inline void
elf_symbol_table_free (elf_symbol_table_t * s)
{
  vec_free (s->symbols);
  vec_free (s->string_table);
  hash_free (s->symbol_by_name);
}

static always_inline u8 *
elf_symbol_name (elf_symbol_table_t * t, elf64_symbol_t * sym)
{ return vec_elt_at_index (t->string_table, sym->name); }

typedef struct {
  elf64_relocation_t * relocations;

  u32 section_index;
} elf_relocation_table_t;

static always_inline void
elf_relocation_table_free (elf_relocation_table_t * r)
{
  vec_free (r->relocations);
}

typedef struct {
  elf64_section_header_t header;

  void * contents;
} elf_section_t;

typedef struct {
  elf64_segment_header_t header;

  u8 * contents;
} elf_segment_t;

typedef struct {
  u8 need_byte_swap;

  elf_first_header_t first_header;

  elf64_file_header_t file_header;

  elf_segment_t * segments;

  elf_section_t * sections;

  uword * section_by_name;

  elf_symbol_table_t * symbol_tables;
  elf_relocation_table_t * relocation_tables;
} elf_main_t;

static always_inline void
elf_main_init (elf_main_t * em)
{
  memset (em, 0, sizeof (em[0]));
}

static always_inline void
elf_main_free (elf_main_t * em)
{
  uword i;

  for (i = 0; i < vec_len (em->segments); i++)
    vec_free (em->segments[i].contents);
  vec_free (em->segments);

  for (i = 0; i < vec_len (em->sections); i++)
    vec_free (em->sections[i].contents);
  vec_free (em->sections);

  hash_free (em->section_by_name);
  for (i = 0; i < vec_len (em->symbol_tables); i++)
    elf_symbol_table_free (em->symbol_tables + i);
  for (i = 0; i < vec_len (em->relocation_tables); i++)
    elf_relocation_table_free (em->relocation_tables + i);
}

static always_inline void *
elf_get_contents (elf_main_t * em,
		  void * data,
		  uword file_offset,
		  uword file_size,
		  uword elt_size)
{
  u8 * v = 0;

  if (file_size > 0)
    {
      vec_add (v, data + file_offset, file_size);
      ASSERT (vec_len (v) % elt_size == 0);
      _vec_len (v) /= elt_size;
    }

  return v;
}

static always_inline void *
elf_section_contents (elf_main_t * em,
		      void * data,
		      uword section_index,
		      uword elt_size)
{
  elf_section_t * s;
  s = vec_elt_at_index (em->sections, section_index);
  if (! s->contents)
    s->contents = elf_get_contents (em, data, s->header.file_offset, s->header.file_size, elt_size);
  return s->contents;
}

static always_inline void *
elf_segment_contents (elf_main_t * em,
		      void * data,
		      uword segment_index)
{
  elf_segment_t * s;
  s = vec_elt_at_index (em->segments, segment_index);
  if (! s->contents)
    s->contents = elf_get_contents (em, data, s->header.file_offset, s->header.file_size, sizeof (u8));
  return s->contents;
}

static always_inline u8 *
elf_section_name (elf_main_t * em, elf_section_t * s)
{
  elf_section_t * es = vec_elt_at_index (em->sections, em->file_header.section_header_string_table_index);
  return vec_elt_at_index (es->contents, s->header.name);
}

format_function_t format_elf_main;

/* Read headers: sections + segments but no symbols/relocations. */
clib_error_t *
elf_parse (elf_main_t * em,
	   void * data,
	   uword data_bytes);

/* Read symbols & relocations. */
void elf_parse_symbols (elf_main_t * em, void * data);

clib_error_t * elf_read_file (elf_main_t * em, char * file_name);
clib_error_t * elf_write_file (elf_main_t * em, char * file_name);
clib_error_t * elf_delete_named_section (elf_main_t * em, char * section_name);
void
elf_create_section_with_contents (elf_main_t * em,
				  char * section_name,
				  elf64_section_header_t * header,
				  void * contents,
				  uword n_content_bytes);
uword elf_delete_segment_with_type (elf_main_t * em, elf_segment_type_t segment_type);

#endif /* included_clib_elf_h */
