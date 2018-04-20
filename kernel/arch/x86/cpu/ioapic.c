#include "i_ioapic.h"
#include "i_cpu.h"
#include "vxmod.h"
#include "i_repository.h"
#include "mp_scan.h"
#include "addr.h"
#include "err.h"

#define DEFAULT_IO_APIC_ADDR	0xFEC00000

#define IOREGSEL  0x00
#define IOWIN     (0x10U / sizeof(_u32))

#define IOAPICID   0x00U
#define IOAPICVER  0x01U
#define IOAPICARB  0x02U
#define IOREDTBL   0x10U

typedef union __attribute__((packed)) {
	_u32	value;
	struct {
		_u8	reg_addr;   		/* APIC Register Address. */
		_u32 			: 24;  /* Reserved. */
	}__attribute__ ((packed));
	struct { /* I/O Redirection Register. (LO) */
		_u8 	vector;               /* Interrupt Vector. */
		_u32	delivery_mode	: 3;  /* Delivery Mode. */
		_u32	destination_mode: 1;  /* Destination mode. */
		_u32	delivery_status	: 1;  /* Delivery status (RO). */
		_u32	polarity	: 1;  /* Interrupt Input Pin Polarity. */
		_u32	irr 		: 1;  /* Remote IRR (RO). */
		_u32	trigger_mode 	: 1;  /* Trigger Mode. */
		_u32	masked 		: 1;  /* Interrupt Mask. */
		_u32			: 15; /* Reserved. */
	} __attribute__ ((packed));
	struct { /* I/O Redirection Register. (HI) */
		_u32			: 24; /* Reserved. */
		_u8	destination	: 8;  /* Destination Field. */
	} __attribute__ ((packed));
	struct { /* ID register */
		_u32			 : 24;/* reserved */
		_u32	 apic_id	 : 4; /* IO APIC ID. */
		_u32			 : 4; /* Reserved. */
	} __attribute__ ((packed));
}_ioapic_register_t;

/* MP record for I/O APIC */
static _mpc_ioapic_t *_g_mpc_ioapic_ = NULL;

static void *ioapic_get_base(void) {
	void *r = NULL;

	if(_g_mpc_ioapic_)
		r = (void *)p_to_k((_u64)_g_mpc_ioapic_->ptr);
	else
		r = (void *)p_to_k((_u64)DEFAULT_IO_APIC_ADDR);
	return r;
}

static _u32 read(_u8 addr) {
	_ioapic_register_t reg;
	_u32 *base = (_u32 *)ioapic_get_base();
	_u32 r = 0;

	if(base) {
		reg.value = base[IOREGSEL];
		reg.reg_addr = addr;
		base[IOREGSEL] = reg.value;
		r = base[IOWIN];
	}

	return r;
}

static void write(_u8 addr, _u32 value) {
	_ioapic_register_t reg;
	_u32 *base = (_u32 *)ioapic_get_base();
	if(base) {
		reg.value = base[IOREGSEL];
		reg.reg_addr = addr;
		base[IOREGSEL] = reg.value;
		base[IOWIN] = value;
	}
}

static _u8 ioapic_get_id(void) {
	_u8 r = 0;

	if(_g_mpc_ioapic_)
		r = _g_mpc_ioapic_->id;
	else {
		_ioapic_register_t reg;
		reg.value = read(IOAPICID);
		r = reg.apic_id;
	}

	return r;
}

static _u8 ioapic_get_version(void) {
	_u8 r = 0;
	if(_g_mpc_ioapic_)
		r = _g_mpc_ioapic_->version;
	else {
		/* ??? */
	}

	return r;
}

static _u8 ioapic_get_flags(void) {
	_u8 r = 1;
	if(_g_mpc_ioapic_)
		r = _g_mpc_ioapic_->flags;
	return r;
}

static void ioapic_set_irq(_u8 irq, _u16 dst_cpu, _u8 vector, _u32 flags) {
	_ioapic_register_t reg_lo;
	_ioapic_register_t reg_hi;

	reg_lo.value = read(IOREDTBL + (irq << 1));
	reg_hi.value = read(IOREDTBL + (irq << 1) + 1);

	reg_hi.destination      = (_u8)dst_cpu;
	reg_lo.vector           = vector;
	reg_lo.delivery_mode    = (flags & IRQF_LO_PRIORITY)   ? 1 : 0;
	reg_lo.destination_mode = (flags & IRQF_DST_LOGICAL)   ? 1 : 0;
	reg_lo.polarity         = (flags & IRQF_POLARITY_LO)   ? 1 : 0;
	reg_lo.trigger_mode     = (flags & IRQF_TRIGGER_LEVEL) ? 1 : 0;
	reg_lo.masked           = (flags & IRQF_MASKED)        ? 1 : 0;

	write(IOREDTBL + (irq << 1), reg_lo.value);
	write(IOREDTBL + (irq << 1) + 1, reg_hi.value);
}

static _i_ioapic_t _g_interface_ = {
	.get_base	= ioapic_get_base,
	.get_id		= ioapic_get_id,
	.get_version	= ioapic_get_version,
	.get_flags	= ioapic_get_flags,
	.set_irq	= ioapic_set_irq
};

static _vx_res_t _mod_ctl_(_u32 cmd, ...) {
	_u32 r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
			if(!_g_mpc_ioapic_) {
				_mp_t *p_mpf = mp_find_table();
				if(p_mpf)
					_g_mpc_ioapic_ = mp_get_ioapic(p_mpf, 0);
			}
			r = VX_OK;
		} break;
		case MODCTL_DESTROY_CONTEXT: {
			r = VX_OK;
		} break;
	}
	va_end(args);

	return r;
}

DEF_VXMOD(
	MOD_IO_APIC,		/* module name */
	I_IO_APIC,		/* interface name */
	&_g_interface_,		/* interface pointer */
	NULL,			/* static data context */
	0,			/* size of data context (for dynamic allocation) */
	_mod_ctl_,		/* pointer to module controll routine */
	1,0,1,			/* version */
	"I/O APIC"		/* description */
);

