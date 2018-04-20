#ifndef __I_SYNC_H__
#define __I_SYNC_H__

#include "vxmod.h"

#define I_MUTEX		"i_mutex"
#define I_EVENT		"i_event"
#define I_SYNC_CALL	"i_sync_call"

typedef _u64	HMUTEX;

typedef struct {
	HMUTEX (*try_lock)(_p_data_t, HMUTEX);
	HMUTEX (*lock)(_p_data_t, HMUTEX);
	void (*unlock)(_p_data_t, HMUTEX);
}_i_mutex_t;

typedef struct {
	_u32 (*check)(_p_data_t, _u32);
	_u32 (*wait)(_p_data_t, _u32);
	void (*set)(_p_data_t, _u32);
}_i_event_t;

typedef struct {
	/*...*/
}_i_sync_call_t;

#endif

