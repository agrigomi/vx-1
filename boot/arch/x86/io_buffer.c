#include "boot.h"
#include "bootx86.h"
#include "lib.h"
#include "code16gcc.h"

#define MAX_IO_BUFFER	4
#define IO_ADDR_LIMIT	0xA0000

typedef struct {
	_u16	size;   /* buffer size */
	_u32	offset; /* offset in current data segment */
}__attribute__((packed)) _io_buffer_t;

static _io_buffer_t _g_io_map_[MAX_IO_BUFFER];

void __NOINLINE__ __REGPARM__ io_buffer_init(void) {
	mem_set((_u8 *)&_g_io_map_[0], 0, sizeof(_g_io_map_));
}

void __NOINLINE__ __REGPARM__ *alloc_io_buffer(volatile _u16 size) {
	void *r = 0;
	volatile _u32 last_io_addr = IO_ADDR_LIMIT;
	_u16 i;

	for(i = 0; i < MAX_IO_BUFFER; i++) {
		if(_g_io_map_[i].offset) {
			last_io_addr -= _g_io_map_[i].size;
		} else {
			if(_g_io_map_[i].size) {
				if(size <= _g_io_map_[i].size) {
					_g_io_map_[i].offset = (_u32)(last_io_addr - size);
					r = (void *)_g_io_map_[i].offset;
					break;
				} else
					last_io_addr -= _g_io_map_[i].size;
			} else {
				_g_io_map_[i].offset = (_u32)(last_io_addr - size);
				_g_io_map_[i].size = size;
				r = (void *)_g_io_map_[i].offset;
				break;
			}
		}
	}

	return r;
}

void __NOINLINE__ __REGPARM__ free_io_buffer(void *p_io_buffer) {
	_u16 i = 0;

	for(i = 0; i < MAX_IO_BUFFER; i++) {
		void *ptr = (void *)(_u32)_g_io_map_[i].offset;
		if(ptr == p_io_buffer) {
			_g_io_map_[i].offset = 0;
			break;
		}
	}
}

