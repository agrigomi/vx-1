#ifndef __I_REPOSITORY_H__
#define __I_REPOSITORY_H__

#include "vxmod.h"

#define I_REPOSITORY	"i_repository"

#define REPOSITORY	__g_p_i_repository__

typedef _ulong HMARRAY;

typedef struct {
	/* extend repository with new .vxmod array and return array index */
	HMARRAY (*add_mod_array)(_vx_mod_t *array, _u32 count);

	/* init array */
	void (*init_mod_array)(_ulong harray);

	/* create new context */
	HCONTEXT (*create_context_by_interface)(_cstr_t interface_name);
	HCONTEXT (*create_context_by_name)(_cstr_t mod_name);
	HCONTEXT (*create_limited_context_by_interface)(_cstr_t interface_name, _ulong addr_limit);
	HCONTEXT (*create_limited_context_by_name)(_cstr_t mod_name, _ulong addr_limit);

	/* get static context */
	HCONTEXT (*get_context_by_name)(_cstr_t mod_name);
	HCONTEXT (*get_context_by_interface)(_cstr_t i_name);

	/* release module context */
	void (*release_context)(HCONTEXT hc);

	/* get module descriptor */
	_vx_mod_t *(*mod_info_by_name)(_cstr_t mod_name);
	_vx_mod_t *(*mod_info_by_index)(_u32 mod_index);

	/* get number of modules */
	_u32 (*mod_count)(void);

	/* remove .vxmod array from repository */
	void (*remove_mod_array)(HMARRAY hmod_array);
}_i_repository_t;

extern _i_repository_t	*__g_p_i_repository__;

#endif

