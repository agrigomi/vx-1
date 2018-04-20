#ifndef __I_VFS_H__
#define __I_VFS_H__

#include "vxvfs.h"
#include "i_sync.h"

#define I_SSP		"i_single_storage_pool"	
#define I_MSP		"i_multiple_storage_pool"

#define INVALID_STORAGE_INDEX	0xffffffff

typedef struct {
	void (*init)(_p_data_t, _u32 max_storages, _u32 unit_size);
	/* attach storage and return index (INVALID_STORAGE_INDEX for error) */
	_u32 (*attach)(_p_data_t, HFILE, _ulong offset, _ulong size);
	/* detach storage by index */
	_vx_res_t (*detach)(_p_data_t, _u32 index);
	/* external synchro */
	HMUTEX (*lock)(_p_data_t, HMUTEX hlock);
	void (*unlock)(_p_data_t, HMUTEX hlock);
	/*------------------------------------------------*/
	/* buffer operations */
	/*------------------------------------------------*/
	/* return buffer number as success result, otherwise  INVALID_BUFFER_NUMBER */
	_u32 (*buffer_alloc)(_p_data_t, _ulong unit_number, HMUTEX);
	/* release buffer by number */
	void (*buffer_free)(_p_data_t, _u32 bn, HMUTEX);
	/* get pointer to buffer content */
	_p_data_t (*buffer_ptr)(_p_data_t, _u32 bn, HMUTEX); 
	/* return LBA of storage unit by buffer number */
	_ulong (*buffer_unit)(_p_data_t, _u32 bn, HMUTEX);
	/* flush single buffer */
	void (*buffer_flush)(_p_data_t, _u32 bn, HMUTEX);
	/* make buffer dirty */
	void (*buffer_dirty)(_p_data_t, _u32 bn, HMUTEX);
	/* flush whole buffer map */
	void (*flush)(_p_data_t, HMUTEX);
	/* rollback whole buffer map (refresh all dirty buffers from storage) */
	void (*rollback)(_p_data_t, HMUTEX);
}_i_storage_pool_t;

#endif

