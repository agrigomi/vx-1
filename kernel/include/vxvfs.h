#ifndef __VX_VFS_H__
#define __VX_VFS_H__

#include "vxdev.h"
#include "vxusr.h"

#define MAX_FS_IDENT	16
#define MAX_FS_FDATA	128

/* file permissions */
#define FP_ALL_EXEC		(0x01 << 0)
#define FP_ALL_WRITE		(0x01 << 1)
#define FP_ALL_READ		(0x01 << 2)
#define FP_GROUP_EXEC		(0x01 << 3)
#define FP_GROUP_WRITE		(0x01 << 4)
#define FP_GROUP_READ		(0x01 << 5)
#define FP_OWNER_EXEC		(0x01 << 6)
#define FP_OWNER_WRITE		(0x01 << 7)
#define FP_OWNER_READ		(0x01 << 8)

/* file type */
#define FT_DIR			(0x01 << 0)
#define FT_HIDDEN		(0x01 << 1)
#define FT_SLINK		(0x01 << 2)
#define FT_DEV			(0x01 << 3)

/* file handle state */
#define FH_OPEN			(0x01 << 0)
#define FH_IO_PENDING		(0x01 << 1)
#define FH_CLOSE_PENDING	(0x01 << 2)

#define HFILE		_vx_file_handle_t*

typedef struct vx_vfs		_vx_vfs_t;
typedef struct vx_file_handle	_vx_file_handle_t;
typedef struct vx_file		_vx_file_t;

#define INVALID_BUFFER_NUMBER	0xffffffff
#define INVALID_UNIT_NUMBER	0xffffffffffffffffLLU

/* storage type */
#define ST_DEVICE	1
#define ST_FILE		2

struct vx_file { /* file descriptor */
	_str_t		_f_name_;		/* file name */
	_u16		_f_pmask_;		/* permissions bit mask */
	_u8		_f_tmask_;		/* type bit mask */
	_u64		_f_size_;		/* file size */
	_u32 		_f_ctime_;		/* creation timestamp */
	_u32		_f_mtime_;		/* last modification timestamp */
	_u32		_f_hcount_;		/* number of handles */
	HFILE		_f_parent_;		/* parent directory (NULL for root) */
	HUSR		_f_owner_;		/* file owner context */
	HCONTEXT	_f_pool_;		/* context of storage pool */
	_vx_vfs_t	*_f_vfs_;
	_u8		_f_data_[MAX_FS_FDATA];	/* FS implementation specific data */
};

struct vx_file_handle { /* file handle descriptor */
	_u8		_h_state_;		/* handle state bitmask */
	HUSR		_h_user_;		/* user context */
	_u64		_h_cpos_;		/* current position */
	_vx_file_t	*_h_file_;		/* pointer to file descriptor */
};

struct vx_vfs { /* virtual filesystem descriptor */
	_s8		_v_ident_[MAX_FS_IDENT];/* ident string */
	_str_t		_v_mod_;		/* module name */
	_u8		_v_index_;		/* context index */
	_vx_version_t	_v_ver_;		/* fs version */
	_ctl_t		*_v_ctl_;		/* fs controll */
	_p_data_t	_v_data_;		/* FS module data context (impl. specific) */
};

/* vfs controll commands */
#define VFSCTL_INIT	201	/* args: _i_vfs_t*, _vx_vfs_t* */
#define VFSCTL_UNINIT	202	/* args: _i_vfs_t*, _vx_vfs_t* */
#define VFSCTL_CREATE	203	/* create file
				   args: _str_t,	file name
				   	_u32,		flags
					HUSR,		user context
					HFILE,		directory handle (NULL for root)
					HFILE*		result (NULL for error)
				*/
#define VFSCTL_OPEN	204	/* open file
				   args: _str_t,	file name ('.' for root, if directory handle is NULL)
				   	_u32,		flags
					HUSR,		user context
					HFILE,		directory handle (NULL for root)
					HFILE*		result (NULL for error)
				*/
#define VFSCTL_CLOSE	205	/* close file
				   args: HFILE	*/
#define VFSCTL_READ	206	/* args: HFILE,	 	file handle
					_u32,		number of bytes to read
					_p_data_t,	input buffer
					_u32*		result (number of bytes in input buffer)
				*/
#define VFSCTL_WRITE	207	/* args: HFILE,	 	file handle
					_u32,		number of bytes to write
					_p_data_t,	output buffer
					_u32*		result (number of written bytes)
				*/
#define VFSCTL_SEEK	208	/* change file position
				   args: HFILE, 
				   	_ulong, 	new position
					_ulong*		result (current position)
				*/
#define VFSCTL_DELETE	209	/* delete file
				   args: HFILE,		directory handle
				   	_str_t,		file name
				   	HCONTEXT	user context
				*/
#define VFSCTL_MOVE	210	/* move file
				   args: HFILE,		source directory
				   	_str_t,		source file name
					HFILE,		destination directory
					_str_t		destination file name
				*/
#define VFSCTL_IDENT	220	/* identify storage
				   args: _vx_storage_t*
				*/
/* ... */
			
#endif

