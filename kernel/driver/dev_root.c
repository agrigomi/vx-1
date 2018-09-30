#include "vxmod.h"
#include "vxdev.h"
#include "i_dev_root.h"
#include "i_memory.h"
#include "i_repository.h"
#include "err.h"
#include "i_str.h"
#include "i_system_log.h"

#define MAX_SLOTS	255	/* 0 -- 0xfe */
#define NODE_MEM	(MAX_SLOTS * sizeof(_ulong))
#define DEV_ROOT_IDENT	"dev_root"

DEF_SYSLOG();

HCONTEXT g_hc_llist = NULL;
HCONTEXT g_hc_heap  = NULL;

static _i_repository_t	*_g_pi_repo  = NULL;
static _vx_dev_t	*_g_p_dev_root = NULL;
static _i_str_t		*_g_pi_str = NULL;

static HDEV create_device(_ctl_t *p_ctl, HDEV host, _p_data_t data);
static HDEV create_device_from_context(HCONTEXT hc, HDEV host);
static _u32 remove_device_node(HDEV dev, HMUTEX hlock);
static _u32 remove_device(HDEV dev, HMUTEX hlock);
static void sys_update(void);
static HDEV request_device(_dev_request_t *p_dev_req, HDEV dev_start, HMUTEX hlock);
static _u32 suspend_dev_node(HDEV dev, HMUTEX hlock);
static _u32 resume_dev_node(HDEV dev, HMUTEX hlock);
static _u32 dev_read(HDEV h, _ulong offset, _u32 nb, _p_data_t buffer, HMUTEX hlock);
static _u32 dev_write(HDEV h, _ulong offset, _u32 nb, _p_data_t buffer, HMUTEX hlock);
static _vx_res_t dev_get_config(HDEV h, _p_data_t cfg, HMUTEX hlock);
static _vx_res_t dev_set_config(HDEV h, _p_data_t cfg, HMUTEX hlock);
static HMUTEX lock(HDEV dev, HMUTEX hlock);
static void unlock(HDEV dev, HMUTEX hlock);

static _vx_dev_t *dev_ptr(HDEV dev) {
	_vx_dev_t *r = NULL;

	/* because in future
	HDEV can be different by _vx_dev_t*  */
	_vx_dev_t *_r = (_vx_dev_t *)dev;

	if(_r) {
		if((_r->_d_state_ & DSTATE_INITIALIZED) && (_r->_d_state_ & DSTATE_ATTACHED))
			r = _r;
	}
	return r;
}

HDEV root_hdev(void) {
	return (HDEV)_g_p_dev_root;
}

static _i_dev_root_t _g_i_dev_root = {
	.create 	= create_device,
	.create_from_context= create_device_from_context,
	.remove 	= remove_device,
	.remove_node  	= remove_device_node,
	.update	  	= sys_update,
	.request	= request_device,
	.get_hdev	= root_hdev,
	.devptr		= dev_ptr,
	.suspend_node 	= suspend_dev_node,
	.resume_node  	= resume_dev_node,
	.read		= dev_read,
	.write		= dev_write,
	.get_config	= dev_get_config,
	.set_config	= dev_set_config,
	.lock	  	= lock,
	.unlock	  	= unlock
};

static _u32 _mod_ctl_(_u32 cmd, ...) {
	_u32 r = VX_UNSUPPORTED_COMMAND;

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
				va_list args;
				va_start(args, cmd);

				if((_g_pi_repo = va_arg(args, _i_repository_t*))) {
					GET_SYSLOG(_g_pi_repo);
					if(!g_hc_llist) { /* create own linked list */
						if((g_hc_llist = _g_pi_repo->create_context_by_interface(I_LLIST))) {
							/* initialize linked list context */
							_i_llist_t *pi_list = HC_INTERFACE(g_hc_llist);
							_p_data_t pd_list = HC_DATA(g_hc_llist);

							if(pi_list && pd_list)
								pi_list->init(pd_list, LLIST_VECTOR, 1, NO_ALLOC_LIMIT);
						}
					}
					if(!g_hc_heap) /* get instance to system heap */
						g_hc_heap  = _g_pi_repo->get_context_by_interface(I_HEAP);

					if(!_g_pi_str) {
						HCONTEXT hcstr = _g_pi_repo->get_context_by_interface(I_STR);
						if(hcstr)
							_g_pi_str = HC_INTERFACE(hcstr);
					}

					if(g_hc_llist && g_hc_heap) {
						/* create device entry (ROOT for all device drivers)
						   and store the entry pointer, because it will be used
						   as default start point for dev_request */
						if((_g_p_dev_root = create_device(_mod_ctl_, NULL, NULL))) {
							/* force attach */
							_g_p_dev_root->_d_state_ |= DSTATE_ATTACHED;
							_g_p_dev_root->_d_state_ &= ~DSTATE_PENDING;
							r = VX_OK;
						}
					}
				}

				va_end(args);
			}
			break;
		case DEVCTL_INIT: {
				va_list args;
				va_start(args, cmd);
				_i_dev_root_t *pi_droot = va_arg(args, _i_dev_root_t*);
				_vx_dev_t *p_dev = va_arg(args, _vx_dev_t*);
				if(p_dev && pi_droot) {
					p_dev->_d_type_ = DTYPE_VBUS;
					p_dev->_d_class_ = DCLASS_SYS;
					_g_pi_str->str_cpy(p_dev->_d_ident_, (_str_t)DEV_ROOT_IDENT, sizeof(p_dev->_d_ident_));
				}

				va_end(args);
				_LOG(LMT_INFO, "init device root");
				r = VX_OK;
			}
			break;
		case DEVCTL_SUSPEND:
		case DEVCTL_RESUME:
			r = VX_OK;
			break;
	}

	return r;
}

static HMUTEX lock_dev(_vx_dev_t *p_dev, HMUTEX hlock) {
	HMUTEX r = 0;

	if(p_dev->_d_hcmutex_) {
		_p_data_t pd_mutex = HC_DATA(p_dev->_d_hcmutex_);
		_i_mutex_t *pi_mutex = HC_INTERFACE(p_dev->_d_hcmutex_);

		if(pd_mutex && pi_mutex)
			r = pi_mutex->lock(pd_mutex, hlock);
	}

	return r;
}

static HMUTEX lock(HDEV dev, HMUTEX hlock) {
	return lock_dev(dev_ptr(dev), hlock);
}

static void unlock_dev(_vx_dev_t *p_dev, HMUTEX hlock) {
	if(p_dev->_d_hcmutex_) {
		_p_data_t pd_mutex = HC_DATA(p_dev->_d_hcmutex_);
		_i_mutex_t *pi_mutex = HC_INTERFACE(p_dev->_d_hcmutex_);

		if(pd_mutex && pi_mutex)
			pi_mutex->unlock(pd_mutex, hlock);
	}
}

static void unlock(HDEV dev, HMUTEX hlock) {
	unlock_dev(dev_ptr(dev), hlock);
}

static HDEV add_to_list(_vx_dev_t *p_dev) {
	HDEV r = NULL;
	if(g_hc_llist) {
		_i_llist_t *pi_list = HC_INTERFACE(g_hc_llist);
		_p_data_t pd_list = HC_DATA(g_hc_llist);

		if(pi_list && pd_list)
			r = pi_list->add(pd_list, p_dev, sizeof(_vx_dev_t), 0);
	}
	return r;
}

static _bool attach_to_host(_vx_dev_t *p_host, _vx_dev_t *p_dev, HMUTEX hlock) {
	_bool r = _false;

	if(p_host && p_dev) {
		HMUTEX hml = lock_dev(p_host, hlock);
		if(!p_host->_d_node_) {
			/* allocate node memory */
			if(g_hc_heap) {
				_i_heap_t *pi_heap = HC_INTERFACE(g_hc_heap);
				_p_data_t pd_heap = HC_DATA(g_hc_heap);
				if(pi_heap && pd_heap) {
					if((p_host->_d_node_ = pi_heap->alloc(pd_heap, NODE_MEM, NO_ALLOC_LIMIT)))
						_g_pi_str->mem_set(p_host->_d_node_, 0, NODE_MEM);
				}
			}
		}
		if(p_host->_d_node_) {
			_u32 slot = p_dev->_d_slot_;
			if(slot < MAX_SLOTS) {
				if(p_host->_d_node_[slot]) {
					/* requested slot is not empty.
					   Try to find another slot */
					_u32 i = slot;
					for(; i < MAX_SLOTS; i++) {
						if(p_host->_d_node_[i] == NULL) {
							slot = i;
							break;
						}
					}
				}
				if(slot < MAX_SLOTS) {
					p_host->_d_node_[slot] = p_dev;
					p_dev->_d_slot_ = slot;
					p_dev->_d_host_ = p_host;
					p_dev->_d_state_ |= DSTATE_ATTACHED;
					r = _true;
				}
			}
		}
		unlock_dev(p_host, hml);
	}

	return r;
}

static void detach_from_host(_vx_dev_t *p_dev, HMUTEX hlock) {
	if(p_dev) {
		_vx_dev_t *p_host = NULL;
		if((p_host = p_dev->_d_host_)) {
			HMUTEX hl = lock_dev(p_host, hlock);
			if(p_host->_d_node_[p_dev->_d_slot_] == p_dev) {
				p_host->_d_node_[p_dev->_d_slot_] = NULL;
				p_dev->_d_slot_ = INVALID_DEV_SLOT;
				p_dev->_d_state_ &= ~DSTATE_ATTACHED;
			}
			unlock_dev(p_host, hl);
		}
	}
}

static void sys_update(void) {
	if(g_hc_llist) {
		_i_llist_t *pi_list = HC_INTERFACE(g_hc_llist);
		_p_data_t pd_list  = HC_DATA(g_hc_llist);

		if(pi_list && pd_list) {
			/* search in devices list to find pending devices */
			_u32 sz = 0;
			HMUTEX hl = pi_list->lock(pd_list, 0);
			_vx_dev_t *p_dev = pi_list->first(pd_list, &sz, hl);
			if(p_dev) {
				do {
					HMUTEX hm = lock_dev(p_dev, 0);
					if(p_dev->_d_state_ & DSTATE_PENDING) {
						if(!(p_dev->_d_state_ & DSTATE_INITIALIZED)) {
							if(p_dev->_d_ctl_) {
								pi_list->unlock(pd_list, hl);
								if(p_dev->_d_ctl_(DEVCTL_INIT, &_g_i_dev_root, p_dev) == VX_OK)
									p_dev->_d_state_ |= DSTATE_INITIALIZED;
								hl = pi_list->lock(pd_list, 0);
								pi_list->sel(pd_list, p_dev, hl);
							}
						}
						if(p_dev->_d_host_ && !(p_dev->_d_state_ & DSTATE_ATTACHED) &&
									(p_dev->_d_state_ & DSTATE_INITIALIZED)) {
							if(attach_to_host(p_dev->_d_host_, p_dev, hm)) {
								p_dev->_d_state_ &= ~DSTATE_PENDING;
								LOG(LMT_INFO, "[sys update] attach '%s' slot=%d",
										p_dev->_d_ident_, p_dev->_d_slot_);
							}
						}
					}
					unlock_dev(p_dev, hm);
				}while((p_dev = pi_list->next(pd_list, &sz, hl)));
			}
			pi_list->unlock(pd_list, hl);
		}
	}
}

static HDEV create_device(_ctl_t *p_ctl, HDEV host, _p_data_t data) {
	HDEV r = NULL;
	_vx_dev_t tmp;
	_vx_dev_t *_r = NULL;

	_g_pi_str->mem_set(&tmp, 0, sizeof(_vx_dev_t));
	tmp._d_ctl_ 	= p_ctl;
	tmp._d_host_ 	= host;
	tmp._d_data_	= data;
	tmp._d_hcmutex_ = (_g_pi_repo)?_g_pi_repo->create_context_by_interface(I_MUTEX):NULL;
	/* because 0 is a valid slot value */
	tmp._d_slot_	= INVALID_DEV_SLOT;

	if((_r = (_vx_dev_t *)add_to_list(&tmp))) {
		r = _r;
		/* initialize driver */
		_r->_d_state_ |= DSTATE_PENDING;
		if(p_ctl(DEVCTL_INIT, &_g_i_dev_root, _r) == VX_OK)
			_r->_d_state_ |= DSTATE_INITIALIZED;

		if((_r->_d_state_ & DSTATE_INITIALIZED) && _r->_d_host_) {
			/* attach to host */
			if(attach_to_host(_r->_d_host_, _r, 0)) {
				_r->_d_state_ &= ~DSTATE_PENDING;
				LOG(LMT_INFO, "[reg driver] '%s' slot=%d", _r->_d_ident_, _r->_d_slot_);
				sys_update();
			}
		} else {
			if(_g_pi_str->str_cmp(_r->_d_ident_, DEV_ROOT_IDENT) != 0)
				/* skyp device root because it can't be attached to another host */
				LOG(LMT_WARNING, "[reg driver] '%s' pending ...", _r->_d_ident_);
		}
	}

	return r;
}

static HDEV create_device_from_context(HCONTEXT hc, HDEV host) {
	return create_device(hc->_c_mod_->_m_ctl_, host, HC_DATA(hc));
}

static void remove_from_list(_vx_dev_t *p_dev) {
	if(g_hc_llist) {
		_i_llist_t *pi_list = HC_INTERFACE(g_hc_llist);
		_p_data_t pd_list  = HC_DATA(g_hc_llist);

		if(pi_list && pd_list) {
			HMUTEX hl = pi_list->lock(pd_list, 0);
			if(pi_list->sel(pd_list, p_dev, hl))
				pi_list->del(pd_list, hl);
			pi_list->unlock(pd_list, hl);
		}
	}
}

static void free_dnode(_vx_dev_t *p_dev, HMUTEX hlock) {
	if(p_dev->_d_node_ && g_hc_heap) {
		HMUTEX hm = lock_dev(p_dev, hlock);
		_i_heap_t *pi_heap = HC_INTERFACE(g_hc_heap);
		_p_data_t pd_heap = HC_DATA(g_hc_heap);
		if(pi_heap && pd_heap) {
			/* release node memory */
			pi_heap->free(pd_heap, p_dev->_d_node_, NODE_MEM);
			p_dev->_d_node_ = NULL;
		}
		unlock_dev(p_dev, hm);
	}
}

static void release_dmutex(_vx_dev_t *p_dev) {
	if(p_dev->_d_hcmutex_ && _g_pi_repo) {
		_g_pi_repo->release_context(p_dev->_d_hcmutex_);
		p_dev->_d_hcmutex_ = NULL;
	}
}

static _u32 remove_device(HDEV dev, HMUTEX hlock) {
	_u32 r = VX_DEVICE_BUSY;
	_u32 i = 0;
	_vx_dev_t *p_dev = dev_ptr(dev);
	HMUTEX hm = lock_dev(p_dev, hlock);

	for(; i < MAX_SLOTS; i++) {
		_vx_dev_t *p = dev_ptr(p_dev->_d_node_[i]);
		if(p) {
			if(p->_d_ctl_(DEVCTL_UNINIT, &_g_i_dev_root, p) == VX_OK) {
				p->_d_state_ &= ~DSTATE_INITIALIZED;
				detach_from_host(p, hm);
				p->_d_state_ |= DSTATE_PENDING;
			}
		}
	}

	unlock_dev(p_dev, hm);

	if((r = p_dev->_d_ctl_(DEVCTL_UNINIT, &_g_i_dev_root, p_dev)) == VX_OK) {
		hm = lock_dev(p_dev, hlock);
		p_dev->_d_state_ &= ~DSTATE_INITIALIZED;
		detach_from_host(p_dev, hm);
		if(!(p_dev->_d_state_ & DSTATE_ATTACHED)) {
			free_dnode(p_dev, hm);
			unlock_dev(p_dev, hm);
			release_dmutex(p_dev);
			remove_from_list(p_dev);
		} else {
			r = VX_ERR;
			p_dev->_d_state_ |= DSTATE_PENDING;
			unlock_dev(p_dev, hm);
		}
	}

	if(r != VX_OK)
		/* rollback */
		sys_update();

	return r;
}

static _u32 remove_device_node(HDEV dev, HMUTEX hlock) {
	_u32 r = VX_DEVICE_BUSY;
	_u32 i = 0;
	_vx_dev_t *p_dev = dev_ptr(dev);
	HMUTEX hm = lock_dev(p_dev, hlock);

	for(; i < MAX_SLOTS; i++) {
		_vx_dev_t *p = dev_ptr(p_dev->_d_node_[i]);
		if(p)
			remove_device_node(p, hm);
	}

	unlock_dev(p_dev, hm);

	if(p_dev->_d_ctl_(DEVCTL_UNINIT, &_g_i_dev_root, p_dev) == VX_OK) {
		hm = lock_dev(p_dev, hlock);
		p_dev->_d_state_ &= ~DSTATE_INITIALIZED;
		detach_from_host(p_dev, hm); /* detach */
		if(!(p_dev->_d_state_ & DSTATE_ATTACHED)) {
			free_dnode(p_dev, hm);
			release_dmutex(p_dev);
			unlock_dev(p_dev, hm);
			remove_from_list(p_dev);
			r = VX_OK;
		} else {
			p_dev->_d_state_ |= DSTATE_PENDING;
			unlock_dev(p_dev, hm);
		}
	}

	if(r != VX_OK)
		/* rollback */
		sys_update();

	return r;
}

static HDEV request_device(_dev_request_t *p_dev_req, HDEV dev_start, HMUTEX hlock) {
	HDEV r = NULL;

	_vx_dev_t *p_sdev = (dev_start)?dev_ptr(dev_start):_g_p_dev_root;

	if(p_sdev) {
		HMUTEX hm = lock_dev(p_sdev, hlock);

		if(p_dev_req->flags & DRF_IDENT) {
			if(_g_pi_str->str_cmp((_str_t)p_sdev->_d_ident_, (_str_t)p_dev_req->_d_ident_) != 0)
				goto _continue_;
		}
		if(p_dev_req->flags & DRF_TYPE) {
			if(p_sdev->_d_type_ != p_dev_req->_d_type_)
				goto _continue_;
		}
		if(p_dev_req->flags & DRF_MODE) {
			if(p_sdev->_d_mode_ != p_dev_req->_d_mode_)
				goto _continue_;
		}
		if(p_dev_req->flags & DRF_CLASS) {
			if(p_sdev->_d_class_ != p_dev_req->_d_class_)
				goto _continue_;
		}
		if(p_dev_req->flags & DRF_SLOT) {
			if(p_sdev->_d_slot_ != p_dev_req->_d_slot_)
				goto _continue_;
		}

		unlock_dev(p_sdev, hm);
		return p_sdev;
_continue_:
		if(p_sdev->_d_node_) {
			_u32 i = 0;
			for(; i < MAX_SLOTS; i++) {
				if(p_sdev->_d_node_[i]) {
					if((r = request_device(p_dev_req, p_sdev->_d_node_[i], hlock)))
						break;
				}
			}
		}
		unlock_dev(p_sdev, hm);
	}
	return r;
}

static _u32 dev_read(HDEV h, _ulong offset, _u32 nb, _p_data_t buffer, HMUTEX hlock) {
	_u32 r = 0;
	_vx_dev_t *p_dev = dev_ptr(h);
	if(p_dev) {
		_dev_io_t dio = {
			.offset = offset, .size = nb, .buffer = buffer
		};
		HMUTEX hm = lock_dev(p_dev, hlock);
		p_dev->_d_ctl_(DEVCTL_READ, p_dev, &dio);
		r = dio.result;
		unlock_dev(p_dev, hm);
	}
	return r;
}

static _u32 dev_write(HDEV h, _ulong offset, _u32 nb, _p_data_t buffer, HMUTEX hlock) {
	_u32 r = 0;
	_vx_dev_t *p_dev = dev_ptr(h);
	if(p_dev) {
		_dev_io_t dio = {
			.offset = offset, .size = nb, .buffer = buffer
		};
		HMUTEX hm = lock_dev(p_dev, hlock);
		p_dev->_d_ctl_(DEVCTL_WRITE, p_dev, &dio);
		r = dio.result;
		unlock_dev(p_dev, hm);
	}
	return r;
}

static _vx_res_t dev_get_config(HDEV h, _p_data_t cfg, HMUTEX hlock) {
	_vx_res_t r = VX_ERR;
	_vx_dev_t *p_dev = dev_ptr(h);
	if(p_dev) {
		HMUTEX hm = lock_dev(p_dev, hlock);
		r = p_dev->_d_ctl_(DEVCTL_GET_CONFIG, cfg);
		unlock_dev(p_dev, hm);
	}
	return r;
}
static _vx_res_t dev_set_config(HDEV h, _p_data_t cfg, HMUTEX hlock) {
	_vx_res_t r = VX_ERR;
	_vx_dev_t *p_dev = dev_ptr(h);
	if(p_dev) {
		HMUTEX hm = lock_dev(p_dev, hlock);
		r = p_dev->_d_ctl_(DEVCTL_SET_CONFIG, cfg);
		unlock_dev(p_dev, hm);
	}
	return r;
}

static _u32 suspend_dev_node(HDEV dev, HMUTEX hlock) {
	_u32 r = VX_ERR;
	_vx_dev_t *p_dev = dev_ptr(dev);

	if(p_dev) {
		_u32 i = 0;

		HMUTEX hm = lock_dev(p_dev, hlock);
		for(; i < MAX_SLOTS; i++) {
			if(p_dev->_d_node_[i])
				suspend_dev_node(p_dev->_d_node_[i], hlock);
		}
		unlock_dev(p_dev, hm);

		if(p_dev->_d_ctl_) {
			if((r = p_dev->_d_ctl_(DEVCTL_SUSPEND, p_dev)) == VX_OK)
				p_dev->_d_state_ |= DSTATE_SUSPEND;
		}
	}

	return r;
}

static _u32 resume_dev_node(HDEV dev, HMUTEX hlock) {
	_u32 r = VX_ERR;
	_vx_dev_t *p_dev = dev_ptr(dev);

	if(p_dev) {
		if(p_dev->_d_ctl_) {
			if((r = p_dev->_d_ctl_(DEVCTL_RESUME, p_dev)) == VX_OK) {
				HMUTEX hm = lock_dev(p_dev, hlock);
				_u32 i = 0;

				p_dev->_d_state_ &= ~DSTATE_SUSPEND;
				for(; i < MAX_SLOTS; i++) {
					if(p_dev->_d_node_[i])
						resume_dev_node(p_dev->_d_node_[i], hlock);
				}

				unlock_dev(p_dev, hm);
			}
		}
	}
	return r;
}

DEF_VXMOD(
	MOD_DEV_ROOT,
	I_DEV_ROOT,
	&_g_i_dev_root,
	NULL, /* no static data context */
	0, /* no static data size */
	_mod_ctl_, /* mod controll */
	1,0,1,
	"device root"
);

