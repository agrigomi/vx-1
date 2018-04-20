#ifndef __REG_ALG_H__
#define __REG_ALG_H__

#include "mgtype.h"

#define INVALID_INDEX	0xffffffff

typedef struct reg_array	_reg_array_t;

struct reg_array {
	_u32		fnum;		/* number of free items */
	_u8		*items;		/* array */
	_reg_array_t	*array;		/* next array */
};

typedef struct {
	void *(*mem_alloc)(_u32 size, _ulong limit, void *udata);
	void  (*mem_free)(void *ptr, _u32 size, void *udata);
	void  (*mem_set)(void *, _u8, _u32);
	void  (*mem_cpy)(void *, void *, _u32);
	_u64  (*lock)(_u64 hlock, void *udata);
	void  (*unlock)(_u64 hlock, void *udata);

	_ulong 		addr_limit;	/* address allocation limit */
	_u32		data_size;	/* data size of one item */
	_u32		inum;		/* initial number of items */
	void		*udata;
	_u32		anum;		/* array count */
	_reg_array_t	*array;		/* first (initial) array */
}_reg_context_t;

/* initialize context. Expect values in: 
 mem_alloc, mem_free, lock, unlock,
 addr_limit, data_size, inum, udata */
void reg_init(_reg_context_t *p_cxt);
/* destroy whole register */
void reg_uninit(_reg_context_t *p_cxt, _u64 hlock);
/* add one element (copy number of bytes (data_size) from p_data) */
_u32 reg_add(_reg_context_t *p_cxt, void *p_data, _u64 hlock);
/* return pointer to data content by index */
void *reg_get(_reg_context_t *p_cxt, _u32 idx, _u64 hlock);
/* delete one item by index */
void reg_del(_reg_context_t *p_cxt, _u32 idx, _u64 hlock);

#endif

