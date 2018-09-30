#include "i_repository.h"
#include "i_memory.h"
#include "i_system_log.h"
#include "vxmod.h"
#include "str.h"
#include "err.h"
#include "kernel_config.h"

/* module state */
#define MSTATE_READY_STATIC	(1<<0)
#define MSTATE_DISABLED		(1<<1)

typedef struct {
	_vx_mod_t	*p_mod; /* array of module descriptors */
	_u32 		count; /* number of descriptors */
}_mod_array_t;

static _mod_array_t 	g_main_array = {NULL, 0};
static HCONTEXT		g_hc_heap = NULL;
static _i_llist_t 	*g_pi_llist = NULL;
static _p_data_t	g_pd_array_list = NULL;
static _p_data_t	g_pd_context_list = NULL;

/* system log */
static HCONTEXT __g_hc_sys_log__ = NULL;
static _i_system_log_t *__g_pi_sys_log__ = NULL;
static _p_data_t __g_pd_sys_log__ = NULL;


static _vx_mod_t *get_vx_mod_by_name(_cstr_t name);
static _vx_mod_t *get_vx_mod_by_interface(_cstr_t name);
static void _init_mod_array(_ulong harray);
static HMARRAY _add_mod_array(_vx_mod_t *p_array, _u32 count);
static HCONTEXT _get_context_by_name(_cstr_t mod_name);
static HCONTEXT _get_context_by_interface(_cstr_t i_name);
static HCONTEXT _create_context_by_name(_cstr_t mod_name);
static HCONTEXT _create_context_by_interface(_cstr_t i_name);
static HCONTEXT _create_limited_context_by_name(_cstr_t mod_name, _ulong addr_limit);
static HCONTEXT _create_limited_context_by_interface(_cstr_t i_name, _ulong addr_limit);
static void init_static_context(_vx_mod_t *p_mod);
static void _release_context(HCONTEXT hc);
static _vx_mod_t *_mod_info_by_name(_cstr_t mod_name);
static _vx_mod_t *_mod_info_by_index(_u32 mod_index);
static _u32 _mod_count(void);
static void _remove_mod_array(HMARRAY hmod_array);

static _i_repository_t _g_i_repository = {
	.add_mod_array		= _add_mod_array,
	.init_mod_array		= _init_mod_array,
	.get_context_by_name	= _get_context_by_name,
	.get_context_by_interface	= _get_context_by_interface,
	.create_context_by_name	= _create_context_by_name,
	.create_limited_context_by_name	= _create_limited_context_by_name,
	.create_context_by_interface	= _create_context_by_interface,
	.create_limited_context_by_interface	= _create_limited_context_by_interface,
	.release_context	= _release_context,
	.mod_info_by_name 	= _mod_info_by_name,
	.mod_info_by_index	= _mod_info_by_index,
	.mod_count		= _mod_count,
	.remove_mod_array 	= _remove_mod_array
};

static _vx_mod_t *find_vx_mod_by_name(_mod_array_t *p_array, _cstr_t mod_name) {
	_vx_mod_t *r = NULL;
	_u32 i = 0;
	_vx_mod_t *p_mod = p_array->p_mod;

	for(; i < p_array->count; i++) {
		if(_str_cmp((_str_t)((p_mod + i)->_m_name_), (_str_t)mod_name) == 0) {
			r = (p_mod + i);
			break;
		}
	}

	return r;
}
static _vx_mod_t *find_vx_mod_by_interface(_mod_array_t *p_array, _cstr_t i_name) {
	_vx_mod_t *r = NULL;
	_u32 i = 0;
	_vx_mod_t *p_mod = p_array->p_mod;

	for(; i < p_array->count; i++) {
		if(_str_cmp((_str_t)((p_mod + i)->_m_iname_), (_str_t)i_name) == 0) {
			r = (p_mod + i);
			break;
		}
	}

	return r;
}

static _i_heap_t *get_heap(_p_data_t *p_dheap) {
	_i_heap_t *r = NULL;

	if(g_hc_heap) {
		r = (_i_heap_t *)HC_INTERFACE(g_hc_heap);
		*p_dheap = HC_DATA(g_hc_heap);
	} else {
		_vx_mod_t *p_mod_heap = find_vx_mod_by_interface(&g_main_array, I_HEAP);
		if(p_mod_heap) {
			init_static_context(p_mod_heap);
			if(p_mod_heap->_m_state_ & MSTATE_READY_STATIC) {
				g_hc_heap = &(p_mod_heap->_m_context_);
				r = (_i_heap_t *)HC_INTERFACE(g_hc_heap);
				*p_dheap = HC_DATA(g_hc_heap);
			}
		}
	}
	return r;
}

static _u8 __create_context(_vx_mod_t *p_mod, _ulong addr_limit, _vx_context_t *pc) {
	_u8 r = 0;

	init_static_context(p_mod);
	if((p_mod->_m_state_ & MSTATE_READY_STATIC) && !(p_mod->_m_state_ & MSTATE_DISABLED)) {
		if(p_mod->_m_dsize_) {
			/* can create new context */
			_p_data_t pd_heap = NULL;
			_i_heap_t *pi_heap = get_heap(&pd_heap);
			if(pi_heap && pd_heap && p_mod->_m_ctl_) {
				if((pc->_c_data_ = pi_heap->alloc(pd_heap, p_mod->_m_dsize_, addr_limit))) {
					if(p_mod->_m_ctl_(MODCTL_INIT_CONTEXT, &_g_i_repository, pc->_c_data_) == VX_OK) {
						pc->_c_mod_ = p_mod;
						p_mod->_m_refc_++;
						r = 1;
					} else {
						/* release context data */
						pi_heap->free(pd_heap, pc->_c_data_, p_mod->_m_dsize_);
						pc->_c_data_ = NULL;
						LOG(LMT_ERROR, "ERROR in dynamic context '%s'", p_mod->_m_name_);
					}
				}
			}
		} else {
			/* can't create new context, because
			    data size is 0. Return static context.
			 */
			pc->_c_data_= p_mod->_m_context_._c_data_;
			pc->_c_mod_ = p_mod;
			p_mod->_m_refc_++;
			r = 1;
		}
	}
	return r;
}

static _i_llist_t *create_list(_p_data_t *pd_list) {
	_i_llist_t *r = NULL;
	_vx_mod_t *p_mod_llist = find_vx_mod_by_interface(&g_main_array, I_LLIST);

	if(p_mod_llist) {
		_vx_context_t cl;
		if(__create_context(p_mod_llist, NO_ALLOC_LIMIT, &cl)) {
			r = (_i_llist_t *)cl._c_mod_->_m_interface_;
			*pd_list = cl._c_data_;
			r->init(*pd_list, LLIST_VECTOR, 1, NO_ALLOC_LIMIT);
		}
	}

	return r;
}

static _i_llist_t *get_array_list(_p_data_t *p_dlist) {
	_i_llist_t *r = NULL;

	if(g_pi_llist && g_pd_array_list) {
		r = g_pi_llist;
		*p_dlist = g_pd_array_list;
	} else {
		/* create list context for .vxmod array */
		if((r = g_pi_llist = create_list(&g_pd_array_list)))
			*p_dlist = g_pd_array_list;
	}

	return r;
}

static _i_llist_t *get_context_list(_p_data_t *p_dlist) {
	_i_llist_t *r = NULL;

	if(g_pi_llist && g_pd_context_list) {
		r = g_pi_llist;
		*p_dlist = g_pd_context_list;
	} else {
		/* create list context for context array */
		if((r = g_pi_llist = create_list(&g_pd_context_list)))
			*p_dlist = g_pd_context_list;
	}

	return r;
}

static _vx_mod_t *get_vx_mod_by_name(_cstr_t name) {
	_vx_mod_t *r = find_vx_mod_by_name(&g_main_array, name);

	if(!r) {
		/* mot in main array, continue search in extensions */
		_p_data_t p_ldata = NULL;
		_i_llist_t *pi_llist = get_array_list(&p_ldata);
		if(pi_llist && p_ldata) {
			_u32 sz = 0;
			HMUTEX hlock = pi_llist->lock(p_ldata, 0);
			_mod_array_t *p_ma = (_mod_array_t *)pi_llist->first(p_ldata, &sz, hlock);
			if(p_ma) {
				do {
					if((r = find_vx_mod_by_name(p_ma, name)))
						break;
				} while((p_ma = (_mod_array_t *)pi_llist->next(p_ldata, &sz, hlock)));
			}
			pi_llist->unlock(p_ldata, hlock);
		}
	}

	return r;
}

static _vx_mod_t *get_vx_mod_by_interface(_cstr_t name) {
	_vx_mod_t *r = find_vx_mod_by_interface(&g_main_array, name);

	if(!r) {
		/* mot in main array, continue search in extensions */
		_p_data_t p_ldata = NULL;
		_i_llist_t *pi_llist = get_array_list(&p_ldata);
		if(pi_llist && p_ldata) {
			_u32 sz = 0;
			HMUTEX hlock = pi_llist->lock(p_ldata, 0);
			_mod_array_t *p_ma = (_mod_array_t *)pi_llist->first(p_ldata, &sz, hlock);
			if(p_ma) {
				do {
					if((r = find_vx_mod_by_interface(p_ma, name)))
						break;
				} while((p_ma = (_mod_array_t *)pi_llist->next(p_ldata, &sz, hlock)));
			}
			pi_llist->unlock(p_ldata, hlock);
		}
	}

	return r;
}
static _u32 repo_ctl(_u32 cmd, ...) {
	_u32 r = VX_ERR;

	switch(cmd) {
		case MODCTL_INIT_CONTEXT:
			r = VX_OK;
			break;
	}

	return r;
}

static void init_static_context(_vx_mod_t *p_mod) {
	if(p_mod->_m_ctl_ && !(p_mod->_m_state_ & (MSTATE_READY_STATIC|MSTATE_DISABLED))) {
		p_mod->_m_state_ |= MSTATE_READY_STATIC;
		if(p_mod->_m_ctl_(MODCTL_INIT_CONTEXT, &_g_i_repository, p_mod->_m_context_._c_data_) != VX_OK) {
			p_mod->_m_state_ &= ~MSTATE_READY_STATIC;
			LOG(LMT_ERROR, "ERROR in static context '%s'", p_mod->_m_name_);
		}
	}
}

static HCONTEXT _get_context_by_name(_cstr_t mod_name) {
	HCONTEXT r = NULL;
	_vx_mod_t *p_mod = get_vx_mod_by_name(mod_name);

	if(p_mod) {
		init_static_context(p_mod);
		if((p_mod->_m_state_ & MSTATE_READY_STATIC) && !(p_mod->_m_state_ & MSTATE_DISABLED)) {
			r = &(p_mod->_m_context_);
			p_mod->_m_refc_++;
		}
	}

	return r;
}

static HCONTEXT _get_context_by_interface(_cstr_t i_name) {
	HCONTEXT r = NULL;
	_vx_mod_t *p_mod = get_vx_mod_by_interface(i_name);

	if(p_mod) {
		init_static_context(p_mod);
		if((p_mod->_m_state_ & MSTATE_READY_STATIC) && !(p_mod->_m_state_ & MSTATE_DISABLED)) {
			r = &(p_mod->_m_context_);
			p_mod->_m_refc_++;
		}
	}

	return r;
}

static HCONTEXT _create_context(_vx_mod_t *p_mod, _ulong addr_limit) {
	HCONTEXT r = NULL;
	if(p_mod) {
		_vx_context_t cxt;
		if(__create_context(p_mod, addr_limit, &cxt)) {
			if(cxt._c_data_ != p_mod->_m_context_._c_data_) {
				/* dynamic data context,
				 	store to context list
				*/
				_p_data_t pd_list = NULL;
				_i_llist_t *pi_list = get_context_list(&pd_list);
				if(pi_list && pd_list)
					r = pi_list->add(pd_list, &cxt, sizeof(_vx_context_t), 0);
			} else
				/* static data context */
				r = &(p_mod->_m_context_);
		}
	}
	return r;
}

static HCONTEXT _create_limited_context_by_name(_cstr_t mod_name, _ulong addr_limit) {
	_vx_mod_t *p_mod = get_vx_mod_by_name(mod_name);
	return _create_context(p_mod, addr_limit);
}
static HCONTEXT _create_limited_context_by_interface(_cstr_t i_name, _ulong addr_limit) {
	_vx_mod_t *p_mod = get_vx_mod_by_interface(i_name);
	return _create_context(p_mod, addr_limit);
}
static HCONTEXT _create_context_by_name(_cstr_t mod_name) {
	return _create_limited_context_by_name(mod_name, NO_ALLOC_LIMIT);
}
static HCONTEXT _create_context_by_interface(_cstr_t i_name) {
	return _create_limited_context_by_interface(i_name, NO_ALLOC_LIMIT);
}

static HMARRAY _add_mod_array(_vx_mod_t *p_array, _u32 count) {
	HMARRAY r = 0;

	if(g_main_array.p_mod == NULL && g_main_array.count == 0) {
		/* first (main) array */
		g_main_array.p_mod = p_array;
		g_main_array.count = count;
		r = (HMARRAY)&g_main_array;
	} else {
		_mod_array_t tma;
		_p_data_t p_ldata = NULL;
		_i_llist_t *pi_list = get_array_list(&p_ldata);

		if(pi_list && p_ldata) {
			/* add to list */
			tma.p_mod = p_array;
			tma.count = count;
			r = (HMARRAY)pi_list->add(p_ldata, &tma, sizeof(_mod_array_t), 0);
		}
	}

	return r;
}

static void _init_mod_array(_ulong harray) {
	_mod_array_t *p_array = (_mod_array_t *)harray;
	_vx_mod_t *p_mod = p_array->p_mod;
	_u32 i = 0;

	if(p_array == &g_main_array) {
		/* init system heap */
		_vx_mod_t *p_mod_heap = find_vx_mod_by_interface(&g_main_array, I_HEAP);
		if(p_mod_heap)
			init_static_context(p_mod_heap);

		/* init system log */
		if((__g_hc_sys_log__ = _get_context_by_interface(I_SYSTEM_LOG))) {
			__g_pi_sys_log__ = HC_INTERFACE(__g_hc_sys_log__);
			__g_pd_sys_log__ = HC_DATA(__g_hc_sys_log__);
			if(__g_pi_sys_log__ && __g_pd_sys_log__)
				__g_pi_sys_log__->init(__g_pd_sys_log__, SYSLOG_CAPACITY);
		}
	}

	for(; i < p_array->count; i++)
		/* init static data context */
		init_static_context((p_mod + i));
}

static void _release_context(HCONTEXT hc) {
	_vx_context_t *p_cxt = (_vx_context_t *)hc;
	if(p_cxt) {
		_vx_mod_t *p_mod = p_cxt->_c_mod_;
		if(p_mod) {
			if(&(p_mod->_m_context_) != p_cxt) {
				/* destroy dynamic data context */
				if(p_mod->_m_ctl_(MODCTL_DESTROY_CONTEXT, _g_i_repository, p_cxt->_c_data_) == VX_OK) {
					_p_data_t pd_list = NULL;
					_i_llist_t *pi_list = get_context_list(&pd_list);
					if(pi_list && pd_list) {
						HMUTEX hm = pi_list->lock(pd_list, 0);
						if(pi_list->sel(pd_list, p_cxt, hm)) {
							/* now context is current in list */
							_p_data_t pd_heap = NULL;
							_i_heap_t *pi_heap = get_heap(&pd_heap);
							if(pi_heap && pd_heap) {
								/* release memory for data context */
								pi_heap->free(pd_heap, p_cxt->_c_data_, p_cxt->_c_mod_->_m_dsize_);
								p_cxt->_c_data_ = NULL;
							}
							/* delete current context entry */
							pi_list->del(pd_list, hm);
						}
						pi_list->unlock(pd_list, hm);
					}
				}
			}

			if(p_mod->_m_refc_)
				p_mod->_m_refc_--;
		}
	}
}

static _vx_mod_t *_mod_info_by_name(_cstr_t mod_name) {
	return get_vx_mod_by_name(mod_name);
}

static _vx_mod_t *_mod_info_by_index(_u32 mod_index) {
	_vx_mod_t *r = NULL;
	_p_data_t pd_list = NULL;
	_i_llist_t *pi_list = get_array_list(&pd_list);

	if(pi_list && pd_list) {
		_u32 sz = 0;
		HMUTEX hm = pi_list->lock(pd_list, 0);
		_u32 cc = 0;
		_mod_array_t *p_ma = (_mod_array_t *)pi_list->first(pd_list, &sz, hm);
		if(p_ma) {
			do {
				if(mod_index < (cc + p_ma->count)) {
					r = &(p_ma->p_mod[mod_index - cc]);
					break;
				}
				cc += p_ma->count;
			}while((p_ma = (_mod_array_t *)pi_list->next(pd_list, &sz, hm)));
		}
		pi_list->unlock(pd_list, hm);
	}

	return r;
}

static _u32 _mod_count(void) {
	_u32 r = 0;
	_p_data_t pd_list = NULL;
	_i_llist_t *pi_list = get_array_list(&pd_list);

	if(pi_list && pd_list) {
		_u32 sz = 0;
		HMUTEX hm = pi_list->lock(pd_list, 0);
		_mod_array_t *p_ma = (_mod_array_t *)pi_list->first(pd_list, &sz, hm);
		if(p_ma) {
			do {
				r += p_ma->count;
			}while((p_ma = (_mod_array_t *)pi_list->next(pd_list, &sz, hm)));
		}
		pi_list->unlock(pd_list, hm);
	}

	return r;
}

static void _remove_mod_array(HMARRAY hmod_array) {
	_mod_array_t *p_mod_array = (_mod_array_t *)hmod_array;

	if(p_mod_array != &g_main_array) { /* because can't remove main array */
		_p_data_t pd_list = NULL;
		_i_llist_t *pi_list = get_array_list(&pd_list);

		if(pi_list && pd_list) {
			HMUTEX hm = pi_list->lock(pd_list, 0);
			/*#warning do not forget to implement here !*/
			/* ... */
			pi_list->unlock(pd_list, hm);
		}
	}
}

DEF_VXMOD(
	MOD_REPOSITORY,
	I_REPOSITORY,
	&_g_i_repository,
	NULL,
	0,
	repo_ctl,
	1,0,1,
	"module repository"
);


