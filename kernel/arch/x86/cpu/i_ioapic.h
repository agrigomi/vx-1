#ifndef __I_IOAPIC_H__
#define __I_IOAPIC_H__

#include "mgtype.h"

#define I_IO_APIC	"i_ioapic"

typedef struct {
	void *(*get_base)(void);
	_u8 (*get_id)(void);
	_u8 (*get_version)(void);
	_u8 (*get_flags)(void);
	void (*set_irq)(_u8 irq, _u16 dst_cpu, _u8 vector, _u32 flags);
}_i_ioapic_t;

#endif
