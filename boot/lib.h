#ifndef __BOOT_LIB__
#define __BOOT_LIB__

#include "boot.h"

#define KEY_UP_ARROU	0x4800
#define KEY_DOWN_ARROW	0x5000

/* lib */
_u32 __NOINLINE__ __REGPARM__ _pow32(_u32 base, _u8 p);
_u32 __NOINLINE__ __REGPARM__ mem_cmp(_u8 *p1, _u8 *p2, _u32 sz);
void __NOINLINE__ __REGPARM__ mem_cpy(_u8 *dst, _u8* src, _u32 sz);
_u16 __NOINLINE__ __REGPARM__ str_len(_str_t str);
_u32 __NOINLINE__ __REGPARM__ str2i(_str_t str, _s8 sz);
void __NOINLINE__ __REGPARM__ mem_set(_u8 *ptr, _u8 pattern, _u16 sz);
_u32 __NOINLINE__ __REGPARM__ txt_cmp(_str_t p1, _str_t p2, _u32 sz);

#endif
