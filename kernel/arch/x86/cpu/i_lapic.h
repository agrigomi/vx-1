#ifndef __I_LAPIC_H__
#define __I_LAPIC_H__

#include "vxmod.h"

#define I_LAPIC		"i_lapic"

#define APIC_TIMER_ONESHOT	0
#define APIC_TIMER_PERIODIC	1

typedef _p_data_t  _lapic_cxt_t;

typedef struct {
	void (*init)(_lapic_cxt_t);
	_p_data_t (*get_base)(_lapic_cxt_t); /* return physical LAPIC base address */
	_u32 (*error_code)(_lapic_cxt_t);
	void (*enable)(_lapic_cxt_t);
	void (*disable)(_lapic_cxt_t);
	_u16 (*get_id)(_lapic_cxt_t);
	void (*set_id)(_lapic_cxt_t, _u16 id);
	_u8  (*get_version)(_lapic_cxt_t);
	void (*set_sri)(_lapic_cxt_t, _u8 vector); /* set spurious interrupt vector */
	void (*set_timer)(_lapic_cxt_t, _u8 vector, _u32 countdown, _u8 mode);
	_u32 (*get_timer)(_lapic_cxt_t);
	void (*end_of_interrupt)(_lapic_cxt_t);
	_bool (*send_init_ipi)(_lapic_cxt_t, _u16 dst_cpu_id, _u32 startup_vector); /* setup another CPU */
	_bool (*send_ipi)(_lapic_cxt_t, _u16 dst_cpu_id, _u8 vector);
}_i_lapic_t;

#endif

