#include "boot.h"
#include "lib.h"
#include "code16gcc.h"

_str_t _boot_config=(_str_t)"/boot/boot.config";

void _main_(void) {
	_u8 finfo[FILE_INFO_SIZE]="";
	_s8 core_file[100]="";
	_s8 core_params[128]="";
	_u16 name_sz = sizeof(core_file);
	_u16 arg_sz = sizeof(core_params);

/*
	_u8 a = '+';
	_u8 b = 2;
	_u16 c = 0xaa55;
	_u32 d = 0x11223344;

	print_char(a);
	print_char(' ');
	print("alabala");
	print_char(' ');
	print_byte(b);
	print_char(' ');
	print_word(c);
	print_char(' ');
	print_dword(d);
*/
	print("\r\n"); // new line	
	io_buffer_init();
	fs_init();

__boot_menu__:
	if(fs_get_file_info(_boot_config, finfo) == 0) {
		_u32 ptr = get_core_space_ptr();
		if(fs_read_file(finfo, ptr)) {
			mem_set((_u8 *)core_file, 0, sizeof(core_file));
			mem_set((_u8 *)core_params, 0, sizeof(core_params));
			name_sz = sizeof(core_file);
			arg_sz = sizeof(core_params);
			clear_screen();
			menu((_u8 *)ptr, fs_get_file_size(finfo), core_file, &name_sz, core_params, &arg_sz);
			clear_screen();
			print_text("\r\n", 2);
			if(fs_get_file_info(core_file, finfo) == 0) {
				if(load_kernel(finfo) == 0) {
					print_text("\r\n", 2);
					start_kernel(core_params);
				}
			} else
				goto __boot_menu__;
		} else
			print("error reading boot config\r\n");
	} else
		print("missing boot configuration file\r\n");

	halt();
	
}
