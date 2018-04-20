/* Text display emulation */
#include "vxmod.h"
#include "vxdev.h"
#include "i_repository.h"
#include "i_dev_root.h"
#include "i_sync.h"
#include "i_tty.h"
#include "i_str.h"
#include "err.h"

#include "stdio.h"

#define FMT_BUFFER_SIZE	1024
#define DEFAULT_SLOT	8

static HDEV _g_hdev_ = NULL;
static _i_str_t	*_g_pi_str_ = NULL;
static HCONTEXT	_g_hc_mutex_ = NULL;
static _u8 _g_color_ = 7;

static void disp_init(_u8 _UNUSED_ max_row, _u8 _UNUSED_ max_col) {
}

static void disp_position(_u8 _UNUSED_ row, _u8 _UNUSED_ col) {
}

static _u8 disp_color(_u8 clr) {
	_u8 r = _g_color_;
	_g_color_ = clr;
	printf("\x1B[%dm", clr+30);
	fflush(stdout);
	return r;
}

static void disp_scroll(void) {
	printf("\n");
}

static void disp_clear(void) {
}

static _u32 disp_write(_p_data_t buffer, _u32 _UNUSED_ size) {
	return printf("%s", (char *)buffer);
}

static _u32 disp_vfwrite(_cstr_t fmt, va_list args) {
	return vprintf(fmt, args);
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
	.vfwrite	= disp_vfwrite
};

static _vx_res_t _disp_ctl_(_u32 cmd, ...) {
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
					_g_pi_str_->str_cpy(p_dev->_d_ident_, (_str_t)"txt_disp", sizeof(p_dev->_d_ident_));
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
				_ulong _UNUSED_ offset = va_arg(args, _ulong);
				_u32 nb_write = va_arg(args, _u32);
				_p_data_t buffer = va_arg(args, _p_data_t);
				_u32 *res = va_arg(args, _u32*);

				*res = disp_write(buffer, nb_write);
				r = VX_OK;
			} break;
		case DEVCTL_GET_CONFIG: {
				_vx_dev_t _UNUSED_ *p_dev = va_arg(args, _vx_dev_t*);
				_p_data_t _UNUSED_ *ptr = va_arg(args, _p_data_t*);
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
	"text display emulation"/* description */
);

