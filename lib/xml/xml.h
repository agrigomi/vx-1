#ifndef __XML_H__
#define __XML_H__

#include "mgtype.h"

#define	INITIAL_ITEMS	16

typedef struct xml_tag	_xml_tag_t;

struct xml_tag {
	_str_t		name;		/* tag name */
	_u8		sz_name;	/* size of tag name */
	_str_t		content;	/* tag content */
	_u32		sz_content;	/* size of tag content */
	_str_t		parameters;	/* tag parameters */
	_u16		sz_parameters;	/* size of tag parameters */
	_xml_tag_t	*p_childs;	/* list of child tags */
	_u32		nchilds;	/* number of child tags in list */
};

typedef struct {
	_xml_tag_t	*p_root; /* xml root tags */
	_u32		nroot;   /* number of root items */
	_str_t		content; /* xml content */
	_u32		size;	 /* size of xml content */
	/* string operations */
	_u32 (*strlen)(_str_t);
	/* memory operations */
	void (*memcpy)(void *, void *, _u32);
	_s32 (*memcmp)(void *, void *, _u32);
	void (*memset)(void *, _u8, _u32);
	void *(*alloc)(_u32);
	void (*free)(void *, _u32);
}_xml_context_t;

void xml_init_context(_xml_context_t *p_xc,
			_str_t content,
			_u32 sz_content,
			_u32 (*strlen)(_str_t),
			void (*memcpy)(void *, void *, _u32),
			_s32 (*memcmp)(void *, void *, _u32),
			void (*memset)(void *, _u8, _u32),
			void *(*alloc)(_u32),
			void (*free)(void *, _u32)
			);
_str_t xml_parse(_xml_context_t *p_xc);
_xml_tag_t *xml_select(_xml_context_t *p_xc, _cstr_t xpath, _xml_tag_t *p_tag, _u32 index);
void xml_clear(_xml_context_t *p_xc);
_str_t xml_parameter(_xml_context_t *p_xc, _xml_tag_t *p_tag, _cstr_t name, _u32 *psz);
_u32 xml_parameter_copy(_xml_context_t *p_xc, _xml_tag_t *p_tag, _cstr_t name, _str_t buffer, _u32 sz_buffer);

#endif

