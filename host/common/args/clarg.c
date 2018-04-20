#include "clarg.h"

#define BSOPT	(1<<0)
#define BLOPT	(1<<1)

static _s32 find_option(_clarg_context_t *p_cxt, _str_t opt, _u8 *pf) {
	_s32 r = -1;
	_u32 n = 0;
	
	if(p_cxt->p_args && p_cxt->strncmp && p_cxt->strlen && pf) {
		n = 0;
		while(p_cxt->p_args[n].type != 0 || p_cxt->p_args[n].opt != 0 || p_cxt->p_args[n].sopt != 0) {
			*pf |= (p_cxt->p_args[n].opt && 
					p_cxt->strncmp(opt, (_str_t)p_cxt->p_args[n].opt, 
						p_cxt->strlen((_str_t)p_cxt->p_args[n].opt)) == 0) 
				? BLOPT : 0;
			*pf |= (p_cxt->p_args[n].sopt && 
					p_cxt->strncmp(opt, (_str_t)p_cxt->p_args[n].sopt, 
						p_cxt->strlen((_str_t)p_cxt->p_args[n].sopt)) == 0) 
				? BSOPT : 0;

			if((*pf & BSOPT) || (*pf & BLOPT)) {
				r = n;
				break;
			}

			n++;
		}
	}

	return r;
}

void clargs_reset(_clarg_context_t *p_cxt) {
	_u32 n = 0;
	
	if(p_cxt->p_args) {
		n = 0;
		while(p_cxt->p_args[n].type) {
			p_cxt->p_args[n].arg = 0;
			n++;
		}
	}
}

_s32 clargs_parse(_clarg_context_t *p_cxt) {
	_s32 r = CLARG_OK;
	_s32 n = 0;
	_u32 i = 0;
	
	clargs_reset(p_cxt);
	
	for(i = 1; i < p_cxt->argc; i++) {
		if(p_cxt->p_argv[i][0] == '-') {
			_u8 f = 0;
			n = find_option(p_cxt, p_cxt->p_argv[i], &f);
			if( n != -1 ) {
				/* option found */
				if(p_cxt->p_args[n].type & ARG_TFLAG) {
					p_cxt->p_args[n].active = 1;
					p_cxt->p_args[n].idx = i;
				}
				
				if(p_cxt->p_args[n].type & ARG_TINPUT) {
					if(f & BLOPT) {
						if(p_cxt->strlen(p_cxt->p_argv[i]) > p_cxt->strlen((_str_t)p_cxt->p_args[n].opt))
							p_cxt->p_args[n].arg = p_cxt->p_argv[i] + p_cxt->strlen((_str_t)p_cxt->p_args[n].opt);
					}

					if(f & BSOPT) {
						if(p_cxt->strlen(p_cxt->p_argv[i]) > p_cxt->strlen((_str_t)p_cxt->p_args[n].sopt))
							p_cxt->p_args[n].arg = p_cxt->p_argv[i] + p_cxt->strlen((_str_t)p_cxt->p_args[n].sopt);
					}
					
					if(p_cxt->p_args[n].arg) {
						if(*(p_cxt->p_args[n].arg) == '=' || *(p_cxt->p_args[n].arg) == ':')
							p_cxt->p_args[n].arg += 1;
						else {
							p_cxt->snprintf(p_cxt->err_text, sizeof(p_cxt->err_text), 
									(_str_t)"Invalid option '%s'\n", p_cxt->p_argv[i]);
							r = CLARG_ERR;
							break;
						}
						
						p_cxt->p_args[n].active = 1;
						p_cxt->p_args[n].idx = i;
						p_cxt->p_args[n].sz_arg = p_cxt->strlen(p_cxt->p_args[n].arg);
					} else {
						p_cxt->snprintf(p_cxt->err_text, sizeof(p_cxt->err_text), 
							(_str_t)"Required argument for option '%s'\n", p_cxt->p_argv[i]);
						r = CLARG_ERR;
						break;
					}
				}
			} else {
				p_cxt->snprintf(p_cxt->err_text, sizeof(p_cxt->err_text), 
					(_str_t)"Unrecognized option '%s'\n", p_cxt->p_argv[i]);
				r = CLARG_ERR;
				break;
			}
		}
	}
	
	return r;
}

_s32 clargs_option(_clarg_context_t *p_cxt, _cstr_t opt, _str_t *p_arg, _u32 *sz) {
	_s32 r = CLARG_ERR;
	_u8 f = 0;
	_s32 n = find_option(p_cxt, (_str_t)opt, &f);
	
	if(n != -1) {
		if(p_cxt->p_args[n].active) {
			if(p_cxt->p_args[n].type & ARG_TINPUT) {
				if(p_arg)
					*p_arg = p_cxt->p_args[n].arg;
				
				if(sz)
					*sz = p_cxt->p_args[n].sz_arg;
			}
			
			r = CLARG_OK;
		}
	}
	
	return r;
}

_str_t clargs_parameter(_clarg_context_t *p_cxt, _u32 index) {
	_str_t r = 0;
	_u32 idx = 0;
	_u32 i = 0;

	for(i = 0; i < p_cxt->argc; i++) {
		if(p_cxt->p_argv[i][0] != '-') {
			if(idx == index) {
				r = p_cxt->p_argv[i];
				break;
			}

			idx++;
		}
	}
	
	return r;
}


