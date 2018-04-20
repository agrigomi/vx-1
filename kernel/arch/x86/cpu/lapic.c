#include "i_lapic.h"
#include "i_repository.h"
#include "msr.h"
#include "addr.h"
#include "err.h"

/* Local apic register offset (in bytes) */
#define LAPIC_ID		0x20
#define LAPIC_VERSION		0x30
#define LAPIC_TPR		0x80  /* task priority */
#define LAPIC_APR		0x90  /* arbitration priority */
#define LAPIC_PPR		0xa0  /* processor priority */
#define LAPIC_EOI		0xb0  /* End of interrupt */
#define LAPIC_RRR		0xc0  /* remote read */
#define LAPIC_LDR		0xd0  /* logical destination register */
#define LAPIC_DFR		0xe0  /* destination format register */
#define LAPIC_SRI		0xf0  /* spurious interrupt */
#define LAPIC_ESR		0x280 /* error status register */
#define LAPIC_LVT_CMCI		0x2f0
#define LAPIC_ICR_LO		0x300 /* interrupt command 0-31 */
#define LAPIC_ICR_HI		0x310 /* interrupt command 32-63 */
#define LAPIC_LVT_TIMER		0x320
#define LAPIC_LVT_TS		0x330 /* thermal sensor */
#define LAPIC_LVT_LINT0		0x350
#define LAPIC_LVT_LINT1		0x360
#define LAPIC_LVT_ERROR		0x370
#define LAPIC_ITC		0x380 /* initial timer count */
#define LAPIC_CTC		0x390 /* current timer count */
#define LAPIC_DIVIDE_TIMER	0x3e0
#define LAPIC_EX_FEATURES	0x400
#define LAPIC_EX_CONTROL	0x410

/* ICR delivery modes */
#define ICR_DMODE_FIXED		0x00
#define ICR_DMODE_LOW_PRI	0x01
#define ICR_DMODE_SMI		0x02
#define ICR_DMODE_NMI		0x04
#define ICR_DMODE_INIT		0x05
#define ICR_DMODE_STARTUP	0x06
#define ICR_DMODE_EXTINT	0x07
/* ICR delivery status */
#define ICR_DSTATE_IDLE		0x00
#define ICR_DSTATE_PENDING	0x01
/* ICR destination modes */
#define ICR_DST_MODE_PHYS	0x00
#define ICR_DST_MODE_LOGIC	0x01
/* destination models */
#define DFR_MODEL_FLAT		0x0f
#define DFR_MODEL_CLUSTER	0x00
/* ICR level */
#define ICR_LEVEL_DEASSERT	0x00
#define ICR_LEVEL_ASSERT	0x01
/* ICR shorthand */
#define ICR_SHORTHAND_NONE	0x00
#define ICR_SHORTHAND_SELF	0x01
#define ICR_SHORTHAND_ALL_IN	0x02
#define ICR_SHORTHAND_ALL_EX	0x03
/* ICR trigger mode */
#define ICR_TRIGGER_EDGE	0x00
#define ICR_TRIGGER_LEVEL	0x01
/* RDCR (Timer divide) values */
#define TDCR_DIV2		0x00
#define TDCR_DIV4		0x01
#define TDCR_DIV8		0x02
#define TDCR_DIV16		0x03
#define TDCR_DIV32		0x08
#define TDCR_DIV64		0x09
#define TDCR_DIV128		0x0a
#define TDCR_DIV1		0x0b

typedef union {
	_u32 value;
	struct {/* Local APIC ID */
		_u32 			:24;	/* reserved */
		_u8	lapic_id;		/* local apic ID */
	}__attribute__ ((packed));
	struct { /* version */
		_u8	lapic_version;		/* version */
		_u32			:8;	/* reserved */
		_u8	max_lvt_entry;		/* Max. LVT entry */
		_u32			:8; 	/* reserved */
	}__attribute__ ((packed));
	struct { /* spurious interrupt vector */
		_u8	sri_vector;
		_u32	lapic_enable	:1;  	/* enable/disable bit */
		_u32	focus_checking	:1;  	/* focus processor checking */
		_u32			:22; 	/* reserved */
	}__attribute__ ((packed));
	struct {/* timer divide configuration register */
		_u32	tdcr_divider	:4;  	/* divider */
		_u32			:28; 	/* reserved */
	}__attribute__ ((packed)); 
	struct { /* LVT error */
		_u8	lvt_err_vector;
		_u32			:4;	/* reserved */
		_u32	lvt_err_delivs	:1;	/* delivery sattus (read only) */
		_u32			:3;	/* reserved */
		_u32	lvt_err_masked	:1;	/* 0:not masked / 1:masked */
		_u32			:15;	/* reserved */
	}__attribute__ ((packed));
	struct { /* LVT lint */
		_u8	lvt_lint_vector;
		_u32	lvt_lint_dmode	:3;	/* delivery mode */
		_u32			:1;	/* reserved */
		_u32	lvt_lint_dstate	:1;	/* delivery status (read only) */
		_u32	lvt_lint_intpp	:1;	/* Interrupt input pin polarity */
		_u32	lvt_lint_irr	:1;	/* remote IRR (read only) */
		_u32	lvt_lint_tm	:1;	/* trigger mode */
		_u32	lvt_lint_masked	:1;	/* 0:not masked / 1:masked */
 		_u32			:15;	/* reserved */
	}__attribute__ ((packed));
	struct { /* LVT timer register */
		_u8	lvt_tm_vector;		/* timer vector */
		_u32			:4;	/* reserved */
		_u32	lvt_tm_dstate	:1;	/* delivery status(read only) */
		_u32			:3;	/* reserved */
		_u32	lvt_tm_masked	:1;	/* interrupt mask */
		_u32	lvt_tm_mode	:1;	/* timer mode (0:one-shot / 1:periodic) */
		_u32			:14;	/* reserved */
	}__attribute__ ((packed));
	struct { /* Task Priority Register */
		_u32	tpr_sub_class	:4;	/* task priority sub-class */
		_u32	tpr_priority	:4;	/* task priority */
		_u32			:24;	/* reserved */
	}__attribute__ ((packed));
	struct { /* Interrupt command register LO */
		_u8	icr_vector;
		_u32	icr_del_mode	:3;	/* delivery mode */
		_u32	icr_dst_mode	:1;	/* destination mode */
		_u32	icr_del_state	:1;	/* delivery status */
		_u32			:1;	/* reserved */
		_u32	icr_level	:1;	/* 0:de-assert/ 1:assert */
		_u32	icr_trigger_mode:1;	/* 0:edge / 1:level */
		_u32			:2;	/* reserved */
		_u32	icr_shorthand	:2;	/* destination shorthand */
		_u32			:12;	/* reserved */
	}__attribute__ ((packed));
	struct { /* Interrupt Command Register HI */
		_u32			:24;	/* reserved */
		_u8	icr_dst_field;		/* destination field */
	}__attribute__ ((packed));
	struct { /* local destination register (LDR) */
		_u32			:24;	/* reserved */
		_u8	ldr_apic_id;		/* local apic id */
	}__attribute__ ((packed));
	struct { /* destination format register (DFR) */
		_u32			:28;	/* reserved */
		_u32	dfr_model	:4;	/* destination model */
	}__attribute__ ((packed));
	struct { /* error status register (ESR) */
		_u32	esr_send_chrcksum	:1;
		_u32	esr_receive_checksum	:1;
		_u32	esr_send_accept		:1;
		_u32	esr_receive_accept	:1;
		_u32				:1;
		_u32	esr_send_illegal_vector	:1;
		_u32	esr_receive_illegal_vector :1;
		_u32	esr_illegal_register	:1;
		_u32				:24;
	}__attribute__ ((packed));
} _lapic_register_t;

typedef struct {
	/* local APIC base address */
	_u32 *p_reg_base;
	_u8   id; /* local APIC id */
	_u32  error;
}_lapic_dc_t; /* data context of local APIC */

/* read virtual address of LAPIC base32 */
#define LAPIC_BASE()	((_u32 *)p_to_k((msr_read(MSR_APIC_BASE) & 0x00000000fffff000)));
/* make pointer to LAPIC register */
#define LAPIC_REG_PTR(base, offset) ((_u32 *)((_u32 *)base + (offset >> 2)))

/* offset to register pointer conversion */
#define REG_PTR(offset)	LAPIC_REG_PTR(pdc->p_reg_base, offset)

static void reg_get(_lapic_dc_t *pdc, _u16 reg, _lapic_register_t *data) {
	data->value = *REG_PTR(reg);
}
static void reg_set(_lapic_dc_t *pdc, _u16 reg, _lapic_register_t *data) {
	*REG_PTR(reg) = data->value;
}

static _bool lapic_error_code(_p_data_t dc) {
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	_bool r = _false;

	if(pdc) {
		_lapic_register_t reg;

		reg_get(pdc, LAPIC_ESR, &reg);
		r = ((pdc->error = reg.value) & 0x000000ef) ? _false : _true;
	}
	return r;
}

static _u32 lapic_get_error_code(_p_data_t dc) {
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	return pdc->error;
}

static void lapic_init(_p_data_t dc) {
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	if(pdc) {
		_lapic_register_t reg;

		/* ID */
		reg_get(pdc, LAPIC_ID, &reg);
		pdc->id = reg.lapic_id;

		/* LVT error */
		reg_get(pdc, LAPIC_LVT_ERROR, &reg);
		reg.lvt_err_masked = 1;
		reg_set(pdc, LAPIC_LVT_ERROR, &reg);

		/* LVT INT0 */
		reg_get(pdc, LAPIC_LVT_LINT0, &reg);
		reg.lvt_lint_masked = 1;
		reg_set(pdc, LAPIC_LVT_LINT0, &reg);

		/* LVT INT1 */
		reg_get(pdc, LAPIC_LVT_LINT1, &reg);
		reg.lvt_lint_masked = 1;
		reg_set(pdc, LAPIC_LVT_LINT1, &reg);

		/* task priority */
		reg_get(pdc, LAPIC_TPR, &reg);
		reg.tpr_sub_class = 0;
		reg.tpr_priority = 0;
		reg_set(pdc, LAPIC_TPR, &reg);

		/* spurious interrupt */
		reg_get(pdc, LAPIC_SRI, &reg);
		reg.focus_checking = 1;
		reg.lapic_enable = 0;
		reg_set(pdc, LAPIC_SRI, &reg);

		/* command register */
		reg_get(pdc, LAPIC_ICR_LO, &reg);
		reg.icr_del_mode = ICR_DMODE_INIT;
		reg.icr_dst_mode = ICR_DST_MODE_PHYS;
		reg.icr_level = ICR_LEVEL_DEASSERT;
		reg.icr_trigger_mode = ICR_TRIGGER_LEVEL;
		reg.icr_shorthand = ICR_SHORTHAND_ALL_IN;
		reg_set(pdc, LAPIC_ICR_LO, &reg);

		/* timer divide configuration register */
		reg_get(pdc, LAPIC_DIVIDE_TIMER, &reg);
		reg.tdcr_divider = TDCR_DIV1;
		reg_set(pdc, LAPIC_DIVIDE_TIMER, &reg);

		/* LVT timer */
		reg_get(pdc, LAPIC_LVT_TIMER, &reg);
		reg.lvt_tm_vector = 0;
		reg.lvt_tm_mode = APIC_TIMER_PERIODIC;
		reg.lvt_tm_masked = 0;
		reg_set(pdc, LAPIC_LVT_TIMER, &reg);
		
		reg_get(pdc, LAPIC_CTC, &reg);
		reg.value = 0;
		reg_set(pdc, LAPIC_CTC, &reg);

		/* local destination register */
		reg_get(pdc, LAPIC_LDR, &reg);
		reg.ldr_apic_id = pdc->id;
		reg_set(pdc, LAPIC_LDR, &reg);

		/* destination format register */
		reg_get(pdc, LAPIC_DFR, &reg);
		reg.dfr_model = DFR_MODEL_FLAT;
		reg_set(pdc, LAPIC_DFR, &reg);
	}
}

static void lapic_enable(_p_data_t dc) {
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	if(pdc) {
		_lapic_register_t reg;

		reg_get(pdc, LAPIC_SRI, &reg);
		reg.lapic_enable = 1;
		reg_set(pdc, LAPIC_SRI, &reg);
	}
}
static void lapic_disable(_p_data_t dc) {
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	if(pdc) {
		_lapic_register_t reg;

		reg_get(pdc, LAPIC_SRI, &reg);
		reg.lapic_enable = 0;
		reg_set(pdc, LAPIC_SRI, &reg);
	}
}
static _p_data_t lapic_get_base(_p_data_t dc) {
	_p_data_t r = NULL;
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	if(pdc)
		r = pdc->p_reg_base;
	return r;
}
static _u16 lapic_get_id(_p_data_t dc) {
	_u16 r = 0;
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	if(pdc)
		r = pdc->id;
	return r;
}
static void lapic_set_id(_p_data_t dc, _u16 id) {
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	if(pdc) {
		_lapic_register_t reg;

		reg_get(pdc, LAPIC_ID, &reg);
		reg.lapic_id = (_u8)id;
		reg_set(pdc, LAPIC_ID, &reg);
	}
}
static _u8 lapic_get_version(_p_data_t dc) {
	_u8 r = 0;
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	_lapic_register_t reg;

	if(pdc) {
		reg_get(pdc, LAPIC_VERSION, &reg);
		r = reg.lapic_version;
	}
	return r;
}
static void lapic_eoi(_p_data_t dc) {
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	if(pdc) {
		_lapic_register_t reg;
		reg.value = 0;
		reg_set(pdc, LAPIC_EOI, &reg);
	}
}
static void lapic_set_timer(_p_data_t dc, _u8 vector, _u32 countdown, _u8 mode) {
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	if(pdc) {
		_lapic_register_t reg;

		reg_get(pdc, LAPIC_LVT_TIMER, &reg);
		reg.lvt_tm_vector = vector;
		reg.lvt_tm_masked = 0;
		reg.lvt_tm_mode = mode;
		reg_set(pdc, LAPIC_LVT_TIMER, &reg);

		reg.value = countdown;
		reg_set(pdc, LAPIC_ITC, &reg);
	}
}
static _u32 lapic_get_timer(_p_data_t dc) {
	_u32 r = 0;
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;
	if(pdc) {
		_lapic_register_t reg;

		reg_get(pdc, LAPIC_CTC, &reg);
		r = reg.value;
	}
	return r;
}
static _bool lapic_send_init_ipi(_p_data_t dc, _u16 dst_cpu_id, _u32 startup_vector) {
	_bool r = _false;
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;

	if(pdc) {
		_lapic_register_t icr_lo;
		_lapic_register_t icr_hi;

		reg_get(pdc, LAPIC_ICR_LO, &icr_lo);
		reg_get(pdc, LAPIC_ICR_HI, &icr_hi);

		icr_lo.icr_del_mode = ICR_DMODE_INIT;
		icr_lo.icr_dst_mode = ICR_DST_MODE_PHYS;
		icr_lo.icr_level = ICR_LEVEL_ASSERT;
		icr_lo.icr_trigger_mode = ICR_TRIGGER_LEVEL;
		icr_lo.icr_shorthand = ICR_SHORTHAND_NONE;
		icr_lo.icr_vector = 0;
		
		icr_hi.icr_dst_field = (_u8)dst_cpu_id;

		reg_set(pdc, LAPIC_ICR_HI, &icr_hi);
		reg_set(pdc, LAPIC_ICR_LO, &icr_lo);

		_u32 timeout = 10000;
		while(timeout--);

		if((lapic_get_version(dc) & 0xf0) != 0x00) {
			_u32 i = 0;
			for(; i < 2; i++) {
				reg_get(pdc, LAPIC_ICR_LO, &icr_lo);
				
				icr_lo.icr_vector = startup_vector >> 12;
				icr_lo.icr_del_mode = ICR_DMODE_STARTUP;
				icr_lo.icr_dst_mode = ICR_DST_MODE_PHYS;
				icr_lo.icr_level = ICR_LEVEL_ASSERT;
				icr_lo.icr_trigger_mode = ICR_TRIGGER_LEVEL;
				icr_lo.icr_shorthand = ICR_SHORTHAND_NONE;
				
				reg_set(pdc, LAPIC_ICR_LO, &icr_lo);

				timeout = 200;
				while(timeout--);
			}
		}

		r = lapic_error_code(dc);
	}
	return r;
}
static _bool lapic_send_ipi(_p_data_t dc, _u16 dst_cpu_id, _u8 vector) {
	_bool r = _false;
	_lapic_dc_t *pdc = (_lapic_dc_t *)dc;

	if(pdc) {
		_lapic_register_t icr_lo;
		_lapic_register_t icr_hi;

		reg_get(pdc, LAPIC_ICR_LO, &icr_lo);
		reg_get(pdc, LAPIC_ICR_HI, &icr_hi);

		icr_lo.icr_del_mode = ICR_DMODE_FIXED;
		icr_lo.icr_dst_mode = ICR_DST_MODE_PHYS;
		icr_lo.icr_level = ICR_LEVEL_ASSERT;
		icr_lo.icr_trigger_mode = ICR_TRIGGER_LEVEL;
		icr_lo.icr_shorthand = ICR_SHORTHAND_NONE;
		icr_lo.icr_vector = vector;

		icr_hi.icr_dst_field = (_u8)dst_cpu_id;

		reg_set(pdc, LAPIC_ICR_HI, &icr_hi);
		reg_set(pdc, LAPIC_ICR_LO, &icr_lo);
		
		r = lapic_error_code(dc);	
	}
	return r;
}

static _i_lapic_t _g_interface_ = {
	.init		= lapic_init,
	.error_code	= lapic_get_error_code,
	.enable 	= lapic_enable,
	.disable	= lapic_disable,
	.get_base	= lapic_get_base,
	.get_id		= lapic_get_id,
	.set_id		= lapic_set_id,
	.get_version	= lapic_get_version,
	.end_of_interrupt = lapic_eoi,
	.set_timer	= lapic_set_timer,
	.get_timer	= lapic_get_timer,
	.send_init_ipi	= lapic_send_init_ipi,
	.send_ipi	= lapic_send_ipi
};

static _vx_res_t _mod_ctl_(_u32 cmd, ...) {
	_u32 r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);
	
	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
			_i_repository_t *pi_repo = va_arg(args, _i_repository_t*);
			if(pi_repo) {
				_lapic_dc_t *pdc = va_arg(args, _lapic_dc_t*);
				if(pdc) {
					pdc->p_reg_base = LAPIC_BASE();
				}
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
	MOD_LOCAL_APIC,		/* module name */
	I_LAPIC,		/* interface name */
	&_g_interface_,		/* interface pointer */
	NULL,			/* static data context */
	sizeof(_lapic_dc_t),	/* size of data context (for dynamic allocation) */
	_mod_ctl_,		/* pointer to module controll routine */
	1,0,1,			/* version */
	"local APIC"		/* description */
);
