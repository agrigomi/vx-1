#include "compiler.h"
#include "bootx86.h"
#include "code16gcc.h"

/* using int 0x15 AX=0xE820 to receive memory map array */
_u16 __attribute__((optimize("O0"))) bios_get_memory_map_e820(_u8 *buffer,_u16 _UNUSED_ sz, _u16 chunk_sz) {
	_u16 r = 0;
	_u32 _ebx = 0;
	_u32 ptr = (_u32)buffer;
	
	do {
		__asm__ __volatile__ (
			"movl	%[__ebx], %%ebx\n"
			"movw	$0x0e820,%%ax\n"
			"movl	$0x534D4150,%%edx\n"
			"movw	%[sz], %%cx\n"
			"movl	%[ptr], %%edi\n"
			"int	$0x15\n"
			"movl	%%ebx, %[_ebx]\n"
			: [_ebx] "=m" (_ebx)
			: [ptr] "m" (ptr), [__ebx] "m" (_ebx), [sz] "m" (chunk_sz)
		);
		ptr += chunk_sz;
		r++;
	} while(_ebx);
	
	return r;
}

_u32 get_core_space_ptr(void) {
	return CORE_SPACE_BEGIN;
}

/* convert 32 bit address to segment:offset */
void addr2so(_u32 addr, /* in */
		 _ptr_t *so /*out */
	    ) {
	so->segment = (_u16)(addr / 0x10);
	so->offset  = (_u16)(addr % 0x10);
}

