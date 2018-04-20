#include "xml.h"

union _parser_status {
	_u8	 	sw;
	struct {
		_u8	_bo:1; /* < */
		_u8	_to:1; /* <xxx> */
		_u8	_tc:1; /* /> or </ */
		_u8	_tp:1; /* <nnn xxx=xxx */
		_u8	_tn:1; /* <nnn */
	} f;
};

typedef union _parser_status _parser_status_t;

void xml_init_context(_xml_context_t *p_xc,
			_str_t content,
			_u32 sz_content,
			_u32 (*strlen)(_str_t),
			void (*memcpy)(void *, void *, _u32),
			_s32 (*memcmp)(void *, void *, _u32),
			void (*memset)(void *, _u8, _u32),
			void *(*alloc)(_u32),
			void (*free)(void *, _u32)
			) {
	if(memset)
		memset(p_xc, 0, sizeof(_xml_context_t));

	p_xc->content = content;
	p_xc->size = sz_content;
	p_xc->strlen = strlen;
	p_xc->memcpy = memcpy;
	p_xc->memcmp = memcmp;
	p_xc->memset = memset;
	p_xc->alloc  = alloc;
	p_xc->free   = free;

	/* alloc root tags */
	if(alloc) {
		if((p_xc->p_root = (_xml_tag_t *)alloc(INITIAL_ITEMS * sizeof(_xml_tag_t)))) {
			if(memset)
				memset(p_xc->p_root, 0, INITIAL_ITEMS * sizeof(_xml_tag_t));
			p_xc->nroot = INITIAL_ITEMS;
		}
	}
}

static void add_child_tag(_xml_context_t *p_xc, /* xml context */
			_xml_tag_t *p_tag, /* current tag (NULL for root) */
			_xml_tag_t *p_child /* new child tag */
			) {
	_xml_tag_t *p_array = (p_tag)?p_tag->p_childs:p_xc->p_root;
	_u32 narray = (p_tag)?p_tag->nchilds:p_xc->nroot;
	_xml_tag_t *p_slot = NULL;
	_u32 i = 0;

_add_child_tag_:
	/* find place in parent tag */
	if(p_array) {
		for(i = 0; i < narray; i++) {
			if(p_array[i].name == NULL) {
				p_slot = &p_array[i];
				break;
			}
		}
	} else {
		p_array = (_xml_tag_t *)p_xc->alloc(INITIAL_ITEMS * sizeof(_xml_tag_t));
		p_xc->memset(p_array, 0, INITIAL_ITEMS * sizeof(_xml_tag_t));
		p_slot = &p_array[0];
		narray = INITIAL_ITEMS;
	}

	if(p_slot)
		/* copy child tag */
		p_xc->memcpy(p_slot, p_child, sizeof(_xml_tag_t));
	else {
		/* reallocate childs array */
		_u32 _narray = narray + INITIAL_ITEMS;
		_xml_tag_t *_p_array = (_xml_tag_t *)p_xc->alloc(_narray * sizeof(_xml_tag_t));
		if(_p_array) {
			/* copy old list */
			p_xc->memset(_p_array, 0, _narray * sizeof(_xml_tag_t));
			p_xc->memcpy(_p_array, p_array, narray * sizeof(_xml_tag_t));
			narray = _narray;
			p_array = _p_array;

			if(p_tag) {
				p_xc->free(p_tag->p_childs, p_tag->nchilds * sizeof(_xml_tag_t));
				p_tag->p_childs = p_array;
				p_tag->nchilds = narray;
			} else { /* root */
				p_xc->free(p_xc->p_root, p_xc->nroot * sizeof(_xml_tag_t));
				p_xc->p_root = p_array;
				p_xc->nroot = narray;
			}

			goto _add_child_tag_;
		}
	}
}

static _xml_tag_t *get_child_tag(_xml_context_t *p_xc, _xml_tag_t *p_tag, _u32 index) {
	_xml_tag_t *r = NULL;

	if(p_tag) {
		if(index < p_tag->nchilds)
			r = &p_tag->p_childs[index];
	} else {
		if(index < p_xc->nroot)
			r = &p_xc->p_root[index];
	}

	return r;
}

static _str_t __parse(_xml_context_t *p_xc, _str_t p, _xml_tag_t *p_tag) {
	_str_t _p = p;
	_xml_tag_t _t;

	p_xc->memset(&_t, 0, sizeof(_xml_tag_t));
	/*
		Parse the packages fenced with "<! ... !> or <? ... ?>"
	*/
	switch(*_p) {
		case '?': {
			if(*(_p - 1) == '<') {
				/**** Get the package name  "<?name ... ?>" ****/
				_t.name = _p + 1;
				do {
					_p++;
				} while(*_p != ' ');

				_t.sz_name = (_u32)(_p - _t.name) ;
				/***********************************************/

				/* Get the package parameters  */
				_t.parameters =  _t.content = _p;
				do {
					_p++;
				}while(p_xc->memcmp((_u8 *)_p,(_u8 *)"?>",2) != 0);

				_t.sz_parameters = _t.sz_content = (_u32)(_p - _t.parameters) ;
				/********************
				Attention:
					In this case the package parameters and tag content are the same
				*/

				_p += 1;
				add_child_tag(p_xc, p_tag, &_t);
			}
		} break;

		case '!': {
			if(*(_p - 1) == '<') {
				if(p_xc->memcmp((_u8 *)_p,(_u8 *)"!--",3) == 0) {
					/* XML comment */
					_t.content = _p + 1;
					do { /* untill end of comment package */
						_p++;
					} while(p_xc->memcmp((_u8 *)_p,(_u8 *)"-->",3) != 0);

					_p += 2;

					_t.sz_content = (_u32)(_p - p) - 1;
					add_child_tag(p_xc, p_tag, &_t);
				} else {
					while(*_p != '>')
						_p++;

					_t.sz_parameters = (_u32)(_p - p) - 1;
					add_child_tag(p_xc, p_tag, &_t);
				}
			}
		}break;
	}

	return _p;
}

static _str_t _parse(_xml_context_t *p_xc, _str_t p, _u8 status, _xml_tag_t *p_tag) {
	_str_t r = NULL;
	_parser_status_t _s;
	_parser_status_t __s;
	_xml_tag_t _t;
	_str_t _p = p;
	_s.sw = 0;
	__s.sw = status;

	p_xc->memset(&_t, 0, sizeof(_xml_tag_t));

	while((_u32)(_p - p_xc->content) < p_xc->size) {
		switch(*_p) {
			case '<':
				_s.f._bo = 1;
				_s.f._tp = 0;

				if(*(_p+1) != '/') {
					if(_s.f._to) {
						_str_t __p = _parse(p_xc, _p, _s.sw, &_t);
						_t.sz_content += (_u32)(__p - _p);
						_p = __p;
					} else {
						_t.name = _p + 1;
						_t.sz_name = 0;
						_s.f._tn = 1;
					}
				}

				break;

			case '>':
				_s.f._bo = 0;
				_s.f._tn = 0;
				_s.f._tp = 0;

				if(_s.f._tc) {
					_s.f._tc = 0;
					_s.f._to = 0;

					add_child_tag(p_xc, p_tag, &_t);
					return _p;
				} else {
					_s.f._to = 1;
					_s.f._tc = 0;
					_t.sz_content = 0;
					_t.content = _p+1;
				}

				break;

			case '/':
				if(_s.f._bo) {
					if(*(_p+1) == '>' || *(_p-1) == '<') {
						_s.f._tn = 0;
						_s.f._tp = 0;
						_s.f._tc = 1;

						_s.f._to = 0;
					}

					if(_s.f._tp)
						_t.sz_parameters++;
				}

				if(_s.f._to)
					_t.sz_content++;

				break;

			case ' ':
				if(_s.f._bo && !_s.f._to) {
					if(_s.f._tp)
						_t.sz_parameters++;

					if(_s.f._tn) {
						_s.f._tn = 0;
						_s.f._tp = 1;
						_t.parameters = _p + 1;
						_t.sz_parameters = 0;
					}
				} else {
					if(_s.f._to)
						_t.sz_content++;
				}

				break;

			case '?':
			case '!':
				if(_s.f._bo && *(_p - 1) == '<') {
					_str_t __p = __parse(p_xc, _p, p_tag);
					_t.sz_content += (_u32)(__p - _p);
					_p = __p;
					if(__s.f._to)
						return _p;
				}

				break;

			default:
				if(_s.f._to)
					_t.sz_content++;

				if(_s.f._tn)
					_t.sz_name++;

				if(_s.f._tp)
					_t.sz_parameters++;

				break;
		}

		_p++;
	}

	return r;
}

_str_t xml_parse(_xml_context_t *p_xc) {
	_str_t r = NULL;
	_parser_status_t status;

	status.sw = 0;
	if(p_xc->content)
		r = _parse(p_xc, p_xc->content, status.sw, NULL);

	return r;
}

static void clear_tag(_xml_context_t *p_xc, _xml_tag_t *p_tag) {
	if(p_tag->p_childs && p_tag->nchilds) {
		_u32 i = 0;
		for(; i < p_tag->nchilds; i++)
			clear_tag(p_xc, &(p_tag->p_childs[i]));

		p_xc->free(p_tag->p_childs, p_tag->nchilds * sizeof(_xml_tag_t));
		p_tag->nchilds = 0;
		p_tag->p_childs = NULL;
	}
}

void xml_clear(_xml_context_t *p_xc) {
	_u32 i = 0;

	if(p_xc->p_root && p_xc->nroot) {
		for(; i < p_xc->nroot; i++)
			clear_tag(p_xc, &p_xc->p_root[i]);
	}
}

_xml_tag_t *xml_select(_xml_context_t *p_xc, _cstr_t xpath, _xml_tag_t *p_tag, _u32 index) {
	_xml_tag_t *r = NULL;
	_xml_tag_t *_t = NULL;
	_u32 i = 0, _i = 0, j = 0;
	_u32 _sz = 0;
	_s8 _lb[256];
	_u32 ix = 0;
	_u32 _vsz = (p_tag)?p_tag->nchilds:p_xc->nroot;

	if(xpath) {
		_sz = p_xc->strlen((_str_t)xpath);
		p_xc->memset(_lb, 0, sizeof(_lb));
		for(i = 0; i < _sz; i++) {
			if(xpath[i] != '/') {
				_lb[j] = xpath[i];
				j++;
				if(j > sizeof(_lb))
					break;
			} else
				break;
		}

		for(_i = 0; _i < _vsz; _i++) {
			if((_t = get_child_tag(p_xc, p_tag, _i))) {
				if(_t->name) {
					if(p_xc->memcmp(_t->name, _lb, _t->sz_name) == 0 && _t->sz_name == p_xc->strlen(_lb)) {
						if(i < _sz) {
							return xml_select(p_xc, xpath+i+1, _t, index);
						} else {
							if(ix == index) {
								r = _t;
								break;
							}

							ix++;
						}
					}
				}
			}
		}
	} else
		r = get_child_tag(p_xc, p_tag, index);

	return r;
}

_str_t xml_parameter(_xml_context_t *p_xc, _xml_tag_t *p_tag, _cstr_t name, _u32 *sz) {
	_u32 pvs = 0; /* size of parameter value */
	_str_t pv = NULL; /* pointer to parameter value */
	_str_t p = p_tag->parameters; /* pointer to parameter content */
	_u32 l = p_tag->sz_parameters; /* size of parameters content */
	_str_t pn = NULL; /* pointer to parameter name */
	_u32 pns = 0; /* size of parameter name */
	_u32 i = 0; /* index */
	_s8 div = ' ';
	_u8 bo = 0;

	for(i = 0; i < l; i++) {
		if(*(p + i) < ' ' && *(p + i) != '\t') {
			pvs++;
			continue;
		}

		switch(*(p + i)) {
			case ' ':
			case '\t':
				if(div == '"') {
					if(pv != 0) {
						/* space in parameter value */
						pvs++;
						break;
					} else {
						pv = (p + i);
						pvs = 1;
					}

				} else {
					pv = 0;
					pn = 0;
					div = ' ';
				}

				break;

			case '"':
				if(div == '"') {
					if(pn != 0) {
						/* compare the parameter name */
						if(p_xc->memcmp((_u8 *)name, pn, pns) == 0) {
							i = l; /* end of 'for' cycle */
							break;
						} else
							pv = 0;
					}

					div = ' ';
					bo = !bo;

				} else
					div = *(p + i);

				break;

			case '=':
				if(!bo) {
					pv = 0;
					div = *(p + i);
				} else
					pvs++;
				break;

			default:
				switch(div) {
					case '=':
						break;

					case '"':
						if(pv)
							pvs++;
						else {
							pv = (p + i);
							pvs = 1;
						}
						break;

					case ' ':
					case '\t':
						if(pn)
							pns++;
						else {
							pn = (p + i);
							pns = 1;
						}
						break;
				}
				break;
		}
	}

	*sz = pvs;

	return pv;
}

_u32 xml_parameter_copy(_xml_context_t *p_xc, _xml_tag_t *p_tag, _cstr_t name, _str_t buffer, _u32 sz_buffer) {
	_u32 r = 0;
	_u32 psz = 0;
	_str_t par = xml_parameter(p_xc, p_tag, name, &psz);

	if(par) {
		p_xc->memset(buffer, 0, sz_buffer);
		p_xc->memcpy(buffer, par, (psz < sz_buffer) ? psz + 1 : sz_buffer - 1);
		r = psz;
	}
	return r;
}

