#include "reg_alg.h"

#define RI_USED	(1<<0)

static _reg_array_t *alloc_array(_reg_context_t *p_cxt) {
	_reg_array_t *r = NULL;

	if((r = p_cxt->mem_alloc(sizeof(_reg_array_t), p_cxt->addr_limit, p_cxt->udata))) {
		if(!(r->items = p_cxt->mem_alloc((p_cxt->data_size+1) * p_cxt->inum, 
						p_cxt->addr_limit, p_cxt->udata))) {
			p_cxt->mem_free(r, sizeof(_reg_array_t), p_cxt->udata);
			r = NULL;
		} else {
			p_cxt->mem_set(r->items, 0, (p_cxt->data_size + 1) * p_cxt->inum);
			r->fnum = p_cxt->inum;
			r->array = NULL;
			p_cxt->anum++;
		}
	}
	return r;
}

static void cleanup_array(_reg_context_t *p_cxt, _reg_array_t *array) {
	if(array) {
		cleanup_array(p_cxt, array->array);
		if(array->items) {
			p_cxt->mem_free(array->items, (p_cxt->data_size+1)*p_cxt->inum, p_cxt->udata);
			array->items = NULL;
		}
		p_cxt->mem_free(array, sizeof(_reg_array_t), p_cxt->udata);
		p_cxt->anum--;
	}
}

void reg_init(_reg_context_t *p_cxt) {
	if(p_cxt->array == NULL) {
		p_cxt->anum = 0;
		p_cxt->array = alloc_array(p_cxt);
	}
}

void reg_uninit(_reg_context_t *p_cxt, _u64 hlock) {
	_u64 hl = p_cxt->lock(hlock, p_cxt->udata);
	cleanup_array(p_cxt, p_cxt->array);
	p_cxt->array = NULL;
	p_cxt->unlock(hl, p_cxt->udata);
}

_u32 reg_add(_reg_context_t *p_cxt, void *p_data, _u64 hlock) {
	_u32 r = INVALID_INDEX;
	_u32 n = 0;
	_reg_array_t *array = p_cxt->array;
	_u32 dsz = p_cxt->data_size + 1;

	_u64 hl = p_cxt->lock(hlock, p_cxt->udata);

	while(array) {
		if(array->items) {
			_u8 *ptr = (_u8 *)array->items;
			if(array->fnum) {
				_u32 i = 0;
				for(; i < p_cxt->inum; i++, n++) {
					if(!(*ptr & RI_USED)) {
						p_cxt->mem_cpy(ptr+1, p_data, p_cxt->data_size);
						*ptr |= RI_USED;
						r = n;
						array->fnum--;
						array = NULL; /* break parent loop */
						break;
					}
					ptr += dsz;
				}
			} else {
				array = array->array;
				if(!array)
					array = array->array = alloc_array(p_cxt);
				n += p_cxt->inum;
			}
		} else
			/* !!! panic !!! */
			break;
	}

	p_cxt->unlock(hl, p_cxt->udata);

	return r;
}

static void *reg_rec(_reg_context_t *p_cxt, _u32 idx, _reg_array_t **pp_array) {
	void *r = NULL;
	
	if(idx < p_cxt->anum * p_cxt->inum) {
		_reg_array_t *array = p_cxt->array;
		_u32 a = 0;
		_u32 i = 0;

		while(array) {
			a++;
			if(idx < (a * p_cxt->inum)) {
				r = array->items + (idx - i);
				*pp_array = array;
				break;
			} else {
				array = array->array;
				i += p_cxt->inum;
			}
		}
	}

	return r;
}

void *reg_get(_reg_context_t *p_cxt, _u32 idx, _u64 hlock) {
	void *r = NULL;
	_u64 hl = p_cxt->lock(hlock, p_cxt->udata);
	_reg_array_t *p_array = NULL;
	_u8 *ptr = reg_rec(p_cxt, idx, &p_array);
	if(ptr) {
		if(*ptr & RI_USED)
			r = ptr+1;
	}
	p_cxt->unlock(hl, p_cxt->udata);
	return r;
}

void reg_del(_reg_context_t *p_cxt, _u32 idx, _u64 hlock) {
	_u64 hl = p_cxt->lock(hlock, p_cxt->udata);
	_reg_array_t *p_array = NULL;
	_u8 *ptr = reg_rec(p_cxt, idx, &p_array);
	if(ptr) {
		if(*ptr & RI_USED) {
			*ptr &= ~RI_USED;
			p_array->fnum++;
		}
	}
	p_cxt->unlock(hl, p_cxt->udata);
}
