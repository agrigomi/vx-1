#include "vxdev.h"
#include "i_tty.h"
#include "i_repository.h"
#include "i_dev_root.h"
#include "i_str.h"
#include "i_sync.h"
#include "addr.h"
#include "err.h"

#define VIDEO_BASE 	(_u8 *)0xb8000
#define FMT_BUFFER_SIZE	1024

#define DEFAULT_SLOT	8

static _u8 _g_max_row_ = 24;
static _u8 _g_max_col_ = 80;
static _u8 _g_row_ = 0;
static _u8 _g_col_ = 0;
static _u8 _g_color_ = 7;
static HDEV _g_hdev_ = NULL;
static _i_str_t	*_g_pi_str_ = NULL;
static HCONTEXT	_g_hc_mutex_ = NULL;

void disp_init(_u8 max_row, _u8 max_col) {
	_g_max_row_ = max_row;
	_g_max_col_ = max_col;
	_g_row_ = _g_col_ = 0;
}

static HMUTEX disp_lock(HMUTEX hlock) {
	HMUTEX r = 0;

	if(_g_hc_mutex_) {
		_i_mutex_t *pi = HC_INTERFACE(_g_hc_mutex_);
		if(pi)
			r = pi->lock(HC_DATA(_g_hc_mutex_), hlock);
	}

	return r;
}

static void disp_unlock(HMUTEX hlock) {
	if(_g_hc_mutex_) {
		_i_mutex_t *pi = HC_INTERFACE(_g_hc_mutex_);
		if(pi)
			pi->unlock(HC_DATA(_g_hc_mutex_), hlock);
	}
}

static void _disp_position(_u8 row, _u8 col) {
	_g_row_ = (row <= _g_max_row_)?row:_g_max_row_;
	_g_col_ = (col <= _g_max_col_)?col:_g_max_col_;
}
static void disp_position(_u8 row, _u8 col) {
	HMUTEX hm = disp_lock(0);
	_disp_position(row, col);
	disp_unlock(hm);
}

static _u8 disp_color(_u8 nc) {
	_u8 r = _g_color_;
	HMUTEX hm = disp_lock(0);
	_g_color_ = nc;
	disp_unlock(hm);
	return r;
}

static void _disp_scroll(void) {
	_u8 *base = (_u8 *)p_to_k(VIDEO_BASE);
	_u8 *src = (_u8 *)(base + (_g_max_col_  * 2));
	_u8 *dst = (_u8 *)base;
	_u32 sz = (((_g_max_row_ + 1) * _g_max_col_) * 2);

	/* scroll video buffer */
	_g_pi_str_->mem_cpy(dst,src,sz);
}
static void disp_scroll(void) {
	HMUTEX hm = disp_lock(0);
	_disp_scroll();
	disp_unlock(hm);
}

static void _disp_clear(void) {
	_u8 *base = (_u8 *)p_to_k(VIDEO_BASE);
	_u16 *ptr = (_u16 *)base;
	_u32 sz = _g_max_row_ * _g_max_col_;
	_u16 pattern = _g_color_;

	pattern = (pattern << 8)|0x20;

	while(sz) {
		*ptr++ = pattern;
		sz--;
	}

	_g_row_ = _g_col_ = 0;
}

static void disp_clear(void) {
	HMUTEX hm = disp_lock(0);
	_disp_clear();
	disp_unlock(hm);
}

static _u32 disp_write(_p_data_t buffer, _u32 size) {
	_u32 r = 0;
	_u32 _sz = size;
	_u8 *p_buffer = buffer;
	_u8 *base = (_u8 *)p_to_k(VIDEO_BASE);

	HMUTEX hm = disp_lock(0);
	while(_sz) {
		if(*(p_buffer + r) == 0x0d) {
			_g_col_ = 0;
		} else {
			if(*(p_buffer + r) == 0x0a) {
				_g_col_ = 0;
				_g_row_++;
				if(_g_row_ > _g_max_row_) {
					_disp_scroll();
					_g_row_ = _g_max_row_;
				}
			} else {
				_u8 *vptr = (_u8 *)(base + (((_g_row_ * _g_max_col_) + _g_col_) * 2));
				if(*(p_buffer + r) != '\t') {
					*vptr = *(p_buffer + r);
					*(vptr + 1) = _g_color_;
				}
				_g_col_++;
				if(_g_col_ >= _g_max_col_) {
					_g_col_ = 0;
					_g_row_++;
					if(_g_row_ > _g_max_row_) {
						_disp_scroll();
						_g_row_ = _g_max_row_;
					}
				}
			}
		}

		_sz--;
		r++;
	}
	disp_unlock(hm);

	return r;
}

static _u32 disp_vfwrite(_cstr_t fmt, va_list args) {
	_s8 lb[FMT_BUFFER_SIZE];
	_u32 sz = _g_pi_str_->vsnprintf(lb, FMT_BUFFER_SIZE, fmt, args);
	return disp_write((_u8 *)lb, sz);
}

static _u32 disp_fwrite(_cstr_t fmt, ...) {
	_u32 r = 0;
	va_list args;
	va_start(args, fmt);
	r = disp_vfwrite(fmt, args);
	va_end(args);
	return r;
}

static _i_txt_disp_t _g_interface_ = {
	.init		= disp_init,
	.position	= disp_position,
	.color		= disp_color,
	.scroll		= disp_scroll,
	.clear		= disp_clear,
	.write		= disp_write,
	.fwrite		= disp_fwrite,
	.vfwrite	= disp_vfwrite,
};

_vx_res_t _disp_ctl_(_u32 cmd, ...) {
	_u32 r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);
	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
				_i_repository_t *pi_repo = va_arg(args, _i_repository_t*);
				if(pi_repo) {
					if(!_g_hc_mutex_)
						_g_hc_mutex_ = pi_repo->create_context_by_interface(I_MUTEX);

					if(!_g_pi_str_) {
						HCONTEXT hc_str = pi_repo->get_context_by_interface(I_STR);
						if(hc_str)
							_g_pi_str_ = HC_INTERFACE(hc_str);
					}

					if(!_g_hdev_) {
						/* create device */
						HCONTEXT hc_droot = pi_repo->get_context_by_interface(I_DEV_ROOT);
						if(hc_droot) {
							_i_dev_root_t *pi_droot = HC_INTERFACE(hc_droot);
							if(pi_droot)
								_g_hdev_ = pi_droot->create(_disp_ctl_, NULL, NULL);
							pi_repo->release_context(hc_droot);
						}
					}
				}
				r = VX_OK;
			} break;
		case MODCTL_DESTROY_CONTEXT: {
				_i_repository_t *pi_repo = va_arg(args, _i_repository_t*);
				if(pi_repo && _g_hc_mutex_) {
					pi_repo->release_context(_g_hc_mutex_);
					_g_hc_mutex_ = NULL;
				}
				r = VX_OK;
			} break;
		case DEVCTL_INIT: {
				_i_dev_root_t *pi_droot = va_arg(args, _i_dev_root_t*);
				_vx_dev_t *p_dev = va_arg(args, _vx_dev_t*);
				if(p_dev) {
					p_dev->_d_type_ = DTYPE_DEV;
					p_dev->_d_mode_ = DMODE_CHAR;
					p_dev->_d_class_ = DCLASS_DISPLAY;
					p_dev->_d_slot_ = DEFAULT_SLOT;
					_g_pi_str_->str_cpy(p_dev->_d_ident_, (_str_t)"x86_txt_disp", sizeof(p_dev->_d_ident_));
					/* get device handle for dev_root */
					p_dev->_d_host_ = pi_droot->get_hdev();
					r = VX_OK;
				} else
					r = VX_ERR;
			} break;
		case DEVCTL_UNINIT:
			r = VX_OK;
			break;
		case DEVCTL_WRITE: {
				_vx_dev_t _UNUSED_ *p_dev = va_arg(args, _vx_dev_t*);
				_dev_io_t *p_io = va_arg(args, _dev_io_t*);

				p_io->result = disp_write(p_io->buffer, p_io->size);
				r = VX_OK;
			} break;
		case DEVCTL_GET_CONFIG: {
				_vx_dev_t _UNUSED_ *p_dev = va_arg(args, _vx_dev_t*);
				_p_data_t _UNUSED_ *ptr = va_arg(args, _p_data_t);
				/* ... */
				r = VX_OK;
			} break;
		case DEVCTL_SET_CONFIG: {
				_vx_dev_t _UNUSED_ *p_dev = va_arg(args, _vx_dev_t*);
				_p_data_t _UNUSED_ ptr = va_arg(args, _p_data_t);
				/* ... */
				r = VX_OK;
			} break;
	}
	va_end(args);

	return r;
}

DEF_VXMOD(
	MOD_TXT_DISP,		/* module name */
	I_TXT_DISP,		/* interface name */
	&_g_interface_,		/* interface pointer */
	NULL,			/* static data context */
	0,			/* size of data context (for dynamic allocation) */
	_disp_ctl_,		/* pointer to controll routine */
	1,0,1,			/* version */
	"x86 text display"	/* description */
);

