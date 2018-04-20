#include "slab.h"

#define RELEASE_BIG_CHUNKS

#define SLAB_MIN_PAGE	4096
#define DEFAULT_SLAB_LIMIT	0xffffffffffffffffLLU

static _u64 _allign(_u32 sz, _u32 page_size) {
	_u64 r = (sz < SLAB_MIN_ALLOC)?SLAB_MIN_ALLOC:sz;
	if(r < page_size) {
		_u64 m = (1L << 63);
		while(!(m & r))
			m >>= 1;
		_u64 n = m >> 1;
		while(n && !(n & r))
			n >>= 1;
		if(n)
			m <<= 1;

		r = m;
	} else {
		r = sz / page_size;
		r += (sz % page_size)?1:0;
		r *= page_size;
	}
	return r;
}

static _slab_t *slab_entry(_slab_context_t *p_scxt, _u32 sz) {
	_slab_t *r = 0;
	_u64 _sz = _allign(sz, p_scxt->page_size);
	_u64 b = SLAB_MIN_ALLOC;
	_u32 pc = p_scxt->page_size / sizeof(_slab_t);
	_u32 p = 0;

	for(; p < pc; p++) {
		if(_sz >= b && _sz < (b<<1)) {
			if((r = p_scxt->p_slab + p)) {
				if(r->osz == 0) {
					p_scxt->p_mem_set((_u8 *)r, 0, sizeof(_slab_t));
					r->osz = _sz;
					r->count = 0;
				}
			}
			break;
		}
		b <<= 1;
	}
	return r;
}

#define SLAB_PATTERN	(_u64)0xcacacacacacacaca
#define SLAB_PATTERN2	(_u64)0xdadadadadadadada

_u8 slab_init(_slab_context_t *p_scxt) {
	_u8 r = 0;

	if(p_scxt->p_slab)
		r = 1;
	else {
		if(p_scxt->p_mem_alloc && p_scxt->page_size >= SLAB_MIN_PAGE) {
			if((p_scxt->p_slab = (_slab_t *)p_scxt->p_mem_alloc(1, DEFAULT_SLAB_LIMIT, p_scxt->p_udata))) {
				p_scxt->p_mem_set((_u8 *)(p_scxt->p_slab), 0, p_scxt->page_size);
				r = 1;
			}
		}
	}
	return r;
}

static _u8 slab_inc_rec_level(_slab_context_t *p_scxt, _slab_t *p_slab) {
	_u8 r = 0;
	/* increase of recursion level */
	_u8 *pnew = (_u8 *)p_scxt->p_mem_alloc(1, DEFAULT_SLAB_LIMIT, p_scxt->p_udata);
	if(pnew) {
		p_scxt->p_mem_set(pnew, 0, p_scxt->page_size);
		p_scxt->p_mem_cpy(pnew, (_u8 *)p_slab, sizeof(_slab_t));
		_u8 i = 0;
		for(; i < SLAB_NODES; i++)
			p_slab->ptr[i] = 0;
		p_slab->ptr[0] = (_u64)pnew;
		p_slab->level++;
		p_slab->count = 0;
		r = 1;
	}
	return r;
}

static void *_slab_alloc(_slab_context_t *p_scxt, _slab_t *p_slab, _u32 size, _ulong limit) {
	void *r = 0;
	_u8 i = 0;

	for(; i < SLAB_NODES; i++) {
_enum_slab_nodes_:
		if(p_slab->ptr[i]) {
			if(p_slab->level) {
				_u32 count = p_scxt->page_size / sizeof(_slab_t);
				_slab_t *ps = (_slab_t *)p_slab->ptr[i];
				_u32 j = 0;
				for(; j < count; j++) {
					if(ps->osz == 0)
						ps->osz = size;
					if(ps->osz == size) {
						if((r = _slab_alloc(p_scxt, ps, size, limit))) {
							i = SLAB_NODES;
							break;
						}
					}
					ps++;
				}
			} else if((p_slab->ptr[i]+size) < limit) {
				_u32 npages = 1;
				if(p_slab->osz > p_scxt->page_size)
					npages = p_slab->osz / p_scxt->page_size;
				_u32 mem_sz = npages * p_scxt->page_size;
				_u32 nobjs = mem_sz / p_slab->osz;
				_u8 *_ptr = (_u8 *)p_slab->ptr[i];
				_u64 *_rptr = 0;
				_u32 j = 0;
				for(; j < nobjs; j++) {
					_u64 *p64 = (_u64 *)(_ptr + (j * p_slab->osz));
					if(*p64 == SLAB_PATTERN2 && !_rptr)
						_rptr = p64;
					if(*p64 == SLAB_PATTERN) {
						r = p64;
						i = SLAB_NODES;
						*p64 = 0;
						p_slab->count++;
						break;
					}
				}

				if(!r && _rptr) {
					r = _rptr;
					*_rptr = 0;
					p_slab->count++;
					break;
				}
			}
		} else {
			_u32 npages = 1;
			if(p_slab->level == 0 && p_slab->osz > p_scxt->page_size)
				npages = p_slab->osz / p_scxt->page_size;
			_u8 *p = (_u8 *)p_scxt->p_mem_alloc(npages, 
					(p_slab->level)?DEFAULT_SLAB_LIMIT:limit, p_scxt->p_udata);
			if(p) {
				_u32 mem_sz = npages * p_scxt->page_size;
				p_scxt->p_mem_set(p, 0, mem_sz);
				if(p_slab->level == 0) {
					_u32 nobjs = mem_sz / p_slab->osz;
					_u32 j = 0;
					for(; j < nobjs; j++) {
						_u64 *p_obj = (_u64 *)(p + (j * p_slab->osz));
						*p_obj = SLAB_PATTERN;
					}
				}
				p_slab->ptr[i] = (_u64)p;
				goto _enum_slab_nodes_;
			} else
				i = SLAB_NODES;
		}
	}
	return r;
}

void *slab_alloc(_slab_context_t *p_scxt, _u32 size, _ulong limit) {
	void *r = 0;
	_slab_t *p_slab = slab_entry(p_scxt, size);
	if(p_slab) {
		_u64 sz = _allign(size, p_scxt->page_size);
		_slab_hlock_t hlock = p_scxt->p_lock(0, p_scxt->p_udata);
		if(p_slab->level == 0 && sz > p_scxt->page_size && sz != p_slab->osz)
			slab_inc_rec_level(p_scxt, p_slab);
		if(!(r = _slab_alloc(p_scxt, p_slab, sz, limit))) {
			if(slab_inc_rec_level(p_scxt, p_slab))
				r = _slab_alloc(p_scxt, p_slab, sz, limit);
		}
		p_scxt->p_unlock(hlock, p_scxt->p_udata);
	}

	return r;
}

static _bool _slab_verify(_slab_t *p_slab, void *ptr, _u32 page_size, _slab_t **pp_slab, _u64 **p_obj, _u32 *p_idx) {
	_bool r = _false;
	_u8 i = 0;

	for(; i < SLAB_NODES; i++) {
		if(p_slab->ptr[i]) {
			if(p_slab->level) {
				_u32 count = page_size / sizeof(_slab_t);
				_slab_t *ps = (_slab_t *)p_slab->ptr[i];
				_u32 j = 0;
				for(; j < count; j++) {
					if(ps->count && ps->osz) {
						if((r = _slab_verify(ps, ptr, page_size, pp_slab, p_obj, p_idx))) {
							i = SLAB_NODES;
							break;
						}
					}
					ps++;
				}
			} else {
				_u32 npages = 1;
				if(p_slab->osz > page_size)
					npages = p_slab->osz / page_size;
				_u32 mem_sz = npages * page_size;
				_u32 nobjs = mem_sz / p_slab->osz;
				_u8 *_ptr = (_u8 *)p_slab->ptr[i];
				_u32 j = 0;
				for(; j < nobjs; j++) {
					_u64 *p64 = (_u64 *)(_ptr + (j * p_slab->osz));
					if(p64 == (_u64 *)ptr) { 
						if(*p64 != SLAB_PATTERN && *p64 != SLAB_PATTERN2) {
							*p_obj = p64;
							*pp_slab = p_slab;
							*p_idx = i;
							r = _true;
						}
						i = SLAB_NODES;
						break;
					}
				}
			}
		}
	}
	return r;
}

void slab_free(_slab_context_t *p_scxt, void *ptr, _u32 size) {
	_slab_t *p_slab = slab_entry(p_scxt, size);

	if(p_slab) {
		_slab_hlock_t hlock = p_scxt->p_lock(0, p_scxt->p_udata);
		_u64 *p_obj = 0;
		_slab_t *_ps = 0;
		_u32 idx = 0;
		if(_slab_verify(p_slab, ptr, p_scxt->page_size, &_ps, &p_obj, &idx)) {
			_u64 *p = (_u64 *)ptr;
			if(*p == *p_obj) {
				*p_obj = SLAB_PATTERN2;
#ifdef RELEASE_BIG_CHUNKS
				if(_ps->osz >= p_scxt->page_size) {
					_u32 npages = _ps->osz / p_scxt->page_size;
					npages += (_ps->osz % p_scxt->page_size)?1:0;
					void *_ptr = (void *)_ps->ptr[idx];
					if(ptr == _ptr) {
						p_scxt->p_mem_free(_ptr, npages, p_scxt->p_udata);
						_ps->ptr[idx] = 0;
					}
				}
#endif
				if(_ps->count)
					_ps->count--;
			}
		}
		p_scxt->p_unlock(hlock, p_scxt->p_udata);
	}
}

_bool slab_verify(_slab_context_t *p_scxt, void *ptr, _u32 size) {
	_u8 r = _false;
	_slab_t *p_slab = slab_entry(p_scxt, size);

	if(p_slab) {
		_slab_hlock_t hlock = p_scxt->p_lock(0, p_scxt->p_udata);
		_u64 *p_obj = 0;
		_slab_t *_ps=0;
		_u32 idx = 0;
		r = _slab_verify(p_slab, ptr, p_scxt->page_size, &_ps, &p_obj, &idx);
		p_scxt->p_unlock(hlock, p_scxt->p_udata);
	}

	return r;
}

static void _slab_status(_slab_t *p_slab, _slab_status_t *p_sst, _u32 page_size) {
	_u8 i = 0;
	for(; i < SLAB_NODES; i++) {
		if(p_slab->ptr[i]) {
			if(p_slab->level) {
				_u32 count = page_size / sizeof(_slab_t);
				_slab_t *ps = (_slab_t *)p_slab->ptr[i];
				_u32 j = 0;

				p_sst->nspg++;

				for(; j < count; j++) {
					if(ps->osz)
						_slab_status(ps, p_sst, page_size);
					ps++;
				}
			} else {
				_u32 npages = 1;
				if(p_slab->osz > page_size)
					npages = p_slab->osz / page_size;
				p_sst->ndpg += npages;
				_u32 mem_sz = npages * page_size;
				_u32 nobjs = mem_sz / p_slab->osz;
				p_sst->nrobj += nobjs;
				_u8 *ptr = (_u8 *)p_slab->ptr[i];
				_u32 j = 0;
				for(; j < nobjs; j++) {
					_u64 *p64 = (_u64 *)(ptr + (j * p_slab->osz));
					if(*p64 != SLAB_PATTERN && *p64 != SLAB_PATTERN2)
						p_sst->naobj++;
				}
			}
		} else
			break;
	}
}

void slab_status(_slab_context_t *p_scxt, _slab_status_t *p_sstatus) {
	p_sstatus->naobj = p_sstatus->nrobj = p_sstatus->ndpg = p_sstatus->nspg = 0;

	if(p_scxt->p_slab) {
		_slab_hlock_t hlock = p_scxt->p_lock(0, p_scxt->p_udata);
		p_sstatus->nspg = 1;
		_u32 count = p_scxt->page_size / sizeof(_slab_t);
		_slab_t *ps = p_scxt->p_slab;
		_u32 i = 0;
		for(; i < count; i++) {
			_slab_status(ps, p_sstatus, p_scxt->page_size);
			ps++;
		}
		p_scxt->p_unlock(hlock, p_scxt->p_udata);
	}
}

static void _destroy_slab(_slab_context_t *p_scxt, _slab_t *p_slab) {
	_u8 i = 0;
	for(; i < SLAB_NODES; i++) {
		if(p_slab->ptr[i]) {
			if(p_slab->level) {
				_u32 count = p_scxt->page_size / sizeof(_slab_t);
				_slab_t *ps = (_slab_t *)p_slab->ptr[i];
				_u32 j = 0;
				for(; j < count; j++) {
					_destroy_slab(p_scxt, ps);
					ps++;
				}
				p_scxt->p_mem_free((void *)p_slab->ptr[i], 1, p_scxt->p_udata);
			} else {
				_u32 npages = 1;
				if(p_slab->osz > p_scxt->page_size)
					npages = p_slab->osz / p_scxt->page_size;

				p_scxt->p_mem_free((void *)p_slab->ptr[i], npages, p_scxt->p_udata);
			}

			p_slab->ptr[i] = 0;
		}
	}
	p_scxt->p_mem_set((_u8 *)p_slab, 0, sizeof(_slab_t));
}

void slab_destroy(_slab_context_t *p_scxt) {
	if(p_scxt->p_slab) {
		_slab_hlock_t hlock = p_scxt->p_lock(0, p_scxt->p_udata);
		_u32 count = p_scxt->page_size / sizeof(_slab_t);
		_slab_t *ps = p_scxt->p_slab;
		_u32 i = 0;
		for(; i < count; i++) {
			_destroy_slab(p_scxt, ps);
			ps++;
		}
		p_scxt->p_mem_free((void *)p_scxt->p_slab, 1, p_scxt->p_udata);
		p_scxt->p_unlock(hlock, p_scxt->p_udata);
	}
}

