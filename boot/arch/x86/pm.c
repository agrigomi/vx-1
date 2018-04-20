#include "mgtype.h"
#include "bootx86.h"
#include "segment.h"
#include "code16gcc.h"

static void gdt_set_entry(_u32 num, _u32 base, _u32 limit, _u8 access, _u8 gran) {
   	_gdt_entry_t *p_gdt = (_gdt_entry_t *)(_gdt + (num * 8));
	
	p_gdt->base_low		= (_u16)(base & 0xFFFF);
	p_gdt->base_middle 	= (_u8)((base >> 16) & 0xFF);
 	p_gdt->base_high   	= (_u8)((base >> 24) & 0xFF);
	p_gdt->limit_low   	= (_u16)(limit & 0xFFFF);
	p_gdt->granularity 	= (_u8)((limit >> 16) & 0x0F);
	p_gdt->granularity 	|= (_u8)(gran & 0xF0);
	p_gdt->access      	= access;
} 

void pm_init_gdt(void) {
	_gdt_ptr_t _lgdt;
	
	_gdt_limit = (sizeof(_gdt_entry_t) * MAX_GDT_RECORDS) - 1;

	_lgdt.limit = _gdt_limit;
	_lgdt.base = (_u32)_gdt;

   	gdt_set_entry(NULL_SEGMENT, 		0, 	0, 		0, 	0);    /* Null segment */
   	gdt_set_entry(FLAT_CODE_SEGMENT,	0,	0xFFFFFFFF, 	0x9A, 	GRAN_32); /* Code segment */
   	gdt_set_entry(FLAT_DATA_SEGMENT,	0, 	0xFFFFFFFF, 	0x92, 	GRAN_32); /* Flat memory data segment */
	gdt_set_entry(FLAT_STACK_SEGMENT, 	0,	0xFFFFFFFF, 	0x92, 	GRAN_32); /* stack segment */
   	gdt_set_entry(FLAT_CODE_SEGMENT_16,	0,	0x0000FFFF, 	0x9A, 	GRAN_16); /* Code segment */
   	gdt_set_entry(FLAT_DATA_SEGMENT_16,	0, 	0x0000FFFF, 	0x92, 	GRAN_16); /* Flat memory data segment */
	gdt_set_entry(FLAT_STACK_SEGMENT_16, 	0,	0x0000FFFF, 	0x92, 	GRAN_16); /* stack segment */
   	gdt_set_entry(FLAT_CODE_SEGMENT_64,	0,	0xFFFFFFFF, 	0x9A, 	GRAN_64); /* Code segment */
   	gdt_set_entry(FLAT_DATA_SEGMENT_64,	0, 	0xFFFFFFFF, 	0x92, 	GRAN_64); /* Flat memory data segment */
	
	__asm__ __volatile__ ("lgdt %0" : :"m"(_lgdt) );
}

