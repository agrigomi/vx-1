#include "i_sync.h"
#include "i_repository.h"
#include "mutex.h"
#include "err.h"
#include "vxmod.h"


static HMUTEX _try_lock(void *mc, HMUTEX hlock) {
	HMUTEX r = 0;
	_mutex_t *p = (_mutex_t *)mc;
	if(p)
		r = mutex_try_lock(p, hlock);
	return r;
}

static HMUTEX _lock(void *mc, HMUTEX hlock) {
	HMUTEX r = 0;
	_mutex_t *p = (_mutex_t *)mc;
	if(p)
		r = mutex_lock(p, hlock, _MUTEX_TIMEOUT_INFINITE_, 0);
	return r;
}

static void _unlock(void *mc, HMUTEX hlock) {
	_mutex_t *p = (_mutex_t *)mc;
	if(p)
		mutex_unlock(p, hlock);
}

static _i_mutex_t _interface = {
	.try_lock = _try_lock,
	.lock = _lock,
	.unlock = _unlock
};

static _u32 mutex_ctl(_u32 cmd, ...) {
	_u32 r = VX_ERR;

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
			va_list args;
			va_start(args, cmd);

			va_arg(args, _i_repository_t*); /* skip first argument */

			_mutex_t *pm = va_arg(args, _mutex_t*);
			if(pm) 
				mutex_reset(pm);
			r = VX_OK;
			va_end(args);
		}
		break;
		case MODCTL_DESTROY_CONTEXT:
			r = VX_OK;
			break;
	}

	return r;
}

DEF_VXMOD(
	MOD_MUTEX,
	I_MUTEX, &_interface,
	NULL,
	sizeof(_mutex_t),
	mutex_ctl,
	1,0,1,
	"mutex"
);
