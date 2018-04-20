#include "i_sync.h"
#include "i_repository.h"
#include "err.h"
#include "vxmod.h"
#include "atom.h"

typedef struct {
	_u32 state;
}_evt_context_t;

static _u32 evt_check(_p_data_t dc, _u32 mask) {
	_evt_context_t *p_ecxt = (_evt_context_t *)dc;
	_u32 r = p_ecxt->state & ~mask;
	__EXCHANGE_L__(r, p_ecxt->state);
	if(mask)
		r &= mask;
	return r;
}

static _u32 evt_wait(_p_data_t dc, _u32 mask) {
	_u32 r = 0;
	while(!(r = evt_check(dc, mask)));
	return r;
}

static void evt_set(_p_data_t dc, _u32 mask) {
	_evt_context_t *p_ecxt = (_evt_context_t *)dc;
	_u32 r = mask | p_ecxt->state;
	__EXCHANGE_L__(r, p_ecxt->state);
}

static _i_event_t _g_interface_ = {
	.wait	= evt_wait,
	.set	= evt_set,
	.check	= evt_check
};

static _vx_res_t event_ctl(_u32 cmd, ...) {
	_vx_res_t r = VX_UNSUPPORTED_COMMAND;

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
			va_list args;
			va_start(args, cmd);
			va_arg(args, _i_repository_t*); /* skip first argument */
			_evt_context_t *p_ecxt = va_arg(args, _evt_context_t*);
			if(p_ecxt)
				p_ecxt->state = 0;
			va_end(args);
		} break;

		case MODCTL_DESTROY_CONTEXT:
			r = VX_OK;
			break;
	}

	return r;
}

DEF_VXMOD(
	MOD_MUTEX,
	I_EVENT, &_g_interface_,
	NULL,
	sizeof(_evt_context_t),
	event_ctl,
	1,0,1,
	"event"
);
