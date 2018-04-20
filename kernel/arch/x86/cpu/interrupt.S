/* copied from HelenOS source */

.extern interrupt_dispatcher
.extern _get_code_selector_

/*
.bss
_code_selector_:	.quad	0
.global _code_selector_
*/
/*
 * Size of the istate structure without the hardware-saved part and without the
 * error word.
 */
#define ISTATE_SOFT_SIZE	144

/**
 * Mask for interrupts 0 - 31 (bits 0 - 31) where 0 means that int
 * has no error word  and 1 means interrupt with error word
 *
 */
//#define ERROR_WORD_INTERRUPT_LIST  0x00027C00
#define ERROR_WORD_INTERRUPT_LIST  0x00000000

#define ISTATE_OFFSET_RAX		0
#define ISTATE_OFFSET_RBX		8
#define ISTATE_OFFSET_RCX		16
#define ISTATE_OFFSET_RDX		24
#define ISTATE_OFFSET_RSI		32
#define ISTATE_OFFSET_RDI		40
#define ISTATE_OFFSET_RBP		48
#define ISTATE_OFFSET_R8		56
#define ISTATE_OFFSET_R9		64
#define ISTATE_OFFSET_R10		72
#define ISTATE_OFFSET_R11		80
#define ISTATE_OFFSET_R12		88
#define ISTATE_OFFSET_R13 		96
#define ISTATE_OFFSET_R14		104
#define ISTATE_OFFSET_R15		112
#define ISTATE_OFFSET_ALIGNMENT		120
#define ISTATE_OFFSET_RBP_FRAME		128
#define ISTATE_OFFSET_RIP_FRAME		136
#define ISTATE_OFFSET_ERROR_WORD	144
#define ISTATE_OFFSET_RIP		152
#define ISTATE_OFFSET_CS		160
#define ISTATE_OFFSET_RFLAGS		168
#define ISTATE_OFFSET_RSP		176
#define ISTATE_OFFSET_SS		184

.macro handler i
.global _int_\i
_int_\i:

	/*
	 * Choose between version with error code and version without error
	 * code.
	 */

	.iflt \i-32
		.if (1 << \i) & ERROR_WORD_INTERRUPT_LIST
			/*
			 * Version with error word.
			 */
			subq $ISTATE_SOFT_SIZE, %rsp
		.else
			/*
			 * Version without error word.
			 */
			subq $(ISTATE_SOFT_SIZE + 8), %rsp
		.endif
	.else
		/*
		 * Version without error word.
		 */
		subq $(ISTATE_SOFT_SIZE + 8), %rsp
	.endif

	/*
	 * Save the general purpose registers.
	 */
	movq %rax, ISTATE_OFFSET_RAX(%rsp)
	movq %rbx, ISTATE_OFFSET_RBX(%rsp)
	movq %rcx, ISTATE_OFFSET_RCX(%rsp)
	movq %rdx, ISTATE_OFFSET_RDX(%rsp)
	movq %rsi, ISTATE_OFFSET_RSI(%rsp)
	movq %rdi, ISTATE_OFFSET_RDI(%rsp)
	movq %rbp, ISTATE_OFFSET_RBP(%rsp)
	movq %r8,  ISTATE_OFFSET_R8(%rsp)
	movq %r9,  ISTATE_OFFSET_R9(%rsp)
	movq %r10, ISTATE_OFFSET_R10(%rsp)
	movq %r11, ISTATE_OFFSET_R11(%rsp)
	movq %r12, ISTATE_OFFSET_R12(%rsp)
	movq %r13, ISTATE_OFFSET_R13(%rsp)
	movq %r14, ISTATE_OFFSET_R14(%rsp)
	movq %r15, ISTATE_OFFSET_R15(%rsp)

	/*
	 * Imitate a regular stack frame linkage.
	 * Stop stack traces here if we came from userspace.
	 */
	/* xorl %edx, %edx */
	/*cmpq $(GDT_SELECTOR(KTEXT_DES)), ISTATE_OFFSET_CS(%rsp)*/

	/*
	call _get_code_selector_
	cmpq %rax, ISTATE_OFFSET_CS(%rsp)
	cmovnzq %rdx, %rbp

	movq %rbp, ISTATE_OFFSET_RBP_FRAME(%rsp)
	movq ISTATE_OFFSET_RIP(%rsp), %rax
	movq %rax, ISTATE_OFFSET_RIP_FRAME(%rsp)
	leaq ISTATE_OFFSET_RBP_FRAME(%rsp), %rbp
	*/

	movq $(\i), %rdi   /* pass intnum in the first argument */
	movq %rsp, %rsi    /* pass istate address in the second argument */
	cld
	/* Call interrupt_dispatcher(i, istate) */
	call interrupt_dispatcher

	/*
	 * Restore all scratch registers and the preserved registers we have
	 * clobbered in this handler (i.e. RBP).
	 */
	movq ISTATE_OFFSET_RAX(%rsp), %rax
	movq ISTATE_OFFSET_RBX(%rsp), %rbx
	movq ISTATE_OFFSET_RCX(%rsp), %rcx
	movq ISTATE_OFFSET_RDX(%rsp), %rdx
	movq ISTATE_OFFSET_RSI(%rsp), %rsi
	movq ISTATE_OFFSET_RDI(%rsp), %rdi
	movq ISTATE_OFFSET_RBP(%rsp), %rbp
	movq ISTATE_OFFSET_R8(%rsp), %r8
	movq ISTATE_OFFSET_R9(%rsp), %r9
	movq ISTATE_OFFSET_R10(%rsp), %r10
	movq ISTATE_OFFSET_R11(%rsp), %r11
	movq ISTATE_OFFSET_R12(%rsp), %r12
	movq ISTATE_OFFSET_R13(%rsp), %r13
	movq ISTATE_OFFSET_R14(%rsp), %r14
	movq ISTATE_OFFSET_R15(%rsp), %r15

	/* $8 = Skip error word */
	addq $(ISTATE_OFFSET_ERROR_WORD + 8), %rsp
	iretq
.endm

#define LIST_0_255 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,\
	21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,\
	42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,\
	64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,\
	86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,\
	106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,\
	123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,\
	141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,\
	159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,\
	177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,\
	195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,\
	213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,\
	231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,\
	249,250,251,252,253,254,255

.text
interrupt_handlers:
.irp cnt, LIST_0_255
	handler \cnt
.endr

