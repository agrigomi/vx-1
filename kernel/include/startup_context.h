#ifndef __I_KERNEL__
#define __I_KERNEL__

#define BOOT_MGFS	1
#define BOOT_CDFS	2

#define MEM_TYPE_FREE		0x01
#define MEM_TYPE_RESERVED	0x02
#define MEM_TYPE_UNKNOWN	0x03
#define MEM_TYPE_BOOT_CODE	0x10
#define MEM_TYPE_CORE_CODE	0x11
#define MEM_TYPE_STACK		0x12
#define MEM_TYPE_PAGE_MAP	0x21
#define MEM_TYPE_PAGE_MAP_32	0x22
#define MEM_TYPE_PAGE_MAP_64	0x23

#define PMA_PAGE_SIZE		4096
#define MAX_MEM_TAGS		32


typedef struct {
	union {
		_u64	address;	
		struct {
			_u32	addr_lo;	/* base address */
			_u32    addr_hi;
		}__attribute__((packed));
	}__attribute__((packed));
	
	union {
		_u64 size;
		struct {
			_u32	size_lo;	/* size in bytes */
			_u32	size_hi;	
		}__attribute__((packed));
	}__attribute__((packed));
	
	_u32	type;		/* memory type */
}__attribute__((packed)) _mem_tag_t;

typedef union {
	struct {
		_s8	vendor[12];
		_u8	iapic_id;	/* initial APIC ID */
		_u32 	ncore;	/* number of cores per socket */
		_u32 	nlcpu;	/* number of logical CPUs per socket */
		_u32	p_gdt; /* Global Descriptor Table */
		_u16	gdt_limit; /* size of GDT in bytes */
		_u16	code_selector;
		_u16	data_selector;
		_u16	free_selector;
		_u32	cpu_init_vector_rm; /* real mode init vector */
		_u32	core_cpu_init_vector; /* core mode init vector */
		_u32 	core_cpu_init_data;   /* core cpu initialization parameter */
	}__attribute__((packed)) _x86;
}__attribute__((packed)) _cpu_type_t;

typedef struct {
	_u32 nsock;	/* number of sockets */
	_cpu_type_t _cpu;
}__attribute__((packed))_cpu_info_t;

typedef struct { /* kernel startup information */
 	_u8 		dev;		/* boot device number */
 	_u8		partition[16];	/* partition table entry */
 	_u8		fs_type;	/* boot file system type */
 	_u8		volume_serial[16]; /* boot volume serial number */
	_u64		p_kargs;	/* kernel parameters (pointer to NULL treminated string) */

/* 	paging info
* table must be completed so that physical addresses 
* match with virtual addresses
*/
	_u32		pt_address;	/* page table address (physical) */
	_u32		pt_page_size;	/* page size in bytes */
	_u32		pt_table_size;	/* table size in bytes */
	_u8		pt_type;	/* type of page table (architecture depend) */
	_u64		pt_range; 	/* range of virtual memory in bytes */
	_u64		vbase;		/* virtual base address for all available memory */
/**********************************/
	_u32		stack_ptr;	/* fixed stack pointer */
	_mem_tag_t	mmap[MAX_MEM_TAGS]; /* memory map array (free memory) */
	_u32		mm_cnt;		/* number of entries in memory map */
	_mem_tag_t	rmap[8];
	_u32		rm_cnt;
	_cpu_info_t	cpu_info;	/* CPU equipment */
	_u32		reserved;
} __attribute__((packed)) _core_startup_t;


#endif /* __I_KERNEL__ */
