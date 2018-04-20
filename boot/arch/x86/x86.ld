/*
 * setup.ld
 *
 * Linker script for the i386 setup code
 */
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS {
	. = 0x2000;
	.text		: { *(.text) }
	.text32		: { *(.text32) }
	. = ALIGN(16);
	.data		: { *(.data*) }
	. = ALIGN(16);
	.rodata		: { *(.rodata*) }
	. = ALIGN(16);
	.bss		: {
		__bss_start = .;
		*(.bss)
		__bss_end = .;
	}

	. = ALIGN(16);
	.stab		: { *(.stab*) }
	
	. = ALIGN(16);
	.stabstr		: { *(.stabstr*) }
	
	. = ALIGN(4096);
	.cpu_init	: { *(.cpu_init*) }

/DISCARD/ :  {
	        *(.note*);
	        *(.comment);
        }
}

