#include "file.h"
#include "inode.h"
#include "dentry.h"
#include "buffer.h"

_u8 mgfs_file_open_root(_mgfs_context_t *p_cxt, /* in */
				_h_file_ hroot, /* out */
				_h_lock_ hlock
			) {
	_u8 r = 0;
	
	hroot->hdentry.hb_dentry = (_h_buffer_)p_cxt->fs.inv_pattern;
	hroot->hdentry.p_dentry = 0;

	if(mgfs_inode_open(p_cxt, p_cxt->fs.root_inode, &hroot->hinode, hlock) == 0)
		r = 1;

	return r;
}

_u8 mgfs_file_open(_mgfs_context_t *p_cxt, _str_t name, _h_file_ hdir, /* in */
			_h_file_ hfile, /* out */
			_h_lock_ hlock
		   ) {
	_u8 r = 0;

	if(hdir && hdir->hinode.p_inode && (hdir->hinode.p_inode->flags & MGFS_DIR)) {
		if(mgfs_dentry_open(p_cxt, &hdir->hinode, name, &hfile->hdentry, hlock)) {
			if(hfile->hdentry.p_dentry->flags & DENTRY_LINK) {
				hfile->hinode.number = (_fsaddr)p_cxt->fs.inv_pattern;
				hfile->hinode.p_inode = 0;
				hfile->hinode.hb_inode = (_h_buffer_)p_cxt->fs.inv_pattern;
				r = 1;
			} else {
				if(mgfs_inode_open(p_cxt, hfile->hdentry.p_dentry->inode_number, &hfile->hinode, hlock) == 0)
					r = 1;
			}
		}
	}

	return r;
}

void mgfs_file_close(_mgfs_context_t *p_cxt, _h_file_ hfile, _h_lock_ hlock) {
	if(hfile->hinode.hb_inode != (_h_buffer_)p_cxt->fs.inv_pattern && hfile->hinode.p_inode)
		mgfs_inode_close(p_cxt, &hfile->hinode, hlock);
	if(hfile->hdentry.hb_dentry != (_h_buffer_)p_cxt->fs.inv_pattern && hfile->hdentry.p_dentry)
		mgfs_dentry_close(p_cxt, &hfile->hdentry, hlock);
}

_u8 mgfs_file_create(_mgfs_context_t *p_cxt, _str_t name, _u16 flags, _u32 owner, _h_file_ hdir, /* in */
			_h_file_ hfile, /* out */
			_h_lock_ hlock
		     ) {
	_u8 r = 0;

	if(hdir->hinode.p_inode->flags & MGFS_DIR) {
		_h_lock_ lock = mgfs_buffer_lock(p_cxt, hlock); /* transaction lock */
		_fsaddr inode_number = mgfs_inode_create(p_cxt, flags, owner, &hfile->hinode, lock);
		if(inode_number != (_fsaddr)p_cxt->fs.inv_pattern) {
			r = mgfs_dentry_create(p_cxt, &hdir->hinode, name, p_cxt->strlen(name) + 1, 
						inode_number, 0, &hfile->hdentry, lock);

			if(r && (flags & MGFS_DIR)) {
				/* create '.' and '..' */
				_fsaddr parent_inode_number = hdir->hinode.number;
				_mgfs_dentry_handle_t hdthis, hdparent;
				mgfs_dentry_create(p_cxt, &hfile->hinode, (_str_t)".", 2, 
							inode_number, 0, &hdthis, lock);
				mgfs_dentry_create(p_cxt, &hfile->hinode, (_str_t)"..", 3, 
							parent_inode_number, 0, &hdparent, lock);

				mgfs_dentry_close(p_cxt, &hdthis, lock);
				mgfs_dentry_close(p_cxt, &hdparent, lock);
			}
		}
		if(r) /* commit */
			mgfs_buffer_flush_all(p_cxt, lock);
		else /* rollback */
			mgfs_buffer_reset_all(p_cxt, lock);

		mgfs_buffer_unlock(p_cxt, lock);
	}

	return r;
}

_u8 mgfs_file_delete(_mgfs_context_t *p_cxt, _str_t name, _h_file_ hdir, _h_lock_ hlock) {
	_u8 r = 0;

	if(hdir->hinode.p_inode->flags & MGFS_DIR) {
		_mgfs_file_handle_t hfile;
		_h_lock_ lock = mgfs_buffer_lock(p_cxt, hlock);
		if(mgfs_file_open(p_cxt, name, hdir, &hfile, lock)) {
			_u8 del = 1;

			if(hfile.hinode.p_inode->flags & MGFS_DIR) {
				/* check for empty directory */
				/*??? */
			}

			mgfs_file_close(p_cxt, &hfile, lock);
			if(del) {
				if(hfile.hinode.p_inode) 
					mgfs_inode_delete(p_cxt, &hfile.hinode, lock);
				r = mgfs_dentry_delete(p_cxt, &hdir->hinode, name, lock);
			}

			if(r) /* commit */
				mgfs_buffer_flush_all(p_cxt, lock);
			else /* rollback */
				mgfs_buffer_reset_all(p_cxt, lock);
		}
		mgfs_buffer_unlock(p_cxt, lock);
	}

	return r;
}

_u8 mgfs_file_move(_mgfs_context_t *p_cxt, _h_file_ hsrcdir, _str_t srcname, _h_file_ hdstdir, _str_t dstname, _h_lock_ hlock) {
	_u8 r = 0;

	_mgfs_file_handle_t hfsrc;
	_h_lock_ lock = mgfs_buffer_lock(p_cxt, hlock);
	
	if(mgfs_file_open(p_cxt, srcname, hsrcdir, &hfsrc, lock)) {
		_mgfs_file_handle_t hfdst;
		if(mgfs_file_open(p_cxt, dstname, hdstdir, &hfdst, lock)) {
			/* check for existing destination */
			if(hfdst.hinode.p_inode->flags & MGFS_DIR) { /* the destination is a directory (allowed) */
				/* create new dentry in 'hfdst' with name located in source dentry */
				_mgfs_dentry_handle_t hdnew;
				_str_t newfname = mgfs_dentry_name(p_cxt, &hfsrc.hdentry);
				_u16 newfname_sz = mgfs_dentry_name_size(&hfsrc.hdentry);
	
				if(mgfs_dentry_create(p_cxt, &hfdst.hinode, newfname, newfname_sz, hfsrc.hinode.number,
							hfsrc.hdentry.p_dentry->flags, &hdnew, lock)) {
					r = mgfs_dentry_delete(p_cxt, &hsrcdir->hinode, srcname, lock);
					mgfs_dentry_close(p_cxt, &hdnew, lock);
				}
			} else
				p_cxt->last_error = MGFS_DENTRY_EXISTS;

			mgfs_file_close(p_cxt, &hfdst, lock);
		} else {
			/* create new dentry in 'hdstdir' with name 'dstname' */
			_mgfs_dentry_handle_t hdnew;
			if(mgfs_dentry_flags(&hfsrc.hdentry) & DENTRY_LINK) {
				/* create link dentry */
				_u32 newfname_sz = p_cxt->strlen(dstname)+1 + p_cxt->strlen(mgfs_dentry_link_name(p_cxt, &hfsrc.hdentry))+1;
				_str_t newfname = (_str_t)mgfs_mem_alloc(p_cxt, newfname_sz);
				if(newfname) {
					p_cxt->memcpy((_u8 *)newfname, (_u8 *)dstname, p_cxt->strlen(dstname)+1);
					p_cxt->memcpy((_u8 *)newfname + p_cxt->strlen(dstname)+1, 
						(_u8 *)mgfs_dentry_link_name(p_cxt, &hfsrc.hdentry),
						p_cxt->strlen(mgfs_dentry_link_name(p_cxt,&hfsrc.hdentry))+1);

					if(mgfs_dentry_create(p_cxt, &hdstdir->hinode, newfname, newfname_sz, hfsrc.hinode.number,
								hfsrc.hdentry.p_dentry->flags, &hdnew, lock)) {
						r = mgfs_dentry_delete(p_cxt, &hsrcdir->hinode, srcname, lock);
						mgfs_dentry_close(p_cxt, &hdnew, lock);
					}
					
					mgfs_mem_free(p_cxt, newfname, newfname_sz);
				}
			} else {
				/* create regular dentry */
				if(mgfs_dentry_create(p_cxt, &hdstdir->hinode, dstname, p_cxt->strlen(dstname)+1, hfsrc.hinode.number,
							hfsrc.hdentry.p_dentry->flags, &hdnew, lock)) {
					r = mgfs_dentry_delete(p_cxt, &hsrcdir->hinode, srcname, lock);
					mgfs_dentry_close(p_cxt, &hdnew, lock);
				}
			}
		}

		mgfs_file_close(p_cxt, &hfsrc, lock);
	}

	if(r)
		/* commit */
		mgfs_buffer_flush_all(p_cxt, lock);
	else
		/* rollback */
		mgfs_buffer_reset_all(p_cxt, lock);

	mgfs_buffer_unlock(p_cxt, lock);

	return r;
}

_u8 mgfs_file_create_hard_link(_mgfs_context_t *p_cxt, _h_file_ hsrcdir, _str_t name,
				_h_file_ hdstdir, _str_t link_name, _h_lock_ hlock) {
	_u8 r = 0;

	_mgfs_file_handle_t hfile;
	_h_lock_ lock = mgfs_buffer_lock(p_cxt, hlock);

	if(mgfs_file_open(p_cxt, name, hsrcdir, &hfile, lock)) {
		_mgfs_dentry_handle_t hdentry;
		hdentry.hb_dentry = (_h_buffer_)p_cxt->fs.inv_pattern;
		r = mgfs_dentry_create(p_cxt, &hdstdir->hinode, link_name, p_cxt->strlen(link_name)+1, 
					hfile.hdentry.p_dentry->inode_number, 0, &hdentry, lock);
		if(r) {
			hfile.hinode.p_inode->lc++;
			mgfs_inode_update(p_cxt, &hfile.hinode, lock);
			mgfs_dentry_close(p_cxt, &hdentry, lock);
		}

		mgfs_file_close(p_cxt, &hfile, lock);
	}

	if(r)
		/* commit */
		mgfs_buffer_flush_all(p_cxt, lock);
	else
		/* rollback */
		mgfs_buffer_reset_all(p_cxt, lock);
	
	mgfs_buffer_unlock(p_cxt, lock);

	return r;
}

_u8 mgfs_file_create_soft_link(_mgfs_context_t *p_cxt, _str_t path_name,
				_h_file_ hdstdir, _str_t link_name, _h_lock_ hlock) {
	_u8 r = 0;
	_u16 name_size = p_cxt->strlen(link_name) + p_cxt->strlen(path_name) + 2;
	_u8 *ptr_name = (_u8 *)mgfs_mem_alloc(p_cxt, name_size);
	if(ptr_name) {
		_h_lock_ lock = mgfs_buffer_lock(p_cxt, hlock);

		p_cxt->memcpy(ptr_name, (_u8 *)link_name, p_cxt->strlen(link_name) + 1);
		p_cxt->memcpy(ptr_name + p_cxt->strlen(link_name) + 1, (_u8 *)path_name, p_cxt->strlen(path_name) + 1);
		_mgfs_dentry_handle_t hdlink;
		r = mgfs_dentry_create(p_cxt, &hdstdir->hinode, (_str_t)ptr_name, name_size, (_fsaddr)p_cxt->fs.inv_pattern, DENTRY_LINK,
					&hdlink, lock);
		if(r)
			mgfs_dentry_close(p_cxt, &hdlink, lock);

		mgfs_mem_free(p_cxt, ptr_name, name_size);

		if(r) /* commit */
			mgfs_buffer_flush_all(p_cxt, lock);
		else
			/* rollback */
			mgfs_buffer_reset_all(p_cxt, lock);

		mgfs_buffer_unlock(p_cxt, lock);
	}

	return r;
}

_u32 mgfs_file_read(_mgfs_context_t *p_cxt, _h_file_ hfile, _u64 offset, void *buffer, _u32 nbytes, _h_lock_ hlock) {
	return mgfs_inode_read(p_cxt, &hfile->hinode, offset, buffer, nbytes, hlock);
}

_u32 mgfs_file_write(_mgfs_context_t *p_cxt, _h_file_ hfile, _u64 offset, void *buffer, _u32 nbytes, _h_lock_ hlock) {
	_u32 r = 0;
	_h_lock_ lock = mgfs_buffer_lock(p_cxt, hlock);
	r = mgfs_inode_write(p_cxt, &hfile->hinode, offset, buffer, nbytes, lock);
	if(r == nbytes)
		/* commit */
		mgfs_buffer_flush_all(p_cxt, lock);
	else { /* rollback */
		mgfs_buffer_reset_all(p_cxt, lock);
		r = 0;
	}
	mgfs_buffer_unlock(p_cxt, lock);
	return r;
}

_u64 mgfs_file_size(_h_file_ hfile) {
	_u64 r = 0;

	if(hfile->hinode.p_inode)
		r = hfile->hinode.p_inode->sz;

	return r;
}

