#include "vxmod.h"
#include "err.h"
#include "i_sync.h"
#include "i_repository.h"
#include "i_system_log.h"
#include "i_memory.h"
#include "i_tty.h"
#include "i_cpu.h"

#define CLR_WARNING	0x03
#define CLR_ERROR	0x01
#define CLR_TEXT	0x07
#define CLR_INFO	0x07

static _i_txt_disp_t *_g_pi_disp_ = NULL;

DEF_SYSLOG();

static void _log_hook_(_u8 lmt, _cstr_t msg) {
	if(_g_pi_disp_) {
		switch(lmt) {
			case LMT_NONE:
				_g_pi_disp_->fwrite("%s\n", msg);
				break;
			case LMT_INFO: {
					_u8 ccolor = _g_pi_disp_->color(CLR_INFO);
					_g_pi_disp_->fwrite("%s\n", msg);
					_g_pi_disp_->color(ccolor);
				} break;
			case LMT_WARNING: {
					_u8 ccolor = _g_pi_disp_->color(CLR_WARNING);
					_g_pi_disp_->fwrite("%s\n", msg);
					_g_pi_disp_->color(ccolor);

				} break;
			case LMT_ERROR: {
					_u8 ccolor = _g_pi_disp_->color(CLR_ERROR);
					_g_pi_disp_->fwrite("%s\n", msg);
					_g_pi_disp_->color(ccolor);
				} break;
		}
	}
}

void start(_i_repository_t _UNUSED_ *p_repo) {
	HCONTEXT hc_cpu_host = p_repo->get_context_by_interface(I_CPU_HOST);
	LOG(LMT_NONE, "--%s--", "start arch. entry (AMD64)");
	if(hc_cpu_host) {
		_i_cpu_host_t *pi_cpu_host = HC_INTERFACE(hc_cpu_host);
		if(pi_cpu_host)
			pi_cpu_host->start();
	}

	/* ... */
}

static _u32 arch_entry_ctl(_u32 cmd, ...) {
	_u32 r = VX_ERR;
	va_list args;
	va_start(args, cmd);

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				if(!_g_pi_disp_) {
					HCONTEXT hcdisp = p_repo->get_context_by_interface(I_TXT_DISP);
					if(hcdisp)
						_g_pi_disp_ = HC_INTERFACE(hcdisp);
				}
				r = VX_OK;
			} break;
		case MODCTL_START: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				GET_SYSLOG(p_repo);
				ADD_SYSLOG_LISTENER(_log_hook_);
				start(p_repo);
				r = VX_OK;
			}
			break;
		case MODCTL_STOP: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);

				REM_SYSLOG_LISTENER(_log_hook_);
				RELEASE_SYSLOG(p_repo);
				r = VX_OK;
			} break;
	}
	va_end(args);
	return r;
}

DEF_VXMOD(
	MOD_X86_64_ENTRY,
	I_ARCH_ENTRY,
	NULL, /* no interface */
	NULL, /* no static data context */
	0,
	arch_entry_ctl,
	1,0,1,
	"x86 architecture entry point"
);

