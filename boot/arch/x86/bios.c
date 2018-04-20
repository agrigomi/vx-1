#include "boot.h"
#include "code16gcc.h"

void wait(volatile _u32 micro_s) {
	volatile _u16 lo = (_u16)micro_s;
	volatile _u16 hi = (_u16)(micro_s >> 16);
	__asm__ __volatile__ (
		"movw	%0,%%dx;\n"
		"movw	%1,%%cx;\n"
		"movb	$0x86,%%ah;\n"
		"int	$0x15;\n"
		:
		:"m"(lo),"m"(hi)
	);
}

/* waiting for keyboard input 
output:
	16 bit word as:
	 scan code in hi byte and
	 ascii code in lo byte */
_u16 wait_key(void) {
	volatile _u16 res = 0;
	
	__asm__ __volatile__ (
		"movb	$0,%%ah\n"
		"int	$0x16\n"
		"movw	%%ax,%0\n"
		:"=m"(res)
		:
	);
	
	return res;
}

/* reading keyboard input buffer
output:
	16 bit word as:
	 scan code in hi byte and
	 ascii code in lo byte */
_u16 get_key(void) {
 	volatile _u16 res = 0;
	
	__asm__ __volatile__ (
		"movb	$1,%%ah\n"
		"int	$0x16\n"
		"jz	bios_get_key_done\n"
		"call	wait_key\n"
		"movw	%%ax,%0\n"
		"bios_get_key_done:\n"
		:"=m"(res)
		:
	);
	
	return res;
}
