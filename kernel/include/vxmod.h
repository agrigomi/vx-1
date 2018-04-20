#ifndef __VXMOD_H__
#define __VXMOD_H__

#include "mgtype.h"
#include "compiler.h"

#ifdef _CORE_
#include "startup_context.h"
extern _core_startup_t *__g_p_core_startup__;
#endif
#ifdef _EXT_
#include "ext_context.h"
extern _ext_startup_t *__g_p_ext_startup__;
#endif

/* section name for all module descriptors */
#define MOD_SECTION	".vxmod"

/* module interface name reserved for architecture entry point */
#define I_ARCH_ENTRY	"i_arch_entry"

typedef union {
	_u32	version;
	struct {
		_u32	revision:16;
		_u32	minor	:8;
		_u32	major	:8;
	}_v;
}_vx_version_t;

typedef void*	_p_interface_t;
typedef void*	_p_data_t;
typedef _u32	_vx_res_t;


/* module controll commands */
#define MODCTL_EARLY_INIT	1 /* no args */
#define MODCTL_INIT_CONTEXT	3 /* args: _i_repository_t*, _p_data_t  */
#define MODCTL_DESTROY_CONTEXT	4 /* args: _i_repository_t*, _p_data_t  */
#define MODCTL_RELEASE_CONTEXT	5 /* args: _i_repository_t*, HCONTEXT 	*/
#define MODCTL_START		6 /* args: _i_repository_t*, _p_data_t  */
#define MODCTL_STOP		7 /* args: _i_repository_t*, _p_data_t  */

/* module controll callback */
typedef _vx_res_t _ctl_t(_u32 cmd, ...);

typedef struct vx_mod _vx_mod_t;

typedef struct {
	_p_data_t	_c_data_;
	_vx_mod_t	*_c_mod_;
}_vx_context_t;

#define HCONTEXT		_vx_context_t*
#define HC_INTERFACE(hc)	hc->_c_mod_->_m_interface_
#define HC_DATA(hc)		hc->_c_data_
#define HC_NAME(hc)		hc->_c_mod_->_m_name_
#define HC_INAME(hc)		hc->_c_mod_->_m_iname_
#define HC_DESCRIPTION(hc)	hc->_c_mod_->_m_description_
#define HC_VERSION(hc)		&(hc->_c_mod_->_m_version_)

struct vx_mod { /* module descriptor */
	_cstr_t		_m_name_;  /* module name */
	_cstr_t		_m_iname_; /* interface name */
	_vx_version_t	_m_version_;
	_ctl_t		*_m_ctl_; /* pointer to module controll */
	_p_interface_t	_m_interface_; /* module interface pointer */
	_vx_context_t	_m_context_; /* data context */
	_u32		_m_dsize_; /* data size */
	_u32		_m_state_; /* module state */
	_u32		_m_refc_; /* context reference counter */
	_cstr_t		_m_description_; /* module description */
};

#define DEF_VXMOD(mod_name, interface_name, p_interface, p_data, szd, ctl, vmajor, vminor, vrevision, description) \
	static _vx_mod_t __attribute__((used, aligned(8), section(MOD_SECTION))) __g_vxmod__##mod_name = { \
		._m_name_ = #mod_name, \
		._m_iname_ = interface_name, \
		._m_ctl_ = ctl, \
		._m_version_._v = { \
			.major = vmajor, \
			.minor = vminor, \
			.revision = vrevision \
		}, \
		._m_interface_ = p_interface, \
		._m_context_ = { \
			._c_data_ = p_data, \
			._c_mod_ = &__g_vxmod__##mod_name \
		}, \
		._m_dsize_ = szd, \
		._m_state_ = 0, \
		._m_refc_ = 0, \
		._m_description_ = description \
	}

#endif

