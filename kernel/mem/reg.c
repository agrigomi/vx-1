#include "vxmod.h"
#include "i_memory.h"
#include "i_repository.h"
#include "reg_alg.h"
#include "err.h"
#include "i_str.h"
#include "mutex.h"

typedef struct {
	_mutex_t	mutex;
	_reg_context_t	context;
}_reg_data_t;

static HCONTEXT _g_heap_ = NULL;
static _i_str_t *_g_pi_str_ = NULL;

static void *reg_mem_alloc(_u32 size, _ulong limit, void _UNUSED_ *udata) {
	void *r = NULL;

	if(_g_heap_) {
		_i_heap_t *pi = HC_INTERFACE(_g_heap_);
		if(pi)
			r = pi->alloc(HC_DATA(_g_heap_), size, limit);
	}
	return r;
}

static void reg_mem_free(void *ptr, _u32 size, void _UNUSED_ *udata) {
	if(_g_heap_) {
		_i_heap_t *pi = HC_INTERFACE(_g_heap_);
		if(pi)
			pi->free(HC_DATA(_g_heap_), ptr, size);
	}
}

static void reg_mem_set(void *ptr, _u8 pattern, _u32 sz) {
	if(_g_pi_str_)
		_g_pi_str_->mem_set(ptr, pattern, sz);
}
static void reg_mem_cpy(void *dst, void *src, _u32 sz) {
	if(_g_pi_str_)
		_g_pi_str_->mem_cpy(dst, src, sz);
}
static _u64 reg_lock(_u64 hlock, void *udata) {
	_reg_data_t *rd = (_reg_data_t *)udata;
	return mutex_lock(&(rd->mutex), hlock, _MUTEX_TIMEOUT_INFINITE_, 0);
}

static void reg_unlock(_u64 hlock, void *udata) {
	_reg_data_t *rd = (_reg_data_t *)udata;
	mutex_unlock(&(rd->mutex), hlock);
}

static _vx_res_t _reg_ctl_(_u32 cmd, ...) {
	_u32 r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);
	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				_reg_data_t *p_cdata = va_arg(args, _reg_data_t*);

				if(!_g_pi_str_) {
					HCONTEXT hcstr = p_repo->get_context_by_interface(I_STR);
					if(hcstr)
						_g_pi_str_ = HC_INTERFACE(hcstr);
				}
				if(!_g_heap_)
					_g_heap_ = p_repo->get_context_by_interface(I_HEAP);

				if(p_cdata) {
					mutex_reset(&p_cdata->mutex);
					p_cdata->context.mem_alloc = reg_mem_alloc;
					p_cdata->context.mem_free  = reg_mem_free;
					p_cdata->context.lock	   = reg_lock;
					p_cdata->context.unlock	   = reg_unlock;
					p_cdata->context.mem_set   = reg_mem_set;
					p_cdata->context.mem_cpy   = reg_mem_cpy;
					p_cdata->context.udata	   = p_cdata;
				}
				r = VX_OK;
			}
			break;
		case MODCTL_DESTROY_CONTEXT: {
				_i_repository_t _UNUSED_ *p_repo = va_arg(args, _i_repository_t*);
				_reg_data_t *p_cdata = va_arg(args, _reg_data_t*);
				if(p_cdata)
					reg_uninit(&(p_cdata->context), 0);
				r = VX_OK;
			}
			break;
	}
	va_end(args);

	return r;
}

static void _init(_p_data_t rd, _u32 data_size, _ulong addr_limit, _u32 count) {
	_reg_data_t *p = (_reg_data_t *)rd;
	if(p) {
		p->context.addr_limit = addr_limit;
		p->context.data_size = data_size;
		p->context.inum = count;
		reg_init(&(p->context));
	}
}

static _reg_idx_t _add(_p_data_t rd, _p_data_t data, HMUTEX hlock) {
	_u32 r = INVALID_REG_INDEX;
	_reg_data_t *p = (_reg_data_t *)rd;
	if(p)
		r = reg_add(&(p->context), data, hlock);
	return r;
}

static _p_data_t _get(_p_data_t rd, _reg_idx_t idx, HMUTEX hlock) {
	_p_data_t r = NULL;
	_reg_data_t *p = (_reg_data_t *)rd;
	if(p)
		r = reg_get(&(p->context), idx, hlock);
	return r;
}

static void _del(_p_data_t rd, _reg_idx_t idx, HMUTEX hlock) {
	_reg_data_t *p = (_reg_data_t *)rd;
	if(p)
		reg_del(&(p->context), idx, hlock);
}

static HMUTEX __lock(_p_data_t rd, HMUTEX hlock) {
	HMUTEX r = 0;
	_reg_data_t *p = (_reg_data_t *)rd;
	if(p)
		r = mutex_lock(&(p->mutex), hlock, _MUTEX_TIMEOUT_INFINITE_, 0);
	return r;
}

static void __unlock(_p_data_t rd, HMUTEX hlock) {
	_reg_data_t *p = (_reg_data_t *)rd;
	if(p)
		mutex_unlock(&(p->mutex), hlock);
}

static _i_reg_t _g_interface_ = {
	.init	= _init,
	.add	= _add,
	.get	= _get,
	.del	= _del,
	.lock	= __lock,
	.unlock	= __unlock
};

DEF_VXMOD(
	MOD_REG,
	I_REGISTER, 
	&_g_interface_, /* interface */ 
	NULL, /* no static data context */
	sizeof(_reg_data_t), /* sizeof data context */
	_reg_ctl_, /* module controll */
	1, 0, 1, /* version */ 
	"data register"
);

