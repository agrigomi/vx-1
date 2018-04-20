#include "vxmod.h"
#include "i_str.h"
#include "i_repository.h"
#include "i_memory.h"
#include "err.h"

typedef struct {
	_u8 	sz_vname;	/* size of variable name */
	_u16	sz_value;
	_u16	sz_vdata;
	_str_t	data;
} __attribute__((packed)) _env_entry_t;

typedef struct {
	HCONTEXT	hc_llist;
} _env_context_t;

static HCONTEXT	ghc_heap = NULL;
static _i_str_t *gpi_str = NULL;

static void *heap_alloc(_u32 size) {
	void *r = NULL;
	if(ghc_heap) {
		_i_heap_t *pi = HC_INTERFACE(ghc_heap);
		_p_data_t *pd = HC_DATA(ghc_heap);
		if(pi && pd)
			r = pi->alloc(pd, size, NO_ALLOC_LIMIT);
	}
	return r;
}

static void heap_free(void *ptr, _u32 size) {
	if(ghc_heap) {
		_i_heap_t *pi = HC_INTERFACE(ghc_heap);
		_p_data_t *pd = HC_DATA(ghc_heap);
		if(pi && pd)
			pi->free(pd, ptr, size);
	}
}

static _str_t alloc_var_buffer(_str_t vname, /* [in] */
			_str_t val,  /* [in] */
			_u16 sz_val, /* [in] */
			_u16 *sz_buffer /* [out] */
			) {
	_str_t r = 0;

	if(gpi_str && ghc_heap) {
		*sz_buffer = gpi_str->str_len(vname) + sz_val + 2;
		if((r = (_str_t)heap_alloc(*sz_buffer))) {
			gpi_str->mem_set((_u8 *)r, 0, *sz_buffer);
			_u32 bidx = gpi_str->str_cpy(r, vname, gpi_str->str_len(vname)+1);
			*(r + bidx) = '=';
			bidx++;
			gpi_str->mem_cpy((_u8 *)r+bidx, (_u8 *)val, sz_val);
		}
	}
	return r;
}

static _env_entry_t *get_entry(_env_context_t *p_cxt, _cstr_t vname) {
	_env_entry_t *r = 0;
	if(p_cxt) {
		_i_llist_t *pli = HC_INTERFACE(p_cxt->hc_llist);
		_p_data_t pld = HC_DATA(p_cxt->hc_llist);
		if(pli && pld) {
			HMUTEX hlock = pli->lock(pld, 0);
			_u32 sz = 0;
			_env_entry_t * _r = (_env_entry_t *)pli->first(pld, &sz, hlock);
			if(_r) {
				do {
					if(gpi_str->str_len((_str_t)vname) == _r->sz_vname) {
						if(gpi_str->mem_cmp((_u8 *)vname, (_u8 *)_r->data, _r->sz_vname) == 0) {
							r = _r;
							break;
						}
					}
				} while((_r = (_env_entry_t *)pli->next(pld, &sz, hlock)));
			}
			pli->unlock(pld, hlock);
		}
	}
	return r;
}

static _str_t env_get(_p_data_t dc, _cstr_t var) {
	_str_t r = NULL;
	_env_context_t *p_cxt = (_env_context_t *)dc;
	if(p_cxt) {
		_env_entry_t *pe = get_entry(p_cxt, var);
		if(pe)
			r = pe->data + pe->sz_vname + 1;
	}
	return r;
}

static void env_set(_p_data_t dc, _cstr_t var, _str_t val) {
	_env_context_t *p_cxt = (_env_context_t *)dc;
	if(p_cxt) {
		_i_llist_t *pli = HC_INTERFACE(p_cxt->hc_llist);
		_p_data_t pld = HC_DATA(p_cxt->hc_llist);
		if(pli && pld) {
			_u32 sz_val = gpi_str->str_len(val);
			_env_entry_t *pe = get_entry(p_cxt, var);
			if(pe) {
				HMUTEX hlock = pli->lock(pld, 0);
				if(pe->sz_value < sz_val) { /* allocate new buffer */
					heap_free(pe->data, pe->sz_vdata);
					pe->data = alloc_var_buffer((_str_t)var, val, sz_val, &pe->sz_vdata);
				} else { /* reuse old buffer */
					gpi_str->mem_set((_u8 *)(pe->data + pe->sz_vname + 1), 0, pe->sz_value);
					gpi_str->mem_cpy((_u8 *)(pe->data + pe->sz_vname + 1), (_u8 *)val, sz_val);
				}
				pe->sz_value = sz_val;
				pli->unlock(pld, hlock);
			} else { /* create new entry */
				_env_entry_t env_new;
				env_new.sz_vname = gpi_str->str_len((_str_t)var);
				env_new.sz_value = sz_val;
				if((env_new.data = alloc_var_buffer((_str_t)var, val, sz_val, &env_new.sz_vdata)))
					pli->add(pld, &env_new, sizeof(_env_entry_t), 0);
			}
		}
	}
}

static void env_clr(_p_data_t dc) {
	_env_context_t *p_cxt = (_env_context_t *)dc;
	if(p_cxt) {
		_i_llist_t *pli = HC_INTERFACE(p_cxt->hc_llist);
		_p_data_t pld = HC_DATA(p_cxt->hc_llist);
		if(pli && pld) {
			_u32 sz = 0;
			HMUTEX hlock = pli->lock(pld, 0);
			_env_entry_t *p = (_env_entry_t *)pli->first(pld, &sz, hlock);

			if(p) {
				do {
					heap_free(p->data, p->sz_vdata);
				} while((p = (_env_entry_t *)pli->next(pld, &sz, hlock)));
			}
			pli->clr(pld, hlock);
			pli->unlock(pld, hlock);
		}
	}
}

static _i_env_t _g_interface_ = {
	.get	= env_get,
	.set	= env_set,
	.clr	= env_clr
};

static _vx_res_t _mod_ctl_(_u32 cmd, ...) {
	_vx_res_t r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);
	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
			_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
			_env_context_t *p_cdata = va_arg(args, _env_context_t*);

			if(!gpi_str) {
				HCONTEXT hc_str = p_repo->get_context_by_interface(I_STR);
				if(hc_str)
					gpi_str = HC_INTERFACE(hc_str);
			}
			if(!ghc_heap)
				ghc_heap = p_repo->get_context_by_interface(I_HEAP);

			if(p_cdata)
				p_cdata->hc_llist = p_repo->create_context_by_interface(I_LLIST);
			r = VX_OK;
		} break;

		case MODCTL_DESTROY_CONTEXT: {
			_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
			_env_context_t *p_cdata = va_arg(args, _env_context_t*);
			p_repo->release_context(p_cdata->hc_llist);
			r = VX_OK;
		} break;
	}

	va_end(args);
	return r;
}

DEF_VXMOD(
	MOD_ENV,		/* module name */
	I_ENV,			/* interface name */
	&_g_interface_,		/* interface pointer */
	NULL,			/* static data context */
	sizeof(_env_context_t),	/* size of data context (for dynamic allocation) */
	_mod_ctl_,		/* pointer to controll routine */
	1,0,1,			/* version */
	"Environment"		/* description */
);
