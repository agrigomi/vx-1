#ifndef _DEBUG
#include "mgtype.h"
#include "bootx86.h"
#else
#include "mgtype.h"
#define BEGIN_CODE32
#define END_CODE32
#endif
#include "pgdef.h"
#include "startup_context.h"

#define LIMIT_TO_512G

#define PAGE2M		0x200000 /* 2M */
#define PAGE4K		0x1000
#define MAX_RECORDS	512	 /* max table entryes */
#define RECORD_SZ	8	 /* size of entry in bytes */

BEGIN_CODE32;
/* build PML4 address translation table for the entire memory in 2M pages */
void __attribute__((optimize("O0"))) _pm_build_pml4_2m(_pml4_info_t *p) {
	_lm_pml4e_t 	*p_pml4 = (_lm_pml4e_t *)p->pt_addr;
	_u8 		*p_pml4_byte = (_u8 *)p_pml4;

	_lm_pdpe_t	*p_pdp  = 0;
	_u8		*p_pdp_byte = 0;
	
	_lm_pde_t	*p_pd	= 0;
	_u8		*p_pd_byte = 0;

	/*_lm_pte_t	*p_pt	= 0;
	_u8		*p_pt_byte = 0; 
	
	_u32 ncpu = (p->p_sc->cpu_info.nsock * 
				p->p_sc->cpu_info._cpu._x86.ncore * p->p_sc->cpu_info._cpu._x86.nlcpu);
	*/
	_u32 pml4_pages = 1;/* ncpu; */
	_u32 pdp_pages;
	_u32 pd_pages;
	_u32 pt_pages;
	
	_u32 pdp_records;
	_u32 pd_records;
	_u32 pt_records;
	
	_u64 ram_sz = 0;
	/*_u16 ram_idx = 0; */
	_u16 ram_chunks = (_u8)p->p_sc->mm_cnt;
	_u64 ram_pages_2m = 0;
	_u64 ram_pages_4k = 0;
	_u64 ram_byte = 0;

	_u32 table_sz = 0;

	_u32 i, ipml4, ipdp, ipd;/*, icpu,ipt; */
	
	/* collect the ram size */
	_mem_tag_t *p_mmap = (_mem_tag_t *)p->p_sc->mmap;
	ram_sz = p_mmap[ram_chunks-1].address + p_mmap[ram_chunks-1].size;
	/*
	for(ram_idx = 0; ram_idx < ram_chunks; ram_idx++) {
		// collect the size of all memory chunks
		ram_sz += p_mmap[ram_idx].size;
	}
	*/

	/* calculate the size of 'pagetable' in page size of 2M */
	ram_pages_2m = ram_sz  / PAGE2M;
	if(ram_sz % PAGE2M) {
		/* calculate the rest of memory in 4k sized pages
		_u64 n4k = (ram_sz - (ram_pages_2m * PAGE2M));
		ram_pages_4k =  n4k / PAGE4K; */
		ram_pages_2m++;
	}

	/* PT size */
	pt_records = ram_pages_4k;
	pt_pages = (pt_records * RECORD_SZ) / PAGE4K;
	pt_pages += ((pt_records * RECORD_SZ) % PAGE4K) ? 1 : 0;

	/* PD size */
	pd_records = ram_pages_2m + ram_pages_4k;
	pd_pages = (pd_records * RECORD_SZ) / PAGE4K;
	pd_pages += ((pd_records * RECORD_SZ) % PAGE4K) ? 1 : 0;
	
	/* PDP size */
	pdp_records = pd_pages;
	/* limit memory range to 512G */
	if(pdp_records > MAX_RECORDS)
		pdp_records = MAX_RECORDS;
	/*/////////////////////////// */
	pdp_pages = (pdp_records * RECORD_SZ) / PAGE4K;
	pdp_pages += ((pdp_records * RECORD_SZ) % PAGE4K) ? 1 : 0;
	

	/* calculate the size of mapping table */
	table_sz = (pdp_pages * PAGE4K) + (pd_pages * PAGE4K) + (pt_pages * PAGE4K) + (pml4_pages * PAGE4K);
	
	/* clear paging table */
	for(i = 0; i < table_sz; i++)
		*(p->pt_addr + i) = 0;
	/* /////////////////////// */
	
	p_pdp_byte = p_pml4_byte + (pml4_pages * PAGE4K);
	p_pdp = (_lm_pdpe_t *)p_pdp_byte;
	
	p_pd_byte = p_pdp_byte  + (pdp_pages  * PAGE4K);
	p_pd = (_lm_pde_t *)p_pd_byte;

	/*p_pt_byte = p_pd_byte + (pd_pages * PAGE4K);
	p_pt = (_lm_pte_t *)p_pt_byte; */
	
	/* Zero record (0x000000000000000) base address */
	ipml4 = 0;
	
	p_pml4[ipml4].f.p = 1;
	p_pml4[ipml4].f.rw = 1;
	p_pml4[ipml4].f.us = 0;
	p_pml4[ipml4].f.pwt = 0; /* 'writeback' caching policy */
	p_pml4[ipml4].f.pcd = 0; /*'cacheable' */
	p_pml4[ipml4].f.a = 0;
	p_pml4[ipml4].f.ign = 0;
	p_pml4[ipml4].f.mbz1 = 0;
	p_pml4[ipml4].f.avl1 = 0;
	p_pml4[ipml4].f.base = PML4_4KPAGE2NUM((_u32)(p_pdp_byte)); /* make PDP index; */
	p_pml4[ipml4].f.mbz2 = 0;
	p_pml4[ipml4].f.avl2 = 0;

	/* calculate the VBASE index */
	ipml4 = (_u32)((VBASE_64 & 0x0000ff8000000000LLU) >> 39);

	p_pml4[ipml4].f.p = 1;
	p_pml4[ipml4].f.rw = 1;
	p_pml4[ipml4].f.us = 0;
	p_pml4[ipml4].f.pwt = 0; /* 'writeback' caching policy */
	p_pml4[ipml4].f.pcd = 0; /*'cacheable' */
	p_pml4[ipml4].f.a = 0;
	p_pml4[ipml4].f.ign = 0;
	p_pml4[ipml4].f.mbz1 = 0;
	p_pml4[ipml4].f.avl1 = 0;
	p_pml4[ipml4].f.base = PML4_4KPAGE2NUM((_u32)(p_pdp_byte)); /* make PDP index; */
	p_pml4[ipml4].f.mbz2 = 0;
	p_pml4[ipml4].f.avl2 = 0;
	
	/* next CPU */
	p_pml4_byte += PAGE4K;
	p_pml4 = (_lm_pml4e_t *)p_pml4_byte;
	
	for(ipdp = 0; ipdp < pdp_records; ipdp++) {
		p_pdp[ipdp].f.p = 1;
		p_pdp[ipdp].f.rw = 1;
		p_pdp[ipdp].f.us = 0;
		p_pdp[ipdp].f.pwt = 0; /* 'writeback' caching policy */
		p_pdp[ipdp].f.pcd = 0; /* 'cacheable' */
		p_pdp[ipdp].f.a = 0;
		p_pdp[ipdp].f.ign = 0;
		p_pdp[ipdp].f.ps = 0;	/* 1:(1G) or (0:4K or 2M) page size */
		p_pdp[ipdp].f.mbz1 = 0;
		p_pdp[ipdp].f.avl1 = 0;
		p_pdp[ipdp].f.base = PML4_4KPAGE2NUM((_u32)p_pd_byte);
		p_pdp[ipdp].f.mbz2 = 0;
		p_pdp[ipdp].f.avl2 = 0;
		
		p_pd_byte += PAGE4K;
	}

	/* prepare PD table to point 2M pages	*/
	for(ipd = 0; ipd < ram_pages_2m; ipd++) {
		p_pd[ipd].f.p = 1;
		p_pd[ipd].f.rw = 1;
		p_pd[ipd].f.us = 0;
		p_pd[ipd].f.pwt = 0; /* 'writeback' caching policy */
		p_pd[ipd].f.pcd = 0; /* 'cacheable' */
		p_pd[ipd].f.a = 0;
		p_pd[ipd].f.d = 0;
		p_pd[ipd].f.ps = 1; /* 2M page size */
		p_pd[ipd].f.t._2m.g = 0;  /* global page */
		p_pd[ipd].f.t._2m.avl = 0;
		p_pd[ipd].f.t._2m.pat = 0;
		p_pd[ipd].f.t._2m.rez = 0;
		p_pd[ipd].f.t._2m.base = PML4_2MPAGE2NUM(ram_byte);
		p_pd[ipd].f.mbz = 0;
		p_pd[ipd].f.avl = 0;
		
		ram_byte += PAGE2M;
	}
/*
	// prepare the PD table to point PT tables
	for(; ipd < pd_records; ipd++) {
		p_pd[ipd].f.p = 1;
		p_pd[ipd].f.rw = 1;
		p_pd[ipd].f.us = 0;
		p_pd[ipd].f.pwt = 0; // 'writeback' caching policy
		p_pd[ipd].f.pcd = 0; // 'cacheable'
		p_pd[ipd].f.a = 0;
		p_pd[ipd].f.d = 0;
		p_pd[ipd].f.ps = 0; // 4K page size
		p_pd[ipd].f.t._4k.g = 0;  // global page
		p_pd[ipd].f.t._4k.avl = 0;
		p_pd[ipd].f.t._4k.base = PML4_4KPAGE2NUM((_u32)p_pt_byte);
		p_pd[ipd].f.mbz = 0;
		p_pd[ipd].f.avl = 0;
		
		p_pt_byte += PAGE4K;
	}

	for(ipt = 0; ipt < pt_records; ipt++) {
		p_pt[ipt].f.p = 1;
		p_pt[ipt].f.rw = 1;
		p_pt[ipt].f.us = 0;
		p_pt[ipt].f.pwt = 0;
		p_pt[ipt].f.pcd = 0; // 'cacheable'
		p_pt[ipt].f.a = 0;
		p_pt[ipt].f.d = 0;
		p_pt[ipt].f.pat = 0;
		p_pt[ipt].f.g = 0;
		p_pt[ipt].f.avl1 = 0;
		p_pt[ipt].f.base = PML4_4KPAGE2NUM(ram_byte);
		p_pt[ipt].f.mbz = 0;
		p_pt[ipt].f.avl2 = 0;

		ram_byte += PAGE4K;
	}
*/
	p->p_sc->rmap[p->p_sc->rm_cnt].addr_lo = (_u64)(_u32)p->pt_addr;
	p->p_sc->rmap[p->p_sc->rm_cnt].size_lo = table_sz;
	p->p_sc->rmap[p->p_sc->rm_cnt].type = MEM_TYPE_PAGE_MAP;
	p->p_sc->rm_cnt++;
	p->p_sc->pt_page_size = PAGE2M;
	p->p_sc->pt_table_size = table_sz;
	p->p_sc->pt_range = ram_byte;
	p->p_sc->pt_type = PG_PML4;
	p->p_sc->vbase = VBASE_64;
}
END_CODE32;

void __attribute__((optimize("O0"))) _build_pml4(_u8 *pt_addr, _core_startup_t *p_sc) {
	_pml4_info_t pi;
	
	pi.pt_addr = pt_addr;
	pi.p_sc = p_sc;

#ifndef _DEBUG	
	_pm_call((_t_pm_proc *)_pm_build_pml4_2m, &pi);
#else
	_pm_build_pml4_2m(&pi);
#endif	
}

