#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "time.h"
#include "vxmod.h"
#include "startup_context.h"
#include "mkfs.h"
#include "i_memory.h"

#define MKMGFS_VERSION	"3.0"

_i_str_t *_g_pi_str_ = NULL;
_i_heap_t *_g_pi_heap_ = NULL;
_p_data_t _g_pd_heap_ = NULL;
_clarg_context_t _g_clargcxt_;
_i_repository_t * _g_vx_repo_ = NULL;
_mgfs_context_t _g_mgfs_context_;
static _u8 *_g_dev_ = NULL;
static _u64 _g_dev_size_=0;
static _u64 last_map_size = 0;
_u8 _g_verbose_ = 0;

extern void _vx_core_init_(_core_startup_t *p_sc);

_clarg_t _g_options_[]={
	{ARG_TFLAG,	"--verbose",	"-V",	"be verbose", NULL, 0, 0, 0},
	{ARG_TFLAG,	"--version",	"-v",	"display version", NULL, 0, 0, 0},	
	{ARG_TFLAG,	"--help",	"-h",	"show this message and exit", NULL, 0, 0, 0},
	{ARG_TINPUT,	"--device",	"-d",	"target device '--device=<file name>'", NULL, 0, 0, 0},
	{ARG_TINPUT,	"--dsize",	"-s",	"device size in Megabytes '--dsize=<value>'", NULL, 0, 0, 0},
	{ARG_TINPUT,	"--usize",	"-u",	"unit size in sectors '--usize=<value>'", NULL, 0, 0, 0},
	{ARG_TINPUT,	"--fmap",	"-f",	"file map '--fmap=<file name>'", NULL, 0, 0, 0},
	{ARG_TINPUT,	"--bootS1",	"-a",	"stage 1 boot loader (volume boot record)", NULL, 0, 0, 0},
	{ARG_TINPUT,	"--bootS2",	"-b",	"stage 2 filesystem boot loader", NULL, 0, 0, 0},
	{ARG_TINPUT,	"--src",	"-S",	"source file", NULL, 0, 0, 0},
	{ARG_TINPUT,	"--dst",	"-D",	"destination file", NULL, 0, 0, 0},
	{ARG_TINPUT,	"--serial",	"-n",	"volume serial number", NULL, 0, 0, 0},
	{0,		0,		0,	0, NULL, 0, 0, 0}
};


_action_t _g_action_[]={
	{"create",	_create_fs_,	"create MGFS3 device (--device; --bootS1; --bootS2; --dsize; --usize;)"},
	{"read",	_read_file_,	"read file from MGFS3 \
(--device; --src; --dst; --bootS1; --bootS2)"},
	{"write",	_write_file_,	"write file to MGFS3 \
(--device; --src; --dst; --bootS1; --bootS2; --fmap)"},
	{"delete",	_delete_file_,	"delete file (--device; --dst)"},
	{"move",	_move_file_,	"move file (--device; --src; --dst)"},
	{"mkdir",	_mkdir_,	"create directory (--device; --dst)"},
	{"rmdir",	_rmdir_,	"delete directory (--device; --dst)"},
	{"slink",	_soft_link_,	"create soft link (--device; --src; --dst)"},
	{"hlink",	_hard_link_,	"create hard link (--device; --src; --dst)"},
	{"list",	_list_,		"list directory (--device; --dst)"},
	{"status",	_status_,	"print status of MGFS3 device (--device)"},
	{NULL,		NULL,		NULL}
};

_mgfs_context_t *get_context(void) {
	return &_g_mgfs_context_;
}

_u8 *map_file(_str_t name,_u64 *sz) {
	_u8 *r = NULL;
	int fd = open(name,O_RDWR);
	unsigned long len;
	
	if(fd != -1) {
		len = (_u64)lseek(fd,0,SEEK_END);
		lseek(fd,0,SEEK_SET);
		r = (_u8 *)mmap(NULL,len,PROT_WRITE,MAP_SHARED,fd,0);
		*sz = last_map_size = len;
	}
	
	return r;
}

void print_usage(void) {
	TRACE("mkmgfs3 version: %s\n", MKMGFS_VERSION);
	TRACE("%s\n", "options:");
	_u32 n = 0;
	while(_g_options_[n].opt) {
		TRACE("%10s\t(%s)\t%s\n", _g_options_[n].opt, _g_options_[n].sopt, _g_options_[n].des);
		n++;
	}
	TRACE("%s\n", "commands:");

	n = 0;
	while(_g_action_[n].action) {
		TRACE("%10s\t\t%s\n", _g_action_[n].action, _g_action_[n].description);
		n++;
	}
}

_u64 get_device_size(void) {
	return _g_dev_size_;
}

_u8 open_device(void) {
	_u8 r = 0;
	_str_t dev_name;
	_u32 sz_name=0;

	if(_g_dev_ && _g_dev_size_) {
		TRACE("%s\n", "Device already open !");
		return 1;
		
	}
	if(clargs_option(&_g_clargcxt_,"-d", &dev_name, &sz_name) == CLARG_OK) {
		if(access(dev_name, R_OK|W_OK) == 0) {
			/* open existing device */
			_g_dev_ = map_file(dev_name, &_g_dev_size_);
			if(_g_dev_) {
				/* check for requested device size */
				_str_t str_dev_sz;
				_u32 dev_sz = 0;
				if(clargs_option(&_g_clargcxt_, "-s", &str_dev_sz, &dev_sz) == CLARG_OK) {
					if((_u64)atol(str_dev_sz) != (_g_dev_size_ / (1024*1024))) {
						TRACE("%s\n","ERROR: device size does not matched with requested size");
					} else
						r = 1;
				} else
					r = 1;
			}
		} else {
			if(access(dev_name, F_OK) == 0) {
				TRACE("%s\n", "ERROR: access denied");
			} else {
				/* create new device */
				_str_t str_dev_sz;
				_u32 dev_sz = 0;

				if(clargs_option(&_g_clargcxt_, "-s", &str_dev_sz, &dev_sz) == CLARG_OK) {
					HCONTEXT hc_heap = __g_p_i_repository__->get_context_by_interface(I_HEAP);
					_i_heap_t *pi_heap = HC_INTERFACE(hc_heap);
					_p_data_t pd_heap = HC_DATA(hc_heap);
					
					if(pi_heap && pd_heap) {
						_u8 *p_mb = (_u8 *)pi_heap->alloc(pd_heap, 1024*1024, NO_ALLOC_LIMIT);
						if(p_mb) {
							TRACE("Create device '%s'\n",dev_name);
							_g_pi_str_->mem_set(p_mb, 0, 1024*1024);
							_u32 nmb = atoi(str_dev_sz);
							_u32 i = 0;
							FILE *f = fopen(dev_name,"w+");
							if(f) {
								for(i = 0; i < nmb; i++) {
									fwrite(p_mb,1024*1024,1,f);
									TRACE("%d MB \r", i);
								}

								TRACE("%d MB done.\n", i);
								fclose(f);
							}

							pi_heap->free(pd_heap, p_mb, 1024*1024);
						}

						__g_p_i_repository__->release_context(hc_heap);
					}
				}

				if((_g_dev_ = map_file(dev_name, &_g_dev_size_)))
					r = 1;
			}
		}
	}

	return r;
}

_s32 dev_read(_u64 sector, _u32 count, void *buffer, void _UNUSED_ *udata) {
	_s32 r = -1;
	if(_g_dev_ && _g_dev_size_) {
		_u64 offset = sector * _g_mgfs_context_.sector_size;
		_u32 size = count * _g_mgfs_context_.sector_size;
		if(offset < _g_dev_size_) {
			_g_pi_str_->mem_cpy(buffer, _g_dev_+offset, size);
			r = 0;
		}
	}
	return r;
}

_s32 dev_write(_u64 sector, _u32 count, void *buffer, void _UNUSED_ *udata) {
	_s32 r = -1;
	if(_g_dev_ && _g_dev_size_) {
		_u64 offset = sector * _g_mgfs_context_.sector_size;
		_u32 size = count * _g_mgfs_context_.sector_size;
		if(offset < _g_dev_size_) {
			_g_pi_str_->mem_cpy(_g_dev_+offset, buffer, size);
			r = 0;
		}
	}
	return r;
}

void *mem_alloc(_u32 size, void _UNUSED_ *udata) {
	void *r = NULL;
	if(_g_pi_heap_ && _g_pd_heap_)
		r = _g_pi_heap_->alloc(_g_pd_heap_, size, NO_ALLOC_LIMIT);
	return r;
}

void mem_free(void *ptr, _u32 size, void _UNUSED_ *udata) {
	if(_g_pi_heap_ && _g_pd_heap_)
		_g_pi_heap_->free(_g_pd_heap_, ptr, size);
}

_u32 timestamp(void) {
	return time(NULL);
}

int main(int argc, char *argv[]) {
	int r = -1;
	_core_startup_t csc;
	_vx_core_init_(&csc);

	_g_vx_repo_ = __g_p_i_repository__;

	HCONTEXT hcstr = _g_vx_repo_->get_context_by_interface(I_STR);
	HCONTEXT hcheap= _g_vx_repo_->get_context_by_interface(I_HEAP);
	if((_g_pi_str_ = HC_INTERFACE(hcstr)) && (_g_pi_heap_ = HC_INTERFACE(hcheap)) && (_g_pd_heap_ = HC_DATA(hcheap))) {
		/* CLARG context */
		_g_clargcxt_.p_args = _g_options_;
		_g_clargcxt_.argc = argc;
		_g_clargcxt_.p_argv = argv;
		_g_clargcxt_.strncmp = _g_pi_str_->str_ncmp;
		_g_clargcxt_.strlen = _g_pi_str_->str_len;
		_g_clargcxt_.snprintf = _g_pi_str_->snprintf;

		/* MGFS3 context */
		_g_pi_str_->mem_set(&_g_mgfs_context_, 0, sizeof(_mgfs_context_t));
		_g_mgfs_context_.sector_size = 512;
		_g_mgfs_context_.udata = 0;
		_g_mgfs_context_.read = dev_read;
		_g_mgfs_context_.write= dev_write;
		_g_mgfs_context_.alloc= mem_alloc;
		_g_mgfs_context_.free = mem_free;
		_g_mgfs_context_.strlen = _g_pi_str_->str_len;
		_g_mgfs_context_.strcmp = _g_pi_str_->str_cmp;
		_g_mgfs_context_.memcpy = _g_pi_str_->mem_cpy;
		_g_mgfs_context_.memset = _g_pi_str_->mem_set;
		_g_mgfs_context_.timestamp = timestamp;

		if(clargs_parse(&_g_clargcxt_) == CLARG_OK) {
			_str_t arg;
			_u32 sz = 0;
			
			if(clargs_option(&_g_clargcxt_, "-h", &arg, &sz) == CLARG_OK || argc < 2) 
				print_usage();
			else {
				if(clargs_option(&_g_clargcxt_, "-v", &arg, &sz)==CLARG_OK) 
					TRACE("mkmgfs3 version: %s\n", MKMGFS_VERSION);
				
				if(clargs_option(&_g_clargcxt_, "-V", &arg, &sz)==CLARG_OK) 
					_g_verbose_ = 1;

				if(open_device()) {
					if(clargs_option(&_g_clargcxt_, "-f", &arg, &sz) == CLARG_OK)
						/* read from command file */
						parse_file_map();
					else {
						_str_t str_action = clargs_parameter(&_g_clargcxt_, 1);
						if(str_action) {
							_u32 n = 0;
							while(_g_action_[n].action) {
								if(_g_pi_str_->str_cmp((_str_t)_g_action_[n].action, 
										str_action) == 0) {
									_g_action_[n].p_action();
									break;
								}

								n++;
							}

							if(!_g_action_[n].action) {
								TRACE("ERROR: Unknown command '%s'\n", str_action);
							}
						}
					}
					/* unmap device */
					munmap(_g_dev_, _g_dev_size_);
					_g_dev_ = NULL;
					_g_dev_size_ = 0;
				} else {
					TRACE("%s\n", "ERROR: can't open device !");
				}
			}
			r = 0;
		} else
			TRACE("%s", _g_clargcxt_.err_text);
	}

	return r;
}
