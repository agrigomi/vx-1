/* File System Boot Record 
input parameters:
	DS:SI	- volume entry in partition table
	DL	- bios device number
	CS:IP	- 0000:1000
*/


.set ORIGIN, 	0
.set LOAD_SEG,	0
.set STACK,	0x00001ff0
.set PTE_SIZE,	0x10

.extern _main_

.code16
.org ORIGIN

.global _start
.global _bios_device_
.global _pt_entry_
.global _pt_indicator_
.global _pt_head_
.global _pt_sector_
.global _pt_cylinder_
.global _pt_system_id_
.global _pt_end_head_
.global _pt_end_sectot_
.global _pt_end_cylinder_
.global _pt_lba_sector_lo_
.global _pt_lba_sector_hi_
.global _pt_lba_size_lo_
.global _pt_lba_size_hi_
.global _fs_super_block_

.text

_start:
	cli
	/* normalize CS */
	jmp	$0, $_zero_CS_
_zero_CS_:
	/* setup stack */
	xorl	%eax, %eax
	movw	%ax, %ss
	movl	$STACK, %esp
	
	/* normalize ES */
	push	%cs	
	pop 	%es

	/* copy partition table entry */
	xorl	%edi, %edi
	xorl	%ecx, %ecx
	movl	$_pt_entry_, %edi
	movl	$PTE_SIZE, %ecx
	rep 	movsb
	
	/* normalize DS */
	push	%cs
	pop	%ds

	movb	%dl, _bios_device_

	jmp	_main_

_bios_device_:		.byte	0	
_pt_entry_:
_pt_indicator_:		.byte	0
_pt_head_:		.byte	0
_pt_sector_:		.byte	1
_pt_cylinder_:		.byte	0
_pt_system_id_:		.byte	0
_pt_end_head_:		.byte	0
_pt_end_sectot_:	.byte	0
_pt_end_cylinder_:	.byte	0
_pt_lba_sector_lo_:	.word	1
_pt_lba_sector_hi_:	.word	0
_pt_lba_size_lo_:	.word	0
_pt_lba_size_hi_:	.word	0

.org ORIGIN+0x100
_fs_super_block_:

.org ORIGIN+0x1fe
.ascii "MG"
.end
