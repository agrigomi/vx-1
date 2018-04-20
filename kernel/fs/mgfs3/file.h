#ifndef __MGFS_FILE_H__
#define __MGFS_FILE_H__

#include "mgfs3.h"

/* file operations */
_u8 mgfs_file_open_root(_mgfs_context_t *p_cxt, /* in */
				_h_file_ hroot, /* out */
				_h_lock_ hlock
			);
_u8 mgfs_file_create(_mgfs_context_t *p_cxt, _str_t name, _u16 flags, _u32 owner, _h_file_ hdir, /* in */
			_h_file_ hfile, /* out */
			_h_lock_ hlock
		     );
_u8 mgfs_file_open(_mgfs_context_t *p_cxt, _str_t name, _h_file_ hdir, /* in */
			_h_file_ hfile, /* out */
			_h_lock_ hlock
		   );
void mgfs_file_close(_mgfs_context_t *p_cxt, _h_file_ hfile, _h_lock_ hlock);
_u8 mgfs_file_delete(_mgfs_context_t *p_cxt, _str_t name, _h_file_ hdir, _h_lock_ hlock);
_u8 mgfs_file_move(_mgfs_context_t *p_cxt, _h_file_ hsrcdir, _str_t srcname, _h_file_ hdstdir, _str_t dstname, _h_lock_ hlock);
_u8 mgfs_file_create_hard_link(_mgfs_context_t *p_cxt, _h_file_ hsrcdir, _str_t name,
				_h_file_ hdstdir, _str_t link_name, _h_lock_ hlock);
_u8 mgfs_file_create_soft_link(_mgfs_context_t *p_cxt, _str_t path_name,
				_h_file_ hdstdir, _str_t link_name, _h_lock_ hlock);
_u32 mgfs_file_read(_mgfs_context_t *p_cxt, _h_file_ hfile, _u64 offset, void *buffer, _u32 nbytes, _h_lock_ hlock);
_u32 mgfs_file_write(_mgfs_context_t *p_cxt, _h_file_ hfile, _u64 offset, void *buffer, _u32 nbytes, _h_lock_ hlock);
_u64 mgfs_file_size(_h_file_ hfile);

#endif

