#include "mutex.h"
#include "atom.h"

#define GET_HANDLE(mutex) ((((_u64)mutex->handle_count) << 32) | (_u32)(_u64)mutex)

#define LOCK_MAGIC	0xdd7799cc
#define UNLOCK_MAGIC	0x82df317d

_u64 mutex_try_lock(_mutex_t *p_mutex, _u64 hm) {
	_u64 r = 0;

	if(hm != GET_HANDLE(p_mutex)) {
		_u32 rlock = LOCK_MAGIC;
		__EXCHANGE_L__(rlock, p_mutex->lock_flag);
		if(rlock == UNLOCK_MAGIC) {
			p_mutex->lock_count = 1;
			p_mutex->handle_count++;
			r = GET_HANDLE(p_mutex);
		}
	} else {
		r = hm;
		p_mutex->lock_count++;
	}

	return r;
}
_u64 mutex_lock(_mutex_t *p_mutex, _u64 hm, _u32 timeout, _u8 flags) {
	_u64 r = 0;
	_u32 _timeout = (timeout) ? timeout : 1;

	while(_timeout) {
		if((r = mutex_try_lock(p_mutex, hm)))
			break;
		else {
			if(timeout != _MUTEX_TIMEOUT_INFINITE_)
				_timeout--;
		}
	}

	if(!_timeout) {
		if(flags & (_MF_TIMEOUT_RESET_|_MF_TIMEOUT_RESET_WITH_HANDLE_))
			mutex_reset(p_mutex);
		if(flags & _MF_TIMEOUT_RESET_WITH_HANDLE_)
			r = GET_HANDLE(p_mutex);
	}

	return r;
}
void mutex_unlock(_mutex_t *p_mutex, _u64 hm) {
	if(hm == GET_HANDLE(p_mutex)) {
		if(p_mutex->lock_count)
			p_mutex->lock_count--;
		
		if(!p_mutex->lock_count) {
			_u32 lr = UNLOCK_MAGIC;
			__EXCHANGE_L__(lr, p_mutex->lock_flag);
		}
	}
}
void mutex_reset(_mutex_t *p_mutex) {
	p_mutex->lock_count = 0;
	p_mutex->lock_flag = UNLOCK_MAGIC;
	p_mutex->handle_count = 0;
}

