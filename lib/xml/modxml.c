#include "i_xml.h"
#include "err.h"
#include "xml.h"
#include "i_memory.h"
#include "i_str.h"
#include "i_repository.h"

static HCONTEXT	ghc_heap = NULL;
static _i_str_t *gpi_str = NULL;

static void _xml_init(_p_data_t dc, _str_t content, _u32 sz) {
	_xml_context_t *pdc = (_xml_context_t *)dc;
	if(pdc) {
		pdc->content = content;
		pdc->size = sz;
		pdc->p_root = NULL;
		pdc->nroot = 0;
	}
}

static _str_t _xml_parse(_p_data_t dc) {
	_str_t r = NULL;
	_xml_context_t *pdc = (_xml_context_t *)dc;
	if(pdc)
		r = xml_parse(pdc);
	return r;
}

static HXMLTAG _xml_select(_p_data_t dc, _cstr_t xpath, HXMLTAG htag, _u32 index) {
	HXMLTAG r = NULL;
	_xml_context_t *pdc = (_xml_context_t *)dc;
	if(pdc)
		r = xml_select(pdc, xpath, (_xml_tag_t *)htag, index);
	return r;
}

static _str_t _xml_parameter(_p_data_t dc, HXMLTAG htag, _cstr_t name, _u32 *psz) {
	_str_t r = NULL;
	_xml_context_t *pdc = (_xml_context_t *)dc;
	if(pdc)
		r = xml_parameter(pdc, (_xml_tag_t *)htag, name, psz);
	return r;
}

static _u32 _xml_parameter_copy(_p_data_t dc, HXMLTAG htag, _cstr_t name, _str_t buffer, _u32 sz) {
	_u32 r = 0;
	_xml_context_t *pdc = (_xml_context_t *)dc;
	if(pdc)
		r = xml_parameter_copy(pdc, (_xml_tag_t *)htag, name, buffer, sz);
	return r;
}

static _i_xml_t _g_interface_={
	.init     	= _xml_init,
	.parse    	= _xml_parse,
	.select   	= _xml_select,
	.parameter	= _xml_parameter,
	.parameter_copy = _xml_parameter_copy
};

static void *heap_alloc(_u32 size) {
	void *r = NULL;
	if(ghc_heap) {
		_i_heap_t *pi = HC_INTERFACE(ghc_heap);
		_p_data_t *pd = HC_DATA(ghc_heap);
		if(pi && pd)
			r = pi->alloc(pd, size, NO_ALLOC_LIMIT);
	}
	return r;
}

static void heap_free(void *ptr, _u32 size) {
	if(ghc_heap) {
		_i_heap_t *pi = HC_INTERFACE(ghc_heap);
		_p_data_t *pd = HC_DATA(ghc_heap);
		if(pi && pd)
			pi->free(pd, ptr, size);
	}
}

static _vx_res_t _mod_ctl_(_u32 cmd, ...) {
	_vx_res_t r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);
	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
				_i_repository_t *p_repo = va_arg(args, _i_repository_t*);
				_xml_context_t *p_cdata = va_arg(args, _xml_context_t*);

				if(!gpi_str) {
					HCONTEXT hc_str = p_repo->get_context_by_interface(I_STR);
					if(hc_str)
						gpi_str = HC_INTERFACE(hc_str);
				}
				if(!ghc_heap)
					ghc_heap = p_repo->get_context_by_interface(I_HEAP);
				if(p_cdata && gpi_str)
					xml_init_context(p_cdata, NULL, 0, 
							gpi_str->str_len, gpi_str->mem_cpy,
							gpi_str->mem_cmp, gpi_str->mem_set,
							heap_alloc, heap_free);
				r = VX_OK;
			} break;
		case MODCTL_DESTROY_CONTEXT: {
				_i_repository_t _UNUSED_ *p_repo = va_arg(args, _i_repository_t*);
				_xml_context_t *p_cdata = va_arg(args, _xml_context_t*);
				if(p_cdata)
					xml_clear(p_cdata);
				r = VX_OK;
			} break;
	}
	va_end(args);
	return r;
}

DEF_VXMOD(
	MOD_XML,		/* module name */
	I_XML,			/* interface name */
	&_g_interface_,		/* interface pointer */
	NULL,			/* static data context */
	sizeof(_xml_context_t),	/* size of data context (for dynamic allocation) */
	_mod_ctl_,		/* pointer to controll routine */
	1,0,1,			/* version */
	"XML operations"	/* description */
);
