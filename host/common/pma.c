#include "vxmod.h"
#include "i_memory.h"
#include "malloc.h"
#include "err.h"

#define PAGE_SIZE	4096

static void *pma_alloc_seq(_u32 npages, _u8 _UNUSED_ type, _ulong _UNUSED_ limit) {
	return malloc(npages * PAGE_SIZE);
}

static _u32 pma_free_seq(void *ptr, _u32 npages) {
	free(ptr);
	return npages;
}

static _u32 pma_alloc_non_seq(_u32 npages, _ulong *page_array, _u8 _UNUSED_ type, _ulong _UNUSED_ limut) {
	_u32 i = 0;

	for(; i < npages; i++) {
		if(!(page_array[i] = (_ulong)malloc(PAGE_SIZE)))
			break;
	}

	return i;
}

static _u32 pma_free_non_seq(_u32 npages, _ulong *page_array) {
	_u32 i = 0;

	for(; i < npages; i++)
		free((void *)page_array[i]);

	return i;
}

static void pma_info(_ram_info_t _UNUSED_ *p_info) {}

static _u32 pma_ctl(_u32 cmd, ...) {
	_u32 r = VX_ERR;

	switch(cmd) {
		case MODCTL_EARLY_INIT:
			r = VX_OK;
			break;
		case MODCTL_INIT_CONTEXT:
		case MODCTL_DESTROY_CONTEXT:
			r = VX_OK;
			break;
	}

	return r;
}

static _i_pma_t _pma_public_ = {
	.alloc_seq	= pma_alloc_seq,
	.free_seq	= pma_free_seq,
	.alloc_non_seq	= pma_alloc_non_seq,
	.free_non_seq	= pma_free_non_seq,
	.info		= pma_info
};


DEF_VXMOD(
	MOD_SIM_PMA,
	I_PMA, 
	&_pma_public_, /* interface */ 
	NULL, /* data context */
	0, /* sizeof data context */
	pma_ctl, /* module controll */
	1, 0, 1, /* version */ 
	"sim. of physical memory allocator"
);
