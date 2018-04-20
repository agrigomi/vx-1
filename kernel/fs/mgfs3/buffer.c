#include "buffer.h"
#include "vxmod.h"
#include "i_memory.h"
#include "i_repository.h"

static _vx_res_t unit_read(_u64 unit, _p_data_t buffer, _p_data_t udata) {
	_mgfs_context_t *p_cxt = (_mgfs_context_t *)udata;
	return p_cxt->read(unit * p_cxt->fs.sz_unit, p_cxt->fs.sz_unit, buffer, p_cxt->udata);
}

static _vx_res_t unit_write(_u64 unit, _p_data_t buffer, _p_data_t udata) {
	_mgfs_context_t *p_cxt = (_mgfs_context_t *)udata;
	return p_cxt->write(unit * p_cxt->fs.sz_unit, p_cxt->fs.sz_unit, buffer, p_cxt->udata);
}

static _i_buffer_map_t *get_buffer_map(_mgfs_context_t *p_cxt, _p_data_t *data) {
	_i_buffer_map_t *r = NULL;
	HCONTEXT hc_bmap = p_cxt->bmap;
	if(!hc_bmap) {
		if((hc_bmap = p_cxt->bmap = __g_p_i_repository__->create_context_by_interface(I_BUFFER_MAP))) {
			r = HC_INTERFACE(hc_bmap);
			*data = HC_DATA(hc_bmap);
			if(r && *data) {
				_u32 unit_size = p_cxt->fs.sz_sector * p_cxt->fs.sz_unit;
				r->init(*data, unit_size, unit_read, unit_write, p_cxt);
			}
		}
	} else {
		r = HC_INTERFACE(hc_bmap);
		*data = HC_DATA(hc_bmap);
	}
	return r;
}

_h_buffer_ mgfs_buffer_alloc(_mgfs_context_t *p_cxt, _u64 unit_number, _h_lock_ hlock) {
	_h_buffer_ r = (_h_buffer_)p_cxt->fs.inv_pattern;
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd)
		r = pi->alloc(pd, unit_number, hlock);
	return r;
}

void *mgfs_buffer_ptr(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock) {
	void *r = NULL;
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd)
		r = pi->ptr(pd, hb, hlock);
	return r;
}

_u64 mgfs_buffer_unit(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock) {
	_u64 r = (_h_buffer_)p_cxt->fs.inv_pattern;
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd)
		r = pi->key(pd, hb, hlock);
	return r;
}

void mgfs_buffer_free(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock) {
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd)
		pi->free(pd, hb, hlock);
}

void mgfs_buffer_dirty(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock) {
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd)
		pi->dirty(pd, hb, hlock);
}

void mgfs_buffer_flush(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock) {
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd)
		pi->flush(pd, hb, hlock);
}

void mgfs_buffer_flush_all(_mgfs_context_t *p_cxt, _h_lock_ hlock) {
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd)
		pi->flush_all(pd, hlock);
}

void mgfs_buffer_reset(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock) {
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd)
		pi->reset(pd, hb, hlock);
}

void mgfs_buffer_reset_all(_mgfs_context_t *p_cxt, _h_lock_ hlock) {
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd)
		pi->reset_all(pd, hlock);
}

void mgfs_buffer_cleanup(_mgfs_context_t *p_cxt, _h_lock_ hlock) {
	HCONTEXT hc_bmap = p_cxt->bmap;
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd) {
		_h_lock_ hm = pi->lock(pd, hlock);
		__g_p_i_repository__->release_context(hc_bmap);
		p_cxt->bmap = NULL;
		pi->unlock(pd, hm);
	}
}

_h_lock_ mgfs_buffer_lock(_mgfs_context_t *p_cxt, _h_lock_ hlock) {
	_h_lock_ r = 0;
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd)
		r = pi->lock(pd, hlock);
	return r;
}

void mgfs_buffer_unlock(_mgfs_context_t *p_cxt, _h_lock_ hlock) {
	_p_data_t pd;
	_i_buffer_map_t *pi = get_buffer_map(p_cxt, &pd);
	if(pi && pd)
		pi->unlock(pd, hlock);
}

