#include "vxmod.h"
#include "err.h"
#include "i_memory.h"
#include "i_sync.h"
#include "i_repository.h"
#include "slab.h"
#include "i_str.h"
#include "mutex.h"

#define SLAB_PAGE_SIZE	4096

static _i_pma_t	*_pi_pma = NULL;
static _i_repository_t *_pi_repo = NULL;
static _i_str_t	*_pi_str = NULL;

typedef struct {
	_mutex_t	_mutex;
	_slab_context_t	_slab;
}_heap_data_t;

static void *page_alloc(_u32 npages, _ulong limit, void _UNUSED_ *p_udata) {
	void *r = NULL;
	if(_pi_pma)
		r = _pi_pma->alloc_seq(npages, RT_SYSTEM, limit);
	return r;
}
static void page_free(void *ptr, _u32 npages, void _UNUSED_ *p_udata) {
	if(_pi_pma)
		_pi_pma->free_seq(ptr, npages);
}
static void mem_set(void *ptr, _u8 pattern, _u32 sz) {
	if(_pi_str)
		_pi_str->mem_set(ptr, pattern, sz);
}
static void mem_cpy(void *dst, void *src, _u32 sz) {
	if(_pi_str)
		_pi_str->mem_cpy(dst, src, sz);
}
static _slab_hlock_t lock(_slab_hlock_t hlock, void *p_udata) {
	_heap_data_t *p = (_heap_data_t *)p_udata;
	return mutex_lock(&(p->_mutex), hlock, _MUTEX_TIMEOUT_INFINITE_, 0);
}
static void unlock(_slab_hlock_t hlock, void *p_udata) {
	_heap_data_t *p = (_heap_data_t *)p_udata;
	mutex_unlock(&(p->_mutex), hlock);
}
static _u32 _init_context(_i_repository_t *p_repo, _heap_data_t *p_heap) {
	_u32 r = VX_ERR;

	if(!_pi_str) {
		HCONTEXT hcstr = p_repo->get_context_by_interface(I_STR);
		if(hcstr)
			_pi_str = HC_INTERFACE(hcstr);
	}

	/* clear context */
	_pi_str->mem_set(p_heap, 0, sizeof(_heap_data_t));

	mutex_reset(&(p_heap->_mutex));

	if(!_pi_pma) {
		HCONTEXT hpma = p_repo->get_context_by_interface(I_PMA);
		if(hpma)
			_pi_pma = (_i_pma_t *)HC_INTERFACE(hpma);
	}

	if(_pi_pma) {
		/* ...
		 init slab context ...*/
		p_heap->_slab.page_size   = SLAB_PAGE_SIZE;
		p_heap->_slab.p_mem_alloc = page_alloc;
		p_heap->_slab.p_mem_free  = page_free;
		p_heap->_slab.p_mem_cpy   = mem_cpy,
		p_heap->_slab.p_mem_set   = mem_set,
		p_heap->_slab.p_lock      = lock;
		p_heap->_slab.p_unlock    = unlock;
		p_heap->_slab.p_udata     = p_heap;

		_pi_repo = p_repo;

		if(slab_init(&p_heap->_slab)) {
			r = VX_OK;
		}
	}

	return r;
}

static _u32 init_context(va_list args) {
	_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
	_heap_data_t *p_heap = va_arg(args, _heap_data_t*);
	return _init_context(p_repo, p_heap);
}

static _u32 destroy_context(va_list args) {
	_u32 r = VX_ERR;
	_i_repository_t _UNUSED_ *p_repo = va_arg(args, _i_repository_t*);
	_heap_data_t *p_heap = va_arg(args, _heap_data_t*);
	if(p_heap) {
		slab_destroy(&p_heap->_slab);
		r = VX_OK;
	}
	return r;
}

static _u32 heap_ctl(_u32 cmd, ...) {
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

static void *heap_alloc(_p_data_t hd, _u32 size, _ulong limit) {
	void *r = NULL;
	_heap_data_t *p_heap = hd;
	if(p_heap)
		r = slab_alloc(&p_heap->_slab, size, limit);
	return r;
}
static void heap_free(_p_data_t hd, void *ptr, _u32 size) {
	_heap_data_t *p_heap = hd;
	if(p_heap)
		slab_free(&p_heap->_slab, ptr, size);
}
static _bool heap_verify(_p_data_t hd, void *ptr, _u32 size) {
	_bool r = _false;
	_heap_data_t *p_heap = hd;
	if(p_heap)
		r = slab_verify(&p_heap->_slab, ptr, size);
	return r;
}
static void heap_info(_p_data_t hd, _heap_info_t *p_info) {
	_heap_data_t *p_heap = hd;
	_slab_status_t sst;

	slab_status(&p_heap->_slab, &sst);
	p_info->base = (_u64)p_heap->_slab.p_slab;
	p_info->size = (sst.ndpg + sst.nspg) * p_heap->_slab.page_size;
	p_info->chunk_size = sizeof(_slab_t);
	p_info->data_load = sst.ndpg * p_heap->_slab.page_size;
	p_info->meta_load = sst.nspg * p_heap->_slab.page_size;
	p_info->free = 0;
	p_info->unused = 0;
	p_info->objects = sst.naobj;
}

static _heap_data_t _g_data; /* static data context */

static _i_heap_t _g_public = {
	.alloc = heap_alloc,
	.free  = heap_free,
	.verify= heap_verify,
	.info  = heap_info
};

DEF_VXMOD(
	MOD_HEAP,
	I_HEAP,
	&_g_public, /* interface */
	&_g_data, /* static data context */
	sizeof(_g_data), /* sizeof data context */
	heap_ctl, /* module controll */
	1, 0, 1, /* version */
	"heap memory allocator, based on slabs"
);
