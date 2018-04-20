// menu.c

#include "code16gcc.h"
#include "boot.h"
#include "lib.h"

extern _str_t str_default;
extern _str_t str_countdown;
extern _str_t str_display;
extern _str_t str_mask;

#define STR_DEFAULT 	((_str_t)str_default)
#define STR_COUNTDOWN	((_str_t)str_countdown)
#define STR_MASK	((_str_t)str_mask)
#define STR_DISPLAY	((_str_t)str_display)

#define MAX_ITEMS	6
#define MAX_CFG_LINES	16
#define MENU_COLOR	7
#define MENU_MARKER	0x10

typedef struct {
	struct {
		_u8	*p;	/* buffer */
		volatile _u16	sz;	/* length */
	} _cfg_item[MAX_ITEMS];
} _cfg_t;

/* return the size of line */
static _u32 __NOINLINE__ __REGPARM__ get_line(_u8 *buffer, _u32 sz) {
	volatile _u32 r = 0;
	
	while(r < sz) {
		if(*(buffer + r) == 0x0a)
			return r+1;
		
		r++;
	}
	
	return r;
}


#define PARSER_OK		0
#define PARSER_INCOMPLETE	1
#define PARSER_COMMENT_LINE	2
#define PARSER_EMPTY_LINE	3

#define ITEM_NAME(cfg)		(_str_t)cfg._cfg_item[0].p
#define ITEM_NAME_SZ(cfg)	cfg._cfg_item[0].sz
	
#define ITEM_VALUE(cfg)		(_str_t)cfg._cfg_item[1].p
#define ITEM_VALUE_SZ(cfg)	cfg._cfg_item[1].sz

#define ITEM_ROW(cfg)		(_str_t)cfg._cfg_item[2].p
#define ITEM_ROW_SZ(cfg)	cfg._cfg_item[2].sz

#define ITEM_COL(cfg)		(_str_t)cfg._cfg_item[3].p
#define ITEM_COL_SZ(cfg)	cfg._cfg_item[3].sz

#define ITEM_EXEC(cfg)		(_str_t)cfg._cfg_item[4].p
#define ITEM_EXEC_SZ(cfg)	cfg._cfg_item[4].sz

#define ITEM_PARAMS(cfg)	(_str_t)cfg._cfg_item[5].p
#define ITEM_PARAMS_SZ(cfg)	cfg._cfg_item[5].sz


static _u8 parse_line(_u8 *buffer, volatile _u32 offset, volatile _u32 sz, _str_t mask, _cfg_t *p_cfg) {
	volatile _u8 r = PARSER_EMPTY_LINE;
	_u8 *bp = buffer + offset;
	volatile _u32 bi = 0;	/* index in source buffer */
	volatile _u16 mi = 0;	/* index in mask */
	volatile _u16 ml = str_len(mask); // mask size */
	volatile _u8  li = 0;	/* item index */
	volatile _u16 ci = 0;	/* item size */
	
	// initialize configuration buffer
	for(li = 0; li < MAX_ITEMS; li++) {
		p_cfg->_cfg_item[li].p = 0;
		p_cfg->_cfg_item[li].sz = 0; 
	}
	
	li = 0;
	
	/* catch the first item by start of line */
	p_cfg->_cfg_item[li].p = bp + bi;
	p_cfg->_cfg_item[li].sz = 0;
	
	while(bi < sz) {
		switch(*(bp + bi)) {
			case '#':
				r = PARSER_COMMENT_LINE;
				bi = sz;
				break;
				
			case 0x0d: /* dos format end line */
				break;
				
			case 0x0a: /* line feed as end of line */
				bi = sz;
				p_cfg->_cfg_item[li].sz = ci;
				break;
				
			default:
				if(*(bp + bi) == mask[mi]) {
					r = PARSER_INCOMPLETE;
					p_cfg->_cfg_item[li].sz = ci;
					ci = 0;
					mi++;
					if(mi >= ml) { /* the mask index is completely used */
						r = PARSER_OK;
						bi = sz;
						break;
					}
					
					li++;
					if(li < MAX_ITEMS) { /* !!! no more items !!! */
						p_cfg->_cfg_item[li].p = bp + bi + 1;
						p_cfg->_cfg_item[li].sz = 0;
					} else {
						bi = sz;
						break;
					}
					
				} else
					ci++;
					
				break;	
		}
		
		bi++;
	}
	
	return r;
}

static _u8 show_menu(_cfg_t *p_cfg, volatile _u8 nitems) {
	volatile _u8 r = 0xff; /* invalid selection */
	volatile _u8 i = 0;
	volatile _u8 row;
	volatile _u32 countdown = 0;
	volatile _u32 _countdown = 0;
	volatile _u8 _default = 0xff;
	volatile _u8 nboot = 0;
	volatile _u8 spc=' ';
	volatile _u16 key = 0xffff;
	volatile _u8 color = MENU_COLOR;
	volatile _u8 boot[MAX_CFG_LINES];
	
	for(i = 0; i < nitems; i++) {
		if(txt_cmp(ITEM_NAME(p_cfg[i]),STR_COUNTDOWN,ITEM_NAME_SZ(p_cfg[i])) == 0) {
			countdown = str2i(ITEM_VALUE(p_cfg[i]),(_s8)ITEM_VALUE_SZ(p_cfg[i]));
			continue;
		}
			
		if(txt_cmp(ITEM_NAME(p_cfg[i]),STR_DEFAULT,ITEM_NAME_SZ(p_cfg[i])) == 0) {
			_default = i;
			continue;
		}
			
		if(txt_cmp(ITEM_NAME(p_cfg[i]),STR_DISPLAY,ITEM_NAME_SZ(p_cfg[i])) == 0) {
			display_text(ITEM_VALUE(p_cfg[i]),
					ITEM_VALUE_SZ(p_cfg[i]),
					(_u8)str2i(ITEM_ROW(p_cfg[i]),(_s8)ITEM_ROW_SZ(p_cfg[i])),
					(_u8)str2i(ITEM_COL(p_cfg[i]),(_s8)ITEM_COL_SZ(p_cfg[i])),MENU_COLOR);
			continue;
		}

		display_text(ITEM_VALUE(p_cfg[i]),
				ITEM_VALUE_SZ(p_cfg[i]),
				(_u8)str2i(ITEM_ROW(p_cfg[i]),(_s8)ITEM_ROW_SZ(p_cfg[i])),
				(_u8)str2i(ITEM_COL(p_cfg[i]),(_s8)ITEM_COL_SZ(p_cfg[i])),MENU_COLOR);
					
		boot[nboot] = i;
		nboot++;
		
		if(r == 0xff)
			r = i;
	}
	
	if(_default != 0xff) {
		for(i = 0; i < nitems; i++) {
			if(txt_cmp(ITEM_NAME(p_cfg[i]),ITEM_VALUE(p_cfg[_default]),ITEM_VALUE_SZ(p_cfg[_default])) == 0) {
				r = i;
				break;
			}
		}
	}

	
	/* hide cursor */
	hide_cursor();
	
	_countdown = countdown * 60000;
	
	do {		
		
		wait(1000);
		_countdown -= 100;

		if(r != 0xff && key != 0) {
			switch(key & 0xff00) {
				/* switch by scan code */
				case KEY_UP_ARROU: /* up arrow */
					row = (_u8)str2i(ITEM_ROW(p_cfg[r]),(_s8)ITEM_ROW_SZ(p_cfg[r]));
					while(row) {
						if(row)
							row--;
						else
							break;
							
						for(i = 0; i < nboot; i++) {
							if(row == str2i(ITEM_ROW(p_cfg[boot[i]]),
									(_s8)ITEM_ROW_SZ(p_cfg[boot[i]]))) {
								r = boot[i];
								row = 0;
								break;
							}
						}
					}
					break;
					
				case KEY_DOWN_ARROW: /* down arrow */
					row = (_u8)str2i(ITEM_ROW(p_cfg[r]),(_s8)ITEM_ROW_SZ(p_cfg[r]));
					while(row < 24) {
						row++;
						
						for(i = 0; i < nboot; i++) {
							if(row == str2i(ITEM_ROW(p_cfg[boot[i]]),
									(_s8)ITEM_ROW_SZ(p_cfg[boot[i]]))) {
								r = boot[i];
								row = 24;
								break;
							}
						}
					}
					break;
					
			}
			
			switch(key & 0x00ff) {
				/* switch by ascii code */
				case 0x00d: /* enter */
					key = 0;
					_countdown = 0;
					break;
			}
			
			if(key)
				_countdown = countdown * 60000;
				
			/* status line */
			for(i = 2; i < 78; i++)
				display_text((_str_t)&spc,1,24,i,MENU_COLOR | MENU_MARKER);
			
			for(i = 0; i < nboot; i++) {
				if(boot[i] == r)
					color = MENU_COLOR | MENU_MARKER;
				else
					color = MENU_COLOR;
					
				display_text(ITEM_VALUE(p_cfg[boot[i]]),
						ITEM_VALUE_SZ(p_cfg[boot[i]]),
						(_u8)str2i(ITEM_ROW(p_cfg[boot[i]]),(_s8)ITEM_ROW_SZ(p_cfg[boot[i]])),
						(_u8)str2i(ITEM_COL(p_cfg[boot[i]]),(_s8)ITEM_COL_SZ(p_cfg[boot[i]])),color);
						
			}

			display_text(ITEM_EXEC(p_cfg[r]),ITEM_EXEC_SZ(p_cfg[r]),24,3,MENU_COLOR | MENU_MARKER);
			display_text(ITEM_PARAMS(p_cfg[r]),
							(ITEM_PARAMS_SZ(p_cfg[r]) < (78 - (ITEM_EXEC_SZ(p_cfg[r]) + 4))) ? 
							(ITEM_PARAMS_SZ(p_cfg[r])) : (78 - (ITEM_EXEC_SZ(p_cfg[r]) + 4)),
							24,(_u8)(ITEM_EXEC_SZ(p_cfg[r]) + 4),MENU_COLOR | MENU_MARKER);
			
		}
		
		key = get_key();
		set_cursor_pos(24,0);
		print_byte((_u8)(_countdown/60000));
	} while(_countdown);
	return r;
}

void menu(_u8 *p_cfg, _u32 cfg_sz, _str_t p_file_name, 
					_u16 *name_sz,_str_t p_args,_u16 *args_sz) {
	_cfg_t _cfg[MAX_CFG_LINES];
	volatile _u32 offset = 0;
	volatile _u32 l;
	volatile _u8 _r;
	volatile _u8 ci=0;
	
	while(offset < cfg_sz) {
		l = get_line(p_cfg + offset, cfg_sz - offset);
		_r = parse_line(p_cfg,offset,l,STR_MASK,&_cfg[ci]);
		if(_r == PARSER_OK || _r == PARSER_INCOMPLETE) {
			ci++;
			if(ci == MAX_CFG_LINES)
				break;
		}

		offset += l;
	}
	_r = show_menu(&_cfg[0], ci);
	if(_r != 0xff) {
		_str_t exec = ITEM_EXEC(_cfg[_r]);
		_str_t args = ITEM_PARAMS(_cfg[_r]);
		_u16 _exec_sz = ITEM_EXEC_SZ(_cfg[_r]);
		_u16 _args_sz = ITEM_PARAMS_SZ(_cfg[_r]);

		mem_cpy((_u8 *)p_file_name, (_u8 *)exec, (_exec_sz < *name_sz)?_exec_sz:*name_sz);
		mem_cpy((_u8 *)p_args, (_u8 *)args, (_args_sz < *args_sz)?_args_sz:*args_sz);

		*name_sz = (_exec_sz < *name_sz)?_exec_sz:*name_sz;
		*args_sz = (_args_sz > *args_sz)?_args_sz:*args_sz;
	}
}

