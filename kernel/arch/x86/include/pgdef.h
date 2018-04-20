#ifndef __PML4_PGDEF__
#define __PML4_PGDEF__

#define PG_PML3	1
#define PG_PML4	2

/* PML4 4K 
 Page physical address to page number */
#define PML4_4K_PAGE_TO_NUM(addr) ((_u64)addr >> 12)
#define PML4_2M_PAGE_TO_NUM(addr) ((_u64)addr >> 21)

/* page number to physical address */
#define PML4_4K_NUM_TO_PAGE(num)  ((_u64)num << 12)
#define PML4_2M_NUM_TO_PAGE(num)  ((_u64)num << 21)

#define VBASE_32		0xf0000000
#define VBASE_64 		0xffffff8000000000LLU

typedef union { /* PML4E */
	_u64	e;
	struct { /* fields */
		_u64	p   :1; /* 0 -   present	*/
		_u64	rw  :1; /* 1 -   read/write	*/
		_u64	us  :1; /* 2 -   user/supervisor*/
		_u64	pwt :1; /* 3 - 			*/
		_u64	pcd :1; /* 4 -			*/
		_u64	a   :1; /* 5 -   accessed	*/
		_u64	ign :1; /* 6 -	 ignorred	*/
		_u64	mbz1:2; /* 7-8   reserved	*/
		_u64	avl1:3; /* 9-11  available	*/
		_u64	base:28;/* 12-39 base address	*/
		_u64	mbz2:12;/* 40-51 reserved	*/ 
		_u64	avl2:12;/* 52-63 available	*/
	} __attribute__((packed)) f;
}__attribute__((packed)) _lm_pml4e_t; /* Long Mode PML4 Entry */

typedef union { /* PDPE */
	_u64	e;
	struct { /* fields */
		_u64	p   :1; /* 0 -   present	*/
		_u64	rw  :1; /* 1 -   read/write	*/
		_u64	us  :1; /* 2 -   user/supervisor*/
		_u64	pwt :1; /* 3 - 			*/
		_u64	pcd :1; /* 4 -			*/
		_u64	a   :1; /* 5 -   accessed	*/
		_u64	ign :1; /* 6 -	 ignored	*/
		_u64	ps  :1; /* 7 -	 page size (0 = 4K,2M; 1 = 1G) */
		_u64	mbz1:1; /* 8     reserved (MBZ)	*/
		_u64	avl1:3; /* 9-11  available	*/
		_u64	base:28;/* 12-39 base address	*/
		_u64	mbz2:12;/* 40-51 reserved	*/
		_u64	avl2:12;/* 52-63 available	*/
	}__attribute__((packed)) f;
}__attribute__((packed)) _lm_pdpe_t; /* Long Mode Page Directory Pointer (PDPE) */

typedef union { /* PDE */
	_u64	e;
	struct { /* fields */
		_u64	p   :1; /* 0 -   present		*/
		_u64	rw  :1; /* 1 -   read/write		*/
		_u64	us  :1; /* 2 -   user/supervisor	*/
		_u64	pwt :1; /* 3 -				*/
		_u64	pcd :1; /* 4 -				*/
		_u64	a   :1; /* 5 -   accessed		*/
		_u64	d   :1; /* 6 -				*/	
		_u64    ps  :1; /* 7 -   1 = 2M, 0 = 4k page size */
		union {
			struct {
				_u64    g    :1; /* 8 -	ignored		*/
				_u64	avl  :3; /* 9-11  available	*/
				_u64	base :28;/* 12-39 4k base of PTE*/
			}__attribute__((packed)) _4k;
			struct {
				_u64    g   :1;	/* 8 -			*/
				_u64	avl :3; /* 9-11  available	*/
				_u64  	pat :1; /* 12    Page Attribute Table */
				_u64  	rez :8; /* 13-20 reserved (MBZ)	*/
				_u64  	base:19;/* 21-39 2m base	*/
			}__attribute__((packed)) _2m;
		}__attribute__((packed)) t;
		_u64	mbz :12;/* 40-51 reserved (MBZ)	*/
		_u64	avl :12;/* 52-63 available	*/
	} __attribute__((packed)) f;
}__attribute__((packed)) _lm_pde_t;

typedef union { /* PTE 4K */
	_u64	e; 	
	struct { /* fields	*/
		_u64	p   :1; /* 0 -   present	*/
		_u64	rw  :1; /* 1 -   read/write	*/
		_u64	us  :1; /* 2 -   user/supervisor*/
		_u64	pwt :1; /* 3 -			*/
		_u64	pcd :1; /* 4 -			*/
		_u64	a   :1; /* 5 -   accessed	*/
		_u64	d   :1; /* 6 -			*/
		_u64    pat :1; /* 7 -			*/
		_u64    g   :1;	/* 8 -			*/
		_u64	avl1:3; /* 9-11  available	*/
		_u64	base:28;/* 12-39 base address	*/
		_u64	mbz :12;/* 40-51 reserved	*/
		_u64	avl2:12;/* 52-63 available	*/
	}__attribute__((packed)) f;
}__attribute__((packed)) _lm_pte_t; /* Page Table Entry (PTE) 4K */

typedef union { /* CR3 long mode */
	_u64	e;
	struct { /* fields */
		_u64	ign1:3; /* 0-2 	 reserved		*/
		_u64	pwt :1; /* 3     Page Write Through	*/
		_u64	pcd :1;	/* 4 	 Page Cache Disable	*/
		_u64	ign2:7; /* 5-11  reserved		*/
		_u64	base:40;/* 12-51 base address		*/
		_u64	mbz :12;/* 52-63 reserved		*/
	}__attribute__((packed)) f;
}__attribute__((packed)) _lm_cr3_t; /* CR3 control register	*/

#endif

