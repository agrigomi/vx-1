#ifndef __BOOT_X86_h__
#define __BOOT_X86_h__

#include "mgtype.h"
#include "startup_context.h"

#define START_ADDRESS		0x000002000
#define CORE_SPACE_BEGIN	0x000007c00
#define CORE_SPACE_END		0x00009fc00
#define PML4_PT_ADDRESS		0x000100000 /* page table start address */
#define SECTOR_SIZE		0x200
#define CORE_OFFSET_ARCH	4
#define CORE_OFFSET_ADDR	5

/* bios */
#define BIOS_FN_READ	0x02
#define BIOS_FN_WRITE	0x03

extern _u8 _fs_super_block_;
extern _u8 _bios_device_;
extern _u8 _pt_entry_;
extern _u8 _pt_indicator_;
extern _u8 _pt_head_;
extern _u8 _pt_sector_;
extern _u8 _pt_cylinder_;
extern _u8 _pt_system_id_;
extern _u8 _pt_end_head_;
extern _u8 _pt_end_sectot_;
extern _u8 _pt_end_cylinder_;
extern _u16 _pt_lba_sector_lo_;
extern _u16 _pt_lba_sector_hi_;
extern _u16 _pt_lba_size_lo_;
extern _u16 _pt_lba_size_hi_;
extern _str_t str_default;
extern _str_t str_countdown;
extern _str_t str_display;
extern _str_t str_mask;
extern _str_t _boot_config;
extern _u8 _gdt[];
extern _u16 _gdt_limit;
extern _str_t err_mem_map;
extern _str_t str_run_kernel_x86;
extern _str_t str_run_kernel_x64;
extern _str_t err_long_mode;
extern _str_t err_a20;
extern _str_t err_read;

/* prototype of protected mode routine */
typedef void _t_pm_proc(void *p_data);
/* prototype of long mode routine */
typedef void _t_lm_proc(void *p_data);
/* prototype of real mode routine */
typedef void _t_rm_proc(void *p_data);
/* core startup context */
extern _core_startup_t _g_sc_;
extern _u32 _g_core_size_;
extern _u8  _g_core_arch_;
extern _u32 _g_core_address_;

/* This structure contains the value of one GDT entry.
 We use the attribute 'packed' to tell GCC not to change
 any of the alignment in the structure. */
typedef struct  {
	_u16	limit_low;           /* The lower 16 bits of the limit. */
	_u16	base_low;            /* The lower 16 bits of the base. */
	_u8  	base_middle;         /* The next 8 bits of the base. */
	_u8	access;              /* Access flags, determine what ring this segment can be used in. */
	_u8	granularity;
	_u8	base_high;           /* The last 8 bits of the base. */
}__attribute__((packed)) _gdt_entry_t;

typedef struct {
	_u16	limit;               /* The upper 16 bits of all selector limits. */
	_u32	base;                /* The address of the first gdt_entry_t structure. */
}__attribute__((packed)) _gdt_ptr_t;

typedef struct {
	volatile _u16	lo;
	volatile _u16	hi;
}__attribute__((packed)) _lba_t;

typedef union {
	_u32 addr;
	struct {
		volatile _u16	offset;
		volatile _u16	segment;
	};
}__attribute__((packed)) _ptr_t;

typedef struct {
	_u8 *pt_addr;
	_core_startup_t *p_sc;
}_pml4_info_t;

void _pm_call(_t_pm_proc *proc,void *p_data);
void _lm_call(_t_lm_proc *proc,void *p_data);
void _lm_call32(_t_lm_proc *proc,void *p_data);
void _lm_cpu_idle(void *p_context);
void _cpu_init_vector(void);
void _cpu_init_vector_x86_64(void);
_s32 arch_init(void);
void pm_init_gdt(void);
int enable_a20(void);
_u16 bios_get_memory_map_e820(_u8 *buffer,_u16 sz, _u16 chunk_sz);
void _pm_build_pml4_2m(_pml4_info_t *p);
void _build_pml4(_u8 *pt_addr, _core_startup_t *p_sc);

/* convert 32 bit address to segment:offset */
void addr2so(_u32 addr, /* in */
		_ptr_t *so /* out */
	    );
/* segment:offset memory copy */
void so_mem_cpy(_ptr_t *dst, _ptr_t *src, _u16 sz);

#define BEGIN_CODE32	asm(".code32\n");
#define END_CODE32	asm(".code16gcc\n");

typedef _u32 addr_t;

#endif

