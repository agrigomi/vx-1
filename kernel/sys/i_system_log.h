#ifndef __I_SYSTEM_LOG_H__
#define __I_SYSTEM_LOG_H__

#include "vxmod.h"
#include "i_sync.h"

#define I_SYSTEM_LOG	"i_system_log"

/* log message type */
#define LMT_NONE	0
#define LMT_TEXT	0
#define LMT_INFO	1
#define LMT_WARNING	2
#define LMT_ERROR	3

typedef void _log_listener_t(_u8 lmt, _cstr_t msg);

typedef struct {
	void (*init)(_p_data_t, _u32 capacity);
	void (*add_listener)(_p_data_t, _log_listener_t*);
	void (*remove_listener)(_p_data_t, _log_listener_t*);
	void (*write)(_p_data_t, _u8 mlt, _cstr_t msg);
	void (*fwrite)(_p_data_t, _u8 lmt, _cstr_t fmt, ...);
	_str_t (*first)(_p_data_t, HMUTEX);
	_str_t (*next)(_p_data_t, HMUTEX);
	HMUTEX (*lock)(_p_data_t);
	void (*unlock)(_p_data_t, HMUTEX);
}_i_system_log_t;

#define DEF_SYSLOG() \
	static HCONTEXT __g_hc_sys_log__ = NULL;\
	static _i_system_log_t *__g_pi_sys_log__ = NULL;\
	static _p_data_t __g_pd_sys_log__ = NULL

#define GET_SYSLOG(repo) \
	if((__g_hc_sys_log__ = repo->get_context_by_interface(I_SYSTEM_LOG))) {\
		__g_pi_sys_log__ = HC_INTERFACE(__g_hc_sys_log__); \
		__g_pd_sys_log__ = HC_DATA(__g_hc_sys_log__); \
	}

#define INIT_SYSLOG(capacity) \
	if(__g_pi_sys_log__ && __g_pd_sys_log__) \
		__g_pi_sys_log__->init(__g_pd_sys_log__, capacity)

#define RELEASE_SYSLOG(repo) \
	if(__g_hc_sys_log__) { \
		repo->release_context(__g_hc_sys_log__); \
		__g_pi_sys_log__ = NULL; \
		__g_pd_sys_log__ = NULL; \
	}

#define LOG(_lmt_, _fmt_, ...) \
	if(__g_pi_sys_log__ && __g_pd_sys_log__) \
		__g_pi_sys_log__->fwrite(__g_pd_sys_log__, _lmt_, _fmt_, __VA_ARGS__)
#define _LOG(_lmt_, msg) \
	if(__g_pi_sys_log__ && __g_pd_sys_log__) \
		__g_pi_sys_log__->write(__g_pd_sys_log__, _lmt_, msg)

#define ADD_SYSLOG_LISTENER(proc) \
	if(__g_pi_sys_log__ && __g_pd_sys_log__) \
		__g_pi_sys_log__->add_listener(__g_pd_sys_log__, proc)

#define REM_SYSLOG_LISTENER(proc) \
	if(__g_pi_sys_log__ && __g_pd_sys_log__) \
		__g_pi_sys_log__->remove_listener(__g_pd_sys_log__, proc)

#endif

