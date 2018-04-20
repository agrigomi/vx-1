#ifndef __MGFS_BUFFER_H__
#define __MGFS_BUFFER_H__

#include "mgfs3.h"

_h_buffer_ mgfs_buffer_alloc(_mgfs_context_t *p_cxt, _u64 unit_number, _h_lock_ hlock);
void *mgfs_buffer_ptr(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock);
_u64 mgfs_buffer_unit(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock);
void mgfs_buffer_free(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock);
void mgfs_buffer_dirty(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock);
void mgfs_buffer_flush(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock);
void mgfs_buffer_flush_all(_mgfs_context_t *p_cxt, _h_lock_ hlock);
void mgfs_buffer_reset(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock);
void mgfs_buffer_reset_all(_mgfs_context_t *p_cxt, _h_lock_ hlock);
void mgfs_buffer_cleanup(_mgfs_context_t *p_cxt, _h_lock_ hlock);
_h_lock_ mgfs_buffer_lock(_mgfs_context_t *p_cxt, _h_lock_ hlock);
void mgfs_buffer_unlock(_mgfs_context_t *p_cxt, _h_lock_ hlock);

#endif
