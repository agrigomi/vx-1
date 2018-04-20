#include "bootx86.h"
#include "boot.h"
#include "code16gcc.h"

/*
conventional read sector by BIOS
input:
	mode:	read or write
	lba:	linear sector address
	dev:	bios device number
	_ptr:	pointer to destination buffer
output:
	error code if CF = 1
	0=OK
*/
_u8 __NOINLINE__ __REGPARM__ __NOOPTIMIZE__ rw_sector_CHS(volatile _u8 mode, volatile _u32 lba, volatile _u8 dev, _ptr_t *p_dst) {
	volatile _u16 mcyl = 0;		/* maximum cylinders */
	volatile _u8  mhd  = 0;		/* maximum heads */
	volatile _u8  mspt = 0;		/* maximum sectors per track */
	volatile _u16 _cyl = 0;		/* cylinder */
	volatile _u8  _hd  = 0;		/* head */
	volatile _u8  _sec = 0;		/* sector */
	volatile _u8  err = 0;		/* error code */
	volatile _u16 temp = 0;
	volatile _u16 dst_segment = 0;
	volatile _u16 dst_offset = 0;

	/* get device parameters */
	__asm__ __volatile__ (
		"movb	%4,%%dl\n"		/* DL = bios device number */
		"movb	$8,%%ah\n"		/*; read device parameters function */
		"int	$0x13\n"
		"movb	%%ah,%0\n"		/* store error code */
		"xorw	%%ax,%%ax\n"		/* AX = 0 */
		"movb	%%cl,%%al\n"		/*; hi order of cylinder number --> AX */
		"andw	$0xc0,%%ax\n"		/*; keep only 2 bits */
		"shlw	$2,%%ax\n"		/*; move 2 bits to hi order of word */
		"movb	%%ch,%%al\n"		/*; load lo part of max number of cylinders */
		"movw	%%ax,%1\n"		/*; store max. munber of cylinders (mcyl) */
		"incb	%%dh\n"			/*; max. head number + 1 */
		"movb	%%dh,%2\n"		/*; store total number of heads (mhd) */
		"movb	%%cl,%%al\n"		/*; load sectors per track */
		"andb	$0x3f,%%al\n"		/*; clear hi order 2 bits for cylinder */
		"movb	%%al,%3\n"		/*; store max. munber of sectors per track (mspt) */
		:"=m"(err),"=m"(mcyl),"=m"(mhd),"=m"(mspt)
		:"m"(dev)
	);
	
	if(!err) { /* calculating CHS */
	 	_cyl = lba / (mhd * mspt);
		temp = lba % (mhd * mspt);
		_hd = temp / mspt;
		_sec = temp % mspt + 1;
		
		dst_segment = p_dst->segment;
		dst_offset  = p_dst->offset;
		/* read sector */		
		__asm__ __volatile__ (
			"movb	%[dev],%%dl\n"		/*; load device number (dev) */
			"movw	%[offset], %%bx\n"	/*; load destination offset */
			"movw	%[cylinder],%%ax\n"	/*; AX = (_cyl) */
			"movb	%%al,%%ch\n"		/*; load cylinder */
			"movb	%[head],%%dh\n"		/*; load head (_hd) */
			"movb	%%ah,%%cl\n"		/*; load hi order of cylinder */
			"shlb	$6,%%cl\n"
			"orb	%[sector],%%cl\n"	/*; load sector (_sec) */
			"movw	%[segment], %%ax\n"	/*; load destination segment */
			"movw	%%ax, %%es\n"		/*; set destination segment */
			"movb	%[mode],%%ah\n"		/*; sector R/W function */
			"movb	$1,%%al\n"		/*; one sector */
			"int	$0x13\n"		/*; call BIOS */
			"mov	%%ah,%[err]\n"
			"movw	%%ds, %%ax\n"
			"movw	%%ax, %%es\n"		/*; restore ES */
			:[err] "=m"(err)
			:[dev] "m"(dev),[segment] "m" (dst_segment),[offset] "m" (dst_offset), 
			 [cylinder] "m" (_cyl), [head] "m" (_hd), [sector] "m"(_sec),[mode] "m"(mode)
		);
	}
	return err;	
}


typedef struct {
	_u8	size;
	_u8	reserved;
	_u16	sectors;
	_u16	dst_offset;
	_u16	dst_segment;
	_u32	sector_lo;
	_u32	sector_hi;
}_dap_t;

/*---- extended sector read function using LBA address
input:
	mode:	read or write
	lba:	linear sector address
	dev:	bios device number
	count:	number of sectors to read
	dst:	destination buffer
output:
	error code if CF = 1
	0=OK
*/
static volatile _dap_t	*p_dap = 0;
_u8 __NOINLINE__ __REGPARM__ __NOOPTIMIZE__ rw_sector_LBA(volatile _u8 mode, _lba_t *p_lba, volatile _u8 dev, 
						volatile _u8 count, _ptr_t *p_dst) {
	volatile _u8 res = 0;
	_dap_t	_dap;
	 p_dap = &_dap;
	volatile _u8 fn = mode|0x40;
	
	_dap.size = sizeof(_dap_t);
	_dap.reserved = 0;
	_dap.sectors = count;
	_dap.dst_offset = p_dst->offset;
	_dap.dst_segment = p_dst->segment;
	_dap.sector_lo = *(_u32 *)p_lba;
	_dap.sector_hi = 0;
	
	__asm__ __volatile__ (
		"movl	%[dap],%%ebx\n"
		"movb	%[dev],%%dl\n"
		"movb	%[fn],%%ah\n"	/* extended read bios function */
		"pushl	%%esi\n"
		"movl	%%ebx, %%esi\n"
		"int	$0x13\n"
		"popl	%%esi\n"
		"movb	%%ah,%[res]\n"	/* set error code */
		:[res]"=m"(res)
		:[dap]"m"(p_dap),[dev]"m"(dev),[fn]"m"(fn)
	);

	return res;	
}

_u8  __NOINLINE__ __REGPARM__ __NOOPTIMIZE__ check_ex_read(_u8 bios_dev) {
	volatile _u8 r = 0;
	
	__asm__ __volatile__ (
		"movb	$0x41,	%%ah\n"
		"movb	%[dev], %%dl\n"
		"movw	$0x55aa, %%bx\n"
		"int	$0x13\n"
		"jc	__no_ex_read__\n"
		"cmpw	$0xaa55, %%bx\n"
		"jnz	__no_ex_read__\n"
		"movb	$1, %[r]\n"
		"__no_ex_read__:\n"
		:[r]"=m"(r)
		:[dev]"m"(bios_dev)
	);
	
	return r;
}

_u32 __NOINLINE__ __REGPARM__ __NOOPTIMIZE__ read_sector(volatile _u32 sector,	/* sector number (at begin of partition) */
		 volatile _u32 count, 	/* number of sectors to read */
		 volatile _u32 dst 	/* destination address */
		) {
	volatile _u32 r = 0;
	_ptr_t ptr; /* destination pointer */
	volatile _u32 _sector = sector;
	
	/* convert destination address to segment:offset format */
	ptr.segment = (_u16)(dst / 0x10);
	ptr.offset  = (_u16)(dst % 0x10);
	
	volatile _u32 *_p = (_u32 *)&_pt_lba_sector_lo_;
	_sector += *_p;
	
	if(check_ex_read(_bios_device_)) {
		_lba_t *p_lba = (_lba_t *)&_sector;
		if(rw_sector_LBA(BIOS_FN_READ, p_lba, _bios_device_, count, &ptr) == 0)
			r = count;
	} else {
		/* CHS read */
		while(r < count) {
			print_dword(_sector);
			print("->");
			print_dword(dst);
			print("->");
			print_word(ptr.segment);
			print(":");
			print_word(ptr.offset);
			if(rw_sector_CHS(BIOS_FN_READ, _sector, _bios_device_, &ptr) == 0) {
				print("->");
				print_hex((_u8*) dst, 16);
				print("\r\n");
				ptr.offset += SECTOR_SIZE;
				_sector++;
				r++;
			} else {
				print("\r\nI/O error\r\n");
				break;
			}
		}
	}

	return r;
}

