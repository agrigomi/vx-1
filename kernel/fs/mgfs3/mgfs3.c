#include "mgfs3.h"

_mgfs_info_t *mgfs_get_info(_mgfs_context_t *p_cxt) {
	_mgfs_info_t *r = 0;
	
	if(p_cxt->fs.magic == MGFS_MAGIC)
		r = &(p_cxt->fs);
	else {
		if(p_cxt->alloc && p_cxt->read && p_cxt->free && p_cxt->memcpy) {
			void *p_sector = p_cxt->alloc(p_cxt->sector_size, p_cxt->udata);
			if(p_sector) {
				if(p_cxt->read(MGFS_SB_SECTOR, 1, p_sector, p_cxt->udata) == 0) {
					p_cxt->memcpy((_u8 *)&(p_cxt->fs), (_u8 *)p_sector + MGFS_SB_OFFSET, sizeof(_mgfs_info_t));
					r = &(p_cxt->fs);
				}
				
				p_cxt->free(p_sector, p_cxt->sector_size, p_cxt->udata);
			}
		}
	}
	
	return r;
}

void mgfs_update_info(_mgfs_context_t *p_cxt) {
	if(p_cxt->alloc && p_cxt->read && p_cxt->write && p_cxt->free && p_cxt->memcpy) {
		void *p_sector = p_cxt->alloc(p_cxt->sector_size, p_cxt->udata);
		if(p_sector) {
			if(p_cxt->read(MGFS_SB_SECTOR, 1, p_sector, p_cxt->udata) == 0) {
				p_cxt->memcpy((_u8 *)p_sector + MGFS_SB_OFFSET, (_u8 *)&(p_cxt->fs), sizeof(_mgfs_info_t));
				p_cxt->write(MGFS_SB_SECTOR, 1, p_sector, p_cxt->udata);
			}
			
			p_cxt->free(p_sector, p_cxt->sector_size, p_cxt->udata);
		}
	}
}

void *mgfs_mem_alloc(_mgfs_context_t *p_cxt, _u32 sz) {
	void *r = 0;
	
	if(p_cxt->alloc)
		r = p_cxt->alloc(sz, p_cxt->udata);
	
	return r;
}

void mgfs_mem_free(_mgfs_context_t *p_cxt, void *ptr, _u32 size) {
	if(p_cxt->free)
		p_cxt->free(ptr, size, p_cxt->udata);
}

_u32 mgfs_unit_size(_mgfs_context_t *p_cxt) {
	_u32 r = 0;
	_mgfs_info_t *p_fs = mgfs_get_info(p_cxt);
	if(p_fs)
		r = p_fs->sz_sector * p_fs->sz_unit;

	return r;
}

_u8 mgfs_flags(_mgfs_context_t *p_cxt) {
	_u8 r = 0;
	_mgfs_info_t *p_fs = mgfs_get_info(p_cxt);
	if(p_fs)
		r = p_fs->flags;

	return r;
}

