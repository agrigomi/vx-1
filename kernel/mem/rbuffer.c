#include "vxmod.h"
#include "i_repository.h"
#include "i_memory.h"
#include "i_str.h"
#include "rb_alg.h"
#include "err.h"

typedef struct {
	_rb_context_t	rbc;
	_u8		flags;
	HCONTEXT	hc_mutex;
}_rbdc_t;

static _i_str_t *_g_i_str = NULL;
static HCONTEXT _g_hc_heap = NULL;

static void _init(_p_data_t dc, _u32 capacity, _u8 flags) {
	_rbdc_t *pdc = (_rbdc_t *)dc;
	if(pdc) {
		rb_init(&pdc->rbc, capacity, pdc);
		pdc->flags = flags;
	}
}

static void _destroy(_p_data_t dc) {
	_rbdc_t *pdc = (_rbdc_t *)dc;
	if(pdc)
		rb_destroy(&pdc->rbc);
}

static void _push(_p_data_t dc, void *data, _u16 size) {
	_rbdc_t *pdc = (_rbdc_t *)dc;
	if(pdc)
		rb_push(&pdc->rbc, data, size);
}

static void *_pull(_p_data_t dc, _u16 *psz) {
	void * r = NULL;
	_rbdc_t *pdc = (_rbdc_t *)dc;
	if(pdc)
		r = rb_pull(&pdc->rbc, psz);
	return r;
}

static void _reset_pull(_p_data_t dc) {
	_rbdc_t *pdc = (_rbdc_t *)dc;
	if(pdc)
		rb_reset_pull(&pdc->rbc);
}

static _u64 _lock(_u64 hlock, void *udata) {
	_u64 r = 0;
	_rbdc_t *prbdc = (_rbdc_t *)udata;

	if(prbdc->hc_mutex) {
		_i_mutex_t *pi = HC_INTERFACE(prbdc->hc_mutex);
		_p_data_t pd = HC_DATA(prbdc->hc_mutex);

		r = pi->lock(pd, hlock);
	}

	return r;
}

static void _unlock(_u64 hlock, void *udata) {
	_rbdc_t *prbdc = (_rbdc_t *)udata;

	if(prbdc->hc_mutex) {
		_i_mutex_t *pi = HC_INTERFACE(prbdc->hc_mutex);
		_p_data_t pd = HC_DATA(prbdc->hc_mutex);

		pi->unlock(pd, hlock);
	}
}

static _i_ring_buffer_t _g_rb_interface = {
	.init 		= _init,
	.destroy 	= _destroy,
	.push		= _push,
	.pull		= _pull,
	.reset_pull	= _reset_pull,
};

static void *_mem_alloc(_u32 sz, void _UNUSED_ *udata) {
	void *r = NULL;
	if(_g_hc_heap) {
		_i_heap_t *pi = HC_INTERFACE(_g_hc_heap);
		_p_data_t pd = HC_DATA(_g_hc_heap);

		if(pi && pd)
			r = pi->alloc(pd, sz, NO_ALLOC_LIMIT);
	}
	return r;
}

static void _mem_free(void *ptr, _u32 sz, void _UNUSED_ *udata) {
	if(_g_hc_heap) {
		_i_heap_t *pi = HC_INTERFACE(_g_hc_heap);
		_p_data_t pd = HC_DATA(_g_hc_heap);

		if(pi && pd)
			pi->free(pd, ptr, sz);
	}
}

static void _mem_set(void *ptr, _u8 pattern, _u32 size, void _UNUSED_ *udata) {
	if(_g_i_str)
		_g_i_str->mem_set(ptr, pattern, size);
}

static void _mem_cpy(void *dst, void *src, _u32 size, void _UNUSED_ *udata) {
	if(_g_i_str)
		_g_i_str->mem_cpy(dst, src, size);
}

static _vx_res_t rb_ctl(_u32 cmd, ...) {
	_vx_res_t r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				_rbdc_t *p_rbdc = va_arg(args, _rbdc_t*);

				if(!_g_i_str) {
					HCONTEXT hc_str = p_repo->get_context_by_interface(I_STR);
					if(hc_str)
						_g_i_str = HC_INTERFACE(hc_str);
				}

				if(!_g_hc_heap)
					_g_hc_heap = p_repo->get_context_by_interface(I_HEAP);

				if(p_rbdc) {
					p_rbdc->hc_mutex = p_repo->create_context_by_interface(I_MUTEX);
					p_rbdc->rbc.mem_alloc 	= _mem_alloc;
					p_rbdc->rbc.mem_free 	= _mem_free;
					p_rbdc->rbc.mem_set 	= _mem_set;
					p_rbdc->rbc.mem_cpy 	= _mem_cpy;
					if(p_rbdc->flags & RBF_SYNC) {
						p_rbdc->rbc.lock 	= _lock;
						p_rbdc->rbc.unlock 	= _unlock;
					} else {
						p_rbdc->rbc.lock 	= 0;
						p_rbdc->rbc.unlock 	= 0;
					}
					p_rbdc->rbc.rb_udata 	= p_rbdc;
					p_rbdc->rbc.rb_addr 	= NULL;
					p_rbdc->rbc.rb_size 	= 0;
					p_rbdc->rbc.rb_first	= 0;
					p_rbdc->rbc.rb_last 	= 0;
					p_rbdc->rbc.rb_pull 	= 0;
				}

				r = VX_OK;
			} break;
		case MODCTL_DESTROY_CONTEXT: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				_rbdc_t *p_rbdc = va_arg(args, _rbdc_t*);

				if(p_rbdc) {
					rb_destroy(&p_rbdc->rbc);
					p_repo->release_context(p_rbdc->hc_mutex);
				}

				r = VX_OK;
			} break;
	}

	va_end(args);

	return r;
}

DEF_VXMOD(
	MOD_RING_BUFFER,	/* module name */
	I_RING_BUFFER,	/* interface name */
	&_g_rb_interface, /* interface */
	NULL, /* no static data context */
	sizeof(_rbdc_t), /* sizeof data context */
	rb_ctl, /* module controll */
	1, 0, 1, /* version */
	"ring buffer"
);

