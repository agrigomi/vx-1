#include "vxmod.h"
#include "i_repository.h"
#include "i_memory.h"
#include "i_str.h"
#include "err.h"

#define BMAP_INITIAL_SIZE	256

typedef struct { /* buffer record */
	_p_data_t	p_buffer;
	_u64		key;
	union {
		_u32 counter;
		struct {
			_u32	_uc	:31; /* usage counter */
			_u32	_wf	:1;  /* dirty flag */
		};
	};
}__attribute__((packed)) _buffer_t;

typedef struct { /* data context of buffer map */
	HCONTEXT	mutex;
	_u32		buffer_size;
	_u32		bmap_size; /* in structures (count of buffers) */
	_buffer_t	*bmap;
	_bio_t		*pcb_read;
	_bio_t		*pcb_write;
	void		*udata;
}_bmdc_t;

static HCONTEXT	_g_heap_  = NULL;
static _i_str_t	*_g_pi_str_ = NULL;

static HMUTEX bm_lock(_p_data_t dc, HMUTEX hlock) {
	HMUTEX r = 0;
	_bmdc_t *pdc = (_bmdc_t *)dc;
	if(pdc->mutex) {
		_i_mutex_t *pi = HC_INTERFACE(pdc->mutex);
		_p_data_t   pd = HC_DATA(pdc->mutex);
		if(pi && pd)
			r = pi->lock(pd, hlock);
	}
	return r;
}
static void bm_unlock(_p_data_t dc, HMUTEX hlock) {
	_bmdc_t *pdc = (_bmdc_t *)dc;
	if(pdc->mutex) {
		_i_mutex_t *pi = HC_INTERFACE(pdc->mutex);
		_p_data_t   pd = HC_DATA(pdc->mutex);
		if(pi && pd)
			pi->unlock(pd, hlock);
	}
}

static void flush_map(_p_data_t dc, HMUTEX hlock) {
	_bmdc_t *pdc = (_bmdc_t *)dc;

	if(pdc) {
		if(pdc->pcb_write) {
			HMUTEX hm = bm_lock(pdc, hlock);
			_u32 i = 0;

			for(; i < pdc->bmap_size; i++) {
				if(pdc->bmap[i]._wf && pdc->bmap[i].key != INVALID_BUFFER_KEY) {
					if(pdc->pcb_write(pdc->bmap[i].key, pdc->bmap[i].p_buffer, pdc->udata) == VX_OK)
						pdc->bmap[i]._wf = 0;
				}
			}
			bm_unlock(pdc, hm);
		}
	}
}

static _vx_res_t realloc_buffer_map(_bmdc_t *pdc, HMUTEX hm) {
	_vx_res_t r = VX_ERR;
	HMUTEX hlock = bm_lock(pdc, hm);
	_buffer_t *p_old_map = pdc->bmap;
	_u32 new_bmap_size = pdc->bmap_size + BMAP_INITIAL_SIZE;

	if(_g_heap_ && pdc->buffer_size) {
		_i_heap_t *pi = HC_INTERFACE(_g_heap_);
		_p_data_t *pd = HC_DATA(_g_heap_);

		if(pi && pd) {
			if((pdc->bmap = pi->alloc(pd, new_bmap_size * sizeof(_buffer_t), NO_ALLOC_LIMIT))) {
				_g_pi_str_->mem_set(pdc->bmap, 0, new_bmap_size * sizeof(_buffer_t));
				_u32 i = 0;
				for(; i < new_bmap_size; i++)
					pdc->bmap[i].key = INVALID_BUFFER_KEY;
				if(p_old_map) {
					_g_pi_str_->mem_cpy(pdc->bmap, p_old_map, pdc->bmap_size * sizeof(_buffer_t));
					pi->free(pd, p_old_map, pdc->bmap_size * sizeof(_buffer_t));
				}

				pdc->bmap_size = new_bmap_size;
				r = VX_OK;
			} else
				pdc->bmap = p_old_map;
		}
	}
	bm_unlock(pdc, hlock);
	return r;
}

static _bid_t alloc_buffer(_p_data_t dc, _u64 key, HMUTEX hlock) {
	_bid_t r = INVALID_BUFFER_ID;
	_bmdc_t *pdc = (_bmdc_t *)dc;
	HMUTEX hm = bm_lock(pdc, hlock);
	_u32 bn_free = INVALID_BUFFER_ID;

_alloc_buffer_:
	if(pdc) {
		_u32 i = 0;

		if(pdc->bmap) {
			for(; i < pdc->bmap_size; i++) {
				if(pdc->bmap[i].key == key) {
					r = i;
					pdc->bmap[i]._uc++;
					break;
				}
				if(pdc->bmap[i]._uc == 0 &&
						pdc->bmap[i]._wf == 0 &&
						bn_free == INVALID_BUFFER_ID)
					bn_free = i;

				if(pdc->bmap[i].key == INVALID_BUFFER_KEY)
					break;
			}
		}

		if(r == INVALID_BUFFER_ID) {
			if(bn_free == INVALID_BUFFER_ID) {
				if(realloc_buffer_map(pdc, hm) == VX_OK)
					goto _alloc_buffer_;
			} else {
				if(pdc->bmap[bn_free].p_buffer == NULL) {
					if(_g_heap_) {
						_i_heap_t *pi = HC_INTERFACE(_g_heap_);
						_p_data_t pd = HC_DATA(_g_heap_);

						pdc->bmap[bn_free].p_buffer = pi->alloc(pd, pdc->buffer_size, NO_ALLOC_LIMIT);
					}
				}

				if(pdc->bmap[bn_free].p_buffer) {
					if(pdc->pcb_read(key, pdc->bmap[bn_free].p_buffer, pdc->udata) == VX_OK) {
						pdc->bmap[bn_free]._uc++;
						pdc->bmap[bn_free].key = key;
						r = bn_free;
					}
				}
			}
		}
	}

	bm_unlock(pdc, hm);
	return r;
}

static void release_buffer(_p_data_t dc, _bid_t bid, HMUTEX hlock) {
	_bmdc_t *pdc = (_bmdc_t *)dc;

	if(pdc) {
		HMUTEX hm = bm_lock(pdc, hlock);
		if(bid < pdc->bmap_size) {
			if(pdc->bmap[bid]._uc)
				pdc->bmap[bid]._uc--;
		}
		bm_unlock(pdc, hm);
	}
}

/* cleanup whole buffer map */
static _vx_res_t release_buffer_map(_bmdc_t *pdc) {
	_vx_res_t r = VX_ERR;
	HMUTEX hlock = bm_lock(pdc, 0);
	if(_g_heap_) {
		_i_heap_t *pi = HC_INTERFACE(_g_heap_);
		_p_data_t *pd = HC_DATA(_g_heap_);

		if(pi && pd) {
			_u32 i = 0;

			flush_map(pdc, hlock);
			for(; i < pdc->bmap_size; i++) {
				if(pdc->bmap[i].p_buffer) {
					pi->free(pd, pdc->bmap[i].p_buffer, pdc->buffer_size);
					pdc->bmap[i].p_buffer = NULL;
				}
				pdc->bmap[i]._uc = 0;
				pdc->bmap[i].key = INVALID_BUFFER_KEY;
			}

			pi->free(pd, pdc->bmap, pdc->bmap_size * sizeof(_buffer_t));
			pdc->bmap = NULL;
			r = VX_OK;
		}
	}
	bm_unlock(pdc, hlock);
	return r;
}

static _vx_res_t _mod_ctl_(_u32 cmd, ...) {
	_vx_res_t r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				_bmdc_t *p_context = va_arg(args, _bmdc_t*);

				if(!_g_heap_) /* first time here */
					/* get system heap */
					_g_heap_  = p_repo->get_context_by_interface(I_HEAP);

				if(!_g_pi_str_) {
					HCONTEXT hcstr = p_repo->get_context_by_interface(I_STR);
					if(hcstr)
						_g_pi_str_ = HC_INTERFACE(hcstr);
				}

				if(p_context) {
					p_context->mutex = p_repo->create_context_by_interface(I_MUTEX);
					p_context->buffer_size = 0;
					p_context->bmap_size = 0;
					p_context->bmap = NULL;
					p_context->pcb_read = NULL;
					p_context->pcb_write = NULL;
				}
				r = VX_OK;
			} break;
		case MODCTL_DESTROY_CONTEXT: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				_bmdc_t *p_context = va_arg(args, _bmdc_t*);

				if(p_context) {
					/* free buffer memory */
					if(release_buffer_map(p_context) == VX_OK)
						/* destroy mutex in data context */
						p_repo->release_context(p_context->mutex);
				}
				r = VX_OK;
			} break;
	}

	va_end(args);
	return r;
}

static _p_data_t bm_ptr(_p_data_t dc, _bid_t bid, HMUTEX hlock) {
	_p_data_t r = NULL;
	_bmdc_t *pdc = (_bmdc_t *)dc;

	if(pdc) {
		HMUTEX hm = bm_lock(pdc, hlock);
		if(bid < pdc->bmap_size)
			r = pdc->bmap[bid].p_buffer;
		bm_unlock(pdc, hm);
	}

	return r;
}

static _u64 bm_key(_p_data_t dc, _bid_t bid, HMUTEX hlock) {
	_u64 r = INVALID_BUFFER_ID;
	_bmdc_t *pdc = (_bmdc_t *)dc;

	if(pdc) {
		HMUTEX hm = bm_lock(pdc, hlock);
		if(bid < pdc->bmap_size)
			r = pdc->bmap[bid].key;
		bm_unlock(pdc, hm);
	}

	return r;
}

static void bm_flush(_p_data_t dc, _bid_t bid, HMUTEX hlock) {
	_bmdc_t *pdc = (_bmdc_t *)dc;
	if(pdc) {
		HMUTEX hm = bm_lock(pdc, hlock);
		if(pdc->pcb_write && bid < pdc->bmap_size && pdc->bmap[bid].key != INVALID_BUFFER_KEY) {
			if(pdc->pcb_write(pdc->bmap[bid].key, pdc->bmap[bid].p_buffer, pdc->udata) == VX_OK)
				pdc->bmap[bid]._wf = 0;
		}
		bm_unlock(pdc, hm);
	}
}

static void bm_dirty(_p_data_t dc, _bid_t bid, HMUTEX hlock) {
	_bmdc_t *pdc = (_bmdc_t *)dc;
	if(pdc) {
		HMUTEX hm = bm_lock(pdc, hlock);
		if(bid < pdc->bmap_size && pdc->bmap[bid].key != INVALID_BUFFER_KEY)
			pdc->bmap[bid]._wf = 1;
		bm_unlock(pdc, hm);
	}
}

static void bm_reset(_p_data_t dc, _bid_t bid, HMUTEX hlock) {
	_bmdc_t *pdc = (_bmdc_t *)dc;
	if(pdc) {
		HMUTEX hm = bm_lock(pdc, hlock);
		if(pdc->pcb_read && bid < pdc->bmap_size && pdc->bmap[bid].key != INVALID_BUFFER_KEY) {
			if(pdc->pcb_read(pdc->bmap[bid].key, pdc->bmap[bid].p_buffer, pdc->udata) == VX_OK)
				pdc->bmap[bid]._wf = 0;
		}
		bm_unlock(pdc, hm);
	}
}

static void bm_reset_map(_p_data_t dc, HMUTEX hlock) {
	_bmdc_t *pdc = (_bmdc_t *)dc;
	if(pdc) {
		if(pdc->pcb_read) {
			HMUTEX hm = bm_lock(pdc, hlock);
			_u32 i = 0;

			for(; i < pdc->bmap_size; i++) {
				if(pdc->bmap[i]._wf && pdc->bmap[i].key != INVALID_BUFFER_KEY) {
					if(pdc->pcb_read(pdc->bmap[i].key, pdc->bmap[i].p_buffer, pdc->udata) == VX_OK)
						pdc->bmap[i]._wf = 0; 
				}
			}
			bm_unlock(pdc, hm);
		}
	}
}

static void bm_init(_p_data_t dc, _u32 buffer_size, _bio_t pcb_read, _bio_t pcb_write, void *udata) {
	_bmdc_t *pdc = (_bmdc_t *)dc;
	if(pdc) {
		pdc->pcb_read = pcb_read;
		pdc->pcb_write = pcb_write;
		pdc->buffer_size = buffer_size;
		pdc->udata = udata;
	}
}

static _i_buffer_map_t _g_interface_ = {
	.init		= bm_init,
	.alloc		= alloc_buffer,
	.free		= release_buffer,
	.flush_all	= flush_map,
	.reset_all	= bm_reset_map,
	.ptr		= bm_ptr,
	.key		= bm_key,
	.flush		= bm_flush,
	.dirty		= bm_dirty,
	.reset		= bm_reset,
	.lock		= bm_lock,
	.unlock		= bm_unlock,
};

DEF_VXMOD(
	MOD_BUFFER_MAP, /* module name */
	I_BUFFER_MAP, /* interface name */
	&_g_interface_, /* interface pointer */
	NULL,	/* no static data context */
	sizeof(_bmdc_t), /* size of data for dynamic context */
	_mod_ctl_,
	1,0,1,
	"buffer map"
);

