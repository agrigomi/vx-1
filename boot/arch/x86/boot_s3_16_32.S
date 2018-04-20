BOOT_S3_SEG		= 0	

.code16
.section ".bstext", "ax"
.extern _g_lgdt


/************************************************************************************************
 * 
 * 			Protected mode
 */


/*; segment indexes in GDT */
__NULL_SEGMENT	 	= 0
__CODE_SEGMENT		= 1
__DATA_SEGMENT		= 2
__STACK_SEGMENT		= 3
__CODE_SEGMENT_16	= 4
__DATA_SEGMENT_16	= 5
__STACK_SEGMENT_16	= 6

.code16
/*;--- void _pm_call(void *proc,void *p_data) --- */
.global _pm_call
_pm_call:
	/*; save code segment */
	xorl	%eax, %eax
	movw	%cs,%ax
	movw	%ax,_cs

	movw	$(__DATA_SEGMENT << 3),%bx		/*;  the data segment descriptor must be at index 2 in GDT */
	movw	$(__STACK_SEGMENT << 3),%cx		/*; stack segment at index 3 */
	

	/*; switch to protected mode */
	cli
	movl	%cr0,%eax
	orb	$1,%al				/*; turn On the PM flag */
	movl	%eax,%cr0

	jmp	$(__CODE_SEGMENT << 3), $_lp_386
	
.code32
_lp_386:
	/*; loading data, stack and flat memory data segments */
	movw	%bx,%ds
	movw	%bx,%es
	movw	%cx,%ss
	movw	%bx,%fs
	movw	%bx,%gs
	
	movl	4(%esp),%eax	/*; EAX = ponter to proc */
	pushl	8(%esp)		/*; push proc parameter (p_data *) */
	call	*%eax		/*; call proc */
	addl	$4,%esp		/*; remove p_data from stack */
	
	movw	$(__DATA_SEGMENT_16 << 3), %bx
	movw	$(__STACK_SEGMENT_16 << 3), %cx
	movw	%bx,%ds
	movw	%bx,%es
	movw	%cx,%ss
	movw	%bx,%fs
	movw	%bx,%gs
	jmp	$(__CODE_SEGMENT_16 << 3), $_lp_switch_real
	
/*; switch back to real mode */
_lp_switch_real:	
	cli
	movl	%cr0,%eax
	andb	$0xfe,%al	/*; clear PM flag */
	movl	%eax,%cr0

	.byte	0xea
	.long	_lp_286	
_cs:	.word	0


.code16
_lp_286:
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	movw	%ax,%gs
	movw	%ax,%fs
	retw

.code32
.global	_32rm_call
/*;--- void _32rm_call(void *proc,void *p_data) --- */	
_32rm_call:
	/*; switch to real mode */
	call	_lp_switch_real
	
.code16
	/*; call 16 bit procedure (proc) */
	movl	4(%esp),%eax	/*; EAX = ponter to proc */
	pushl	8(%esp)		/*; push proc parameter (p_data *) */
	call	*%eax		/*; call proc */
	addl	$4,%esp		/*; remove p_data from stack */

	movw	$(__DATA_SEGMENT << 3),%bx		/*;  the data segment descriptor must be at index 2 in GDT */
	movw	$(__STACK_SEGMENT << 3),%cx		/*;  stack segment at index 3 */
	
	/*; switch back to protected mode */
	movl	%cr0,%eax
	orb	$1,%al		/*; turn On the PM flag */
	movl	%eax,%cr0
	
	jmp	$(__CODE_SEGMENT << 3), $__lp_386

.code32
__lp_386:
	/*; loading data, stack and flat memory data segments */
	movw	%bx,%ds
	movw	%bx,%es
	movw	%cx,%ss
	movw	%bx,%fs
	movw	%bx,%gs
	ret





/************************************************************************************************
 * 
 * 			Long mode
 */

__NULL_SEGMENT		= 0
__CODE_SEGMENT64	= 7
__DATA_SEGMENT64	= 8
__DEFAULT_PT_ADDRESS	= 0x100000

_proc64_addr:		.long	0
_proc64_data:		.long	0
_backup_cr3:		.long	0

.code16gcc

/*;--- void _lm_call(void *proc, void *p_data) --- */
.global _lm_call
_lm_call:
	movl	4(%esp), %eax	/* long mode procedure address */
	movl	8(%esp), %ebx   /* long mode procedure parameters */
	movl	%eax, _proc64_addr
	movl	%ebx, _proc64_data
	
	/* _pm_call(_lm_proc, _lm_data) */
	pushl	%ebx
	pushl	$_pm_switch_long
	call	_pm_call
	addl	$8, %esp
	ret

.code32

.global _lm_call32
_lm_call32:
	movl	4(%esp), %eax	/* long mode procedure address */
	movl	8(%esp), %ebx   /* long mode procedure parameters */
	movl	%eax, _proc64_addr
	movl	%ebx, _proc64_data
	

_pm_switch_long:
/* switch to long mode */
	cli

	movl	%cr3, %eax
	movl	%eax, _backup_cr3

	/* enable PAE */
	movl	%cr4, %eax
	or	$0x6e0, %eax	/* enable bits in CR4 (PAE and ...) */
	movl	%eax, %cr4
	
	/* Enable long mode */
	movl	$0xC0000080, %ecx   /* Set the C-register to 0xC0000080, which is the EFER MSR. */
	rdmsr                       /* Read from the model-specific register. */
	bts	$8, %eax 	    /* Set the LM-bit which is the 9th bit (bit 8). */
	or	$0xd01, %eax	    /* Set NX */
	wrmsr                       /* Write to the model-specific register. */

	xorl	%eax, %eax
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx
	xorl	%edx, %edx

	movw	$(__DATA_SEGMENT64 << 3), %bx
	movl	_proc64_addr, %esi
	movl	_proc64_data, %edi

	/* Set CR3 register */
	movl	$__DEFAULT_PT_ADDRESS, %eax
	movl	%eax, %cr3

	/* enable paging */
	movl	%cr0, %eax
	bts	$31, %eax /* set paging bit (bit 31 of CR0) */
	/*or	$0x8005003b, %eax*/
	movl	%eax, %cr0
	/* jump to long mode */
	jmp 	$(__CODE_SEGMENT64 << 3), $_lp_x86_64


.code64
_lp_x86_64:
	xor	%rax, %rax
	xor	%rbx, %rbx
	xor	%rcx, %rcx
	xor	%rdx, %rdx
	movw	%bx, %ss
	movw	%bx, %ds
	movw	%bx, %es
	movw	%bx, %fs
	movw	%bx, %gs
	
	movl	$_clear_cs, %eax
	jmp	*%rax
	nop

_clear_cs:
	/* reload CR3 */ 
	mov	%cr3, %rax 	
	mov	%rax, %cr3

	/* call long mode procedure */
	xorq	%rax, %rax
	xorq	%rbx, %rbx

	movl	%esi, %eax	/* proc address */
	movl	%edi, %ebx	/* argument */

	xorq	%rdi, %rdi
	movl	%ebx, %edi	/* keep the agrgument in RDI according fast call convention */

	pushq	%rbx
	
	callq	*%rax		/*; call proc */
	addq	$8,%rsp		/*; remove p_data from stack */

	/* STOP HERE BECAUSE I DON'T KNOW HOW TO SWITCH BACK 
	 IN PROTECTED MODE */
	hlt

	/*; switch back to protected mode */
_lp64_exit_long:	
	cli

	movw	$(__DATA_SEGMENT << 3),%bx	/*;  the data segment descriptor must be at index 2 in GDT */
	movw	$(__STACK_SEGMENT << 3),%cx	/*; stack segment at index 3 */
	/*; prepare the segment selectors */

	/*; loading data, stack and flat memory data segments */
	movw	%cx,%ss
	movw	%bx,%ds
	movw	%bx,%es
	movw	%bx,%fs
	movw	%bx,%gs

	movq	%cr0, %rax
	btc	$31, %rax	/* remove paging bit */
	movq	%rax, %cr0
	
	/* disable PAE */
	mov	%cr4, %rax 	/* Read EFER.*/
	btc	$5, %rax 	/* Set PAE=0 */
	mov	%rax, %cr4 	/* Write EFER. */

	/* disable long mode */
	movl	$0xC0000080, %ecx
	rdmsr
	btc	$8, %eax /* clear the LM-bit which is the 9th bit (bit 8). */
	wrmsr
	
	.byte	 0xea	/*; emit jmp instructopn  */
	.long	_mp_x86_64_	/*; jmp offset */
	.word	__CODE_SEGMENT << 3	/*; jmp code segment selector */
.code32
_mp_x86_64_:

	ret


/*****************************************************************/
.code64
.global _lm_cpu_idle
/* void _lm_cpu_idle(void *p_context) */
_lm_cpu_idle:
	nop
	hlt
	nop
	jmp _lm_cpu_idle

.end

