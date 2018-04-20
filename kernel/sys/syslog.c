#include "i_system_log.h"
#include "i_repository.h"
#include "i_memory.h"
#include "i_str.h"
#include "i_sync.h"
#include "err.h"

#define MAX_HOOKS	8
#define MAX_MSG_BUFFER	1024

typedef struct { /* data context */
	HCONTEXT	hcrb; /* context of ring buffer */
	HCONTEXT	hcmutex;
	_log_listener_t	*hooks[MAX_HOOKS]; /* hooks array */
}_syslog_dc_t;

static _i_str_t *_g_pi_str_ = NULL;

static _syslog_dc_t _g_static_dc_; /* static data context */

static HMUTEX _lock(_syslog_dc_t *pdc, HMUTEX hlock) {
	HMUTEX r = 0;
	if(pdc->hcmutex) {
		_i_mutex_t *pi = HC_INTERFACE(pdc->hcmutex);
		_p_data_t pd = HC_DATA(pdc->hcmutex);
		if(pi && pd)
			r = pi->lock(pd, hlock);
	}
	return r;
}

static void _unlock(_syslog_dc_t *pdc, HMUTEX hlock) {
	if(pdc->hcmutex) {
		_i_mutex_t *pi = HC_INTERFACE(pdc->hcmutex);
		_p_data_t pd = HC_DATA(pdc->hcmutex);
		if(pi && pd)
			pi->unlock(pd, hlock);
	}
}

void log_init(_p_data_t dc, _u32 capacity) {
	_syslog_dc_t *pdc = dc;

	if(pdc) {
		/* init ring buffer */
		_i_ring_buffer_t *pi = HC_INTERFACE(pdc->hcrb);
		_p_data_t pd = HC_DATA(pdc->hcrb);
		pi->init(pd, capacity, 0);
	}
}

static void log_sync(_syslog_dc_t *pdc) {
	if(pdc) {
		if(pdc->hcrb) {
			_u32 i = 0;
			_u8 lmt = 0;
			_str_t msg = NULL;
			_i_ring_buffer_t *pi = HC_INTERFACE(pdc->hcrb);
			_p_data_t pd = HC_DATA(pdc->hcrb);
			_u16 sz = 0;

			if(pi && pd) {
				for(i = 0; i < MAX_HOOKS; i++) {
					if(pdc->hooks[i])
						break;
				}

				if(i < MAX_HOOKS) {
					while((msg = (_str_t)pi->pull(pd, &sz))) {
						lmt = msg[0];
						for(i = 0; i < MAX_HOOKS; i++) {
							if(pdc->hooks[i])
								pdc->hooks[i](lmt, (_cstr_t)msg+1);
						}
					}
				}
			}
		}
	}
}

static void log_add_listener(_p_data_t dc, _log_listener_t *proc) {
	_syslog_dc_t *pdc = dc;

	if(pdc) {
		_u32 i = 0;

		HMUTEX hlock = _lock(pdc, 0);

		for(; i < MAX_HOOKS; i++) {
			if(pdc->hooks[i] == proc)
				break;
		}
		if(i == MAX_HOOKS) {
			for(i = 0; i < MAX_HOOKS; i++) {
				if(pdc->hooks[i] == NULL) {
					pdc->hooks[i] = proc;
					break;
				}
			}
		}
		_unlock(pdc, hlock);
	}
}

static void log_remove_listener(_p_data_t dc, _log_listener_t *proc) {
	_syslog_dc_t *pdc = dc;

	if(pdc) {
		_u32 i = 0;
		HMUTEX hlock = _lock(pdc, 0);
		for(; i < MAX_HOOKS; i++) {
			if(pdc->hooks[i] == proc) {
				pdc->hooks[i] = NULL;
				break;
			}
		}
		_unlock(pdc, hlock);
	}
}

static void log_write(_p_data_t dc, _u8 lmt, _cstr_t msg) {
	if(_g_pi_str_ && dc) {
		_syslog_dc_t *pdc = dc;
		if(pdc->hcrb) {
			HMUTEX hlock = _lock(pdc, 0);
			_s8 _msg[MAX_MSG_BUFFER];
			_i_ring_buffer_t *pi = HC_INTERFACE(pdc->hcrb);
			_p_data_t pd = HC_DATA(pdc->hcrb);

			if(pi && pd) {
				_u32 sz_msg = _g_pi_str_->snprintf(_msg+1, sizeof(_msg)-1, "%s", msg);

				_msg[0] = lmt;
				pi->push(pd, _msg, sz_msg+2);
				log_sync(dc);
			}
			_unlock(pdc, hlock);
		}
	}
}

static void log_fwrite(_p_data_t dc, _u8 lmt, _cstr_t fmt, ...) {
	if(_g_pi_str_ && dc) {
		_syslog_dc_t *pdc = dc;
		if(pdc->hcrb) {
			va_list args;
			HMUTEX hlock = _lock(pdc, 0);
			_s8 msg[MAX_MSG_BUFFER];
			_i_ring_buffer_t *pi = HC_INTERFACE(pdc->hcrb);
			_p_data_t pd = HC_DATA(pdc->hcrb);

			if(pi && pd) {
				_u32 sz_msg = 0;

				va_start(args, fmt);

				sz_msg = _g_pi_str_->vsnprintf(msg+1, sizeof(msg)-1, fmt, args);

				msg[0] = lmt;
				pi->push(pd, msg, sz_msg+2);

				va_end(args);
				log_sync(dc);
			}
			_unlock(pdc, hlock);
		}
	}
}

static HMUTEX log_lock(_p_data_t dc) {
	return _lock(dc, 0);
}


static void log_unlock(_p_data_t dc, HMUTEX hlock) {
	_unlock(dc, hlock);
}

static _str_t log_first(_p_data_t dc, HMUTEX hlock) {
	_str_t r = NULL;
	_syslog_dc_t *pdc = dc;
	if(pdc && pdc->hcrb) {
		_i_ring_buffer_t *pi = HC_INTERFACE(pdc->hcrb);
		_p_data_t pd = HC_DATA(pdc->hcrb);

		if(pi && pd) {
			_u16 sz = 0;
			HMUTEX hm = _lock(dc, hlock);
			pi->reset_pull(pd);
			if((r = pi->pull(pd, &sz)))
				r += sizeof(_u16);
			_unlock(dc, hm);
		}
	}
	return r;
}

static _str_t log_next(_p_data_t dc, HMUTEX hlock) {
	_str_t r = NULL;
	_syslog_dc_t *pdc = dc;
	if(pdc && pdc->hcrb) {
		_i_ring_buffer_t *pi = HC_INTERFACE(pdc->hcrb);
		_p_data_t pd = HC_DATA(pdc->hcrb);

		if(pi && pd) {
			_u16 sz = 0;
			HMUTEX hm = _lock(dc, hlock);
			if((r = pi->pull(pd, &sz)))
				r += sizeof(_u16);
			_unlock(dc, hm);
		}
	}
	return r;
}

static _i_system_log_t _g_i_syslog_={
	.init		= log_init,
	.add_listener	= log_add_listener,
	.remove_listener= log_remove_listener,
	.write		= log_write,
	.fwrite		= log_fwrite,
	.lock		= log_lock,
	.unlock		= log_unlock,
	.first		= log_first,
	.next		= log_next
};

static _vx_res_t _mod_ctl_(_u32 cmd, ...) {
	_vx_res_t r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				_syslog_dc_t *p_cdata = va_arg(args, _syslog_dc_t*);

				if(!_g_pi_str_) {
					HCONTEXT hc = p_repo->get_context_by_interface(I_STR);
					if(hc)
						_g_pi_str_ = HC_INTERFACE(hc);
				}

				if(p_cdata) {
					if(_g_pi_str_)
						_g_pi_str_->mem_set(p_cdata, 0, sizeof(_syslog_dc_t));
					p_cdata->hcmutex = p_repo->create_context_by_interface(I_MUTEX);
					p_cdata->hcrb = p_repo->create_context_by_interface(I_RING_BUFFER);
				}
				r = VX_OK;
			} break;
		case MODCTL_DESTROY_CONTEXT: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				_syslog_dc_t *p_cdata = va_arg(args, _syslog_dc_t*);

				if(p_cdata) {
					HMUTEX hlock = _lock(p_cdata, 0);
					if(p_cdata->hcrb) { /* destroy ring buffer */
						_i_ring_buffer_t *pi = HC_INTERFACE(p_cdata->hcrb);
						_p_data_t pd = HC_DATA(p_cdata->hcrb);
						pi->destroy(pd);
						p_repo->release_context(p_cdata->hcrb);
					}
					_unlock(p_cdata, hlock);
					if(p_cdata->hcmutex) { /* destroy mutex */
						p_repo->release_context(p_cdata->hcmutex);
						p_cdata->hcmutex = NULL;
					}
				}
				r = VX_OK;
			} break;
	}

	va_end(args);
	return r;
}

DEF_VXMOD(
	MOD_SYSTEM_LOG,
	I_SYSTEM_LOG,
	&_g_i_syslog_,
	&_g_static_dc_,
	sizeof(_syslog_dc_t),
	_mod_ctl_,
	1,0,1,
	"system log"
);

