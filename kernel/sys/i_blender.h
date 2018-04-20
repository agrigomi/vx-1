#ifndef __I_BLENDER_H__
#define __I_BLENDER_H__

#include "mgtype.h"
#include "i_sync.h"

#define I_BLENDER	"i_blender"

/* thread entry */
typedef _s32 _thread_t(void *p_data);
typedef void _tstate_t(_p_data_t, _u32 id, _u16 nstate, _p_data_t udata);

typedef struct {
	void  (*init)(_p_data_t, _u16 cpu_id);
	void  (*start)(_p_data_t);
	_bool (*is_running)(_p_data_t);
	void  (*set_timer)(_p_data_t, _u32);
	void  (*set_time_div)(_p_data_t, _u8 div);
	_u8   (*get_time_div)(_p_data_t);
	_bool (*enable_interrupts)(_p_data_t, _bool enable);

	/* callbacks */
	void (*idle)(_p_data_t);

	/* interrupts callbacks*/
	void (*timer)(_p_data_t, _p_data_t cpu_state);
	void (*switch_context)(_p_data_t, _p_data_t cpu_state);
	void (*interrupt)(_p_data_t, _p_data_t cpu_state);
	void (*breakpoint)(_p_data_t, _p_data_t cpu_state);
	void (*debug)(_p_data_t, _p_data_t cpu_state);

	/* exception callbacks */
	void (*exception)(_p_data_t, _u8 exn, _p_data_t cpu_state);
	void (*memory_exception)(_p_data_t, _ulong addr, _p_data_t cpu_state);

	/* thread management */
	_u32 (*create_systhread)(_p_data_t, _thread_t *entry, _u32 stack_sz, 
				_p_data_t udata, _bool suspend, _u32 resolution);
	_u32 (*create_usr_systhread)(_p_data_t, _thread_t *entry, _u32 stack_sz, 
				_p_data_t udata, _bool suspend, _u32 resolution);
	void (*terminate_systhread)(_p_data_t, _u32 id);
	void (*sleep_systhread)(_p_data_t, _u32 id, _u32 ms);
	void (*suspend_systhread)(_p_data_t, _u32 id);
	void (*resume_systhread)(_p_data_t, _u32 id);
	void (*set_state_callback)(_p_data_t, _u32 id, _tstate_t *cb, _p_data_t udata);
	_u32 (*current_systhread_id)(_p_data_t);

	/* sync. */
	_u32 (*wait_event)(_p_data_t, _p_data_t cd_event, _u32 mask);
	HMUTEX (*acquire_mutex)(_p_data_t, HMUTEX);
}_i_blender_t;

#endif

