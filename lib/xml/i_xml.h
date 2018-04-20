#ifndef __I_XML_H__
#define __I_XML_H__

#include "vxmod.h"

#define I_XML		"i_xml"

#define HXMLTAG		_p_data_t

typedef struct {
	void (*init)(_p_data_t dc, _str_t content, _u32 size);
	_str_t (*parse)(_p_data_t dc);
	HXMLTAG (*select)(_p_data_t dc, _cstr_t xpath, HXMLTAG htag, _u32 index);
	_str_t (*parameter)(_p_data_t dc, HXMLTAG htag, _cstr_t name, _u32 *psz);
	_u32 (*parameter_copy)(_p_data_t dc, HXMLTAG htag, _cstr_t name, _str_t buffer, _u32 sz);
}_i_xml_t;

#endif

