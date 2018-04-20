#ifndef __MUTEX_API_H__
#define __MUTEX_API_H__

#include "mgtype.h"

#define _MUTEX_TIMEOUT_INFINITE_	0xffffffff

#define _MF_TIMEOUT_RESET_		(1<<0)
#define _MF_TIMEOUT_RESET_WITH_HANDLE_	(1<<1)


typedef struct {
	volatile _u32 lock_count;
	volatile _u32 handle_count;
	volatile _u32 lock_flag;
}_mutex_t;

_u64 mutex_try_lock(_mutex_t *p_mutex, _u64 hm);
_u64 mutex_lock(_mutex_t *p_mutex, _u64 hm, _u32 timeout, _u8 flags);
void mutex_unlock(_mutex_t *p_mutex, _u64 hm);
void mutex_reset(_mutex_t *p_mutex);

#endif

