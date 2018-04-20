#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef _DEBUG_ 
	#include "i_repository.h"
	#include "i_tty.h"
	static _i_txt_disp_t *__g_pi_disp__ = NULL;
	#define INIT_DEBUG() { \
		if(!__g_pi_disp__) { \
			HCONTEXT hc_disp = __g_p_i_repository__->get_context_by_interface(I_TXT_DISP); \
			if(hc_disp) \
				__g_pi_disp__ = HC_INTERFACE(hc_disp); \
		}\
	}

	#define DBG(_fmt_, ...)  {\
		INIT_DEBUG() \
		if(__g_pi_disp__) \
			__g_pi_disp__->fwrite(_fmt_, __VA_ARGS__); \
	}
#else
	
	#define INIT_DEBUG()	;
	#define DBG(_fmt_, ...)	;
#endif /* _DEBUG_ */
#endif

