/* implementation of single storage pool (SSP) */
#include "i_vfs.h"
#include "i_repository.h"
#include "i_memory.h"
#include "i_dev_root.h"
#include "err.h"
#include "i_str.h"

#define BMAP_INITIAL_SIZE	256

typedef struct {
	_ulong	offset; /* offset at begin */
	_ulong	size;   /* size in bytes */
	HFILE	hfile; /* file handle provided by devfs */
}_vx_storage_t;

static HCONTEXT	_g_heap_  = NULL;
static HCONTEXT _g_dev_root_ = NULL;
static _i_str_t	*_g_pi_str_ = NULL;

typedef struct { /* buffer record */
	_p_data_t	p_buffer;
	_ulong		unit_number;
	union {
		_u32 counter;
		struct {
			_u32	_uc	:31; /* usage counter */
			_u32	_wf	:1;  /* write flag */
		};
	};
}__attribute__((packed)) _buffer_t;

typedef struct {
	HCONTEXT	c_mutex;
	_vx_storage_t	c_storage;
	_u32		c_unit_size;
	_u32		c_bmap_size; /* in structures (count of buffers) */
	_buffer_t	*c_bmap;
}_data_context_t;

static void init_pool(_p_data_t, _u32 max_storages, _u32 unit_size);
static _u32 attach_storage(_p_data_t, HFILE, _ulong offset, _ulong size);
static _vx_res_t detach_storage(_p_data_t, _u32 index);
static _u32 alloc_buffer(_p_data_t, _ulong unit_number, HMUTEX);
static void free_buffer(_p_data_t, _u32 bn, HMUTEX);
static _p_data_t get_buffer_ptr(_p_data_t, _u32 bn, HMUTEX);
static _ulong get_buffer_unit(_p_data_t, _u32 bn, HMUTEX);
static void flush_buffer(_p_data_t, _u32 bn, HMUTEX);
static void flush_map(_p_data_t, HMUTEX);
static void set_buffer_dirty(_p_data_t, _u32 bn, HMUTEX);
static void rollback_map(_p_data_t, HMUTEX);

static HMUTEX lock(_p_data_t dc, HMUTEX hlock) {
	HMUTEX r = 0;
	_data_context_t *pdc = (_data_context_t *)dc;
	if(pdc->c_mutex) {
		_i_mutex_t *pi = HC_INTERFACE(pdc->c_mutex);
		_p_data_t   pd = HC_DATA(pdc->c_mutex);
		if(pi && pd)
			r = pi->lock(pd, hlock);
	}
	return r;
}
static void unlock(_p_data_t dc, HMUTEX hlock) {
	_data_context_t *pdc = (_data_context_t *)dc;
	if(pdc->c_mutex) {
		_i_mutex_t *pi = HC_INTERFACE(pdc->c_mutex);
		_p_data_t   pd = HC_DATA(pdc->c_mutex);
		if(pi && pd)
			pi->unlock(pd, hlock);
	}
}

static _vx_res_t realloc_buffer_map(_data_context_t *pdc, HMUTEX hm) {
	_vx_res_t r = VX_ERR;
	HMUTEX hlock = lock(pdc, hm);
	_buffer_t *p_old_map = pdc->c_bmap;
	_u32 new_bmap_size = pdc->c_bmap_size + BMAP_INITIAL_SIZE;

	if(_g_heap_) {
		_i_heap_t *pi = HC_INTERFACE(_g_heap_);
		_p_data_t *pd = HC_DATA(_g_heap_);

		if(pi && pd) {
			if((pdc->c_bmap = pi->alloc(pd, new_bmap_size * sizeof(_buffer_t), NO_ALLOC_LIMIT))) {
				_g_pi_str_->mem_set(pdc->c_bmap, 0, new_bmap_size * sizeof(_buffer_t));
				_u32 i = 0;
				for(; i < new_bmap_size; i++)
					pdc->c_bmap[i].unit_number = INVALID_UNIT_NUMBER;
				if(p_old_map) {
					_g_pi_str_->mem_cpy(pdc->c_bmap, p_old_map, pdc->c_bmap_size * sizeof(_buffer_t));
					pi->free(pd, p_old_map, pdc->c_bmap_size * sizeof(_buffer_t));
				}

				pdc->c_bmap_size = new_bmap_size;
				r = VX_OK;
			} else
				pdc->c_bmap = p_old_map;
		}
	}
	unlock(pdc, hlock);
	return r;
}

/* cleanup whole buffer map */
static _vx_res_t release_buffer_map(_data_context_t *pdc) {
	_vx_res_t r = VX_ERR;
	HMUTEX hlock = lock(pdc, 0);
	if(_g_heap_) {
		_i_heap_t *pi = HC_INTERFACE(_g_heap_);
		_p_data_t *pd = HC_DATA(_g_heap_);

		if(pi && pd) {
			_u32 i = 0;

			flush_map(pdc, hlock);
			for(; i < pdc->c_bmap_size; i++) {
				if(pdc->c_bmap[i].p_buffer) {
					pi->free(pd, pdc->c_bmap[i].p_buffer, pdc->c_unit_size);
					pdc->c_bmap[i].p_buffer = NULL;
				}
				pdc->c_bmap[i]._uc = 0;
				pdc->c_bmap[i].unit_number = INVALID_UNIT_NUMBER;
			}

			pi->free(pd, pdc->c_bmap, pdc->c_bmap_size * sizeof(_buffer_t));
			pdc->c_bmap = NULL;
			r = VX_OK;
		}
	}
	unlock(pdc, hlock);
	return r;
}

static _vx_res_t read_unit(_data_context_t *pdc, _ulong unit, _p_data_t buffer, HMUTEX hlock) {
	_vx_res_t r = VX_ERR;
	_ctl_t *p_ctl = NULL;
	_ulong offset = pdc->c_storage.offset + (unit * pdc->c_unit_size);
	_u32 nb = 0;

	HMUTEX hm = lock(pdc, hlock);
	if((p_ctl = pdc->c_storage.hfile->_h_file_->_f_vfs_->_v_ctl_)) {
		if((r = p_ctl(VFSCTL_SEEK, pdc->c_storage.hfile, offset, &offset)) == VX_OK)
			r = p_ctl(VFSCTL_READ, pdc->c_storage.hfile, pdc->c_unit_size, buffer, &nb);
	}
	unlock(pdc, hm);
	return r;
}

static _vx_res_t write_unit(_data_context_t *pdc, _ulong unit, _p_data_t buffer, HMUTEX hlock) {
	_vx_res_t r = VX_ERR;
	_ctl_t *p_ctl = NULL;
	_ulong offset = pdc->c_storage.offset + (unit * pdc->c_unit_size);
	_u32 nb = 0;

	HMUTEX hm = lock(pdc, hlock);
	if((p_ctl = pdc->c_storage.hfile->_h_file_->_f_vfs_->_v_ctl_)) {
		if((r = p_ctl(VFSCTL_SEEK, pdc->c_storage.hfile, offset, &offset)) == VX_OK)
			r = p_ctl(VFSCTL_WRITE, pdc->c_storage.hfile, pdc->c_unit_size, buffer, &nb);
	}
	unlock(pdc, hm);
	return r;
}
static _ulong get_buffer_unit(_p_data_t dc, _u32 bn, HMUTEX hlock) {
	_ulong r = INVALID_UNIT_NUMBER;
	HMUTEX hm = lock(dc, hlock);
	_data_context_t *pdc = (_data_context_t *)dc;

	if(bn < pdc->c_bmap_size)
		r = pdc->c_bmap[bn].unit_number;
	unlock(dc, hm);
	return r;
}

static _p_data_t get_buffer_ptr(_p_data_t dc, _u32 bn, HMUTEX hlock) {
	_p_data_t r = NULL;
	HMUTEX hm = lock(dc, hlock);
	_data_context_t *pdc = (_data_context_t *)dc;

	if(bn < pdc->c_bmap_size)
		r = pdc->c_bmap[bn].p_buffer;
	unlock(dc, hm);

	return r;
}

static void set_buffer_dirty(_p_data_t dc, _u32 bn, HMUTEX hlock) {
	HMUTEX hm = lock(dc, hlock);
	_data_context_t *pdc = (_data_context_t *)dc;

	if(bn < pdc->c_bmap_size)
		pdc->c_bmap[bn]._wf = 1;

	unlock(dc, hm);
}

static void flush_map(_p_data_t dc, HMUTEX hlock) {
	HMUTEX hm = lock(dc, hlock);
	_data_context_t *pdc = (_data_context_t *)dc;

	if(pdc) {
		_u32 i = 0;

		for(; i < pdc->c_bmap_size; i++) {
			if(pdc->c_bmap[i]._wf) {
				if(write_unit(pdc, pdc->c_bmap[i].unit_number, pdc->c_bmap[i].p_buffer, hm) == VX_OK)
					pdc->c_bmap[i]._wf = 0; 
			}
		}
	}

	unlock(dc, hm);
}

static void flush_buffer(_p_data_t dc, _u32 bn, HMUTEX hlock) {
	HMUTEX hm = lock(dc, hlock);
	_data_context_t *pdc = (_data_context_t *)dc;

	if(pdc) {
		if(bn < pdc->c_bmap_size) {
			if(pdc->c_bmap[bn]._wf) {
				if(write_unit(pdc, pdc->c_bmap[bn].unit_number, pdc->c_bmap[bn].p_buffer, hm) == VX_OK)
					pdc->c_bmap[bn]._wf = 0;
			}
		}
	}

	unlock(dc, hm);
}

static void init_pool(_p_data_t dc, _u32 max_storages, _u32 unit_size) {
	_data_context_t *pdc = (_data_context_t *)dc;
	if(pdc)
		pdc->c_unit_size = unit_size;
}

static void rollback_map(_p_data_t dc, HMUTEX hlock) {
	HMUTEX hm = lock(dc, hlock);
	_data_context_t *pdc = (_data_context_t *)dc;

	if(pdc) {
		_u32 i = 0;

		for(; i < pdc->c_bmap_size; i++) {
			if(pdc->c_bmap[i]._wf) {
				if(read_unit(pdc, pdc->c_bmap[i].unit_number, pdc->c_bmap[i].p_buffer, hm) == VX_OK)
					pdc->c_bmap[i]._wf = 0; 
			}
		}
	}

	unlock(dc, hm);
}

static _u32 attach_storage(_p_data_t dc, HFILE h, _ulong offset, _ulong size) {
	_u32 r = INVALID_STORAGE_INDEX;
	_data_context_t *pdc = (_data_context_t *)dc;

	if(pdc) {
		if(pdc->c_storage.hfile == NULL) {
			pdc->c_storage.offset = offset;
			pdc->c_storage.size = size;
			pdc->c_storage.hfile = h;
			r = 0;
		}
	}

	return r;
}

static _vx_res_t detach_storage(_p_data_t dc, _u32 index) {
	_vx_res_t r = VX_ERR;
	HMUTEX hm = lock(dc, 0);
	_data_context_t *pdc = (_data_context_t *)dc;

	if(pdc && !index) {
		_g_pi_str_->mem_set(&pdc->c_storage, 0, sizeof(_vx_storage_t));
		r = VX_OK;
	}

	unlock(dc, hm);
	return r;
}

static _u32 alloc_buffer(_p_data_t dc, _ulong unit_number, HMUTEX hlock) {
	_u32 r = INVALID_BUFFER_NUMBER;
	HMUTEX hm = lock(dc, 0);
	_data_context_t *pdc = (_data_context_t *)dc;
	_u32 bn_free = INVALID_BUFFER_NUMBER;

_alloc_buffer_:		
	if(pdc) {
		_u32 i = 0;

		for(; i < pdc->c_bmap_size; i++) {
			if(pdc->c_bmap[i].unit_number == unit_number) {
				r = i;
				pdc->c_bmap[i]._uc++;
				break;
			}
			if(pdc->c_bmap[i]._uc == 0 && 
					pdc->c_bmap[i]._wf == 0 && 
					bn_free == INVALID_BUFFER_NUMBER)
				bn_free = i;

			if(pdc->c_bmap[i].unit_number == INVALID_UNIT_NUMBER)
				break;
		}

		if(r == INVALID_BUFFER_NUMBER) {
			if(bn_free == INVALID_BUFFER_NUMBER) {
				if(realloc_buffer_map(pdc, hm) == VX_OK)
					goto _alloc_buffer_;
			} else {
				if(pdc->c_bmap[bn_free].p_buffer == NULL) {
					if(_g_heap_) {
						_i_heap_t *pi = HC_INTERFACE(_g_heap_);
						_p_data_t pd = HC_DATA(_g_heap_);

						pdc->c_bmap[bn_free].p_buffer = pi->alloc(pd, pdc->c_unit_size, NO_ALLOC_LIMIT);
					}
				}

				if(pdc->c_bmap[bn_free].p_buffer) {
					if(read_unit(pdc, unit_number, pdc->c_bmap[bn_free].p_buffer, hm) == VX_OK) {
						pdc->c_bmap[bn_free]._uc++;
						pdc->c_bmap[bn_free].unit_number = unit_number;
						r = bn_free;
					}
				}
			}
		}
	}

	unlock(dc, hm);
	return r;
}

static void free_buffer(_p_data_t dc, _u32 bn, HMUTEX hlock) {
	HMUTEX hm = lock(dc, hlock);
	_data_context_t *pdc = (_data_context_t *)dc;

	if(pdc) {
		if(bn < pdc->c_bmap_size) {
			if(pdc->c_bmap[bn]._uc)
				pdc->c_bmap[bn]._uc--;
		}
	}

	unlock(dc, hm);
}

static _i_storage_pool_t _g_interface_ = {
	.init		= init_pool,
	.attach		= attach_storage,
	.detach 	= detach_storage,
	.lock		= lock,
	.unlock		= unlock,
	/* buffer operations */
	.buffer_alloc 	= alloc_buffer,
	.buffer_free  	= free_buffer,
	.buffer_ptr   	= get_buffer_ptr,
	.buffer_unit  	= get_buffer_unit,
	.buffer_dirty 	= set_buffer_dirty,
	.buffer_flush 	= flush_buffer,
	.flush		= flush_map,
	.rollback 	= rollback_map
};

_vx_res_t _mod_ctl_(_u32 cmd, ...) {
	_vx_res_t r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				_data_context_t *p_context = va_arg(args, _data_context_t*);

				if(!_g_pi_str_) {
					HCONTEXT hcstr = p_repo->get_context_by_interface(I_STR);
					if(hcstr)
						_g_pi_str_ = HC_INTERFACE(hcstr);
				}

				if(!_g_heap_) /* first time here */
					/* get system heap */
					_g_heap_  = p_repo->get_context_by_interface(I_HEAP);
				if(!_g_dev_root_)
					_g_dev_root_ = p_repo->get_context_by_interface(I_DEV_ROOT);

				if(p_context) {
					p_context->c_mutex = p_repo->create_context_by_interface(I_MUTEX);
					_g_pi_str_->mem_set(&(p_context->c_storage), 0, sizeof(_vx_storage_t));
					p_context->c_bmap_size = 0;
					p_context->c_unit_size = 0;
					p_context->c_bmap = NULL;
					r = realloc_buffer_map(p_context, 0);
				} else
					/* may be it's called for static context */
					r = VX_OK;
			}
			break;
		case MODCTL_DESTROY_CONTEXT: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				_data_context_t *p_context = va_arg(args, _data_context_t*);

				/* free buffer memory */
				if((r = release_buffer_map(p_context)) == VX_OK)
					/* destroy mutex in data context */
					p_repo->release_context(p_context->c_mutex);
			}
			break;
	}

	va_end(args);

	return r;
};

DEF_VXMOD(
	MOD_SSP,
	I_SSP,
	&_g_interface_,
	NULL,	/* no static data context */
	sizeof(_data_context_t), /* dynamic context only */
	_mod_ctl_,
	1,0,1,
	"single storage pool"
);

