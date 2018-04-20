/* Volume Boot Record 
input parameters:
	DS:SI	- volume entry in partition table
	DL	- bios device number
	CS:IP	- 0000:7C00
*/


.set ORIGIN, 	0
.set LOAD_SEG,	0x07c0
.set SECTOR_SZ,	0x200
.set MAGIC,	0xaa55
.set STACK,	0x1ff0
.set STAGE2,	0x0200		/* stage 2 segment */
.set STAGE2_SZ,	46		/* max. size of stage 2 in sectors */
.set PTE_SIZE,	0x10

.code16
.org ORIGIN
.global _start
.text

_start:
	cli
	/* setup stack */
	xorl	%eax, %eax
	movw	%ax, %ss
	movw	$STACK, %sp
	

	/* normalize ES */
	movw	$LOAD_SEG, %ax
	movw	%ax, %es

	test	$0x80, %dl /* check bios device */
	jz	_normalize_ds_ /* FDD boot */

	/* copy partition table entry */
	xorw	%di, %di
	xorw	%cx, %cx
	movw	$_pt_entry_, %di
	movw	$PTE_SIZE, %cx
	rep 	movsb
	

_normalize_ds_:	
	/* normalize DS */
	movw	$LOAD_SEG, %ax
	movw	%ax, %ds

	test	$0x80, %dl /* check bios device */
	jz	_CHS_read_

_pt_hdd_boot_:
	/* check for LBA support */
	movb	$0x41, %ah
	movw	$0x55aa, %bx
	int	$0x13
	jc	_CHS_read_
	cmpw	$0xaa55, %bx
	jnz	_CHS_read_

	/* LBA mode read */
	movw	$STAGE2_SZ, %cx
	movw	_pt_lba_sector_lo_, %ax
	movw	_pt_lba_sector_hi_, %bx
	incw	%ax
	jnz	_LBA_read_next_
	incw	%bx
	
_LBA_read_next_:
	movw	%ax, _dap_sector_lo_lo_
	movw	%bx, _dap_sector_lo_hi_
	call	_LBA_read_sector_
	decw	%cx
	jz	_call_stage_2_
	movw	_dap_dst_offset_, %ax
	addw	$SECTOR_SZ, %ax
	movw	%ax, _dap_dst_offset_
	movw	_dap_sector_lo_lo_, %ax
	movw	_dap_sector_lo_hi_, %bx
	incw	%ax
	jnz 	_LBA_read_next_
	incw	%bx
	jnz	_LBA_read_next_
	hlt

_LBA_read_sector_:
	movb	$0x42, %ah
	movw	$_dap_, %si
	int	$0x13
	jc	_read_sector_err_
	jmp	_read_sector_done_


_CS_to_word_:
/*
input:
	CX: cylinder/sector
		10  /  6
output:
	CX: sector
	BX: cylinder
*/
	movw	%cx, %bx
	clc
	shrb	$6, %bl
	clc
	rorw	$2, %bx
	clc
	shrw	$6, %bx
	andw	$0x003f, %cx
	ret

_word_to_CS_:
/*
input:
	CX: sector
	BX: cylinder
output:
	CX: cylinder/sector
		10  /  6
*/
	clc
	shlb	$6, %bh
	clc
	rolw	$2, %bx
	clc
	shlw	$6, %bx
	orw	%bx, %cx
	ret

_CHS_read_:
	pushw	%dx	/* save bios device number */
	/* get drive parameters */ 
	xorw	%di, %di
	movw	%di, %es
	movb	$8, %ah
	int	$0x13
	jc	_read_sector_err_

	/* restore ES */
	movw	$LOAD_SEG, %ax
	movw	%ax, %es

	/* store device parameters */
	incb	%dh
	movb	%dh, _heads_
	call	_CS_to_word_
	incw	%cx
	movw	%cx, _sectors_
	incw	%bx
	movw	%bx, _cylinders_
	pop	%dx	/* restore bios device number */

	/* load PT entry */
	movw	$0, %si /* sector number */
	movb	_pt_cylinder_, %ch
	movb	_pt_head_, %dh
	movb	_pt_sector_, %cl

_CHS_read_next_:
	call	_CS_to_word_
	incw	%cx
	cmpw	_sectors_, %cx
	jnz	_CS_read_next_sector_
	movb	$1, %cl
	incb	%dh
	cmpb	_heads_, %dh
	jnz	_CS_read_next_sector_
	movb	$0, %dh
	incw	%bx
	cmpw	_cylinders_, %bx
	jnz	_CS_read_next_sector_
	hlt

_CS_read_next_sector_:
	call	_word_to_CS_
	pushw	%es
	movw	$STAGE2, %ax
	movw	%ax, %es
	movw	_offset_, %bx
	call	_CHS_read_sector_
	addw	$SECTOR_SZ, %bx
	movw	%bx, _offset_
	incw	%si
	cmpw	$STAGE2_SZ, %si
	jnz	_CHS_read_next_

_call_stage_2_:
	/* jump in stage 2 */
	movw	$_pt_entry_, %si
	jmp	$STAGE2, $0

_CHS_read_sector_:
/*
input parameters:
	CX	- Track / Sector
	DH	- Head
	DL	- Drive
	ES:BX	- Destination buffer
*/
	movb	$2, %ah
	movb	$1, %al
	int	$0x13
	jnc	_read_sector_done_
_read_sector_err_:
	movw	$LOAD_SEG, %ax
	movw	%ax, %es
	movw	$_err2_, %di
	jmp	_print_error_
_read_sector_done_:
	movw	$0x0e2e, %ax
	int	$0x10
	/* delay */
	pushw	%dx
	pushw	%cx
	movb	$0x86, %ah
	movw	$0, %cx
	movw	$0x8fff, %dx
	int	$0x15
	popw	%cx
	popw	%dx
	ret

_print_error_:
/*
input parameters:
	DI	- pointer to string
*/
	movb	$0x0e, %ah
_print_char_:
	movb	(%di), %al
	cmpb	$0, %al
	jz	_end_print_
	int	$0x10
	incw	%di
	jmp	_print_char_
_end_print_:	
	hlt

_err2_:	.ascii "ERROR: read sector"; .byte 0

_offset_:		.word	0
_heads_:		.byte	0xff /* last head */
_sectors_:		.word	0xffff /* last sector */
_cylinders_:		.word	0xffff /* last cylinder */

.org ORIGIN+0x1de
_dap_:
_dap_size_:		.byte 	0x10
_dap_reserved_:		.byte	0
_dap_sectors_:		.word	1
_dap_dst_offset_:	.word	0
_dap_dst_segment_:	.word	STAGE2
_dap_sector_lo_lo_:	.word	1
_dap_sector_lo_hi_:	.word	0
_dap_sector_hi_lo_:	.word	0
_dap_sector_hi_hi_:	.word	0


.org ORIGIN+0x1ee
_pt_entry_:
_pt_indicator_:		.byte	0
_pt_head_:		.byte	0
_pt_sector_:		.byte	1
_pt_cylinder_:		.byte	0
_pt_system_id_:		.byte	0
_pt_end_head_:		.byte	0
_pt_end_sectot_:	.byte	0
_pt_end_cylinder_:	.byte	0
_pt_lba_sector_lo_:	.word	0
_pt_lba_sector_hi_:	.word	0
_pt_lba_size_lo_:	.word	0
_pt_lba_size_hi_:	.word	0

.org ORIGIN+0x1fe
.word MAGIC
.end
