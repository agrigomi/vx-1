#include "bootx86.h"
#include "boot.h"
#include "code16gcc.h"

void __NOINLINE__ __REGPARM__ print_char(_u8 c) {
	__asm__ __volatile__ ("int  $0x10" : : "a"(0x0E00 | c), "b"(7));
}

/*---- print text with attributes 
input:
   color: 	attribute if string contains only characters (bit 1 of AL is zero).
	0      0000      black
	1      0001      blue
	2      0010      green
	3      0011      cyan
	4      0100      red
	5      0101      magenta
	6      0110      brown
	7      0111      light gray
	8      1000      dark gray
	9      1001      light blue
	A      1010      light green
	B      1011      light cyan
	C      1100      light red
	D      1101      light magenta
	E      1110      yellow
	F      1111      white
   sz:  	number of characters in string (attributes are not counted).
   row,col:  	row,column at which to start writing.
   _ptr: 	points to string to be printed. */
void __NOINLINE__ __REGPARM__ display_text(_str_t _ptr, volatile _u16 sz, 
					volatile _u8 row, volatile _u8 col, 
					volatile _u8 color) {
 	__asm__ __volatile__ (
		"movw	%0,%%cx\n"
		"movb	%1,%%dl\n"
		"movb	%2,%%dh\n"
		"movb	%3,%%bl\n"
		"movl	%4,%%ebp\n"
		"movb	$0x0,%%al\n"
		"movb	$0x0,%%bh\n"
		"movb	$0x13,%%ah\n"
		"int	$0x10\n"
		:
		:"m"(sz),"m"(col),"m"(row),"m"(color),"m"(_ptr)
	);
}

void __NOINLINE__ __REGPARM__ print_text(_str_t p, _u16 sz) {
	volatile _u32 _sz = sz;
	volatile _u32 i = 0;
	
	while(_sz) {
		print_char(*(p + i));
		
		i++;
		_sz--;
	}	
}
void __NOINLINE__ __REGPARM__ print(_str_t s) {
	while(*s) {
		__asm__ __volatile__ ("int  $0x10" : : "a"(0x0E00 | *s), "b"(7));
		s++;
	}
}
/* display byte in hex format */
void __NOINLINE__ __REGPARM__ print_byte(_u8 c) {
	volatile _u8 al = c;

	al = ((c & 0xf0) >> 4) & 0x0f;
	if(al >= 0x0a)
		al += 7;
	al += 0x30;
	print_char(al);
	al = c & 0x0f;
	if(al >= 0x0a)
		al += 7;
	al += 0x30;
	print_char(al);
}
/* display word in hex format */
void __NOINLINE__ __REGPARM__ print_word(_u16 w) {
	volatile _u8 x;
	
	x = (_u8)(w >> 8);
 	print_byte(x);
	x = (_u8)w;
	print_byte(x);
}

/* display double word in hex format */
void __NOINLINE__ __REGPARM__ print_dword(_u32 dw) {
	_u16 x = (_u16)(dw>>16);
	
	print_word(x);
	x = (_u16)dw;
	print_word(x);
}

void __NOINLINE__ __REGPARM__ print_qword(_u32 qw[2]) {
	print_dword(qw[1]);
	print_dword(qw[0]);
}

/*
static _u8 __NOINLINE__ __REGPARM__ get_byte(_u8 *ptr) {
	_u8 r = 0;
	volatile _u16 seg = (_u16)((_u32)ptr / 16);
	volatile _u16 off = (_u16)((_u32)ptr % 16);
	
	__asm__ __volatile__ (
		"movw	%%es, %%cx\n"
		"movw	%[seg], %%ax\n"
		"movw	%%ax, %%es\n"
		"movw	%[off], %%bx\n"
		"movb	%%es:(%%bx), %%al\n"
		"movb	%%al, %[r]\n"
		"movw	%%cx, %%es\n"
		:[r] "=m" (r)
		:[seg] "m" (seg), [off] "m" (off)
	);
	
	return r;
}
*/
/* display hex dump */
void __NOINLINE__ __REGPARM__ print_hex(_u8 *ptr,_u16 sz) {
	volatile _u32 c = sz;
	volatile _u32 i = 0;
	
	while(c--) {
	 	print_byte(*(ptr+i));
		/*_u8 b = get_byte(ptr+i);
		print_byte(b);*/
		print_char(0x20);
		
		i++;
	}	
}

/* hide display cursor */
void hide_cursor(void) {
	__asm__ __volatile__ (
		"movb	$1,%ch\n"
		"shlb	$5,%ch\n"
		"movb	$0,%cl\n"
		"movb	$1,%ah\n"
		"int	$0x10\n"
	);
}

void __NOINLINE__ __REGPARM__ set_cursor_pos(volatile _u8 row, volatile _u8 col) {
	/* set cursor position */
	__asm__ __volatile__ (
		"movb	$2,%%ah\n"
		"movb	$0,%%bl\n"
		"movb	%0,%%dl\n"
		"movb	%1,%%dh\n"
		"int	$0x10\n"
		:
		:"m"(col),"m"(row)
	);
}

void clear_screen(void) {
 	__asm__ __volatile__ (
		"movw	0x7d0,%cx\n"
		"movw	$0x00,%di\n"
		"movw	$0xb800,%ax\n"
		"movw	%ax,%es\n"
		"__write_to_video:\n"
		"movw	$0x0720,%ax\n"
		"movw	%ax,%es:(%di)\n"
		"addw	$0x02,%di\n"
		"decw	%cx\n"
		"cmpw	$0x00,%cx\n"
		"jne	__write_to_video\n"
		"movw	%ds,%ax\n"
		"movw	%ax,%es\n"
	);
}
