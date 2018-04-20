#include "mkfs.h"
#include "dentry.h"
#include "file.h"

static _str_t get_src(void) {
	_str_t r = 0;
	_u32 sz = 0;
	
	clargs_option(&_g_clargcxt_, "-S", &r, &sz);
	if(!r)
		r = clargs_parameter(&_g_clargcxt_, 2);

	return r;
}

static _str_t get_dst(void) {
	_str_t r = 0;
	_u32 sz = 0;

	clargs_option(&_g_clargcxt_, "-D", &r, &sz);
	if(!r)
		r = clargs_parameter(&_g_clargcxt_, 3);

	return r;
}

void create_soft_link(_str_t src, _str_t dst) {
	_mgfs_file_handle_t hdstdir;
	_str_t dstname;

	if(path_parser(dst, 0, &hdstdir, &dstname)) {
		/* create soft link with 'dstname' */
		if(mgfs_file_create_soft_link(get_context(), src, &hdstdir, dstname, 0)) {
			VTRACE("Create soft link '%s' --> '%s'\n", dst, src);
		} else
			TRACE("ERROR: Failed to create soft link '%s' --> '%s'\n", dst, src);

		mgfs_file_close(get_context(), &hdstdir, 0);
	} else
		TRACE("ERROR: Incorrect path '%s'\n", dst);
}

void _soft_link_(void) {
	_str_t src = get_src();
	_str_t dst = get_dst();

	create_soft_link(src, dst);
}

void create_hard_link(_str_t src, _str_t dst) {
	_mgfs_file_handle_t hsrcdir;
	_str_t srcname;

	if(path_parser(src, 0, &hsrcdir, &srcname)) {
		_mgfs_file_handle_t hdstdir;
		_str_t dstname;

		if(path_parser(dst, 0, &hdstdir, &dstname)) {
			if(mgfs_file_create_hard_link(get_context(), &hsrcdir, srcname, &hdstdir, dstname, 0)) {
				VTRACE("Create hard link '%s' --> '%s'\n", dst, src);
			} else
				TRACE("ERROR: Failed to create hard link '%s' --> '%s'\n", dst, src);
			mgfs_file_close(get_context(), &hdstdir, 0);
		} else
			TRACE("ERROR: Incorrect path '%s'\n", dst);

		mgfs_file_close(get_context(), &hsrcdir, 0);
	} else
		TRACE("ERROR: Incorrect path '%s'\n", src);
}

void _hard_link_(void) {
	_str_t src = get_src();
	_str_t dst = get_dst();

	create_hard_link(src, dst);
}

