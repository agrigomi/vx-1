/*
 * vxcore.ld
 *
 * Linker script for the x86-64 setup c
*/
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
ENTRY(_start)


CORE_ARCH 	= 0x64;
CORE_ADDR 	= 0x2000;
CORE_VADDR 	= 0xffff800000000000+CORE_ADDR;
CORE_VERSION	= 0x0101;

SECTIONS
{
	. = CORE_VADDR;
	.bstext		: { *(.bstext) }
	.bsdata		: { *(.bsdata) }

	. = ALIGN(16);
	.text		: { *(.text) }
	.text32		: { *(.text32) }
	.group		: { *(.group) }

	. = ALIGN(16);
	.rodata		: { *(.rodata*) }

	. = ALIGN(16);
	.data		: { *(.data*) }

	. = ALIGN(16);
	.stab		: { *(.stab*) }
	
	. = ALIGN(16);
	.stabstr		: { *(.stabstr*) }

	.bss		: {
		__bss_start = .;
		*(.bss)
		__bss_end = .;
	}
	
	.ctors          : {
		start_ctors = .; _start_ctors = .; __start_ctors = .;
		*(.init_array);
		*(.ctors);
		end_ctors = .; _end_ctors = .; __end_ctors = .; 
    		/*KEEP (*(SORT(.ctors.*)))
    		KEEP (*(.ctors))*/
  	}

       .dtors          : {
		start_dtors = .; _start_dtors = .; __start_dtors = .;
		*(.dtors);
		*(.fini_array);
		end_dtors = .; _end_dtors = .; __end_dtors = .;
                /*KEEP (*(SORT(.dtors.*)))
                KEEP (*(.dtors))*/
       }

	.vxmod : { 
		PROVIDE_HIDDEN(_vxmod_start_ = .);
		KEEP(*(.vxmod))
		PROVIDE_HIDDEN(_vxmod_end_ = .);
	}
        /DISCARD/ : {
                *(.stab*)
                *(.comment)
                *(.note)
        }
        
	_GLOBAL_OFFSET_TABLE_ = .;
        all : {*(.text, .rodata, .rdata, .data, .stab, .stabstr, .bss, .bstext, .bsdata, .ctors, .init_array, .dtors, .fini_array) }
	
	. = ALIGN(16);
	_end = .;
}
