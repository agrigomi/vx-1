#ifndef __I_DEV_ROOT_H__
#define __I_DEV_ROOT_H__

#include "mgtype.h"
#include "vxdev.h"
#include "i_sync.h"

#define I_DEV_ROOT	"i_device_root"

typedef struct {
	/* cteate device entry (host and data, can be NULL) */
	HDEV (*create)(_ctl_t *p_dev_ctl, HDEV host, _p_data_t data);
	/* create device entry from module context */
	HDEV (*create_from_context)(HCONTEXT hc, HDEV host);
	/* remove device entry (return VX_OK if success ) */
	_vx_res_t (*remove)(HDEV dev, HMUTEX);
	/* remove device node (return VX_OK if success ) */
	_vx_res_t (*remove_node)(HDEV dev, HMUTEX);
	/* update device tree (reinit, reattach all pending drivers) */
	void (*update)(void);
	/* find driver by driver request struct and start point.
	  start point NULL = dev_root */
	HDEV (*request)(_dev_request_t *p_dev_req, HDEV start, HMUTEX);
	/* return handle of device root */
	HDEV (*get_hdev)(void);
	/* return pointer to common device struct. by HDEV */
	_vx_dev_t *(*devptr)(HDEV);
	/* suspend and resume device node */
	_vx_res_t (*suspend_node)(HDEV dev, HMUTEX);
	_vx_res_t (*resume_node)(HDEV dev, HMUTEX);
	/* read/write */
	_u32 (*read)(HDEV, _ulong offset, _u32 nb, _p_data_t buffer, HMUTEX);
	_u32 (*write)(HDEV, _ulong offset, _u32 nb, _p_data_t buffer, HMUTEX);
	/* configure device */
	_vx_res_t (*get_config)(HDEV, _p_data_t, HMUTEX);
	_vx_res_t (*set_config)(HDEV, _p_data_t, HMUTEX);
	/* lock/unlock device mutex */
	HMUTEX (*lock)(HDEV, HMUTEX);
	void (*unlock)(HDEV, HMUTEX);
}_i_dev_root_t;

#endif

