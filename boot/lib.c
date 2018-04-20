/* lib.c */
#include "code16gcc.h"
#include "mgtype.h"
#include "lib.h"

/* return base ^ p */
_umax __NOINLINE__ __REGPARM__ _pow32(_umax base, _u8 p) {
	_umax res = 1;
	_u8 i;
	
	for(i = 0; i < p; i++)
		res = res * base;
	
	return res;
}

/* compare two parts of memory with size 'sz' and return 0 if equal */
_u32 __NOINLINE__ __REGPARM__ mem_cmp(_u8 *p1, _u8 *p2, _u32 sz) {
	_u32 _sz = sz;
	_u32 i = 0;
	
	while(_sz) {
		if(*(p1 + i) != *(p2 + i))
			break;
			
		i++;
		_sz--;
	}
	
	return _sz;	
}

void __NOINLINE__ __REGPARM__ mem_set(_u8 *ptr, _u8 pattern, _u16 sz) {
 	_u16 _sz = sz;
	_u16 i = 0;
	
	while(_sz) {
	 	*(ptr + i) = pattern;
		i++;
		_sz--;
	}
}

/* copy 'sz' bytes from 'src' buffer to 'dst' buffer */
void __NOINLINE__ __REGPARM__ mem_cpy(_u8 *dst, _u8* src, _u32 sz) {
	_u32 _sz = sz;
	_u32 i = 0;
	
	while(_sz) {
		*(dst + i) = *(src + i);
		i++;
		_sz--;	
	}
}

_u16 __NOINLINE__ __REGPARM__ str_len(_str_t str) {
	_u16 i = 0;
	
	while(*(str+i))
		i++;
		
	return i;	
}

_u32 __NOINLINE__ __REGPARM__ txt_cmp(_str_t p1, _str_t p2, _u32 sz) {
	_u32 _sz = sz;
	_u32 i = 0;
	
	_char_t b1,b2;
	
	while(_sz) {
		b1 = *(p1 + i);
		b2 = *(p2 + i);
		
		if(b1 >= 'a' && b1 <= 'z')
			b1 += 0x20; /* to upper case */

		if(b2 >= 'a' && b2 <= 'z')
			b2 += 0x20; /* to upper case */
			
		if(b1 != b2)
			break;
			
		i++;
		_sz--;		
	}	
	
	return _sz;
}

_u32 __NOINLINE__ __REGPARM__ str2i(_str_t str, _s8 sz) {
	_u32 r = 0;
	_s8 _sz = sz-1;
	_u32 m = 1;
	
	while(_sz >= 0) {
		if(*(str + _sz) >= '0' && *(str + _sz) <= '9') {
			r += (*(str + _sz) - 0x30) * m;
			m *= 10;
		}
		
		_sz--;
	}
	
	return r;
}
