#include "mgtype.h"
#include "cpu.h"

#define VENDOR_INTEL	"GenuineIntel"
#define VENDOR_AMD	"AuthenticAMD"

typedef struct {
	_u32 eax;
	_u32 ebx;
	_u32 ecx;
	_u32 edx;
}_cpuid_reg_t;

static _s32  _mem_cmp(void *_p1, void *_p2, _u32 sz) {
	_s32 r = 0;
	_u32 _sz = sz;
	_u32 i = 0;
	_u8 *p1 = (_u8 *)_p1;
	_u8 *p2 = (_u8 *)_p2;
	
	while(_sz) {
		if((r = *(p1 + i) - *(p2 + i)))
			break;
		i++;
		_sz--;
	}
	
	return r;
}

static void reg_clr(_cpuid_reg_t *reg) {
	reg->eax = reg->ebx = reg->ecx = reg->edx = 0;
}

static void cpu_id(_cpuid_reg_t *reg) {
	__asm__ __volatile__(
		"cpuid\n"
		: "=a" (reg->eax), "=b" (reg->ebx), "=c" (reg->ecx), "=d" (reg->edx)
		:  "a" (reg->eax), "b" (reg->ebx), "c" (reg->ecx), "d" (reg->edx)
	);
}

/* require min 12 byte by output */
void cpu_vendor_id(_s8 *out) {
	_cpuid_reg_t vend;
	reg_clr(&vend);
	cpu_id(&vend);
	*(_u32 *)(out + 0) = vend.ebx;
	*(_u32 *)(out + 4) = vend.edx;
	*(_u32 *)(out + 8) = vend.ecx;
}

_u16 cpu_logical_count(void) {
	_u16 r = 0;
	
	_cpuid_reg_t  lunits;
	reg_clr(&lunits);
	lunits.eax = 1;
	cpu_id(&lunits);
	r = (lunits.ebx & 0xff0000) >> 16;
	return r;
}

_u8 cpu_get_cores(void) {
	_u8 r = 1;
	_cpuid_reg_t func4;

	reg_clr(&func4);
	cpu_id(&func4);

	if(func4.eax >= 4) {
		_s8 vendor[14]="";
		_cpuid_reg_t cores;
		cpu_vendor_id(vendor);
		if(_mem_cmp(vendor, VENDOR_INTEL, 12) == 0) {
			reg_clr(&cores);
			cores.eax = 4;
			cpu_id(&cores);
			r += ((cores.eax & 0xfc000000) >> 26);
		} else {
			if(_mem_cmp(vendor, VENDOR_AMD, 12) == 0) {
				reg_clr(&cores);
				cores.eax = 0x80000008;
				cpu_id(&cores);
				r += (_u8)cores.ecx;
			}
		}
	}
	return r;
}

_u8  cpu_apic_id(void) {
	_u8 r = 0;
	_cpuid_reg_t apics;
	reg_clr(&apics);
	apics.eax = 1;
	cpu_id(&apics);
	r = (apics.ebx & 0xff000000) >> 24;
	return r;
}
