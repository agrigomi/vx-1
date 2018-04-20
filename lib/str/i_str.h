#ifndef __I_STR_H__
#define __I_STR_H__

#include "mgtype.h"

#define I_STR	"i_str"

typedef struct {
	void (*mem_cpy)(void *dst, void *src, _u32 sz);
	_s32 (*mem_cmp)(void *, void *, _u32);
	void (*mem_set)(void *, _u8, _u32);
	_u32 (*str_len)(_str_t);
	_s32 (*str_cmp)(_str_t, _str_t);
	_s32 (*str_ncmp)(_str_t, _str_t, _u32);
	_u32 (*str_cpy)(_str_t dst, _str_t src, _u32 sz);
	_u32 (*vsnprintf)(_str_t buf, _u32 sz, _cstr_t fmt, va_list args);
	_u32 (*snprintf)(_str_t buf, _u32 sz, _cstr_t fmt, ...);
	_s32 (*find_string)(_str_t text, _str_t str);
	_str_t (*toupper)(_str_t);
	_u32 (*str2i)(_str_t str, _s32 sz);
	void (*trim_left)(_str_t);
	void (*trim_right)(_str_t);
	void (*clrspc)(_str_t);
	_u8 (*div_str)(_str_t str, _str_t p1, _u32 sz_p1, _str_t p2, _u32 sz_p2, _cstr_t div);
	_u8 (*div_str_ex)(_str_t str, _str_t p1, _u32 sz_p1, _str_t p2, _u32 sz_p2, _cstr_t div, _s8 start_ex, _s8 stop_ex);
	_u32 (*wildcmp)(_str_t text, _str_t wild);
	_str_t (*itoa)(_s32 n, _str_t str, _u8 base);
	_str_t (*uitoa)(_u32 n, _str_t str, _u8 base);
	_str_t (*ulltoa)(_u64 n, _str_t str, _u8 base);
}_i_str_t;

#endif
