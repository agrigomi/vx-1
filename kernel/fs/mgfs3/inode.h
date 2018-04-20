#ifndef __MGFS_INODE_H__
#define __MGFS_INODE_H__

#include "mgfs3.h"

/* inode operations */

/* validate inode handle */
_u8 mgfs_is_valid_inode_handle(_mgfs_context_t *p_cxt, _h_inode_ h);

/* return count of data unit numbers in "p_u_buffer" */
_u32 mgfs_inode_list_dunit(_mgfs_context_t *p_cxt, 
			   _h_inode_ hinode,
			   _fsaddr first, /* first needed block in file */
			   _u32 count, /* count of units after first */
			   _fsaddr *p_u_buffer, /* [out] array of data unit numbers */
			   _h_lock_ hlock
			  );

/* return the size of requested data in bytes */
_u32 mgfs_inode_calc_data_pos(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 inode_offset, _u32 size, /* in */
			      _fsaddr *p_block_number, _u32 *p_block_offset, _u32 *p_block_count /* out */
		             );
/* return 0 for success */
_u32 mgfs_inode_open(_mgfs_context_t *p_cxt, _u32 inode_number, /* in */
		     _h_inode_ hinode, /* out */
		     _h_lock_ hlock
		    );
_u32 mgfs_inode_meta_open(_mgfs_context_t *p_cxt, _u8 meta_inode_number, /* in */
			  _h_inode_ hinode, /* out */
			  _h_lock_ hlock
			 );
_u32 mgfs_inode_delete(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_lock_ hlock);

_u32 mgfs_inode_owner(_mgfs_context_t *p_cxt, _h_inode_ hinode);

/* append initialized blocks to inode, and return the number of new appended blocks */
_u32 mgfs_inode_append_blocks(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u32 nblocks, _u8 pattern, _h_lock_ hlock);

/* return inode number for success, else INVALID_INODE_NUMBER */
_fsaddr mgfs_inode_create(_mgfs_context_t *p_cxt, _u16 flags, _u32 owner_id, /* in */
		       _h_inode_ hinode, /* out */
		       _h_lock_ hlock
		      );

/* return buffer handle for requested inode block */
_h_buffer_ mgfs_inode_read_block(_mgfs_context_t *p_cxt, _h_inode_ hinode, _fsaddr block_number, _h_lock_ hlock);

/* return the actual number of bytes */
_u32 mgfs_inode_read(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 offset, void *p_buffer, _u32 size, _h_lock_ hlock);
_u32 mgfs_inode_write(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 offset, void *p_buffer, _u32 size, _h_lock_ hlock);

void mgfs_inode_close(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_lock_ hlock);
void mgfs_inode_update(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_lock_ hlock);

/* truncate inode to size passed in 'new_size' parameter and return the new inode size */
_u64 mgfs_inode_truncate(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 new_size, _h_lock_ hlock);

#endif

