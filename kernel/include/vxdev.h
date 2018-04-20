#ifndef __VXDEV_H__
#define __VXDEV_H__

#include "vxmod.h"

/* common device type */
#define DTYPE_VBUS	0	/* virtual  bus */
#define DTYPE_BUS	1	/* system bus */
#define DTYPE_CTRL	2	/* controller */
#define DTYPE_DEV	3	/* device */
#define DTYPE_VOL	4	/* volume */

/* device I/O mode */
#define DMODE_CHAR	1
#define DMODE_BLOCK	2
#define DMODE_LAN	3

/* device class */
#define DCLASS_CDROM	1 	/* CD/DVD drive */
#define DCLASS_HDD	2 	/* hard disk drive */
#define DCLASS_DISPLAY	3 	/* video adapter */
#define DCLASS_FDC	4 	/* floppy controller */
#define DCLASS_FDD	5 	/* floppy disk drive */
#define DCLASS_HDC	6 	/* hard disk controller */
#define DCLASS_HID	7 	/* USB device (human interface) */
#define DCLASS_1394	8 	/* IEEE 1394 host controller */
#define DCLASS_IMG	9 	/* Cameras and scanners */
#define DCLASS_KBD	10 	/* keyboard */
#define DCLASS_MODEM	11 	/* modems */
#define DCLASS_MOUSE	12 	/* pointing devices */
#define DCLASS_MEDIA	13 	/* Audio and video devices */
#define DCLASS_NET	14 	/* network adapter */
#define DCLASS_PORT	15 	/* serial and paralel port */
#define DCLASS_SCSI	16 	/* SCSI and RAID controllers */
#define DCLASS_SYS	17 	/* System buses, bridges, etc (BUS or VBUS) */
#define DCLASS_USB	18 	/* USB host controllers and hubs */
#define DCLASS_RDD	19 	/* RAM disk drive */
#define DCLASS_UMS	20 	/* USB mass storage */

/* limits */
#define INVALID_DEV_SLOT	0xff
#define MAX_DRIVER_IDENT	16
#define MAX_VOLUME_SERIAL	16
#define MAX_BUS_TYPE		6
#define MAX_CTRL_TYPE		6
#define MAX_LAN_MAC		6

/* status */
#define DSTATE_INITIALIZED	(1<<0)
#define DSTATE_ATTACHED		(1<<1)
#define DSTATE_SUSPEND		(1<<2)
#define DSTATE_PENDING		(1<<3)
#define DSTATE_BUSY		(1<<4)

#define HDEV	_p_data_t

typedef struct { /* _vx_dev_t */
	_u8		_d_type_ :4;	/* device type */
	_u8		_d_mode_ :4;	/* device I/O mode */
	_u8		_d_class_;	/* device class */
	_u8		_d_slot_;	/* driver index in host node */
	_u8		_d_state_;	/* device status */
	_s8		_d_ident_[MAX_DRIVER_IDENT]; /* ident string */
	_ctl_t		*_d_ctl_;	/* pointer to device controll */
	_p_data_t	_d_data_;	/* driver data context */
	HCONTEXT	_d_hcmutex_;
	HDEV		_d_host_;	/* host driver (down level driver) */
	HDEV		*_d_node_;	/* up level drivers (managed by dev root) */

	union {
		struct { /* BUS */
			_u8	_d_bus_id_;
			_u8	_d_bus_type_[MAX_BUS_TYPE];
		};
		struct { /* controller */
			_u8 	_d_ctrl_index_;	/* controller index */
			_u8	_d_ctrl_type_[MAX_CTRL_TYPE]; /* name of controller type */
		};
		struct { /* block device info */
			_u8	_d_block_dev_;	/* block device number */
			_u16	_d_block_size_;	/* physical block size in bytes */
			_u16	_d_block_max_io_;	/* maximum number of physical blocks per I/O operation */
			_u32	_d_block_dev_size_;	/* number of physical device blocks */
		};
		struct { /* volume info */
			_u8	_d_vol_number_;	/* seq. number, per block device */
			_u16	_d_vol_block_size_; /* physical block size */
			_u32	_d_vol_offset_;	/* first volume block (dep. by device block size) */
			_u32	_d_vol_size_;	/* size of volume in device blocks */
			_s8	_d_vol_serial_[MAX_VOLUME_SERIAL]; /* serial number */
		};
		struct { /* character device info */
			_u8	_d_char_dev_;	/* char device number */
			_u8	_d_char_irq_;
			_u16	_d_char_flags_;
		};
		struct { /* lan info */
			_u8	_d_lan_dev_;	/* lan device number */
			_u8	_d_lan_irq_;
			_u8	_d_lan_mac_[MAX_LAN_MAC];	/* MAC address */
		};
	};
}_vx_dev_t;

typedef struct {
	_u8		flags;
	_u8		_d_type_ :4;
	_u8		_d_mode_ :4;
	_u8		_d_class_;
	_u8		_d_slot_;
	_cstr_t		_d_ident_;
}_dev_request_t;

typedef struct {
	_ulong		offset; /* offset at begin of device memory */
	_u32		size; /* number of bytes to transfer */
	_p_data_t	buffer;
	_u32		result; /* number of actual trans. bytes */
}_dev_io_t;

/* device request flags */
#define DRF_IDENT	(1<<3)
#define DRF_TYPE	(1<<4)
#define DRF_MODE	(1<<5)
#define DRF_CLASS	(1<<6)
#define DRF_SLOT	(1<<7)

/* device controll commands */
#define DEVCTL_INIT		101	/* rags: _i_dev_root_t*, _vx_dev_t* 	*/
#define DEVCTL_UNINIT		102	/* args: _i_dev_root_t*, _vx_dev_t* 	*/
#define DEVCTL_SUSPEND		103	/* args: _vx_dev_t* 			*/
#define DEVCTL_RESUME		104	/* args: _vx_dev_t* 			*/
#define DEVCTL_READ		105	/* args: _vx_dev_t*, _dev_io_t*		*/
#define DEVCTL_WRITE		106	/* args: _vx_dev_t*, _dev_io_t*		*/
#define DEVCTL_GET_CONFIG	107	/* args: _vx_dev_t*, _p_data_t  	*/
#define DEVCTL_SET_CONFIG	108	/* args: _vx_dev_t*, _p_data_t		*/
/* ... */

#endif

