#include "mkfs.h"
#include "buffer.h"
#include "file.h"

static _u8 comp_hfile(_h_file_ hfile1, _h_file_ hfile2) {
	_u8 r = 0;

	if(hfile1 && hfile2) {
		if(hfile1->hinode.p_inode == hfile2->hinode.p_inode)
			r = 1;
	}

	return r;
}

_u8 path_parser(_str_t path,  _h_file_ hcdir,/* in */
		 _h_file_ hdir, _str_t *fname /* out */
		) {
	_u8 r = 0;
	_mgfs_file_handle_t hstart_dir;
	_mgfs_context_t *p_cxt = get_context();
	_u32 lpath = _g_pi_str_->str_len(path);

	_s8 dname[256]="";
	_u32 i,j = 0;
	_str_t name = path;

	if(hcdir) {
		_g_pi_str_->mem_cpy((_u8 *)&hstart_dir, (_u8 *)hcdir, sizeof(_mgfs_file_handle_t));
	} else {
		if(!mgfs_file_open_root(p_cxt, &hstart_dir, 0))
			goto _path_parser_done_;
	}

	_g_pi_str_->mem_cpy((_u8 *)hdir, (_u8 *)&hstart_dir, sizeof(_mgfs_file_handle_t));

	for(i = 0; i < lpath && j < 256; i++, name++) {
		switch(path[i]) {
			case '/':
				dname[j] = 0;
				*fname = name+1;
				if(_g_pi_str_->str_len(dname)) {
					if(!mgfs_file_open(p_cxt, dname, hdir, &hstart_dir, 0)) {
						if(!comp_hfile(hcdir, hdir))
							mgfs_file_close(p_cxt, hdir, 0);
						goto _path_parser_done_;
					} else {
						if(!comp_hfile(hcdir, hdir))
							mgfs_file_close(p_cxt, hdir, 0);
						_g_pi_str_->mem_cpy((_u8 *)hdir, (_u8 *)&hstart_dir, sizeof(_mgfs_file_handle_t));
					}
				}
				j = 0;
				break;
			default:
				dname[j] = path[i];
				j++;
		}
	}

	r = 1;
	
_path_parser_done_:
	return r;
}

void create_directory(_str_t path, _h_file_ hdstdir, _str_t name) {
	_mgfs_file_handle_t hfile;
	_mgfs_context_t *p_cxt = get_context();
	if(mgfs_file_create(p_cxt, name, MGFS_DIR, 0, hdstdir, &hfile, 0)) {
		VTRACE("Create directory '%s'\n", path);
		mgfs_file_close(p_cxt, &hfile, 0);
		mgfs_buffer_flush_all(p_cxt, 0);
	} else {
		mgfs_buffer_reset_all(p_cxt, 0);
		TRACE("ERROR: Failed to create directory '%s'\n", path);
	}
}

static _str_t get_dst(void) {
	_str_t r = 0;
	_u32 ldst;

	if(clargs_option(&_g_clargcxt_, "-D", &r, &ldst) != CLARG_OK)
		r = clargs_parameter(&_g_clargcxt_, 2);

	return r;
}

void _mkdir_(void) {
	_str_t sdst = get_dst();

	_mgfs_file_handle_t hdir;
	_str_t fname;

	if(path_parser(sdst, 0, &hdir, &fname)) {
		if(_g_pi_str_->str_len(fname)) 
			create_directory(sdst, &hdir, fname);
		mgfs_file_close(get_context(), &hdir, 0);
	} else {
		TRACE("ERROR: Incorrect path '%s'\n", sdst);
	}
}

void _rmdir_(void) {
	_str_t sdst = get_dst();

	_mgfs_file_handle_t hdir;
	_str_t fname;
	_mgfs_context_t *p_cxt = get_context();

	if(path_parser(sdst, 0, &hdir, &fname)) {
		if(_g_pi_str_->str_len(fname)) {
			if(mgfs_file_delete(p_cxt, fname, &hdir, 0)) {
				VTRACE("Delete directory '%s'\n", sdst);
			} else {
				TRACE("ERROR: Failed to delete directory '%s'\n", sdst);
			}
		}

		mgfs_file_close(p_cxt, &hdir, 0);
	} else {
		TRACE("ERROR: Incorrect path '%s'\n", fname);
	}
}

