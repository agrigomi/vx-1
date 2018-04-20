#include "vxmod.h"
#include "startup_context.h"
#include "i_sync.h"
#include "i_repository.h"
#include "i_cpu.h"
#include "i_lapic.h"
#include "i_str.h"
#include "i_memory.h"
#include "i_blender.h"
#include "err.h"
#include "mp_scan.h"
#include "intdef.h"
#include "cpu.h"
/*#define _DEBUG_*/
#include "debug.h"

/* CPU flags */
#define FLAG_C	 (1<<0)  /* carry */
#define FLAG_P	 (1<<2)	 /* parity */
#define FLAG_A	 (1<<4)	 /* Auxiliary */
#define FLAG_Z	 (1<<6)	 /* Zero */
#define FLAG_S	 (1<<7)	 /* Sign */
#define FLAG_T	 (1<<8)	 /* Trap */
#define FLAG_I	 (1<<9)	 /* Interrupt */
#define FLAG_D	 (1<<10) /* Direction */
#define FLAG_O	 (1<<11) /* Overflow */
#define FLAG_N	 (1<<14) /* Nested task */
#define FLAG_R	 (1<<16) /* Resume */
#define FLAG_VM  (1<<17) /* Virtual-8086 Mode */
#define FLAG_AC  (1<<18) /* Alignment Check */
#define FLAG_VIF (1<<19) /* Virtual Interrupt Flag */
#define FLAG_VIP (1<<20) /* Virtual Interrupt Pending */
#define FLAG_ID	 (1<<21) /* ID */

#define RAM_LIMIT		p_to_k(0xffffffff)
#define PAGE_SIZE		4096
#define FPU_STATE_SIZE		512
#define DEFAULT_IST_INDEX	1
#define DEFAULT_STACK_SIZE	PAGE_SIZE
#define DEFAULT_TSS_SIZE	PAGE_SIZE * 2
#define INT_STACK_SIZE		PAGE_SIZE
#define DEFAULT_IDT_SIZE	PAGE_SIZE

/* descriptor type in long mode GDT */
#define CODE_DESC_LM	0x18
#define CODE_DESC_C_LM	0x1c /* conforming */
#define DATA_DESC_LM	0x02
/* system descriptor type in GDT */
#define TSS_64		0x09
#define INT_64		0x0e
#define TRAP_64		0x0f

/* segment mode */
#define SM_32_G0	0x04
#define SM_32_G1	0x0c
#define SM_64_G0	0x02
#define SM_64_G1	0x0a

#define STACK_PAGES	1
#define INT_STACK_PAGES	1

#define MAX_GDT		16
/* segment selector
 WARNING: code and data segment selectors
	 are incoming from boot loader in 'starupt_context' */
#define GDT_TSS_64	10	/* two placess needed by TSS 64 */
#define GDT_LDT_64	12	/*  and LDT 64 system descriptors */

#define CPU_ADDRESS(addr) (_ulong)addr
#define HALT \
	__asm__ __volatile__ ("hlt\n");

/* CPU context */
typedef struct {
	_u64 rax;
	_u64 rbx;
	_u64 rcx;
	_u64 rdx;
	_u64 rsi;
	_u64 rdi;
	_u64 rbp;
	_u64 r8;
	_u64 r9;
	_u64 r10;
	_u64 r11;
	_u64 r12;
	_u64 r13;
	_u64 r14;
	_u64 r15;
	_u64 alignment;   /* align rbp_frame on multiple of 16 */
	_u64 rbp_frame;   /* imitation of frame pointer linkage */
	_u64 rip_frame;   /* imitation of return address linkage */
	_u64 error_word;  /* real or fake error word */
	_u64 rip;
	_u64 cs;
	_u64 rflags;
	_u64 rsp;         /* only if istate_t is from uspace */
	_u64 ss;          /* only if istate_t is from uspace */
}__attribute__((packed)) _cpu_state_t;

typedef union {
	_u64	flags;
	struct {
		_u32	carry		:1;	/* 0  Carry flag */
		_u32			:1;	/* 1 */
		_u32	parity		:1;	/* 2  Parity flag */
		_u32			:1;	/* 3 */
		_u32	adjust		:1;	/* 4  Adjust flag */
		_u32			:1;	/* 5  */
		_u32	zero		:1;	/* 6  Zero flag */
		_u32	sign		:1;	/* 7  Sign flag */
		_u32	trap		:1;	/* 8  Trap flag (single step) */
		_u32	interrupt	:1;	/* 9  Interrupt enable flag */
		_u32	direction	:1;	/* 10 Direction flag */
		_u32	overflow	:1;	/* 11 Overflow flag */
		_u32	iopl		:2;	/* 12-13 I/O privilege level (286+ only), always 11 on 8086 and 186 */
		_u32	nested_task	:1;	/* 14 Nested task flag (286+ only), always 1 on 8086 and 186 */
		_u32			:1;	/* 15  */
		_u32	resume		:1;	/* 16 Resume flag (386+ only) */
		_u32	v8086		:1;	/* 17 Virtual 8086 mode flag (386+ only) */
		_u32	align_check	:1;	/* 18 Alignment check (486SX+ only) */
		_u32	v_int		:1;	/* 19 Virtual interrupt flag (Pentium+) */
		_u32	v_int_pending	:1;	/* 20 Virtual interrupt pending (Pentium+) */
		_u32	id		:1;	/* 21 (Able to use CPUID instruction (Pentium+)) */
		_u32			:10;	/* 22-31 */
		_u32	reserved;
	};
} _cpu_flags_t;

/* prototype of interrupt service routine */
typedef void _isr_t(void);

typedef struct { /* interrupt/trap gate */
	_u16	addr1;		/* first part of code segment offset */
	_u16	cs;		/* code segment selector */
	_u8	ist;		/* Interrupt Stack Table (3 bit) */
	_u8	dtype	:5;
	_u8	dpl	:2;
	_u8	present	:1;
	_u16	addr2;		/* Second patr of the offset */
	_u32	addr3;		/* Tird part of the offset */
	_u32	reserved;
} __attribute__((packed)) _idt_gate_x64_t;

typedef union { /* segment descriptor (legacy mode) */
	_u64 value;
	struct {
		_u16		limit1;		/* segment limit (first part) */
		_u16		base1;		/* base (first part) */
		_u8		base2;		/* base (second part) */
		_u8		dtype	:5;
		_u8		dpl	:2;
		_u8		present	:1;
		unsigned	limit2	:4;	/* limit (second part) */
		unsigned	mode	:4;
		_u8		base3;		/* base (third part) */
	};
}__attribute__((packed)) _sd_x86_t;

typedef struct { /* system segment descriptor */
	_u16		limit1;		/* segment limit */
	_u16		base1;		/* base address (first part) */
	_u8		base2;		/* base address (second part) */
	_u8		dtype	:5;
	_u8		dpl	:2;
	_u8		present	:1;
	unsigned	limit2	:4;	/* segment limit (second part) */
	unsigned	mode	:4;
	_u8		base3;		/* base address (third part) */
	_u32		base4;		/* base address (last part) */
	_u32		reserved;	/* reserved */
}__attribute__((packed)) _ssd_x64_t;

typedef struct { /* task state segment */
	_u32	reserved1;	/* 0 */
	_u64	rsp[3];		/* +4 */
	_u64	ist[8]; 	/* WARNING: do not use ist[0] */
	_u64	reserved2;	/* +92 */
	_u16	reserved3;	/* +100 */
	_u16	io_map_offset;	/* +102 */
}__attribute__((packed)) _tss_x64_t;

typedef struct {
	_u16	limit;               /* The upper 16 bits of all limits. */
	_ulong	base;                /* The address of the first structure. */
}__attribute__((packed)) _dt_ptr_t;

typedef union {
	_u64 value;
	struct {
		_u64	pe	:1;	/* protection enabled  RW */
		_u64	mp	:1;	/* Monitor Coprocessor RW */
		_u64	em	:1;	/* Emulation RW */
		_u64	ts	:1;	/* Task Switched RW */
		_u64	et	:1;	/* Extension Type R */
		_u64	ne	:1;	/* Numeric Error RW */
		_u64		:10;	/* reserved */
		_u64	wp	:1;	/* Write Protect RW */
		_u64		:1;	/* reserved */
		_u64	am	:1;	/* Alignment Mask RW */
		_u64		:10;	/* reserved */
		_u64	nw	:1;	/* Not Writethrough RW */
		_u64	cd	:1;	/* Cache Disable RW (0:enable/1:disable) */
		_u64	pg	:1;	/* Paging RW */
		_u64		:32;	/* reserved */
	};
}__attribute__((packed)) _cr0_t;

typedef union {
	_u64 value;
	struct {
		_u32	vme	:1;	/* Virtual-8086 Mode Extensions */
		_u32	pmi	:1;	/* Protected-Mode Virtual Interrupts */
		_u32	tsd	:1;	/* Time Stamp Disable */
		_u32	de	:1;	/* Debugging Extensions */
		_u32	pse	:1;	/* Page Size Extensions */
		_u32	pae	:1;	/* Physical-Address Extension */
		_u32	mce	:1;	/* Machine Check Enable */
		_u32	pge	:1;	/* Page-Global Enable */
		_u32	pce	:1;	/* Performance-Monitoring Counter Enable */
		_u32	osfxsr	:1;	/* Operating System FXSAVE/FXRSTOR Support */
		_u32	osx	:1;	/* Operating System Unmasked Exception Support */
		_u32		:21;	/* reserved */
		_u32	reserved;	/* reserved */
	};
}__attribute__((packed)) _cr4_t;

static void flags_get(_cpu_flags_t *p_flags) {
	__asm__ __volatile__ (
		"pushfq\n"
		"movq	(%%rsp), %%r8\n"
		"movq	%%r8, %[flags]\n"
		"addq	$8, %%rsp\n"
		:[flags] "=m" (p_flags->flags)
	);
}

static void idt_gate_set_addr(_idt_gate_x64_t *p_gate, _u64 addr) {
	p_gate->addr1 = (_u16)addr;
	p_gate->addr2 = (_u16)(addr >> 16);
	p_gate->addr3 = (_u32)(addr >> 32);
}
static _u64 _UNUSED_ idt_gate_get_addr(_idt_gate_x64_t *p_gate) {
	return ((_u64)p_gate->addr3 << 32) | ((_u64)p_gate->addr2 << 16) | p_gate->addr1;
}
static void idt_gate_init(_idt_gate_x64_t *p_gate, _u64 base, _u16 _cs, _u8 _ist, _u8 _type, _u8 _dpl) {
	p_gate->dtype = _type;		/* set type */
	p_gate->dpl = _dpl;
	p_gate->present = 1;
	p_gate->cs   = _cs << 3;	/* set code selector */
	p_gate->ist  = _ist;		/* set interrupt stack index */
	p_gate->reserved = 0;
	idt_gate_set_addr(p_gate, base);
}

static void sd_set_limit(_sd_x86_t *p_sd, _u32 limit) {
	p_sd->limit1 = (_u16)limit;
	p_sd->limit2 = (_u8)(limit >> 16);
}
static void sd_set_base(_sd_x86_t *p_sd, _u64 base) {
	p_sd->base1 = (_u16)base;
	p_sd->base2 = (_u8)(base >> 16);
	p_sd->base3 = (_u8)(base >> 24);
}
static void sd_init(_sd_x86_t *p_sd, _u8 type, _u8 _dpl, _u8 _mode, _u64 _base, _u32 _limit) {
	p_sd->dtype = type;
	p_sd->dpl = _dpl;
	p_sd->present = 1;
	sd_set_base(p_sd, _base);
	sd_set_limit(p_sd, _limit);
	p_sd->mode = _mode;
}

static void ssd_set_limit(_ssd_x64_t *p_ssd, _u32 limit) {
	p_ssd->limit1 = (_u16)limit;
	p_ssd->limit2 = (_u8)(limit >> 16);
}
static void ssd_set_base(_ssd_x64_t *p_ssd, _u64 base) {
	p_ssd->base1 = (_u16)base;
	p_ssd->base2 = (_u8)(base >> 16);
	p_ssd->base3 = (_u8)(base >> 24);
	p_ssd->base4 = (_u32)(base >> 32);
}
static void ssd_init(_ssd_x64_t *p_ssd, _u8 _type, _u8 _dpl, _u8 _mode, _u64 _base, _u32 _limit) {
	p_ssd->dtype = _type;
	p_ssd->dpl = _dpl;
	p_ssd->present = 1;
	ssd_set_base(p_ssd, _base);
	ssd_set_limit(p_ssd, _limit);
	p_ssd->mode = _mode;
	p_ssd->reserved = 0;
}

static void cr0_get(_cr0_t *p_cr0) {
	__asm__ __volatile__ ("mov %%cr0, %%rax\n" : "=a" (p_cr0->value));
}
static void cr0_set(_cr0_t *p_cr0) {
	__asm__ __volatile__ ("mov %%rax, %%cr0\n" :: "a" (p_cr0->value));
}

static void cr4_get(_cr4_t *p_cr4) {
	__asm__ __volatile__ ("mov %%cr4, %%rax\n" : "=a" (p_cr4->value));
}
static void cr4_set(_cr4_t *p_cr4) {
	__asm__ __volatile__ ("mov %%rax, %%cr4\n" :: "a" (p_cr4->value));
}

_u64 _get_code_selector_(void) {
	return (_u64)(__g_p_core_startup__->cpu_info._cpu._x86.code_selector);
}

/* runtime cpu context flags */
#define RCCF_READY	(1<<0)
#define RCCF_IDLE	(1<<1)

/* global repository interface */
static _i_repository_t *_gpi_repo_ = NULL;
/* string operations interface */
static _i_str_t *_gpi_str_ = NULL;
/* heap context */
static HCONTEXT _ghc_heap_ = NULL;
/* global VMM context */
static HCONTEXT _ghc_vmm_ = NULL;

typedef struct { /* cpu AMD64 data context */
	_cpu_init_info_t cpu_info;
	_idt_gate_x64_t	 *p_idt_gate;
	_u8		 rccf; /* Runtime CPU Context Flags */
	_sd_x86_t	 gdt[MAX_GDT]; /* Glopbal Descriptor Table */
	_u8		 *p_stack; /* stack frame */
	_tss_x64_t	 *p_tss; /* Task State Segment */
	_u32		 tss_limit; /* TSS limit */
	HCONTEXT	 hc_mutex;
	HCONTEXT	 hc_blender; /* blender context */
	HCONTEXT	 hc_vmm; /* VMM module context */
	HCONTEXT	 hc_lapic; /* local APIC */
	_u32 		 new_timer_value;
	_u16		 sys_code_selector;
	_u16		 usr_code_selector;
	_u16		 sys_data_selector;
	_u16		 usr_data_selector;
}_cpu_dc_t;

static HMUTEX lock(_p_data_t dc, HMUTEX hlock) {
	_cpu_dc_t *pdc = dc;
	HMUTEX r = 0;
	if(pdc) {
		_i_mutex_t *pi = HC_INTERFACE(pdc->hc_mutex);
		if(pi)
			r = pi->lock(HC_DATA(pdc->hc_mutex), hlock);
	}

	return r;
}

static void unlock(_p_data_t dc, HMUTEX hlock) {
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		_i_mutex_t *pi = HC_INTERFACE(pdc->hc_mutex);
		if(pi)
			pi->unlock(HC_DATA(pdc->hc_mutex), hlock);
	}
}

static _p_data_t heap_alloc(_u32 size, _ulong limit) {
	_p_data_t r = NULL;
	if(_ghc_heap_) {
		_i_heap_t *pi = HC_INTERFACE(_ghc_heap_);
		if(pi)
			r = pi->alloc(HC_DATA(_ghc_heap_), size, limit);
	}
	return r;
}

static void heap_free(_p_data_t ptr, _u32 size) {
	if(_ghc_heap_) {
		_i_heap_t *pi = HC_INTERFACE(_ghc_heap_);
		if(pi)
			pi->free(HC_DATA(_ghc_heap_), ptr, size);
	}
}

static _bool _is_boot(_p_data_t dc) {
	_bool r = _false;
	_cpu_dc_t *pdc = dc;
	if(pdc->cpu_info.flags & CCPUF_BOOT)
		r = _true;
	return r;
}

static _u16 _cpu_id(_p_data_t dc) {
	_cpu_dc_t *pdc = dc;
	return pdc->cpu_info.id;
}

static _u16 _ccpu_id(_p_data_t dc) {
	_u16 r = 0xffff;
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		if(pdc->hc_lapic) {
			_i_lapic_t *pi = HC_INTERFACE(pdc->hc_lapic);
			_p_data_t pd = HC_DATA(pdc->hc_lapic);
			if(pi && pd)
				r = pi->get_id(pd);
		}
	}
	return r;
}

static HCONTEXT _get_vmm(_p_data_t dc) {
	HCONTEXT r = NULL;
	_cpu_dc_t *pdc = dc;
	if(pdc)
		r = pdc->hc_vmm;

	return r;
}

static HCONTEXT _get_blender(_p_data_t dc) {
	HCONTEXT r = NULL;
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		HMUTEX hlock = lock(dc, 0);
		r = pdc->hc_blender;
		unlock(dc, hlock);
	}
	return r;
}

static _bool _enable_interrupts(_bool enable) {
	_cpu_flags_t flags;
	flags_get(&flags);
	if(enable) {
		__asm__ __volatile__("sti": : :"memory");
	} else {
		__asm__ __volatile__("cli": : :"memory");
	}

	/* return prev. int. state */
	return (_bool)(flags.interrupt);
}

static void _set_blender(_p_data_t dc, HCONTEXT hc_blender) {
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		_bool irqs = _enable_interrupts(_false);
		HMUTEX hlock = lock(dc, 0);
		pdc->hc_blender = hc_blender;
		unlock(dc, hlock);
		_enable_interrupts(irqs);
	}
}

static _bool _is_ready(_p_data_t dc) {
	_bool r = _false;
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		if(pdc->rccf & RCCF_READY)
			r = _true;
	}
	return r;
}

static _bool _send_init_ipi(_p_data_t dc, _u16 cpu_id, _u32 startup_vector) {
	_bool r = _false;
	_cpu_dc_t *pdc = dc;

	if(pdc) {
		if(pdc->hc_lapic) {
			_i_lapic_t *pi = HC_INTERFACE(pdc->hc_lapic);
			if(pi)
				r = pi->send_init_ipi(HC_DATA(pdc->hc_lapic), cpu_id, startup_vector);
		}
	}
	return r;
}

static _u32 _cpu_state_size(void) {
	return sizeof(_cpu_state_t);
}

static _u32 _fpu_state_size(void) {
	return FPU_STATE_SIZE;
}

static void _halt(void) {
	HALT;
}

static void _create_exec_context(_p_data_t dc, _p_data_t stack, _u32 stack_sz, _thread_t *p_entry,
				_u8 dpl, _p_data_t udata, _p_data_t context_buffer) {
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		_cpu_state_t *cpu_state = (_cpu_state_t *)context_buffer;
		/* set entry point */
		cpu_state->rip = (_u64)p_entry;
		cpu_state->rip_frame = (_u64)p_entry;
		/* ////////////////// */

		/* align stack */
		_u64 u64_stack = (_u64)stack;
		u64_stack += stack_sz-1;
		while(u64_stack % 16)
			u64_stack--;

		_u64 *stack_ptr = (_u64 *)u64_stack;

		/* push data pointer */
		stack_ptr--;
		*stack_ptr = (_u64)udata;
		cpu_state->rdi = (_u64)udata; /* according fast call */
		/* //////////////////// */

		/* set stack frame */
		cpu_state->rsp = (_u64)stack_ptr;
		cpu_state->ss = 0;

		if(dpl == DPL_SYS)
			cpu_state->cs = pdc->sys_code_selector << 3;
		else
			cpu_state->cs = pdc->usr_code_selector << 3;

		cpu_state->rflags = FLAG_I;
	}
}

static void _switch_context(void) {
	__asm__ __volatile__ ("int $0xff");
}

static void _set_timer(_p_data_t dc, _u32 countdown) {
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		if(pdc->cpu_info.id == _ccpu_id(dc)) {
			if(pdc->hc_lapic) {
				_i_lapic_t *pi = HC_INTERFACE(pdc->hc_lapic);
				_p_data_t pd = HC_DATA(pdc->hc_lapic);
				if(pi && pd)
					pi->set_timer(pd, INT_LAPIC_TIMER, countdown, APIC_TIMER_PERIODIC);
			}
		} else
			pdc->new_timer_value = countdown;
	}
}

static _u32 _get_timer(_p_data_t dc) {
	_u32 r = 0;
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		if(pdc->cpu_info.id == _ccpu_id(dc)) {
			if(pdc->hc_lapic) {
				_i_lapic_t *pi = HC_INTERFACE(pdc->hc_lapic);
				if(pi)
					r = pi->get_timer(HC_DATA(pdc->hc_lapic));
			}
		}
	}
	return r;
}

static _u64 _timestamp(void) {
	_u64 r = 0;
	_bool irqs = _enable_interrupts(_false);
	__asm__ __volatile__ (
		"xorq	%%rdx, %%rdx\n"
		"xorq	%%rax, %%rax\n"
		"rdtsc\n"
		"shlq	$32, %%rdx\n"
		"orq	%%rdx, %%rax\n"
		"movq	%%rax, %[out]\n"
		:[out] "=m" (r)
	);
	_enable_interrupts(irqs);
	return r;
}

static void _enable_cache(_p_data_t dc, _bool enable) {
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		if(pdc->cpu_info.id == _ccpu_id(dc)) {
			_cr0_t cr0;
			cr0_get(&cr0);
			if(enable)
				cr0.cd = 0;
			else
				cr0.cd = 1;

			cr0_set(&cr0);
			__asm__ __volatile__ ("wbinvd\n");

			/*... call VMM to reload pageteble ...*/
			if(pdc->hc_vmm) {
				_i_vmm_t *pi = HC_INTERFACE(pdc->hc_vmm);
				if(pi)
					pi->activate(HC_DATA(pdc->hc_vmm));
			}
		}
	}
}

static void _enable_lapic(_cpu_dc_t *pdc, _bool enable) {
	if(pdc->hc_lapic) {
		_i_lapic_t *pi = HC_INTERFACE(pdc->hc_lapic);
		_p_data_t pd = HC_DATA(pdc->hc_lapic);
		if(pi && pd) {
			if(enable)
				pi->enable(pd);
			else
				pi->disable(pd);
		}
	}
}

static void init_blender(_cpu_dc_t *pdc) {
	_i_blender_t *pi = NULL;

	if(!pdc->hc_blender && _gpi_repo_)
		pdc->hc_blender = _gpi_repo_->create_context_by_interface(I_BLENDER);

	if(pdc->hc_blender) {
		pi = HC_INTERFACE(pdc->hc_blender);
		if(pi)
			pi->init(HC_DATA(pdc->hc_blender), pdc->cpu_info.id);
	}
}

static void _idle(_p_data_t dc) {
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		if(pdc->cpu_info.id == _ccpu_id(dc)) {
			if((pdc->rccf & RCCF_READY) && !(pdc->rccf & RCCF_IDLE)) {
				_i_blender_t *pi_blender = NULL;
				_p_data_t     pd_blender = NULL;

				pdc->rccf |= RCCF_IDLE;
				_enable_cache(dc, _true);
				_enable_lapic(pdc, _true);
				_enable_interrupts(_true);

				while(1) {
					_bool irqs = _enable_interrupts(_false);
					if(pdc->new_timer_value) {
						_set_timer(dc, pdc->new_timer_value);
						pdc->new_timer_value = 0;
					}
					if(pdc->hc_blender) {
						pi_blender = HC_INTERFACE(pdc->hc_blender);
						pd_blender = HC_DATA(pdc->hc_blender);
					}
					if(pi_blender && pd_blender) {
						if(pi_blender->is_running(pd_blender))
							pi_blender->idle(pd_blender);
						else
							pi_blender->start(pd_blender);
					} else
						init_blender(pdc);
					_enable_interrupts(irqs);
					HALT;
				}
			}
		}
	}
}

static void _copy_cpu_state(_p_data_t dst, _p_data_t context) {
	if(_gpi_str_)
		_gpi_str_->mem_cpy(dst, context, sizeof(_cpu_state_t));
}

static void _save_fpu_state(_p_data_t buffer) {
	_u8 fpu[FPU_STATE_SIZE] __attribute__((aligned(16)));
	if(_gpi_str_) {
		__asm__ __volatile__("fxsaveq %0" : "=m" (fpu): : "memory");
		_gpi_str_->mem_cpy(buffer, fpu, FPU_STATE_SIZE);
	}
}

static void _restore_fpu_state(_p_data_t buffer) {
	_u8 fpu[FPU_STATE_SIZE] __attribute__((aligned(16)));
	if(_gpi_str_) {
		_gpi_str_->mem_cpy(fpu, buffer, sizeof(fpu));
		__asm__ __volatile__("fxrstorq %0" :: "m" (*(const _u8 *)fpu) : "memory");
	}
}

static void _init(_p_data_t dc, _cpu_init_info_t *info) {
	_cpu_dc_t *pdc = dc;
	if(pdc && _gpi_str_)
		_gpi_str_->mem_cpy(&pdc->cpu_info, info, sizeof(_cpu_init_info_t));
}

static void init_stack(_cpu_dc_t *pdc) {
	_ulong r_stack;

	/* get stack pointer */
	__asm__ __volatile__ ("movq %%rsp, %0" :"=m"(r_stack):);

	/* virtualize */
	_u32 nb = __g_p_core_startup__->stack_ptr - r_stack;
	r_stack = p_to_k(r_stack);

	/* alloc stack frame for this CPU */
	if((pdc->p_stack = (_u8 *)heap_alloc(DEFAULT_STACK_SIZE, RAM_LIMIT))) {
		/* copy from boot stack */
		_u8 *stack_ptr = pdc->p_stack;
		stack_ptr += DEFAULT_STACK_SIZE - 0x10;
		stack_ptr -= nb;
		if(_gpi_str_)
			_gpi_str_->mem_cpy(stack_ptr, (_u8 *)r_stack, nb);

		/* activate new stack */
		__asm__ __volatile__ ("movq %0, %%rsp": :"m"(stack_ptr));
	}
}

static void init_mm(_cpu_dc_t *pdc) {
	if(pdc->hc_vmm) {
		_i_vmm_t *pi = HC_INTERFACE(pdc->hc_vmm);
		if(pi) {/* set mapping base */
			pi->init(HC_DATA(pdc->hc_vmm), __g_p_core_startup__->pt_address);

			/* activate mapping */
			pi->activate(HC_DATA(pdc->hc_vmm));
		}
	}
}

static void init_tss(_cpu_dc_t *pdc) { /* setup Task State Segment */
	if((pdc->p_tss = (_tss_x64_t *)heap_alloc(DEFAULT_TSS_SIZE, RAM_LIMIT))) {
		pdc->tss_limit = DEFAULT_TSS_SIZE - 1;
		_gpi_str_->mem_set(pdc->p_tss, 0, DEFAULT_TSS_SIZE);

		/* allocate interrupt stack frame */
		_u64 isf = (_u64)heap_alloc(INT_STACK_SIZE, RAM_LIMIT);
		if(isf) {
			/* adjust stack pointer */
			isf += INT_STACK_SIZE - 0x10;
			pdc->p_tss->ist[DEFAULT_IST_INDEX] = isf;
			pdc->p_tss->io_map_offset = (_u16)(sizeof(_tss_x64_t) + 4);
		}
	}
}

static _u16 find_free_selector(_cpu_dc_t *pdc) {
	_u16 r = 0;
	_u8 i = 1;
	for(; i < MAX_GDT; i++) {
		if(pdc->gdt[i].value == 0) {
			r = i;
			break;
		}
	}

	return r;
}

static void init_gdt(_cpu_dc_t *pdc) {
	_gpi_str_->mem_set(pdc->gdt, 0, sizeof(pdc->gdt));

	/* init code 64 segment descriptor */
	_sd_x86_t *p_cs = &pdc->gdt[__g_p_core_startup__->cpu_info._cpu._x86.code_selector];
	sd_init(p_cs, CODE_DESC_LM, DPL_SYS, SM_64_G0, 0, 0xffffffff);
	pdc->sys_code_selector = __g_p_core_startup__->cpu_info._cpu._x86.code_selector;

	/* init data 64 segment descriptor */
	_sd_x86_t *p_ds = &pdc->gdt[__g_p_core_startup__->cpu_info._cpu._x86.data_selector];
	sd_init(p_ds, DATA_DESC_LM, DPL_SYS, SM_64_G0, 0, 0xffffffff);
	pdc->sys_data_selector = __g_p_core_startup__->cpu_info._cpu._x86.data_selector;

	/* init TSS record in GDT */
	_ssd_x64_t *p_tss = (_ssd_x64_t *)&pdc->gdt[GDT_TSS_64];
	ssd_init(p_tss, TSS_64, DPL_SYS, SM_64_G0, CPU_ADDRESS(pdc->p_tss), pdc->tss_limit);
	if((pdc->usr_code_selector = find_free_selector(pdc)))
		sd_init(&pdc->gdt[pdc->usr_code_selector], CODE_DESC_LM, DPL_USR, SM_64_G0, 0,0xffffffff);
	if((pdc->usr_data_selector = find_free_selector(pdc)))
		sd_init(&pdc->gdt[pdc->usr_data_selector], DATA_DESC_LM, DPL_USR, SM_64_G0, 0,0xffffffff);

	/* init GDTR */
	_dt_ptr_t _lgdt;
	_lgdt.limit = sizeof(pdc->gdt)-1;
	_lgdt.base = CPU_ADDRESS(&pdc->gdt[0]);

	/* load Global Descriptor Table (GDT) */
	__asm__ __volatile__ ( "lgdt %0" : :"m"(_lgdt) );

	_u16 gdt_tss_64 = GDT_TSS_64;

	/* load Task State Segment register (TSS) */
	__asm__ __volatile__ (
		"movw	%0, %%ax\n"
		"shlw	$3, %%ax\n"
		"ltr	%%ax\n"
		: :"m"(gdt_tss_64)
	);
}

static void init_cr4(void) {
	_cr4_t cr4;

	cr4_get(&cr4);
	cr4.osfxsr = 1; /* enable FPU context save (fxsave/fxrstor) */
	cr4_set(&cr4);
}

void _int_0(void);
void _int_1(void);
static _idt_gate_x64_t	*_g_p_idt_ = 0;

static void init_idt(void) {
	_u16 idt_sz = DEFAULT_IDT_SIZE / sizeof(_idt_gate_x64_t);
	_u16 idt_limit = DEFAULT_IDT_SIZE - 1;

	if(!_g_p_idt_) {
		if((_g_p_idt_ = (_idt_gate_x64_t *)heap_alloc(DEFAULT_IDT_SIZE, RAM_LIMIT))) {
			/* set interrupt handlers */
			_isr_t *int0 = _int_0;
			_isr_t *int1 = _int_1;

			_u8 *i0 = (_u8 *)int0;
			_u8 *i1 = (_u8 *)int1;

			_u32 handler_sz = i1 - i0;
			_u32 i = 0;
			for(; i < idt_sz; i++) {
				idt_gate_init(&_g_p_idt_[i], ((_u64)(i0 + (i * handler_sz))),
						__g_p_core_startup__->cpu_info._cpu._x86.code_selector,
						DEFAULT_IST_INDEX, INT_64, DPL_USR);
			}
		}
	}
	if(_g_p_idt_) {
		/* Load IDTR */
		_dt_ptr_t _lidt;
		_lidt.limit = idt_limit;
		_lidt.base = CPU_ADDRESS(_g_p_idt_);
		__asm__ __volatile__ ("lidt %0" : :"m"(_lidt) );
	}
}

static void init_lapic(_cpu_dc_t *pdc) {
	if(pdc->hc_lapic) {
		_i_lapic_t *pi_lapic = HC_INTERFACE(pdc->hc_lapic);
		_p_data_t   pd_lapic = HC_DATA(pdc->hc_lapic);
		if(pi_lapic && pd_lapic) {
			_p_data_t lapic_base = pi_lapic->get_base(pd_lapic);
			_i_vmm_t *pi_vmm = HC_INTERFACE(pdc->hc_vmm);
			_p_data_t pd_vmm = HC_DATA(pdc->hc_vmm);
			if(pi_vmm && pd_vmm) {
				_page_info_t page;
				if(!pi_vmm->test(pd_vmm, (_vaddr_t)lapic_base, &page, 0)) {
					/* map local apic in VM */
					page.vaddr = (_vaddr_t)lapic_base;
					page.paddr = (_vaddr_t)k_to_p((_ulong)lapic_base);
					page.vmps  = VMPS_4K;
					page.flags = VMMF_PRESENT|VMMF_WRITABLE|VMMF_NOT_CACHEABLE;

					if(!pi_vmm->map(pd_vmm, &page, 1, 0)) {
						DBG("CPU(%d): unable to map local apic\n", pdc->cpu_info.id);
						return;
					}
				}
				pi_lapic->init(pd_lapic); /* init lapic registers */
				pi_lapic->set_sri(pd_lapic, INT_LAPIC_SRI); /* init spurious interrupt vector */
				pdc->cpu_info.id = pi_lapic->get_id(pd_lapic); /* get CPU ID from local apic */
				pdc->cpu_info.ncores = cpu_get_cores();
			}
		}
	}
}

static void _start_cpu(_p_data_t dc) {
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		if(!(pdc->rccf & RCCF_READY)) {
			_enable_interrupts(_false);
			init_stack(pdc);
			init_mm(pdc);
			init_tss(pdc);
			init_gdt(pdc);
			init_cr4();
			init_idt();
			init_lapic(pdc);
			init_blender(pdc);
			/*...*/
			pdc->rccf |= RCCF_READY;
			if(!_is_boot(dc))
				_idle(dc);
		}
	}
}

#define SHD_INTERRUPT	0
#define SHD_EXCEPTION	1
#define SHD_CTX_SWITCH	2
#define SHD_DEBUG	3
#define SHD_BREAKPOINT	4
#define SHD_MEMORY	6
#define SHD_TIMER	7

_u8 _g_int_type_[]={
	[INT_LAPIC_TIMER]	=SHD_TIMER,
	[INT_LAPIC_SRI]		=SHD_EXCEPTION,
	[INT_SYSCALL]		=SHD_CTX_SWITCH,
	[INT_TASK_SWITCH]	=SHD_CTX_SWITCH,
	[EX_DE]			=SHD_EXCEPTION,	/* [#DE-0] divide by zero */
	[EX_DB]			=SHD_DEBUG,	/* [#DB-1]" debug */
	[EX_NMI]		=SHD_EXCEPTION,	/* [#NMI-2] non-maskable interrupt */
	[EX_BP]			=SHD_BREAKPOINT,/* [#BP-3] breakpoint */
	[EX_OF]			=SHD_EXCEPTION, /* [#OF-4] overflow */
	[EX_BR]			=SHD_EXCEPTION, /* [#BR-5] bound range */
	[EX_DU]			=SHD_EXCEPTION, /* [#DU-6] invalid opcode */
	[EX_NM]			=SHD_EXCEPTION, /* [#NM-7] device not available */
	[EX_DF]			=SHD_EXCEPTION, /* [#DF-8] double fault */
	[EX_TS]			=SHD_EXCEPTION, /* [#TS-10] invalid TSS */
	[EX_NP]			=SHD_MEMORY,	/* [#NP-11] not present */
	[EX_SS]			=SHD_EXCEPTION, /* [#SS-12] stack */
	[EX_GP]			=SHD_EXCEPTION, /* [#GP-13] general protection */
	[EX_PF]			=SHD_MEMORY,	/* [#PF-14] page fault */
	[EX_MF]			=SHD_EXCEPTION, /* [#MF-16] x87 floating point exception */
	[EX_AC]			=SHD_EXCEPTION,	/* [#AC-17] alignment check */
	[EX_MC]			=SHD_EXCEPTION, /* [#MC-18] machine check */
	[EX_XF]			=SHD_EXCEPTION,	/* [#XF-19] DIMD floating point */
	[EX_SX]			=SHD_EXCEPTION, /* [#SX-30] */
};

void _interrupt(_p_data_t dc, _u8 in, _p_data_t context) {
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		_u8 type = _g_int_type_[in];
		_u16 cpu_id = pdc->cpu_info.id;

		if(cpu_id != _ccpu_id(dc) && context == NULL) {
			/* interrupt redirection
			   because it's not for this CPU */
			if(pdc->hc_lapic) {
				_i_lapic_t *pi_lapic = HC_INTERFACE(pdc->hc_lapic);
				if(pi_lapic)
					pi_lapic->send_ipi(HC_DATA(pdc->hc_lapic), cpu_id, in);
			}
		} else {
			_i_blender_t *pi_blender = NULL;
			_p_data_t     pd_blender = NULL;

#ifdef _DEBUG_
			if(!((_cpu_state_t *)context)->rflags * FLAG_I)
				DBG("CPU(%d): Nested interrupt %d\n", cpu_id, in);
#endif

			HMUTEX hlock = lock(dc, 0); /* lock for blender */
			if(pdc->hc_blender) {
				pi_blender = HC_INTERFACE(pdc->hc_blender);
				pd_blender = HC_DATA(pdc->hc_blender);
			}

			if(pi_blender && pd_blender) {
				if(pi_blender->is_running(pd_blender)) {
					switch(type) {
						case SHD_TIMER:
							pi_blender->timer(pd_blender, context);
							break;
						case SHD_INTERRUPT:
							pi_blender->interrupt(pd_blender, context);
							break;
						case SHD_CTX_SWITCH:
							pi_blender->switch_context(pd_blender, context);
							break;
						case SHD_EXCEPTION:
							pi_blender->exception(pd_blender, in, context);
							break;
						case SHD_MEMORY: {
								_ulong ex_point;
								__asm__ __volatile__ (
									"movq	%%cr2, %%rax\n"
									:"=a"(ex_point) :
								);
								pi_blender->memory_exception(pd_blender, ex_point, context);
							} break;
						case SHD_BREAKPOINT:
							pi_blender->breakpoint(pd_blender, context);
							break;
						case SHD_DEBUG:
							pi_blender->breakpoint(pd_blender, context);
							break;
					}
				}
			}
			unlock(dc, hlock);
		}
	}
}

static void _end_int(_p_data_t dc) {
	_cpu_dc_t *pdc = dc;
	if(pdc) {
		if(pdc->hc_lapic) {
			_i_lapic_t *pi = HC_INTERFACE(pdc->hc_lapic);
			_p_data_t   pd = HC_DATA(pdc->hc_lapic);
			if(pi && pd)
				pi->end_of_interrupt(pd);
		}
	}
}

static void _init_info(_p_data_t dc, _cpu_init_info_t *pii) {
	_cpu_dc_t *pdc = dc;

	if(_gpi_str_ && pdc)
		_gpi_str_->mem_cpy(pii, &pdc->cpu_info, sizeof(_cpu_init_info_t));
}

static _i_cpu_t _g_interface_ = {
	.is_boot		= _is_boot,
	.cpu_id			= _cpu_id,
	.ccpu_id		= _ccpu_id,
	.get_vmm		= _get_vmm,
	.get_blender		= _get_blender,
	.is_ready		= _is_ready,
	.set_blender		= _set_blender,
	.enable_interrupts 	= _enable_interrupts,
	.send_init_ipi		= _send_init_ipi,
	.cpu_context_size 	= _cpu_state_size,
	.fpu_context_size 	= _fpu_state_size,
	.halt			= _halt,
	.create_exec_context 	= _create_exec_context,
	.switch_exec_context 	= _switch_context,
	.set_timer		= _set_timer,
	.get_timer		= _get_timer,
	.timestamp		= _timestamp,
	.enable_cache		= _enable_cache,
	.idle			= _idle,
	.copy_exec_context 	= _copy_cpu_state,
	.save_fpu_context 	= _save_fpu_state,
	.restore_fpu_context 	= _restore_fpu_state,
	.init			= _init,
	.init_info		= _init_info,
	.start			= _start_cpu,
	.interrupt		= _interrupt,
	.end_of_interrupt 	= _end_int
};

static _vx_res_t _mod_ctl_(_u32 cmd, ...) {
	_u32 r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
			_i_repository_t *pi_repo = va_arg(args, _i_repository_t*);
			if(pi_repo) {
				if(!_gpi_repo_)
					_gpi_repo_ = pi_repo;

				if(!_gpi_str_) { /* get interface to string operations */
					HCONTEXT hc_str = pi_repo->get_context_by_interface(I_STR);
					if(hc_str)
						_gpi_str_ = HC_INTERFACE(hc_str);
				}
				if(!_ghc_heap_)
					_ghc_heap_ = pi_repo->get_context_by_interface(I_HEAP);
				if(!_ghc_vmm_)
					_ghc_vmm_ = pi_repo->create_context_by_interface(I_VMM);

				_cpu_dc_t *p_dc = va_arg(args, _cpu_dc_t*);
				if(p_dc) {
					p_dc->hc_vmm = _ghc_vmm_;
					p_dc->hc_lapic = pi_repo->create_context_by_interface(I_LAPIC);
					p_dc->hc_mutex = pi_repo->create_context_by_interface(I_MUTEX);
					p_dc->hc_blender = pi_repo->create_context_by_interface(I_BLENDER);
				}
			}
			r = VX_OK;
		} break;

		case MODCTL_DESTROY_CONTEXT: {
			_i_repository_t *pi_repo = va_arg(args, _i_repository_t*);
			if(pi_repo) {
				_cpu_dc_t *p_dc = va_arg(args, _cpu_dc_t*);
				if(p_dc) {
					HMUTEX hlock = lock(p_dc, 0);
					pi_repo->release_context(p_dc->hc_blender);
					p_dc->hc_blender = NULL;
					pi_repo->release_context(p_dc->hc_lapic);
					p_dc->hc_lapic = NULL;
					if(p_dc->p_stack) {
						heap_free(p_dc->p_stack, DEFAULT_STACK_SIZE);
						p_dc->p_stack = NULL;
					}
					if(p_dc->p_tss) {
						heap_free(p_dc->p_tss, DEFAULT_TSS_SIZE);
						p_dc->p_tss = NULL;
					}
					unlock(p_dc, hlock);
					pi_repo->release_context(p_dc->hc_mutex);
					p_dc->hc_mutex = NULL;
				}
			}
			r = VX_OK;
		} break;
	}

	va_end(args);

	return r;
}

DEF_VXMOD(
	MOD_CPU_AMD64,		/* module name */
	I_CPU,			/* interface name */
	&_g_interface_,		/* interface pointer */
	NULL,			/* static data context */
	sizeof(_cpu_dc_t),	/* size of data context (for dynamic allocation) */
	_mod_ctl_,		/* pointer to module controll routine */
	1,0,1,			/* version */
	"CPU x86-64"		/* description */
);

