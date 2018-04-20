#ifndef __I_TTY_H__
#define __I_TTY_H__

#include "mgtype.h"

#define I_TXT_DISP	"i_txt_disp"
#define I_TTY		"i_tty"

typedef struct {
	void (*init)(_u8 max_row, _u8 max_col);
	void (*position)(_u8 row, _u8 col);
	_u8  (*color)(_u8 clr); /* return current color, and set new one */
	void (*scroll)(void);
	void (*clear)(void);
	_u32 (*write)(_p_data_t buffer, _u32 size);
	_u32 (*fwrite)(_cstr_t fmt, ...);
	_u32 (*vfwrite)(_cstr_t fmt, va_list args);
}_i_txt_disp_t;

typedef struct {
	_vx_res_t (*get_config)(_p_data_t cd, _p_data_t *cfg);
	_vx_res_t (*set_config)(_p_data_t cd, _p_data_t cfg);
	_u32 (*read)(_p_data_t cd, _p_data_t buffer, _u32 size);
	_u32 (*write)(_p_data_t cd, _p_data_t buffer, _u32 size);
	_u32 (*fwrite)(_p_data_t cd, _cstr_t fmt, ...);
	_u32 (*vfwrite)(_p_data_t cd, _cstr_t fmt, va_list args);
}_i_tty_t;

#endif

