#include "compiler.h"
#include "bootx86.h"
#include "segment.h"
#include "lib.h"
#include "boot.h"
#include "code16gcc.h"

void __attribute__((optimize("Os"))) init_context(void) {
	_u16 sz_mem_map=sizeof(_g_sc_.mmap); /* memory map in bytes */
	
	/* fix the stack pointer */
	__asm__ __volatile__("movl %%esp, %0" :"=m"(_g_sc_.stack_ptr):);
	
	/* get memory map */
	_g_sc_.mm_cnt = bios_get_memory_map_e820((_u8 *)&_g_sc_.mmap[0], sz_mem_map, sizeof(_mem_tag_t));

	if(!_g_sc_.mm_cnt) {
	 	print_text(err_mem_map,str_len(err_mem_map));
		halt();
	}

	/* fs */
	_g_sc_.dev = _bios_device_;
	mem_cpy(_g_sc_.partition, &_pt_entry_, sizeof(_g_sc_.partition));
	fs_get_serial(_g_sc_.volume_serial, sizeof(_g_sc_.volume_serial));

	/* I/O */
	_g_sc_.rmap[_g_sc_.rm_cnt].address = 0x00;
	_g_sc_.rmap[_g_sc_.rm_cnt].size = 0x1000;
	_g_sc_.rmap[_g_sc_.rm_cnt].type = MEM_TYPE_RESERVED;
	_g_sc_.rm_cnt++;

	/* add stack area to memory map */
	_g_sc_.rmap[_g_sc_.rm_cnt].address = 0x1000;
	_g_sc_.rmap[_g_sc_.rm_cnt].size = START_ADDRESS - 0x1000;
	_g_sc_.rmap[_g_sc_.rm_cnt].type = MEM_TYPE_STACK;
	_g_sc_.rm_cnt++;

	/* add boot area in memory map */
	_g_sc_.rmap[_g_sc_.rm_cnt].address = START_ADDRESS;
	_g_sc_.rmap[_g_sc_.rm_cnt].size = (_u32)(_g_core_address_ - START_ADDRESS);
	_g_sc_.rmap[_g_sc_.rm_cnt].type = MEM_TYPE_BOOT_CODE;
	_g_sc_.rm_cnt++;
	
	/* add core area to memory map */
	_g_sc_.rmap[_g_sc_.rm_cnt].address = _g_core_address_;
	_g_sc_.rmap[_g_sc_.rm_cnt].size = _g_core_size_;
	_g_sc_.rmap[_g_sc_.rm_cnt].type = MEM_TYPE_CORE_CODE;
	_g_sc_.rm_cnt++;
	
	/* GDT */
	_g_sc_.cpu_info._cpu._x86.p_gdt = (_u32)_gdt;
	_g_sc_.cpu_info._cpu._x86.gdt_limit = _gdt_limit;

	/* CPU info by default */
	/* the bootloader knows only about the current CPU */
	_g_sc_.cpu_info.nsock = 1; /* one socket */
	_g_sc_.cpu_info._cpu._x86.nlcpu = 1;
	_g_sc_.cpu_info._cpu._x86.ncore = 1;
	/*_g_sc_.cpu_info._cpu._x86.iapic_id = 0; */

	_g_sc_.cpu_info._cpu._x86.code_selector=FLAT_CODE_SEGMENT_64;
	_g_sc_.cpu_info._cpu._x86.data_selector=FLAT_DATA_SEGMENT_64;
	_g_sc_.cpu_info._cpu._x86.free_selector = FREE_SEGMENT_INDEX;
}

_u8 __attribute__((optimize("O0"))) load_kernel(void *finfo) {
	_u8 r = FSERR;

	_u32 blocks = fs_get_file_blocks(finfo);
	if(fs_read_file_block(finfo, 0, CORE_SPACE_BEGIN) == 0) {
		_u8 *arch = (_u8 *)(CORE_SPACE_BEGIN + CORE_OFFSET_ARCH);
		_u32 *addr = (_u32 *)(CORE_SPACE_BEGIN + CORE_OFFSET_ADDR);
		_g_core_arch_ = *arch;
		_g_core_address_ = *addr;
		if(fs_read_file(finfo, _g_core_address_) == blocks) {
			_g_core_size_ = blocks * get_unit_size();
			r = 0;
		}
	}

	return r;
}

BEGIN_CODE32;
void __attribute__((optimize("Os"))) _start_kernel(void _UNUSED_ *p_data) {
	_pml4_info_t pi;

	_g_sc_.pt_address = PML4_PT_ADDRESS;

	pi.pt_addr = (_u8 *)PML4_PT_ADDRESS;
	pi.p_sc = &_g_sc_;
	
	_pm_build_pml4_2m(&pi);
	_lm_call32((_t_lm_proc *)_g_core_address_, &_g_sc_);
}

END_CODE32;

void  __attribute__((optimize("O0"))) start_kernel(_str_t arg) {
	_u32 i;
	_u64 mb=0;

	mem_set((_u8 *)&_g_sc_, 0, sizeof(_core_startup_t));

	arch_init();
	init_context();

	_g_sc_.p_kargs = (_u64)(_u32)arg;
	_g_sc_.cpu_info._cpu._x86.cpu_init_vector_rm = (_u32) _cpu_init_vector_x86_64;
	
	/*_build_pml4((_u8 *)PML4_PT_ADDRESS, &_g_sc_); */
	
	print_text("\r\n", 2);
	for(i = 0; i < _g_sc_.mm_cnt; i++) {
		_u64 a = _g_sc_.mmap[i].address;
		_u64 s = _g_sc_.mmap[i].size;
		_u16 t = _g_sc_.mmap[i].type;
		print_qword((_u32 *)&a);
		print_char('/');
		print_qword((_u32 *)&s);
		print_char('/');
		print_word(t);
		print_text("\r\n", 2);

		if(t == 1) {
			mb += s;
		}
	}

	mb /= (1024*1024);
	print_qword((_u32 *)&mb);
	print_text(" MB\r\n", 5);		
	
	print_text((_str_t)"---\r\n",5);
	for(i = 0; i < _g_sc_.rm_cnt; i++) {
		print_qword((_u32 *)&_g_sc_.rmap[i].address);
		print_char('/');
		print_qword((_u32 *)&_g_sc_.rmap[i].size);
		print_char('/');
		print_dword(_g_sc_.rmap[i].type);
		print_text("\r\n", 2);
	}
	/*asm("hlt");	*/
	
	/*print_text(str_run_kernel_x64, str_len(str_run_kernel_x64)); */
	_pm_call(_start_kernel, 0);
}

