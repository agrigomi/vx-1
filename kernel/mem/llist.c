#include "vxmod.h"
#include "i_memory.h"
#include "i_repository.h"
#include "ll_alg.h"
#include "err.h"
#include "i_str.h"
#include "mutex.h"

typedef struct { /* llist context data */
	_mutex_t	m_mutex;
	_ll_context_t	m_llc; /* algorithm context */
}_llc_data_t;

static _i_str_t		*_g_pi_str = NULL;
static _i_heap_t	*_g_pi_heap = NULL;
static _p_data_t	_g_pd_heap = NULL;

static void *_ll_alloc(_u32 size, _ulong limit, void _UNUSED_ *p_udata) {
	void *r = NULL;
	if(_g_pi_heap && _g_pd_heap)
		r = _g_pi_heap->alloc(_g_pd_heap, size, limit);
	return r;
}
static void _ll_free(void *ptr, _u32 size, void _UNUSED_ *p_udata) {
	if(_g_pi_heap && _g_pd_heap)
		_g_pi_heap->free(_g_pd_heap, ptr, size);
}
static _u64 _ll_lock(_u64 hlock, void *p_udata) {
	_llc_data_t *p = (_llc_data_t *)p_udata;
	return mutex_lock(&(p->m_mutex), hlock, _MUTEX_TIMEOUT_INFINITE_, 0);
}
static void _ll_unlock(_u64 hlock, void *p_udata) {
	_llc_data_t *p = (_llc_data_t *)p_udata;
	mutex_unlock(&(p->m_mutex), hlock);
}

static _u32 init_context(va_list args) {
	_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
	_llc_data_t *p_ll_context = va_arg(args, _llc_data_t*);

	if(!_g_pi_str) {
		HCONTEXT hcstr = p_repo->get_context_by_interface(I_STR);
		if(hcstr)
			_g_pi_str = HC_INTERFACE(hcstr);
	}

	if(p_ll_context) {
		_g_pi_str->mem_set(p_ll_context, 0, sizeof(_llc_data_t));
		if(!_g_pi_heap && !_g_pd_heap) {
			HCONTEXT hc_heap = p_repo->get_context_by_interface(I_HEAP);
			if(!hc_heap)
				return VX_ERR;
			_g_pi_heap = (_i_heap_t *)HC_INTERFACE(hc_heap);
			_g_pd_heap = HC_DATA(hc_heap);
		}
		/* init members in llist context */
		mutex_reset(&(p_ll_context->m_mutex));
		p_ll_context->m_llc.p_alloc = _ll_alloc;
		p_ll_context->m_llc.p_free  = _ll_free;
		p_ll_context->m_llc.p_lock  = _ll_lock;
		p_ll_context->m_llc.p_unlock= _ll_unlock;
		p_ll_context->m_llc.p_udata = p_ll_context;
	}
	return VX_OK;
}
static _u32 destroy_context(va_list args) {
	_u32 r = VX_OK;
	_i_repository_t _UNUSED_ *p_repo = va_arg(args, _i_repository_t*);
	_llc_data_t *p_ll_context = va_arg(args, _llc_data_t*);

	ll_clr(&p_ll_context->m_llc, 0);
	ll_uninit(&p_ll_context->m_llc);

	return r;
}

static _u32 ll_ctl(_u32 cmd, ...) {
	_u32 r = VX_ERR;
	va_list args;

	va_start(args, cmd);
	switch(cmd) {
		case MODCTL_INIT_CONTEXT:
			r = init_context(args);
			break;
		case MODCTL_DESTROY_CONTEXT:
			r = destroy_context(args);
			break;
	}
	va_end(args);

	return r;
}

static void _ll_init(_p_data_t ld, _u8 mode, _u8 ncol, _ulong addr_limit) {
	_llc_data_t *p = (_llc_data_t *)ld;
	ll_init(&p->m_llc, mode, ncol, addr_limit);
}
static void *_ll_get(_p_data_t ld, _u32 index, _u32 *p_size, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return ll_get(&p->m_llc, index, p_size, hlock);
}
static void *_ll_add(_p_data_t ld, void *p_data, _u32 size, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return ll_add(&p->m_llc, p_data, size, hlock);
}
static void *_ll_ins(_p_data_t ld, _u32 index, void *p_data, _u32 size, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return ll_ins(&p->m_llc, index, p_data, size, hlock);
}
static void _ll_rem(_p_data_t ld, _u32 index, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	ll_rem(&p->m_llc, index, hlock);
}
static void _ll_del(_p_data_t ld, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	ll_del(&p->m_llc, hlock);
}
static void _ll_clr(_p_data_t ld, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	ll_clr(&p->m_llc, hlock);
}
static _u32 _ll_cnt(_p_data_t ld, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return ll_cnt(&p->m_llc, hlock);
}
static void _ll_col(_p_data_t ld, _u8 col, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	ll_col(&p->m_llc, col, hlock);
}
static _u8 _ll_sel(_p_data_t ld, void *p_data, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return ll_sel(&p->m_llc, p_data, hlock);
}
static _u8 _ll_mov(_p_data_t ld, void *p_data, _u8 col, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return ll_mov(&p->m_llc, p_data, col, hlock);
}
static void *_ll_next(_p_data_t ld, _u32 *p_size, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return ll_next(&p->m_llc, p_size, hlock);
}
static void *_ll_current(_p_data_t ld, _u32 *p_size, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return ll_current(&p->m_llc, p_size, hlock);
}
static void *_ll_first(_p_data_t ld, _u32 *p_size, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return ll_first(&p->m_llc, p_size, hlock);
}
static void *_ll_last(_p_data_t ld, _u32 *p_size, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return ll_last(&p->m_llc, p_size, hlock);
}
static void *_ll_prev(_p_data_t ld, _u32 *p_size, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return ll_prev(&p->m_llc, p_size, hlock);
}
static void _ll_roll(_p_data_t ld, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	ll_roll(&p->m_llc, hlock);
}
static HMUTEX __ll_lock(_p_data_t ld, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	return mutex_lock(&(p->m_mutex), hlock, _MUTEX_TIMEOUT_INFINITE_, 0);
}
static void __ll_unlock(_p_data_t ld, HMUTEX hlock) {
	_llc_data_t *p = (_llc_data_t *)ld;
	mutex_unlock(&(p->m_mutex), hlock);
}

static _i_llist_t	_g_ll_interface = {
	.init	= _ll_init,
	.get	= _ll_get,
	.add	= _ll_add,
	.ins	= _ll_ins,
	.rem	= _ll_rem,
	.del	= _ll_del,
	.clr	= _ll_clr,
	.cnt	= _ll_cnt,
	.col	= _ll_col,
	.sel	= _ll_sel,
	.mov	= _ll_mov,
	.next	= _ll_next,
	.current= _ll_current,
	.first	= _ll_first,
	.last 	= _ll_last,
	.prev	= _ll_prev,
	.roll	= _ll_roll,
	.lock	= __ll_lock,
	.unlock	= __ll_unlock
};

DEF_VXMOD(
	MOD_LLIST,
	I_LLIST,
	&_g_ll_interface, /* interface */
	NULL, /* no static data context */
	sizeof(_llc_data_t), /* sizeof data context */
	ll_ctl, /* module controll */
	1, 0, 1, /* version */
	"linked lists"
);

