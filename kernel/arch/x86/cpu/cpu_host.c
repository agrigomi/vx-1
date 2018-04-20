#include "i_cpu.h"
#include "startup_context.h"
#include "i_sync.h"
#include "i_repository.h"
#include "i_str.h"
#include "i_memory.h"
#include "err.h"
#include "mp_scan.h"
#include "cpu.h"
#include "intdef.h"
#include "i_system_log.h"
#include "i_dev_root.h"
#include "i_ioapic.h"
#include "addr.h"
/*#define _DEBUG_*/
#include "debug.h"

#define RAM_LIMIT	p_to_k(0xffffffff)

DEF_SYSLOG();

typedef struct {
	_int_callback_t *p_isr;
	_p_data_t	udata;
}_isr_info_t;

static _isr_info_t _g_isr_map_[256];

static _i_str_t *_gpi_str_ = NULL;
static _i_cpu_host_t *_gpi_cpu_host_ = NULL;
static HCONTEXT _ghc_heap_ = NULL;
static HCONTEXT _ghc_ioapic_ = NULL;

/* global repository pointer */
static _i_repository_t *_gpi_repo_ = NULL;
/* list of CPU's  */
static HCONTEXT *_g_hc_cpu_list_  = NULL;
static _u16 _g_cpu_count_ = 0;
static _bool _g_cpu_host_running_ = _false;


void smp_init_cpu(HCONTEXT hc_cpu) {
	HCONTEXT hccpu = ((_u64)hc_cpu & __g_p_core_startup__->vbase) ? hc_cpu : (HCONTEXT)p_to_k(hc_cpu);
	if(hccpu) {
		_i_cpu_t *pi = HC_INTERFACE(hccpu);
		_p_data_t pd = HC_DATA(hccpu);

		if(pi && pd)
			pi->start(pd);
	}
}

void interrupt_dispatcher(_u64 inum, _p_data_t cpu_state) {
	if(_gpi_cpu_host_) {
		HCONTEXT hc_cpu = _gpi_cpu_host_->get_current_cpu();
		if(hc_cpu) {
			_i_cpu_t *pi_cpu = HC_INTERFACE(hc_cpu);
			_p_data_t pd_cpu = HC_DATA(hc_cpu);
			_int_callback_t *p_isr = _gpi_cpu_host_->get_isr((_u8)inum);

			if(p_isr)
				p_isr(hc_cpu, cpu_state, _gpi_cpu_host_->get_isr_data((_u8)inum));
			else
				pi_cpu->interrupt(pd_cpu, (_u8)inum, cpu_state);

			pi_cpu->end_of_interrupt(pd_cpu);
		}
	}
}

static HCONTEXT _create_cpu(_u32 signature, _u16 id, _u8 flags) {
	HCONTEXT r = NULL;
	if(_gpi_repo_) {
		/* check for existing ID */
		if(!(r = _g_hc_cpu_list_[id])) {
			if((r = _gpi_repo_->create_limited_context_by_interface(I_CPU, RAM_LIMIT))) {
				_i_cpu_t *pi = HC_INTERFACE(r);
				_cpu_cxt_t pd = HC_DATA(r);

				_cpu_init_info_t ii;
				ii.signature = signature;
				ii.id  = id;
				ii.flags = flags;

				pi->init(pd, &ii);
			}
		}
	}
	return r;
}

static void _destroy_cpu(HCONTEXT hcpu) {
	if(_gpi_repo_)
		_gpi_repo_->release_context(hcpu);
}

static void register_boot_cpu(_mpc_cpu_t *p_cpu_info) {
	HCONTEXT hccpu = _create_cpu(p_cpu_info->sign,
					p_cpu_info->lapic_id,
					p_cpu_info->flags | CCPUF_SOCKET);
	if(hccpu) {
		smp_init_cpu(hccpu);
		_i_cpu_t *pi = HC_INTERFACE(hccpu);
		_p_data_t pd = HC_DATA(hccpu);
		if(pi && pd) {
			_u16 id = pi->cpu_id(pd);
			_g_hc_cpu_list_[id] = hccpu;
		}
	} else
		LOG(LMT_ERROR, "failed to register boot CPU", "");
}

static HCONTEXT _get_cpu(_u16 cpu_id) {
	HCONTEXT r = NULL;
	if(cpu_id < _g_cpu_count_)
		r = _g_hc_cpu_list_[cpu_id];
	return r;
}

static HCONTEXT _get_boot_cpu(void) {
	HCONTEXT r = NULL;
	_u32 i = 0;

	for(; i < _g_cpu_count_; i++) {
		HCONTEXT hc = _g_hc_cpu_list_[i];
		if(hc) {
			_i_cpu_t *pi = HC_INTERFACE(hc);
			_p_data_t pd = HC_DATA(hc);
			if(pi->is_boot(pd)) {
				r = hc;
				break;
			}
		}
	}

	return r;
}

static HCONTEXT _get_this_cpu(void) {
	HCONTEXT r = NULL;
	_u32 i = 0;

	for(; i < _g_cpu_count_; i++) {
		HCONTEXT hc = _g_hc_cpu_list_[i];
		if(hc) {
			_i_cpu_t *pi = HC_INTERFACE(hc);
			_p_data_t pd = HC_DATA(hc);
			if(pi->cpu_id(pd) == pi->ccpu_id(pd)) {
				r = hc;
				break;
			}
		}
	}
	return r;
}

static void init_ioapic(void) {
	if(_ghc_ioapic_) {
		_i_ioapic_t *pi_piapic = HC_INTERFACE(_ghc_ioapic_);
		HCONTEXT hc_cpu = _get_this_cpu();
		if(hc_cpu) {
			_i_cpu_t *pi_cpu = HC_INTERFACE(hc_cpu);
			_p_data_t pd_cpu = HC_DATA(hc_cpu);

			if(pi_piapic && pi_cpu && pd_cpu) {
				HCONTEXT hc_vmm = pi_cpu->get_vmm(pd_cpu);
				if(hc_vmm) {
					_i_vmm_t *pi_vmm = HC_INTERFACE(hc_vmm);
					_p_data_t pd_vmm = HC_DATA(hc_vmm);
					if((pi_piapic->get_flags() & 1) && pi_vmm && pd_vmm) {
						_vaddr_t ioapic_base = (_vaddr_t)pi_piapic->get_base();
						_page_info_t pgi;
						if(!pi_vmm->test(pd_vmm, ioapic_base, &pgi, 0)) {
							pgi.vaddr = ioapic_base;
							pgi.paddr = (_vaddr_t)k_to_p((_ulong)ioapic_base);
							pgi.vmps  = VMPS_4K;
							pgi.flags = VMMF_WRITABLE|VMMF_PRESENT|VMMF_NOT_CACHEABLE;

							pi_vmm->map(pd_vmm, &pgi, 1, 0);
						}
					}
				} else
					LOG(LMT_ERROR, "%s: no VMM context", __FUNCTION__);
			}
		} else
			LOG(LMT_ERROR, "%s: no boot CPU", __FUNCTION__);
	}
}

static _u16 _get_cpu_count(void) {
	_u16 r = 0;
	_u16 i = 0;

	while(i < _g_cpu_count_) {
		if(_g_hc_cpu_list_[i])
			r++;
		i++;
	}
	return r;
}

static void wakeup_cpu(_mpc_cpu_t *pmp_cpu) {
	HCONTEXT hccpu = _create_cpu(pmp_cpu->sign, pmp_cpu->lapic_id, pmp_cpu->flags);
	if(hccpu) {
		_i_cpu_t *pi_cpu = HC_INTERFACE(hccpu);
		_p_data_t pd_cpu = HC_DATA(hccpu);

		__g_p_core_startup__->cpu_info._cpu._x86.core_cpu_init_data = (_u32)k_to_p((_u64)hccpu);

		HCONTEXT hc_boot_cpu = _get_boot_cpu();
		if(hc_boot_cpu) {
			/* use boot CPU to wakeup another */
			_i_cpu_t *pi_boot_cpu = HC_INTERFACE(hc_boot_cpu);
			_p_data_t pd_boot_cpu = HC_DATA(hc_boot_cpu);

			if(pi_boot_cpu->send_init_ipi(pd_boot_cpu, pi_cpu->cpu_id(pd_cpu),
					__g_p_core_startup__->cpu_info._cpu._x86.cpu_init_vector_rm)) {
#ifdef _DEBUG_
				_u32 tmout = 0x04c00000;
#else
				_u32 tmout = 0x02000000;
#endif
				while(pi_cpu->is_ready(pd_cpu) == _false && tmout)
					tmout--;
				if(pi_cpu->is_ready(pd_cpu)) {
					_u16 id = pi_cpu->cpu_id(pd_cpu);
					LOG(LMT_NONE, "WakeUp CPU(%d)", id);
					_g_hc_cpu_list_[id] = hccpu;
				}
			} else {
				LOG(LMT_ERROR, "Unable to wakeup CPU(%d)", pi_cpu->cpu_id(pd_cpu));
				_destroy_cpu(hccpu);
			}
		} else
			_destroy_cpu(hccpu);
	}
}

static void _start(void) {
	if(_g_cpu_host_running_)
		return;

	_g_cpu_host_running_ = _true;

	LOG(LMT_NONE, "running CPU HOST ...", "");

	_u16 ncores = cpu_get_cores();
	_mp_t *p_mpf = mp_find_table();
	if(p_mpf) {
		_u16 ncpu = mp_cpu_count(p_mpf);
		/* calculate size of CPU list */
		_g_cpu_count_ = ncpu * ncores;
		_u32 szcpul = _g_cpu_count_ * sizeof(HCONTEXT);
		_i_heap_t *pi_heap = HC_INTERFACE(_ghc_heap_);
		_p_data_t  pd_heap = HC_DATA(_ghc_heap_);
		if(pi_heap && pd_heap) { /* alloc memory for CPU list */
			if((_g_hc_cpu_list_ = (HCONTEXT *)pi_heap->alloc(pd_heap, szcpul, 0xffffffff))) {
				_u16 cpu_idx = 0;
				_mpc_cpu_t *p_cpu_info = NULL;

				_gpi_str_->mem_set(_g_hc_cpu_list_, 0, szcpul);

				/* create and init boot (this) CPU */
				while((p_cpu_info = mp_get_cpu(p_mpf, cpu_idx))) {
					if((p_cpu_info->flags & CCPUF_ENABLED) && (p_cpu_info->flags & CCPUF_BOOT)) {
						p_cpu_info->flags |= CCPUF_SOCKET;
						LOG(LMT_NONE, "CPU(%d) sign=%X flags=%X apic_id=%d",
							(_u32)p_cpu_info->lapic_id,
							(_u32)p_cpu_info->sign, (_u32)p_cpu_info->flags,
							(_u32)p_cpu_info->lapic_id);
						register_boot_cpu(p_cpu_info);
						break;
					}
					cpu_idx++;
				}

				/* init I/O apic ... */
				init_ioapic();
				/* set CMOS register ... */
				/*...*/

				/* Set warm-boot vector segment */
				*(_u16 *)(p_to_k((0x467 + 0))) = 0;
				/* Set warm-boot vector offset */
				*(_u16 *)(p_to_k((0x467 + 2))) = (_u16)__g_p_core_startup__->cpu_info._cpu._x86.cpu_init_vector_rm;

				/* set core CPU init vector */
				__g_p_core_startup__->cpu_info._cpu._x86.core_cpu_init_vector = (_u32)k_to_p((_u64)smp_init_cpu);

				/* wakeup CPUs by socket */
				cpu_idx = 0;
				while((p_cpu_info = mp_get_cpu(p_mpf, cpu_idx))) {
					if((p_cpu_info->flags & CCPUF_ENABLED) && !(p_cpu_info->flags & CCPUF_BOOT)) {
						p_cpu_info->flags |= CCPUF_SOCKET;
						LOG(LMT_NONE, "CPU(%d) sign=%X flags=%X apic_id=%d",
							(_u32)p_cpu_info->lapic_id,
							(_u32)p_cpu_info->sign, (_u32)p_cpu_info->flags,
							(_u32)p_cpu_info->lapic_id);
						wakeup_cpu(p_cpu_info);
					}
					cpu_idx++;
				}
			}
		}
	} else {
		/* single CPU system */
		_mpc_cpu_t ci = {0, 0, CCPUF_ENABLED|CCPUF_BOOT|CCPUF_SOCKET, 0, 0, {0,0}};
		register_boot_cpu(&ci);
	}

	/* wakeup CPU cores */
	_u32 i = 0;

	for(; i < _g_cpu_count_; i++) {
		HCONTEXT hccpu = _g_hc_cpu_list_[i];

		if(hccpu) {
			_i_cpu_t *pi = HC_INTERFACE(hccpu);
			_p_data_t pd = HC_DATA(hccpu);

			if(pi && pd) {
				_cpu_init_info_t cii;

				pi->init_info(pd, &cii);
				_u16 ncores = cii.ncores;

				if((cii.flags & CCPUF_SOCKET) && ncores > 1) {
					_mpc_cpu_t ci;
					_u16 j = cii.id + 1;
					_u16 n = cii.id + ncores;

					for(; j < n; j++) {
						ci.lapic_id = j;
						ci.lapic_version = 0;
						ci.flags = CCPUF_ENABLED;
						ci.sign = cii.signature;
						ci.fflags = 0;
						wakeup_cpu(&ci);
					}
				}
			}
		}
	}
}

static _int_callback_t *_get_isr(_u8 num) {
	return _g_isr_map_[num].p_isr;
}
static _p_data_t _get_isr_data(_u8 num) {
	return _g_isr_map_[num].udata;
}
static void _set_isr(_u8 num, _int_callback_t *p_isr, _p_data_t udata) {
	_g_isr_map_[num].p_isr = p_isr;
	_g_isr_map_[num].udata = udata;
}
static _bool _set_irq(_u8 irq, _u16 dst_cpu, _u8 int_vector, _u32 flags) {
	_bool r = _false;
	if(_ghc_ioapic_) {
		_i_ioapic_t *pi = HC_INTERFACE(_ghc_ioapic_);
		if(pi) {
			pi->set_irq(irq, dst_cpu, int_vector, flags);
			r = _true;
		}
	}
	return r;
}

static _bool _is_running(void) {
	return _g_cpu_host_running_;
}

static _i_cpu_host_t _g_interface_ = {
	.create_cpu	= _create_cpu,
	.get_cpu	= _get_cpu,
	.get_boot_cpu	= _get_boot_cpu,
	.get_current_cpu= _get_this_cpu,
	.get_cpu_count	= _get_cpu_count,
	.get_isr	= _get_isr,
	.get_isr_data	= _get_isr_data,
	.set_isr	= _set_isr,
	.set_irq	= _set_irq,
	.start		= _start,
	.is_running	= _is_running
};

static HDEV _g_hdev_ = NULL;

static _vx_res_t _mod_ctl_(_u32 cmd, ...) {
	_u32 r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
			_i_repository_t *pi_repo = va_arg(args, _i_repository_t*);
			if(pi_repo) {
				_gpi_repo_ = pi_repo;
				/* get syslog only.
				Because we are expected that syslog to been initialized outside */
				GET_SYSLOG(pi_repo);
				_LOG(LMT_INFO, "init CPU host module");
				/* save interface pointer */
				_gpi_cpu_host_ = &_g_interface_;

				if(!_gpi_str_) { /* get string operations */
					HCONTEXT hcstr = pi_repo->get_context_by_interface(I_STR);
					if(hcstr)
						_gpi_str_ = HC_INTERFACE(hcstr);
				}
				if(_gpi_str_) {
					_gpi_str_->mem_set(_g_isr_map_, 0, sizeof(_g_isr_map_));
				}
				if(!_ghc_heap_) /* get heap static context */
					_ghc_heap_ = pi_repo->get_context_by_interface(I_HEAP);
				if(!_ghc_ioapic_)
					_ghc_ioapic_ = pi_repo->get_context_by_interface(I_IO_APIC);

				/* register CPU HOST as device */
				HCONTEXT hcdroot = pi_repo->get_context_by_interface(I_DEV_ROOT);
				if(hcdroot) {
					_i_dev_root_t *pi_droot = HC_INTERFACE(hcdroot);
					if(pi_droot)
						_g_hdev_ = pi_droot->create(_mod_ctl_, pi_droot->get_hdev(), NULL);
					pi_repo->release_context(hcdroot);
				}
			}
			r = VX_OK;
		} break;
		case MODCTL_DESTROY_CONTEXT: {
			_i_repository_t *pi_repo = va_arg(args, _i_repository_t*);
			if(pi_repo) {
				RELEASE_SYSLOG(pi_repo);
				/*...*/
			}
			r = VX_OK;
		} break;
		case MODCTL_START:
			_start();
			r = VX_OK;
			break;
		case DEVCTL_INIT:
			_LOG(LMT_INFO, "create CPU host device");
			/*...*/
			r = VX_OK;
			break;
		case DEVCTL_UNINIT:
			/*...*/
			r = VX_OK;
			break;
		case DEVCTL_SUSPEND:
			/*...*/
			r = VX_OK;
			break;
		case DEVCTL_RESUME:
			/*...*/
			r = VX_OK;
			break;
	}
	va_end(args);

	return r;
}

DEF_VXMOD(
	MOD_CPU_HOST,		/* module name */
	I_CPU_HOST,		/* interface name */
	&_g_interface_,		/* interface pointer */
	NULL,			/* static data context */
	0,			/* size of data context (for dynamic allocation) */
	_mod_ctl_,		/* pointer to module controll routine */
	1,0,1,			/* version */
	"CPU host"		/* description */
);
