#include "mkfs.h"
#include "dentry.h"
#include "file.h"
#include "inode.h"
#include "bitmap.h"

static void _dentry_list_(_u16 inode_flags, _u8 dentry_flags, _str_t name, _str_t link_path, 
			_u32 _UNUSED_ ct, _u32 _UNUSED_ mt, _u32 _UNUSED_ owner, _u64 size, void _UNUSED_ *p_udata) {
	if(dentry_flags & DENTRY_LINK) {
		TRACE("\t\t%s --> %s\n", name, link_path);
	} else {
		TRACE("%lu\t\t%s", (_ulong)size, name);
		if(inode_flags & MGFS_DIR)
			TRACE("%c",'/');
		TRACE("%c",'\n');
	}
}

void _list_(void) {
	_str_t dst;
	_u32 ldst;

	if(clargs_option(&_g_clargcxt_, "-D", &dst, &ldst) != CLARG_OK)
		dst = clargs_parameter(&_g_clargcxt_, 2);

	if(dst) {
		_mgfs_file_handle_t hdir;
		_str_t fname = 0;
		if(path_parser(dst, 0, &hdir, &fname)) {
			_mgfs_file_handle_t hfile;
			if(_g_pi_str_->str_len(fname)) {
				if(mgfs_file_open(get_context(), fname, &hdir, &hfile, 0)) {
					mgfs_dentry_list(get_context(), &hfile.hinode, _dentry_list_, NULL, 0);
					mgfs_file_close(get_context(), &hfile, 0);
				} else
					TRACE("ERROR: Failed to open '%s'\n", dst);
			} else 
				mgfs_dentry_list(get_context(), &hdir.hinode, _dentry_list_, NULL, 0);

			mgfs_file_close(get_context(), &hdir, 0);
		} else
			TRACE("ERROR: Incorrect path '%s'\n", dst);
	}
}

void _status_(void) {
	_mgfs_inode_handle_t hsbitmap, hsbshadow;

	_u64 bytes = get_context()->fs.sz_sector * get_context()->fs.sz_unit;

	if(mgfs_inode_meta_open(get_context(), SPACE_BITMAP_IDX, &hsbitmap, 0) == 0) {
		_u32 units = mgfs_bitmap_free_state(get_context(), &hsbitmap, 0);
		TRACE("Primar bitmap free space in units: %u (%lu bytes)\n",units, (long)(units * bytes));
		mgfs_inode_close(get_context(), &hsbitmap, 0);
	}
	
	if(mgfs_inode_meta_open(get_context(), SPACE_BITMAP_SHADOW_IDX, &hsbshadow, 0) == 0) {
		_u32 units = mgfs_bitmap_free_state(get_context(), &hsbitmap, 0);
		TRACE("Shadow bitmap free space in units: %u (%lu bytes)\n",units, (long)(units * bytes));
		mgfs_inode_close(get_context(), &hsbshadow, 0);
	}
}

