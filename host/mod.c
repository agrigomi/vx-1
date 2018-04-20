#include "stdio.h"
#include "stdarg.h"
#include "vxmod.h"

typedef struct {
	/*... */
}_my_data_t;

typedef struct {
	void (*f1)(void);
	_u32 (*f2)(void *);
}_my_interface_t;

_my_data_t _g_data;

_u32 my_mod_ctl(_u32 cmd, ...) {
	va_list args;
	va_start(args, cmd);

	_vx_context_t *p_context = va_arg(args, _vx_context_t*); /* first argument */
	if(p_context) {
		/* ... */
	}

	va_end(args);
	return 0;
}

void my_f1(void) {
}
_u32 my_f2(void *p) {
	return 0;
}

_my_interface_t _g_interface = {
	.f1 = my_f1,
	.f2 = my_f2
};

DEF_VXMOD("mod1", &_g_interface, &_g_data, sizeof(_my_data_t), my_mod_ctl, 1,0,1);
/*
DEF_VXMOD("mod2", &_g_interface, &_g_data, sizeof(_g_data), my_mod_ctl, 1,2,3);
DEF_VXMOD("mod3", &_g_interface, NULL, 0, my_mod_ctl, 1,1,4);
DEF_VXMOD("mod4", &_g_interface, NULL, 0, my_mod_ctl, 1,0,5);
*/

extern unsigned long _vxmod_start_;
extern unsigned long _vxmod_end_;

int main(void) {
	_vx_mod_t *p_bmod_ = (_vx_mod_t *)&_vxmod_start_;
	_vx_mod_t *p_emod_ = (_vx_mod_t *)&_vxmod_end_;
	unsigned int nmodr = (unsigned int)((unsigned long)p_emod_ - (unsigned long)p_bmod_);
	unsigned int sz_modinfo = sizeof(_vx_mod_t);

	printf("sizeof(_mod_info_t) = %d\r\n", sz_modinfo);
	nmodr = nmodr / sizeof(_vx_mod_t);

	printf(".mod records = %d\r\n", nmodr);
	while(p_bmod_ < p_emod_) {
		printf("module: %s, version: %x\r\n",p_bmod_->_m_name_, p_bmod_->_m_context_._c_version_.version);
		p_bmod_++;
	}
	return 0;
}

