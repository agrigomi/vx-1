#include "boot.h"
#include "bootx86.h"
#include "segment.h"
#include "lib.h"
#include "code16gcc.h"

_str_t err_a20=(_str_t)"Can't enable a20 line !\n";
_str_t err_file=(_str_t)"Can't find file: ";
_str_t err_read=(_str_t)"Error reading file: ";
_str_t err_read_block=(_str_t)"\r\nError reading file block: ";
_str_t err_long_mode=(_str_t)"No long mode support !\r\n";
_str_t err_mem_map=(_str_t)"Can't get memory map by BIOS !\n";
_str_t str_default=(_str_t)"default";
_str_t str_countdown=(_str_t)"countdown";
_str_t str_display=(_str_t)"display";
_str_t str_mask=(_str_t)"=;;;;;";
_str_t str_load_kernel = (_str_t)"\r\nLoading kernel ";
_str_t str_run_kernel_x86 = (_str_t)"Running kernel x86...\r\n";
_str_t str_run_kernel_x64 = (_str_t)"Running kernel x86-64...\r\n";
_u16 _gdt_limit=0;
_u8 _gdt[MAX_GDT_RECORDS*8] __attribute__((aligned(16))) = "";

#include "startup_context.h"
#include "bootx86.h"

_u32 _g_core_address_ = 0x7c00;
_u32 _g_core_size_ = 0;
_u8  _g_core_arch_ = 0;

_core_startup_t _g_sc_;


void halt(void) {
	__asm__ __volatile__("hlt\n");
}

_s32 arch_init(void) {
	_s32 r = 0;
	
	pm_init_gdt();
	if((r = enable_a20()) == -1) 
		print_text(err_a20,str_len(err_a20));
	return r;
}

void __attribute__((optimize("O0"))) __attribute__((section(".cpu_init"))) _cpu_init_vector_x86_64(void) {
	/* restore stack pointer */
	__asm__ __volatile__ (
		"jmp	$0, $_lp_cpu_init_\n" /* normalize CS */
		"nop\n"
		"_lp_cpu_init_:\n"
		"movl 	%0, %%esp\n" 
		: :"m"(_g_sc_.stack_ptr) 
	);
	if(arch_init() != -1)
		/* switch in long mode to call 'void (*core_cpu_init_vector)(_u32 data)' */
		_lm_call((_t_lm_proc *)_g_sc_.cpu_info._cpu._x86.core_cpu_init_vector,
				(void *)_g_sc_.cpu_info._cpu._x86.core_cpu_init_data);
}

