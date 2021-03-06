/*
  Copyright (c) 2005 Eliot Dresselhaus

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
	
#if defined(__x86_64__)
	.global clib_setjmp
	.align 4
	.type clib_setjmp, @function
clib_setjmp:
	movq %rbx, 8*0(%rdi)
	movq %rbp, 8*1(%rdi)
	movq %r12, 8*2(%rdi)
	movq %r13, 8*3(%rdi)
	movq %r14, 8*4(%rdi)
	movq %r15, 8*5(%rdi)

	/* Save SP after return. */
	leaq 8(%rsp), %rdx
	movq %rdx, 8*6(%rdi)
	
	/* Save PC we are returning to from stack frame. */
	movq 0(%rsp), %rax
	movq %rax, 8*7(%rdi)
	
	/* Give back user's return value. */
	movq %rsi, %rax
	ret
	
	.global clib_longjmp
	.align 4
	.type clib_longjmp, @function
clib_longjmp:	
        /* Restore regs. */
	movq 8*0(%rdi), %rbx
	movq 8*1(%rdi), %rbp
	movq 8*2(%rdi), %r12
	movq 8*3(%rdi), %r13
	movq 8*4(%rdi), %r14
	movq 8*5(%rdi), %r15
	movq 8*6(%rdi), %rsp
	movq 8*7(%rdi), %rdx
	
	/* Give back user's return value. */
	movq %rsi, %rax
	
	/* Away we go. */
	jmpq *%rdx	
	
	.global clib_calljmp
	.align 4
	.type clib_calljmp, @function
clib_calljmp:
	/* Make sure stack is 16-byte aligned. */
	movq %rdx, %rax
	andq $0xf, %rax
	subq %rax, %rdx
	
	/* Get return address. */
	pop %rax
	
	/* Switch to new stack. */
	xchgq %rsp, %rdx
	
	/* Save return address on new stack. */
	push %rax
	
	/* Save old stack pointer on new stack. */
	push %rdx
	
	/* Get function. */
	movq %rdi, %rdx
	
	/* Move argument into place. */
	movq %rsi, %rdi
	
	/* Away we go. */
	callq *%rdx
	
	/* Switch back to old stack. */
	movq 8(%rsp), %rdx
	movq 0(%rsp), %rcx
	xchgq %rcx, %rsp
	
	/* Return to caller. */
	jmpq *%rdx

#elif defined(i386)
	.global clib_setjmp
	.align 4
	.type clib_setjmp, @function
clib_setjmp:
	movl 4(%esp), %ecx
	
	movl %ebp, 4*0(%ecx)
	movl %ebx, 4*1(%ecx)
	movl %edi, 4*2(%ecx)
	movl %esi, 4*3(%ecx)

	/* Save SP after return. */
	leal 4(%esp), %edx
	movl %edx, 4*4(%ecx)
	
	/* Save PC we are returning to from stack frame. */
	movl 0(%esp), %eax
	movl %eax, 4*5(%ecx)
	
	/* Give back user's return value. */
	movl 8(%esp), %eax
	ret
	
	.global clib_longjmp
	.align 4
	.type clib_longjmp, @function
clib_longjmp:	
	movl 4(%esp), %ecx
	
	/* Give back user's return value. */
	movl 8(%esp), %eax
	
        /* Restore regs. */
	movl 4*0(%ecx), %ebp
	movl 4*1(%ecx), %ebx
	movl 4*2(%ecx), %edi
	movl 4*3(%ecx), %esi
	movl 4*4(%ecx), %esp
	movl 4*5(%ecx), %edx
	
	/* Away we go. */
	jmp *%edx	
	
	.global clib_calljmp
	.align 4
	.type clib_calljmp, @function
clib_calljmp:	
	/* Get new stack pointer. */
	movl 12(%esp), %edx
	
	/* Switch stacks. */
	xchgl %esp, %edx
	
	/* Save old stack pointer on new stack. */
	sub $8, %esp
	movl %edx, 4(%esp)
	
	/* Put function argument in stack frame. */
	movl 8(%edx), %eax
	movl %eax, 0(%esp)
	
	/* Get function. */
	movl 4(%edx), %eax
	
	/* Away we go. */
	call *%eax
	
	/* Switch back to old stack. */
	movl 4(%esp), %edx
	xchgl %edx, %esp
	
	/* Return to caller. */
	ret
	
#elif defined(__SPU__)
	
#elif defined(__powerpc64__)
	
	.text

#define _prologue(n)				\
    .align 2 ;					\
    .globl n, .##n ;				\
    .section ".opd", "aw" ;			\
    .align 3 ;					\
n:  .quad .##n, .TOC.@tocbase, 0 ;		\
    .previous ;					\
    .size n, 24 ;				\
    .type .##n, @function ;			\
.##n:

#define _foreach_14_31							\
_ (14, 0)  _ (15, 1)  _ (16, 2)  _ (17, 3)  _ (18, 4)  _ (19, 5)	\
_ (20, 6)  _ (21, 7)  _ (22, 8)  _ (23, 9)  _ (24, 10) _ (25, 11)	\
_ (26, 12) _ (27, 13) _ (28, 14) _ (29, 15) _ (30, 16) _ (31, 17)

#define _foreach_20_31						\
_ (20, 0) _ (21, 1) _ (22, 2) _ (23, 3) _ (24, 4)  _ (25, 5)	\
_ (26, 6) _ (27, 7) _ (28, 8) _ (29, 9) _ (30, 10) _ (31, 11)
	
#ifdef __ALTIVEC__
#define CLIB_POWERPC_ALTIVEC_N_REGS 12
#else
#define CLIB_POWERPC_ALTIVEC_N_REGS 0
#endif

_prologue (clib_setjmp)
	mflr 0
	std 0, 8*0(3)
	std 1, 8*1(3)
	std 2, 8*2(3)
	mfcr 0
	std 0, 8*3(3)
	mfspr 0, 256
	stw 0, 8*4(3)
	
	/* gprs 14 - 31 */
#define _(a,b) std a, 8*((b) + 4 + 18*0)(3) ; 
	_foreach_14_31
#undef _
	
	/* fprs 14 - 31 */
#define _(a,b) stfd a, 8*((b) + 4 + 18*1)(3) ; 
	_foreach_14_31
#undef _

#if CLIB_POWERPC_ALTIVEC_N_REGS > 0
	/* vrs 20 - 31 */
	li 5, 8*(4 + 18*2)
#define _(a,b) stvx a, 5, 3 ; addi 5, 5, 16 ;
	_foreach_20_31
#undef _
#endif /* CLIB_POWERPC_ALTIVEC_N_REGS > 0 */
	
	/* Return value. */
	mr 3, 4
	
	blr
	
_prologue (clib_longjmp)
	ld 0, 8*0(3)
	mtlr 0
	ld 1, 8*1(3)
	ld 2, 8*2(3)
	ld 0, 8*3(3)
	mtcrf 0xff, 0
	lwz 0, 8*3(3)
	mtspr 256, 0
	
	/* gprs 14 - 31 */
#define _(a,b) ld a, 8*((b) + 4 + 18*0)(3) ; 
	_foreach_14_31
#undef _
	
	/* fprs 14 - 31 */
#define _(a,b) lfd a, 8*((b) + 4 + 18*1)(3) ; 
	_foreach_14_31
#undef _
	
#if CLIB_POWERPC_ALTIVEC_N_REGS > 0
	/* vrs 20 - 31 */
	li 5, 8*(4 + 18*2)
#define _(a,b) lvx a, 5, 3 ; addi 5, 5, 16 ;
	_foreach_20_31
#undef _
#endif /* CLIB_POWERPC_ALTIVEC_N_REGS > 0 */
	
	/* Return value. */
	mr 3, 4
	
	blr

_prologue (clib_calljmp)
	/* Make sure stack is 16 byte aligned. */
	andi. 0, 5, 0xf
	sub  5, 5, 0
	addi 5, 5, -16
	
	/* Save old stack/link pointer on new stack. */
	std 1, 0(5)
	mflr 0
	std 0, 8(5)
	
	/* Switch stacks. */
	mr 1, 5
	
	/* Get function pointer. */
	ld 0, 0(3)
	mtctr 0
	
	/* Move argument into place. */
	mr 3, 4
	
	/* Away we go. */
	bctrl
	
	/* Switch back to old stack. */
	ld 0, 8(1)
	mtlr 0
	ld 0, 0(1)
	mr 1, 0
	
	/* Return to caller. */
	blr
	
#elif defined(__powerpc__)
	
#define _foreach_14_31							\
_ (14, 0)  _ (15, 1)  _ (16, 2)  _ (17, 3)  _ (18, 4)  _ (19, 5)	\
_ (20, 6)  _ (21, 7)  _ (22, 8)  _ (23, 9)  _ (24, 10) _ (25, 11)	\
_ (26, 12) _ (27, 13) _ (28, 14) _ (29, 15) _ (30, 16) _ (31, 17)

#define _foreach_20_31						\
_ (20, 0) _ (21, 1) _ (22, 2) _ (23, 3) _ (24, 4)  _ (25, 5)	\
_ (26, 6) _ (27, 7) _ (28, 8) _ (29, 9) _ (30, 10) _ (31, 11)
	
#ifdef __ALTIVEC__
#define CLIB_POWERPC_ALTIVEC_N_REGS 12
#else
#define CLIB_POWERPC_ALTIVEC_N_REGS 0
#endif

	.global clib_setjmp
	.align 4
	.type clib_setjmp, @function
clib_setjmp:
	mflr 0
	stw 0, 4*0(3)
	stw 1, 4*1(3)
	mfcr 0
	stw 0, 4*2(3)
#if CLIB_POWERPC_ALTIVEC_N_REGS > 0
	mfspr 0, 256
#endif
	stw 0, 4*3(3)
	
#if CLIB_POWERPC_ALTIVEC_N_REGS > 0
	li 5, 4*4
#define _(a,b) stvx a, 3, 5 ; addi 5, 5, 16 ;
	_foreach_20_31
#undef _
#endif /* CLIB_POWERPC_ALTIVEC_N_REGS > 0 */
	
	/* gp 14 - 31 */
#define _(a,b) stw a,  4*(1*(b) + 4 + 4*CLIB_POWERPC_ALTIVEC_N_REGS + 0*18)(3) ; 
	_foreach_14_31
#undef _
	
	/* fp 14 - 31 */
#define _(a,b) stfd a, 4*(2*(b) + 4 + 4*CLIB_POWERPC_ALTIVEC_N_REGS + 1*18)(3) ;
	_foreach_14_31
#undef _

	/* Return value. */
	mr 3, 4
	
	blr
	
	.global clib_longjmp
	.align 4
	.type clib_longjmp, @function
clib_longjmp:	
	
	lwz 0, 4*0(3)
	mtlr 0
	lwz 1, 4*1(3)
	lwz 0, 4*2(3)
	mtcr 0
	lwz 0, 4*3(3)
#if CLIB_POWERPC_ALTIVEC_N_REGS > 0
	mtspr 256, 0
#endif
	
#if CLIB_POWERPC_ALTIVEC_N_REGS > 0
	li 5, 4*4
#define _(a,b) lvx a, 3, 5 ; addi 5, 5, 16 ;
	_foreach_20_31
#undef _
#endif /* CLIB_POWERPC_ALTIVEC_N_REGS > 0 */
	
	/* gp 14 - 31 */
#define _(a,b) lwz a, 4*(1*(b) + 4 + 4*CLIB_POWERPC_ALTIVEC_N_REGS + 0*18)(3) ;
	_foreach_14_31
#undef _
	
	/* fp 14 - 31 */
#define _(a,b) lfd a, 4*(2*(b) + 4 + 4*CLIB_POWERPC_ALTIVEC_N_REGS + 1*18)(3) ;
	_foreach_14_31
#undef _

	/* Return value. */
	mr 3, 4
	
	blr

	.global clib_calljmp
	.align 4
	.type clib_calljmp, @function
clib_calljmp:	
	/* Make sure stack is 16 byte aligned. */
	andi. 0, 5, 0xf
	sub  5, 5, 0
	addi 5, 5, -16
	
	/* Save old stack/link pointer on new stack. */
	stw 1, 0(5)
	mflr 0
	stw 0, 4(5)
	
	/* account for (sp, lr) tuple, and keep aligned */
	addi 5, 5, -16
	
	/* Switch stacks. */
	mr 1, 5
	
	/* Move argument into place. */
	mtctr 3
	mr 3, 4
	
	/* Away we go. */
	bctrl
	
	/* back to our synthetic frame */
	addi 1,1,16
	
	/* Switch back to old stack. */
	lwz 0, 4(1)
	mtlr 0
	lwz 0, 0(1)
	mr 1, 0
	
	/* Return to caller. */
	blr
	
#elif defined(__arm__)
	
	.global clib_setjmp
	.align 4
	.type clib_setjmp, %function
clib_setjmp:
	mov ip, r0		/* jmp buffer */

	/* Save integer registers */
	stmia ip!, {v1-v6, sl, fp, sp, lr}
	
#ifdef __IWMMXT__
	/* Save the call-preserved iWMMXt registers.  */
	wstrd wr10, [ip], #8
	wstrd wr11, [ip], #8
	wstrd wr12, [ip], #8
	wstrd wr13, [ip], #8
	wstrd wr14, [ip], #8
	wstrd wr15, [ip], #8
#endif

	/* Give back user's return value. */
	mov r1, r0
	bx lr
	
	.global clib_longjmp
	.align 4
	.type clib_longjmp, %function
clib_longjmp:	
	mov ip, r0		/* jmp buffer */

	/* Restore integer registers. */
	ldmia     ip!,  {v1-v6, sl, fp, sp, lr}
	
#ifdef __IWMMXT__
	/* Save the call-preserved iWMMXt registers.  */
	wldrd wr10, [ip], #8
	wldrd wr11, [ip], #8
	wldrd wr12, [ip], #8
	wldrd wr13, [ip], #8
	wldrd wr14, [ip], #8
	wldrd wr15, [ip], #8
#endif
	
	/* Give back user's return value. */
	mov r1, r0
	bx lr

	.global clib_calljmp
	.align 4
	.type clib_calljmp, %function
clib_calljmp:	
	/* Make sure stack is 8 byte aligned. */
	bic r2, r2, #7
	
	/* Allocate space for stack/link pointer on new stack. */
	sub r2, r2, #8	
	
	/* Save old stack/link pointer on new stack. */
	str sp, [r2, #0]
	str lr, [r2, #4]
	
	/* Switch stacks. */
	mov sp, r2
	
	/* Save function to call. */
	mov ip, r0
	
	/* Move argument into place. */
	mov r0, r1
	
	/* Away we go. */
	bx ip
	
	/* Switch back to old stack. */
	ldr lr, [sp, #4]
	ldr ip, [sp, #0]
	mov sp, ip
	
	/* Return to caller. */
	bx lr
	
#elif defined(__xtensa__)
	
	/* FIXME implement if needed. */
	.global clib_setjmp
	.align 4
	.type clib_setjmp, %function
clib_setjmp:
1:	j 1b

	.global clib_longjmp
	.align 4
	.type clib_longjmp, @function
clib_longjmp:	
1:	j 1b
	
	.global clib_calljmp
	.align 4
	.type clib_calljmp, %function
clib_calljmp:	
1:	j 1b
	
#elif defined(__TMS320C6X__)
	
	/* FIXME implement if needed. */
	.global clib_setjmp
	.align 4
	.type clib_setjmp, %function
clib_setjmp:
1:	B	.S1     1b

	.global clib_longjmp
	.align 4
	.type clib_longjmp, @function
clib_longjmp:	
1:	B	.S1     1b
	
	.global clib_calljmp
	.align 4
	.type clib_calljmp, %function
clib_calljmp:	
1:	B	.S1     1b
	
#else
#error "unknown machine"
#endif	
