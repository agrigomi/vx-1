#ifndef __MSR_H__
#define __MSR_H__

#include "mgtype.h"

#define MST_TCS		0x00000010	/* time stamp counter */
#define MSR_APIC_BASE	0x0000001b	/* local apic base */
#define MSR_MPERF	0x000000e7	/* efective frequency */

_u64 msr_read(_u64 msr);	
void msr_write(_u64 msr, _u64 value);

union _msr_apic_base_t {
	_u64	value;
	struct {
		_u64		   :8;	/* reserved */
		_u64 bsp	   :1;	/* BSP processor */
		_u64		   :1;	/* reserved */
		_u64 x2apic_enable :1;  /* 1-enable/0-disable X2apic mode */
		_u64 apic_enable   :1;  /* enable/disable local apic */
		_u64 apic_base	   :24; /* base address of local apic */
		_u64 		   :28; /* reserved */
	};
};

#endif

