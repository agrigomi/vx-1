#ifndef __SLAB_H__
#define __SLAB_H__

#include "mgtype.h"

#define SLAB_NODES	7
#define SLAB_MIN_ALLOC	16

typedef _u64	_slab_hlock_t;

typedef void *_mem_alloc_t(_u32 npages, _ulong limit, void *p_udata);
typedef void _mem_free_t(void *ptr, _u32 npages, void *p_udata);
typedef void _mem_cpy_t(void *dst, void *src, _u32 sz);
typedef void _mem_set_t(void *ptr, _u8 pattern, _u32 sz);
typedef _slab_hlock_t _lock_t(_slab_hlock_t hlock, void *p_udata);
typedef void _unlock_t(_slab_hlock_t hlock, void *p_udata);

typedef struct {
	_u32	osz;		/* object size */
	_u64	ptr[SLAB_NODES];/* pointer array */
	_u32	level	:4;	/* recursion level*/
	_u32	count	:28;	/* objects count */
}__attribute__((packed)) _slab_t;

typedef struct {
	_u32	naobj;	/* number of active objects */
	_u32	nrobj;	/* number of reserved objects */
	_u32	ndpg;	/* number of data pages */
	_u32	nspg;	/* number of slab pages */
}_slab_status_t;

typedef struct {
	_u32		page_size;
	_mem_alloc_t	*p_mem_alloc;
	_mem_free_t	*p_mem_free;
	_mem_cpy_t	*p_mem_cpy;
	_mem_set_t	*p_mem_set;
	_lock_t		*p_lock;
	_unlock_t	*p_unlock;
	void		*p_udata;
	_slab_t		*p_slab;
}_slab_context_t;

_u8 slab_init(_slab_context_t *p_scxt);
void *slab_alloc(_slab_context_t *p_scxt, _u32 size, _ulong limit);
void slab_free(_slab_context_t *p_scxt, void *ptr, _u32 size);
_bool slab_verify(_slab_context_t *p_scxt, void *ptr, _u32 size);
void slab_status(_slab_context_t *p_scxt, _slab_status_t *p_sstatus);
void slab_destroy(_slab_context_t *p_scxt);

#endif

