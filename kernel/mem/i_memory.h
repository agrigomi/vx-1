#ifndef __I_MEMORY_H__
#define __I_MEMORY_H__

#include "mgtype.h"
#include "i_sync.h"

#define I_PMA		"i_pma"
#define I_HEAP		"i_heap"
#define I_LLIST		"i_llist"
#define I_REGISTER	"i_register"
#define I_RING_BUFFER	"i_ring_buffer"
#define I_BUFFER_MAP	"i_buffer_map"
#define I_ENV		"i_env"
#define I_VMM		"i_vmm"

/* default HI address limit */
#define NO_ALLOC_LIMIT	0xffffffffffffffffLLU

/* type of ram allocation */
#define RT_SYSTEM	0x01
#define RT_USER		0x03

typedef struct {
	_u16 	ps;	/* page size */
	_u32 	pr;	/* ram pages (all available pages) */
	_u32 	pu;	/* pages in use */
	_u32	pp;	/* PMA pages (reserved by PMA algorithm) */
}_ram_info_t;

typedef struct { /* physical memory allocator */
	/* sequentially alloc */
	void *(*alloc_seq)(_u32 npages, _u8 type, _ulong limit);

	/* sequentially free, return number of freed pages */
	_u32 (*free_seq)(void *ptr, _u32 npages);

	/* non sequentially alloc */
	_u32 (*alloc_non_seq)(/* in */_u32 npages, /* number of pages to alloc */
			   /* out */_ulong *page_array, /* array of page addresses in PM */
			   /* in */_u8 type,
		  	   /* in */ _ulong limit);

	/* non sequentially free */
	_u32 (*free_non_seq)(_u32 npages, _ulong *page_array);

	/* get PMA state */
	void (*info)(_ram_info_t *p_info);
}_i_pma_t;

#ifdef _64_
typedef _u64 _vaddr_t;
#else
typedef _u32 _vaddr_t;
#endif

/* mapping flags */
#define VMMF_NOT_CACHEABLE	(1<<0)
#define VMMF_USER_ACCESS	(1<<1)
#define VMMF_WRITABLE		(1<<2)
#define VMMF_PRESENT		(1<<3)
#define VMMF_PMA		(1<<4) /* allocated by PMA */

/* VMM page size */
enum {
	VMPS_4K = 1,
	VMPS_8K,
	VMPS_16K,
	VMPS_32K,
	VMPS_64K,
	VMPS_2M
};

typedef _p_data_t _vmm_cxt_t;

typedef struct { /* mapping page info */
	_vaddr_t	vaddr; /* virtual address */
	_ulong		paddr; /* physical address */
	_u8		vmps;  /* page size (enum) */
	_u8		flags; /* mapping flags */
}_page_info_t;

typedef struct {
	/* initialize object */
	void (*init)(_vmm_cxt_t, /* VMM data context */
			_vaddr_t mapping /* physical address of mapping table or NULL*/
			);
	void (*activate)(_vmm_cxt_t);
	/* mapping physical pages in virtual address translation table &
		return the number of actualy mapped pages */
	_u32 (*map)(_vmm_cxt_t, /* VMM data context */
			_page_info_t *p_mpi, /* page info array */
			_u32 nmpi, /* number of structures */
			HMUTEX hlock);
	/* remove mapping from 'vaddr_start' to 'vaddr_end' */
	_u32 (*unmap)(_vmm_cxt_t, /* vmm data context */
			_vaddr_t vaddr_start, /* start address */
			_vaddr_t vaddr_end, /* end address */
			HMUTEX hlock);
	/* fill mapping array and return actual number of pages */
	_u32 (*array)(_vmm_cxt_t, /* vmm data context */
			_vaddr_t vaddr_start, /* start virtual address */
			_vaddr_t vaddr_end, /* end virtual address */
			_page_info_t *p_mpi, /* array of page info structures */
			_u32 nmpi, /* number of reserved structures */
			HMUTEX hlock);
	/* mapping test */
	_bool (*test)(_vmm_cxt_t, /* vmm data context */
			_vaddr_t vaddr, /* virtual address */
			_page_info_t *p_mpi, /* [out] */
			HMUTEX hlock);
	/* change mapping flags */
	void (*set_flags)(_vmm_cxt_t, /* data context */
			_vaddr_t vaddr_start, /* start address */
			_vaddr_t vaddr_end, /* end address */
			_u8 flags, /* mapping flags */
			HMUTEX hlock);
	HMUTEX (*lock)(_vmm_cxt_t, HMUTEX);
	void (*unlock)(_vmm_cxt_t, HMUTEX);
}_i_vmm_t;

typedef struct {
	_u64 base; /* base address */
	_u32 size; /* total heap size in bytes */
	_u32 chunk_size; /* size of metadata chunk */
	_u32 data_load; /* data load in bytes */
	_u32 meta_load; /* metadata load in bytes */
	_u32 free; /* free space in bytes */
	_u32 unused; /* unused space in bytes (for static allocated) */
	_u32 objects; /* number of objects in heap */
} _heap_info_t;

typedef struct { /* heap allocator */
	/* used only for atatic heap allocation (can be NULL) */
	void (*init)(_p_data_t hd, void *heap_base, _u32 heap_size);

	/* byte allocator */
	void *(*alloc)(_p_data_t hd, _u32 size, _ulong limit);

	/* free allocated memory
	* (size parameter is needed to improve performance, but can be 0)
	*/
	void (*free)(_p_data_t hd, void *ptr, _u32 size);

	/* return 1 if pointer currently present in heap, else 0 */
	_bool (*verify)(_p_data_t hd, void *ptr, _u32 size);

	/* heap status */
	void (*info)(_p_data_t hd, _heap_info_t *p_infp);
}_i_heap_t;

#define LLIST_VECTOR	1
#define LLIST_QUEUE	2

typedef struct {
	/* set list mode (by default is LLIST_VECTOR) */
	void (*init)(_p_data_t ld, _u8 mode, _u8 ncol, _ulong addr_limit);

	/* get record by index */
	void *(*get)(_p_data_t ld, _u32 index, _u32 *p_size, HMUTEX hlock);

	/* add new record at end of list */
	void *(*add)(_p_data_t ld, void *p_data, _u32 size, HMUTEX hlock);

	/* insert record at position*/
	void *(*ins)(_p_data_t ld, _u32 index, void *p_data, _u32 size, HMUTEX hlock);

	/* remove record by index */
	void  (*rem)(_p_data_t ld, _u32 index, HMUTEX hlock);

	/* delete current record */
	void  (*del)(_p_data_t ld, HMUTEX hlock);

	/* remove all records */
	void  (*clr)(_p_data_t ld, HMUTEX hlock);

	/* number of records */
	_u32  (*cnt)(_p_data_t ld, HMUTEX hlock);

	/* select column */
	void  (*col)(_p_data_t ld, _u8 col, HMUTEX hlock);

	/* select 'p_data' sa current record (if it's a part of storage, return 1) */
	_u8   (*sel)(_p_data_t ld, void *p_data, HMUTEX hlock);

	/* move between columns (return 1 if success) */
	_u8   (*mov)(_p_data_t ld, void *p_data, _u8 col, HMUTEX hlock);

	/* get next (from current) record */
	void *(*next)(_p_data_t ld, _u32 *p_size, HMUTEX hlock);

	/* pointer to current record */
	void *(*current)(_p_data_t ld, _u32 *p_size, HMUTEX hlock);

	/* get pointer to first record */
	void *(*first)(_p_data_t ld, _u32 *p_size, HMUTEX hlock);

	/* get pointer to last record */
	void *(*last)(_p_data_t ld, _u32 *p_size, HMUTEX hlock);

	/* get pointer to prev. (from current) record */
	void *(*prev)(_p_data_t ld, _u32 *p_size, HMUTEX hlock);

	/* queue mode specific (last-->first-->seccond ...) */
	void (*roll)(_p_data_t ld, HMUTEX hlock);

	HMUTEX (*lock)(_p_data_t ld, HMUTEX hlock);
	void (*unlock)(_p_data_t ld, HMUTEX hlock);
}_i_llist_t;

#define INVALID_REG_INDEX	0xffffffff
typedef _u32	_reg_idx_t;

typedef struct { /* data register */
	/* initialize register */
	void (*init)(_p_data_t,
			_u32 data_size,	/* size of data chunk in bytes */
			_ulong addr_limit, /* address limit  */
			_u32 count	/* initial number of items */
			);
	/* add data chunk to register (return index) */
	_reg_idx_t (*add)(_p_data_t, /* register context */
			_p_data_t, /* data to add */
			HMUTEX /* mutex handle */
			);
	/* get pointer to data chunk by index */
	_p_data_t (*get)(_p_data_t,	/* register context */
			_reg_idx_t,	/* index of data chunk */
			HMUTEX		/* mutex handle */
			);
	/* delete data chunk from register */
	void (*del)(_p_data_t,		/* register context */
			_reg_idx_t,	/* index of data chunk */
			HMUTEX		/* mutex handle */
			);
	HMUTEX (*lock)(_p_data_t, HMUTEX);
	void (*unlock)(_p_data_t, HMUTEX);
}_i_reg_t;

/* ring buffer flags */
#define RBF_SYNC	1

typedef struct { /* ring buffer */
	void (*init)(_p_data_t, _u32 capacity, _u8 flags);
	void (*destroy)(_p_data_t);
	void (*push)(_p_data_t, void *data, _u16 size);
	void *(*pull)(_p_data_t, _u16 *psize); /* return NULL as end of pull */
	void (*reset_pull)(_p_data_t); /* set pull position at begin of buffer */
}_i_ring_buffer_t;

#define INVALID_BUFFER_ID	0xffffffff
#define INVALID_BUFFER_KEY	0xffffffffffffffffLLU

typedef _u32 _bid_t; /* buffer ID */
/* buffer I/O callback prototype */
typedef _vx_res_t _bio_t(_u64 key, _p_data_t buffer, _p_data_t udata);

typedef struct { /* buffer map */
	void (*init)(_p_data_t, /* context data */
			_u32 buffer_size, /* buffer size in bytes */
			_bio_t *pcb_read, /* buffer read callback */
			_bio_t *pcb_write, /* buffer write callback */
			void *udata
			);
	/* alloc new buffer */
	_bid_t (*alloc)(_p_data_t, /* data context */
			_u64, /* buffer key */
			HMUTEX
			);
	/* release buffer */
	void (*free)(_p_data_t,
			_bid_t, /* buffer ID */
			HMUTEX
			);
	/* get pointer to buffer content */
	_p_data_t (*ptr)(_p_data_t, /* data_context */
			_bid_t, /* buffer ID */
			HMUTEX
			);
	/* get buffer key by ID */
	_u64 (*key)(_p_data_t, /* data context */
			_bid_t, /* buffer ID */
			HMUTEX
			);
	/* save buffer */
	void (*flush)(_p_data_t, /* data context */
			_bid_t, /* buffer ID */
			HMUTEX
			);
	/* set dirty flag */
	void (*dirty)(_p_data_t, /* data context */
			_bid_t, /* buffer ID */
			HMUTEX
			);
	/* reset content when buffer dirty */
	void (*reset)(_p_data_t, /* data context */
			_bid_t, /* buffer ID */
			HMUTEX
			);
	/* save all dirty buffers */
	void (*flush_all)(_p_data_t, /* data context */
			HMUTEX
			);
	/* reset all buffers */
	void (*reset_all)(_p_data_t, /* data context */
			HMUTEX
			);
	HMUTEX (*lock)(_p_data_t, HMUTEX);
	void (*unlock)(_p_data_t, HMUTEX);
}_i_buffer_map_t;

typedef struct {
	_str_t (*get)(_p_data_t, _cstr_t var); /* get variable value by name */
	void (*set)(_p_data_t, _cstr_t var, _str_t val); /* set variable/value */
	void (*clr)(_p_data_t); /* clear environment memory */
}_i_env_t;

#endif

