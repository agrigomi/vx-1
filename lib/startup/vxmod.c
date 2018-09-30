#include "vxmod.h"
#ifdef _CORE_
#include "startup_context.h"
#endif
#ifdef _EXT_
#include "ext_context.h"
#endif
#include "i_repository.h"

extern _ulong _vxmod_start_; /* section start */
extern _ulong _vxmod_end_; /* section end */

/* global pointer to repository interface */
_i_repository_t	*__g_p_i_repository__ = NULL;
_ulong		__g_h_mod_array__ = 0;

#ifdef _CORE_
#include "str.h"
_core_startup_t *__g_p_core_startup__ = NULL;
void _vx_core_init_(_core_startup_t *p_sc) {
	__g_p_core_startup__ = p_sc;
#endif /* _CORE_ */

#ifdef _EXT_
_ext_startup_t *__g_p_ext_startup__ = NULL;
void _vx_ext_init_(_ext_startup_t *p_sc) {
	__g_p_ext_startup__ = p_sc;
	__g_p_i_repository__ = (_i_repository_t *)p_sc->p_i_repository;
#endif /* _EXT_ */

	_vx_mod_t *p_smod = (_vx_mod_t *)&_vxmod_start_;
	_vx_mod_t *p_emod = (_vx_mod_t *)&_vxmod_end_;
	_ulong mod_count = ((_ulong)p_emod - (_ulong)p_smod) / sizeof(_vx_mod_t);
	_u32 i = 0;

#ifdef _CORE_
	_vx_mod_t *p_arch_entry = NULL;

	/* search for repository */
	for(; i < mod_count; i++) {
		if(_str_cmp((_str_t)((p_smod + i)->_m_iname_), I_REPOSITORY) == 0) {
			/* keep interface pointer only, because we do not expect
			    public data context for repository
			*/
			__g_p_i_repository__ = (p_smod + i)->_m_interface_;
			break;
		}
	}
#endif /* _CORE_ */

	if(__g_p_i_repository__ && mod_count) {
		/* add local '.vxmod' array to repository  */
		__g_h_mod_array__ =__g_p_i_repository__->add_mod_array(p_smod, mod_count);

		/* early init */
		for(i = 0; i < mod_count; i++) {
#ifdef _CORE_
			if(!p_arch_entry) {
				if(_str_cmp((_str_t)(p_smod + i)->_m_iname_, I_ARCH_ENTRY) == 0)
					/* found architecture entry point */
					p_arch_entry = (p_smod + i);
			}
#endif

			if((p_smod + i)->_m_ctl_)
				/* call module with early init command (withowt args) */
				(p_smod + i)->_m_ctl_(MODCTL_EARLY_INIT);
		}

		__g_p_i_repository__->init_mod_array(__g_h_mod_array__);

#ifdef _CORE_
		if(p_arch_entry)
			/* call architecture entry point */
			p_arch_entry->_m_ctl_(MODCTL_START, __g_p_i_repository__, p_arch_entry->_m_context_._c_data_);
#endif
	}
}

