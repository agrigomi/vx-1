#ifndef __MGFS_DENTRY_H__
#define __MGFS_DENTRY_H__

#include "mgfs3.h"

#define MGFS_DENTRY_EXISTS	__ERR-2
#define MGFS_DIR_NOT_EMPTY	__ERR-3

/* dentry list callback */
typedef void _mgfs_dentry_list_t(_u16 inode_flags, 
				_u8 dentry_flags, 
				_str_t name, 
				_str_t link_path, 
				_u32 ct, /* creation timestamp */
				_u32 mt, /* last modification timestamp */
				_u32 owner,
				_u64 size, void *p_udata);

/* dentry operations */

/* validate dentry handle */
_u8 mgfs_is_valid_dentry_handle(_mgfs_context_t *p_cxt, _h_dentry_ h);

/* return dentry position */
_u8 mgfs_dentry_open(_mgfs_context_t *p_cxt, _h_inode_ hdir, _str_t name, /* in */
		      _h_dentry_ hd, /* out */
		      _h_lock_ hlock
		     );
_u8 mgfs_dentry_create(_mgfs_context_t *p_cxt, _h_inode_ hdir, _str_t name, /* in */
			_u32 sz_name, _fsaddr inode_number, _u8 flags, /* in */
			_h_dentry_ hd, /* out */
			_h_lock_ hlock
		       );

_u8 mgfs_dentry_rename(_mgfs_context_t *p_cxt, _h_inode_ hdir, _str_t name, _str_t new_name, _h_lock_ hlock);
_u8 mgfs_dentry_delete(_mgfs_context_t *p_cxt, _h_inode_ hdir, _str_t name, _h_lock_ hlock);
void mgfs_dentry_close(_mgfs_context_t *p_cxt, _h_dentry_ hd, _h_lock_ hlock);
void mgfs_dentry_list(_mgfs_context_t *p_cxt, _h_inode_ hdir, 
			_mgfs_dentry_list_t *p_callback, void *p_udata, _h_lock_ hlock);
_str_t mgfs_dentry_name(_mgfs_context_t *p_cxt, _h_dentry_ hd);
_str_t mgfs_dentry_link_name(_mgfs_context_t *p_cxt, _h_dentry_ hd);
_u16 mgfs_dentry_name_size(_h_dentry_ hd);
_u8 mgfs_dentry_flags(_h_dentry_ hd);

#endif

