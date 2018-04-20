#ifndef __CLARGS__
#define __CLARGS__

#include "mgtype.h"

#define ARG_TFLAG	1
#define ARG_TINPUT	(1<<1)

/* error code */
#define CLARG_OK	0
#define CLARG_ERR	(CLARG_OK-1)

typedef struct {
	_u8		type;   /* option type */
	_cstr_t		opt;	/* option */
	_cstr_t		sopt;	/* short option */
	_cstr_t		des;	/* description */
	_str_t		arg;	/* argument */
	_u32		sz_arg; /* argument size */
	_u8		active; /* option present flag */
	_u32		idx;	/* argv index */
} _clarg_t;

typedef struct {
	_clarg_t	*p_args;
	_s8		err_text[256];
	_str_t		*p_argv;
	_u32		argc;
	/* string operations */
	_s32 (*strncmp)(_str_t, _str_t, _u32);
	_u32 (*strlen)(_str_t);
	_u32 (*snprintf)(_str_t buffer, _u32 len, _cstr_t fmt, ...);
} _clarg_context_t;

_s32 clargs_parse(_clarg_context_t *p_cxt);
_s32 clargs_option(_clarg_context_t *p_cxt, _cstr_t opt, _str_t *p_arg, _u32 *sz);
_str_t clargs_parameter(_clarg_context_t *p_cxt, _u32 index);

#endif

