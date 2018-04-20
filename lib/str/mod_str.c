#include "vxmod.h"
#include "i_str.h"
#include "str.h"
#include "err.h"

static _i_str_t _g_interface_ = {
	.mem_cpy	= _mem_cpy,
	.mem_cmp	= _mem_cmp,
	.mem_set	= _mem_set,
	.str_len	= _str_len,
	.str_cmp	= _str_cmp,
	.str_ncmp	= _str_ncmp,
	.str_cpy	= _str_cpy,
	.vsnprintf	= _vsnprintf,
	.snprintf	= _snprintf,
	.find_string	= _find_string,
	.toupper	= _toupper,
	.str2i		= _str2i,
	.trim_left	= _trim_left,
	.trim_right	= _trim_right,
	.clrspc		= _clrspc,
	.div_str	= _div_str,
	.div_str_ex	= _div_str_ex,
	.wildcmp	= _wildcmp,
	.itoa		= _itoa,
	.uitoa		= _uitoa,
	.ulltoa		= _ulltoa
};

static _vx_res_t _mod_ctl_(_u32 cmd, ...) {
	_vx_res_t r = VX_UNSUPPORTED_COMMAND;
	switch(cmd) {
		case MODCTL_INIT_CONTEXT:
		case MODCTL_DESTROY_CONTEXT:
			r = VX_OK;
	}
	return r;
}

DEF_VXMOD(
	MOD_STR,		/* module name */
	I_STR,			/* interface name */
	&_g_interface_,		/* interface pointer */
	NULL,			/* static data context */
	0,			/* size of data context (for dynamic allocation) */
	_mod_ctl_,		/* pointer to controll routine */
	1,0,1,			/* version */
	"string operations"	/* description */
);
