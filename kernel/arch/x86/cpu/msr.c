#include "msr.h"

_u64 msr_read(_u64 msr) {
	_ulong r = 0;

	__asm__ __volatile__ (
		"rdmsr\n"
		:"=a"(r)
		:"c"(msr)
	);

	return r;
}
	
void msr_write(_u64 msr, _u64 value) {
	__asm__ __volatile__ (
		"wrmsr\n"
		:
		:"c"(msr), "a"(value)
	);
}
