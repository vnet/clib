#ifndef included_asm_x86_h
#define included_asm_x86_h

#include <clib/format.h>

typedef union {
  struct {
    u8 code;
    u8 type;
  };
  u8 data[2];
} x86_insn_operand_t;

typedef struct {
  char * name;

  x86_insn_operand_t operands[3];

  u16 flags;
#define X86_INSN_FLAG_MODRM_REG_GROUP	(1 << 0)
#define X86_INSN_FLAG_SSE_GROUP		(1 << 1)
#define X86_INSN_FLAG_MODRM		(1 << 2)
#define X86_INSN_FLAG_DEFAULT_64_BIT	(1 << 3)
#define X86_INSN_FLAG_GROUP(n)		((n) << 8)
#define X86_INSN_FLAG_GET_GROUP(f)	((f) >> 8)
} x86_insn_t;

static always_inline uword
x86_insn_operand_is_valid (x86_insn_t * i, uword o)
{
  ASSERT (o < ARRAY_LEN (i->operands));
  return i->operands[o].code != '_';
}

typedef struct {
  /* Registers in instruction.
     [0] is modrm reg field
     [1] is base reg
     [2] is index reg. */
  u8 regs[3];

  /* Scale for index register. */
  u8 log2_index_scale : 2;
  u8 log2_effective_operand_bytes : 3;
  u8 log2_effective_address_bytes : 3;

  i32 displacement;

  u32 flags;

  i64 immediate;

  x86_insn_t insn;
} x86_insn_parse_t;
		
/* Parser flags. */
#define X86_INSN_PARSE_32_BIT (1 << 0)	/* operand size 32 */
#define X86_INSN_PARSE_64_BIT (1 << 1)	/* long mode */

u8 * x86_insn_parse (x86_insn_parse_t * x, u32 parse_flags, u8 * code_start);
format_function_t format_x86_insn_parse;

#endif /* included_asm_x86_h */
