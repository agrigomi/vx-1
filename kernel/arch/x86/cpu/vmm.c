#include "i_memory.h"
#include "i_repository.h"
#include "i_sync.h"
#include "pgdef.h"
#include "addr.h"
#include "i_str.h"
#include "err.h"

#define KILOBYTE			 1024
#define MEGABYTE			 (KILOBYTE*KILOBYTE)
#define PAGE_4K				 (4*KILOBYTE)
#define PAGE_2M				 (2*MEGABYTE)

#define MAX_1G_OFFSET			 0x000000003fffffffLLU
#define MAX_2M_OFFSET			 0x00000000001fffffLLU
#define MAX_4K_OFFSET			 0x0000000000000fffLLU

#define PML4_INDEX(addr)  (_u16)((addr & 0x0000ff8000000000LLU) >> 39)
#define PDP_INDEX(addr)   (_u16)((addr & 0x0000007fc0000000LLU) >> 30)
#define PD_INDEX(addr)    (_u16)((addr & 0x000000003fe00000LLU) >> 21)
#define PT_INDEX(addr)    (_u16)((addr & 0x00000000001ff000LLU) >> 12)
#define PT4K_OFFSET(addr) (_u32)( addr & MAX_4K_OFFSET)
#define PT2M_OFFSET(addr) (_u32)( addr & MAX_2M_OFFSET)
#define PT1G_OFFSET(addr) (_u32)( addr & MAX_1G_OFFSET)

#define EPT_BIT(entry)    	(entry & 0x0000000000000080LLU) /* end page translation bit (7) */

#define MAX_PT_INDEX			 0x00000000000001ffLLU
#define CANONICAL_MASK			 0xffff000000000000LLU
#define CANONICAL_SIGN			 0x0000800000000000LLU

#define PML4_ADDR(index)  (((_u64)index & MAX_PT_INDEX) << 39)
#define PDP_ADDR(index)   (((_u64)index & MAX_PT_INDEX) << 30)
#define PD_ADDR(index)    (((_u64)index & MAX_PT_INDEX) << 21)
#define PT_ADDR(index)    (((_u64)index & MAX_PT_INDEX) << 12)

/* page number to kernel virtual address */
#define PN4K_TO_KVA(npage) p_to_k(PML4_4K_NUM_TO_PAGE(npage))
#define PN2M_TO_KVA(npage) p_to_k(PML4_2M_NUM_TO_PAGE(npage))
/* kernel virtual address to page number */
#define KVA_TO_PN4K(addr)  PML4_4K_PAGE_TO_NUM(k_to_p(addr))
#define KVA_TO_PN2M(addr)  PML4_2M_PAGE_TO_NUM(k_to_p(addr))

#define MK_VADDR_1G(ipml4, ipdp, offset) ( \
	((PML4_ADDR(ipml4) & CANONICAL_SIGN) ? (PML4_ADDR(ipml4) | CANONICAL_MASK) : PML4_ADDR(ipml4)) | \
	PDP_ADDR(ipdp) | ((_u64)offset & MAX_1G_OFFSET) )

#define MK_VADDR_2M(ipml4, ipdp, ipd, offset) ( \
	((PML4_ADDR(ipml4) & CANONICAL_SIGN) ? (PML4_ADDR(ipml4) | CANONICAL_MASK) : PML4_ADDR(ipml4)) | \
	PDP_ADDR(ipdp) | PD_ADDR(ipd) | ((_u64)offset & MAX_2M_OFFSET) )

#define MK_VADDR_4K(ipml4, ipdp, ipd, ipt, offset) ( \
	((PML4_ADDR(ipml4) & CANONICAL_SIGN) ? (PML4_ADDR(ipml4) | CANONICAL_MASK) : PML4_ADDR(ipml4)) | \
	PDP_ADDR(ipdp) | PD_ADDR(ipd) | PT_ADDR(ipt) | ((_u64)offset & MAX_4K_OFFSET) )

/*#define VMM_ALLOC_LIMIT	0xffffffff*/
#define VMM_ALLOC_LIMIT	NO_ALLOC_LIMIT

#define PG_PMA_FLAG	(1<<0)

static _i_pma_t *_gpi_pma_ = NULL;
static _i_str_t *_gpi_str_ = NULL;

typedef struct {
	_u16 ipml4, ipdp, ipd, ipt;
	_lm_pml4e_t *pml4_base;
	_lm_pdpe_t *pdp_base;
	_lm_pde_t *pd_base;
	_lm_pte_t *pt_base;
	_lm_pml4e_t *pml4e;
	_lm_pdpe_t *pdpe;
	_lm_pde_t *pde;
	_lm_pte_t *pte;
	_p_data_t page_addr;
	_u32 offset;
	_u8 page_size;
}_addr_info_t;

typedef struct {
	_bool		pma; /* signified that PML4 table must me self destroyed */
	_vaddr_t	tbl; /* physical address of mapping table */
	HCONTEXT	hc_mutex;
}_vmm_dc_t;

static HMUTEX vmm_lock(_p_data_t dc, HMUTEX hlock) {
	HMUTEX r = 0;
	_vmm_dc_t *pdc = (_vmm_dc_t *)dc;
	if(pdc->hc_mutex) {
		_i_mutex_t *pi = HC_INTERFACE(pdc->hc_mutex);
		_p_data_t pd = HC_DATA(pdc->hc_mutex);

		if(pi && pd)
			r = pi->lock(pd, hlock);
	}
	return r;
}
static void vmm_unlock(_p_data_t dc, HMUTEX hlock) {
	_vmm_dc_t *pdc = (_vmm_dc_t *)dc;
	if(pdc) {
		if(pdc->hc_mutex) {
			_i_mutex_t *pi = HC_INTERFACE(pdc->hc_mutex);
			_p_data_t pd = HC_DATA(pdc->hc_mutex);

			if(pi && pd)
				pi->unlock(pd, hlock);
		}
	}
}

static void vmm_activate(_p_data_t dc) {
	_vmm_dc_t *pdc = (_vmm_dc_t *)dc;
	if(pdc) {
		_lm_cr3_t lmcr3;

		lmcr3.e = 0;
		lmcr3.f.base = pdc->tbl;

		/* activate mapping table */
		__asm__ __volatile__ (
			"mov	%%rax, %%cr3\n"
			:
			:"a"(lmcr3.e)
		);
	}
}

static void clear_page(_p_data_t ptr, _u32 size) {
	_u64 *p = (_u64 *)ptr;
	_u32 sz = size / sizeof(_u64);
	_u32 i = 0;
	for(; i < sz; i++)
		p[i] = 0;
}

static void vmm_init(_p_data_t dc, _vaddr_t mapping) {
	_vmm_dc_t *pdc = (_vmm_dc_t *)dc;
	if(pdc) {
		HMUTEX hlock = vmm_lock(pdc, 0);
		pdc->tbl = mapping;
		if(mapping)
			vmm_activate(dc);
		vmm_unlock(pdc, hlock);
	}
}

/* return kernel virtual address of first page or 0 */
static _vaddr_t vmm_alloc_page(_u32 npages, _u8 page_size, _ulong limit) {
	_vaddr_t r = 0;
	if((page_size == VMPS_4K || page_size == VMPS_2M) && _gpi_pma_) {
		_ram_info_t ri;
		_gpi_pma_->info(&ri);
		_u32 nbytes = (page_size == VMPS_2M) ? (PAGE_2M * npages) : (PAGE_4K * npages);
		_u32 pma_pages = nbytes / ri.ps;
		if(pma_pages < (ri.pr - ri.pu)) {
			_p_data_t _r = NULL;
			if((_r = _gpi_pma_->alloc_seq(pma_pages, RT_SYSTEM, limit))) {
				clear_page(_r, nbytes);
				r = (_vaddr_t)_r;
			}
		}
	}
	return r;
}

static void vmm_free_page(_vaddr_t addr /* kernel virtual address of first page */
			, _u32 npages, /* number of pages with one and the same size */
			_u8 page_size) {
	if((page_size == VMPS_4K || page_size == VMPS_2M) && _gpi_pma_) {
		_ram_info_t ri;
		_gpi_pma_->info(&ri);
		_u32 nbytes = (page_size == VMPS_2M) ? (PAGE_2M * npages) : (PAGE_4K * npages);
		_u32 pma_pages = nbytes / ri.ps;
		_gpi_pma_->free_seq((void *)addr, pma_pages);
	}
}

static _bool is_page_empty(_vaddr_t vaddr, /* virtual address */
				_u8 page_size) {
	_bool r = _true;
	_u64 *ptr64 = (_u64 *)vaddr;
	_u32 n = (page_size == VMPS_2M) ? (PAGE_2M / sizeof(_u64)) : (PAGE_4K / sizeof(_u64));
	_u32 i = 0;
	for(; i < n; i++) {
		if(*(ptr64 + i)) {
			r = _false;
			break;
		}
	}
	return r;
}

static void addr_decode(_vmm_dc_t *pdc, _vaddr_t addr, _addr_info_t *ai) {
	if(_gpi_str_)
		_gpi_str_->mem_set(ai, 0, sizeof(_addr_info_t));
	
	ai->ipml4 = PML4_INDEX(addr);
	ai->ipdp  = PDP_INDEX(addr);
	ai->ipd   = PD_INDEX(addr);
	ai->ipt   = PT_INDEX(addr);

	if(pdc->tbl) {
		ai->pml4_base = (_lm_pml4e_t *)p_to_k(pdc->tbl);
		if(ai->pml4_base[ai->ipml4].e) {
			if((ai->pdp_base = (_lm_pdpe_t *)PN4K_TO_KVA(ai->pml4_base[ai->ipml4].f.base))) {
				if(ai->pdp_base[ai->ipdp].e) {
					if((ai->pd_base = (_lm_pde_t *)PN4K_TO_KVA(ai->pdp_base[ai->ipdp].f.base))) {
						/* check for 'end page translation' bit */
						if(EPT_BIT(ai->pd_base[ai->ipd].e)) { /* 2m page */
							ai->page_addr = (_p_data_t)PN2M_TO_KVA(ai->pd_base[ai->ipd].f.t._2m.base);
							ai->offset = PT2M_OFFSET(addr);
							ai->page_size = VMPS_2M;
						} else {/* 4k page */
							if(ai->pd_base[ai->ipd].e) {
								if((ai->pt_base = (_lm_pte_t *)PN4K_TO_KVA(ai->pd_base[ai->ipd].f.t._4k.base))) {
									ai->page_addr = (_p_data_t)PN4K_TO_KVA(ai->pt_base[ai->ipt].f.base);
									ai->pte = &(ai->pt_base[ai->ipt]);
								}
							}
							ai->offset = PT4K_OFFSET(addr);
							ai->page_size = VMPS_4K;
						}
						ai->pde = &(ai->pd_base[ai->ipd]);
					}
				}
				ai->pdpe = &(ai->pdp_base[ai->ipdp]);
			}
		}
		ai->pml4e = &(ai->pml4_base[ai->ipml4]);
	}
}

static _bool _test_addr(_addr_info_t *pai) {
	_bool r = _false;
	if(pai->page_size == VMPS_4K) {
		if(pai->pml4_base && pai->pdp_base && pai->pd_base && pai->pt_base &&
				pai->pml4e && pai->pdpe && pai->pde && pai->pte && pai->page_addr) {
			if(pai->pml4e->e && pai->pdpe->e && pai->pde->e && pai->pte->e)
				r = _true;
		}	
	} else if(pai->page_size == VMPS_2M) {
		if(pai->pml4_base && pai->pdp_base && pai->pd_base && 
				pai->pml4e && pai->pdpe && pai->pde && pai->page_addr) {
			if(pai->pml4e->e && pai->pdpe->e && pai->pde->e)
				r = _true;
		}
	}
	return r;
}

static _u8 _get_mapping_flags(_addr_info_t *pai) {
	_u8 r = 0;
	if(pai->page_size == VMPS_2M) {
		_lm_pde_t *ppde = pai->pde;
		if(ppde) {
			if(ppde->f.p)
				r |= VMMF_PRESENT;
			if(ppde->f.rw)
				r |= VMMF_WRITABLE;
			if(ppde->f.us)
				r |= VMMF_USER_ACCESS;
			if(ppde->f.t._2m.avl & PG_PMA_FLAG)
				r |= VMMF_PMA;
			if(ppde->f.pcd)
				r |= VMMF_NOT_CACHEABLE;
		}
	} else if(pai->page_size == VMPS_4K) {
		_lm_pte_t *ppte = pai->pte;
		if(ppte) {
			if(ppte->f.p)
				r |= VMMF_PRESENT;
			if(ppte->f.rw)
				r |= VMMF_WRITABLE;
			if(ppte->f.us)
				r |= VMMF_USER_ACCESS;
			if(ppte->f.pcd)
				r |= VMMF_NOT_CACHEABLE;
			if(ppte->f.avl1 & PG_PMA_FLAG)
				r |= VMMF_PMA;
		}
	}
	return r;
}

static _bool vmm_test(_p_data_t dc, _vaddr_t vaddr, _page_info_t *p_mpi, HMUTEX hlock) {
	_bool r = _false;
	_addr_info_t ai;
	HMUTEX hm = vmm_lock(dc, hlock);
	addr_decode(dc, vaddr, &ai);
	if((r = _test_addr(&ai))) {
		p_mpi->vaddr = vaddr;
		p_mpi->paddr = (_vaddr_t)ai.page_addr;
		p_mpi->vmps = ai.page_size;
		p_mpi->flags = _get_mapping_flags(&ai);
	}
	vmm_unlock(dc, hm);
	return r;
}

static _bool map_pml4(_p_data_t dc, _addr_info_t *pai) {
	_bool r = _false;
	if(!pai->pml4_base) {
		if((pai->pml4_base = (_lm_pml4e_t *)vmm_alloc_page(1, VMPS_4K, VMM_ALLOC_LIMIT))) {
			/* update context addr (tbl) */
			_vmm_dc_t *pdc = (_vmm_dc_t *)dc;
			pdc->tbl = k_to_p((_vaddr_t)pai->pml4_base);
			pdc->pma = _true;
		}
	}
	if(pai->pml4_base) {
		pai->pml4e = pai->pml4_base + pai->ipml4;
		r = _true;
	}
	return r;
}
static _bool map_pdp(_p_data_t dc, _addr_info_t *pai) {
	_bool r = _false;
	if(map_pml4(dc, pai)) {
		_u8 pma_flag = 0;
		if(!pai->pdp_base) {
			pai->pdp_base = (_lm_pdpe_t *)vmm_alloc_page(1, VMPS_4K, VMM_ALLOC_LIMIT);
			pma_flag = PG_PMA_FLAG;
		}
		if(pai->pdp_base) {
			_lm_pml4e_t *pml4e = pai->pml4e;
			pai->pdpe = pai->pdp_base + pai->ipdp;
			pml4e->f.base = KVA_TO_PN4K((_vaddr_t)pai->pdp_base);
			pml4e->f.p  = 1; /* present */
			pml4e->f.rw = 1; /* writable */
			pml4e->f.us = 0; /* supervisor access */
			pml4e->f.pwt= 0; /* write back */
			pml4e->f.pcd= 0; /* cacheable */
			pml4e->f.avl1 |= pma_flag; /* PDP page allocated by PMA */
			r = _true;
		}
	}
	return r;
}
static _bool map_pd(_p_data_t dc, _addr_info_t *pai) {
	_bool r = _false;
	if(map_pdp(dc, pai)) {
		_u8 pma_flag = 0;
		if(!pai->pd_base) {
			pai->pd_base = (_lm_pde_t *)vmm_alloc_page(1, VMPS_4K, VMM_ALLOC_LIMIT);
			pma_flag = PG_PMA_FLAG;
		}
		if(pai->pd_base) {
			_lm_pdpe_t *pdpe = pai->pdpe;
			pai->pde = pai->pd_base + pai->ipd;
			pdpe->f.base = KVA_TO_PN4K((_vaddr_t)pai->pd_base);
			pdpe->f.p  = 1; /* present */
			pdpe->f.rw = 1; /* writable */
			pdpe->f.us = 0; /* supervisor access */
			pdpe->f.pwt= 0; /* write back */
			pdpe->f.pcd= 0; /* cacheable */
			pdpe->f.ps = 0; /* 4k */
			pdpe->f.avl1 |= pma_flag; /* PD page allocated by PMA */
			r = _true;
		}
	}
	return r;
}
static _bool map_pt(_p_data_t dc, _addr_info_t *pai) {
	_bool r = _false;
	if(map_pd(dc, pai)) {
		_u8 pma_flag = 0;
		if(!pai->pt_base) {
			pai->pt_base = (_lm_pte_t *)vmm_alloc_page(1, VMPS_4K, VMM_ALLOC_LIMIT);
			pma_flag = PG_PMA_FLAG;
		}
		if(pai->pt_base) {
			_lm_pde_t *pde = pai->pde;
			pai->pte = pai->pt_base + pai->ipt;
			pde->f.t._4k.base = KVA_TO_PN4K((_vaddr_t)pai->pt_base);
			pde->f.p  = 1; /* present */
			pde->f.rw = 1; /* writable */
			pde->f.us = 0; /* supervisor access */
			pde->f.pwt= 0; /* write back */
			pde->f.pcd= 0; /* cacheable */
			pde->f.ps = 0; /* 4k */
			pde->f.t._4k.avl |= pma_flag;
			r = _true;
		}
	}
	return r;
}

static _u32 vmm_map(_p_data_t dc, _page_info_t *pmpi, _u32 nmpi, HMUTEX hlock) {
	_u32 r = 0;
	_vmm_dc_t *pdc = (_vmm_dc_t *)dc;
	
	if(pdc) {
		HMUTEX hm = vmm_lock(dc, hlock);
		_addr_info_t ai;
		_u32 i = 0;

		while(i < nmpi) {
			addr_decode(pdc, pmpi[i].vaddr, &ai);
			if(!_test_addr(&ai)) {
				/* available for mapping */
				if(pmpi[i].vmps == VMPS_2M) {
					if(map_pd(dc, &ai)) {
						_lm_pde_t *pde = ai.pde;
						/* set page flags */
						pde->f.p   = (pmpi[i].flags & VMMF_PRESENT)?1:0;
						pde->f.rw  = (pmpi[i].flags & VMMF_WRITABLE)?1:0;
						pde->f.us  = (pmpi[i].flags & VMMF_USER_ACCESS)?1:0;
						pde->f.pwt = 0; /* write back caching policy */
						pde->f.ps  = 1; /* end of page translation (2M) */
						pde->f.pcd = (pmpi[i].flags & VMMF_NOT_CACHEABLE)?1:0;
						if(pmpi[i].paddr) {
							/* set page address */
							pde->f.t._2m.base = PML4_2M_PAGE_TO_NUM(pmpi[i].paddr);
							pde->f.t._2m.avl |= (pmpi[i].flags & VMMF_PMA) ? PG_PMA_FLAG : 0;
						} else {
							/* alloc 2M page */
							if((ai.page_addr = (_p_data_t)vmm_alloc_page(1, VMPS_2M, NO_ALLOC_LIMIT))) {
								/* set page address */
								pde->f.t._2m.base = KVA_TO_PN2M((_vaddr_t)ai.page_addr);
								pde->f.t._2m.avl = PG_PMA_FLAG; /* allocated by PMA */
								/* update phys. addr. */
								pmpi[i].paddr = k_to_p((_ulong)ai.page_addr);
							} else {
								pde->e = 0; /* clear entry */
								break;
							}
						}
					} else
						break;
				} else if(pmpi[i].vmps == VMPS_4K) {
					if(map_pt(dc, &ai)) {
						_lm_pte_t *pte = ai.pte;
						/* set page flags */
						pte->f.p   = (pmpi[i].flags & VMMF_PRESENT)?1:0;
						pte->f.rw  = (pmpi[i].flags & VMMF_WRITABLE)?1:0;
						pte->f.us  = (pmpi[i].flags & VMMF_USER_ACCESS)?1:0;
						pte->f.pwt = 0; /* write back caching policy */
						pte->f.pcd = (pmpi[i].flags & VMMF_NOT_CACHEABLE)?1:0;
						if(pmpi[i].paddr) {
							/* set page address */
							pte->f.base = PML4_4K_PAGE_TO_NUM(pmpi[i].paddr);
							pte->f.avl1 |= (pmpi[i].flags & VMMF_PMA) ? PG_PMA_FLAG : 0;
						} else {
							/* alloc 4K page */
							if((ai.page_addr = (_p_data_t)vmm_alloc_page(1, VMPS_4K, NO_ALLOC_LIMIT))) {
								/* set page address */
								pte->f.base = KVA_TO_PN4K((_vaddr_t)ai.page_addr);
								pte->f.avl1 = PG_PMA_FLAG; /* allocated by PMA */
								/* update phys. address */
								pmpi[i].paddr = k_to_p((_ulong)ai.page_addr);
							} else {
								pte->e = 0; /* clear entry */
								break;
							}
						}
					} else
						break;
				} else
					break;
				r++; /* success */
			} else /* stop */
				break;
			i++; /* next */
		}

		vmm_unlock(dc, hm);
	}
	return r;
}

static _bool unmap_pml4(_addr_info_t *pai) {
	_bool r = _false;
	if(pai->pml4_base && pai->pml4e) {
		_lm_pml4e_t *pml4e = pai->pml4e;
		if(pml4e->e) {
			if(pml4e->f.avl1 & PG_PMA_FLAG) {
				/* release PDP page 4K */
				_vaddr_t vaddr = PN4K_TO_KVA(pml4e->f.base);
				vmm_free_page(vaddr, 1, VMPS_4K);
				pai->pdp_base = pai->pdpe = NULL;
			}
			pml4e->e = 0;
		}
		r = _true;
	}
	return r;
}

static _bool unmap_pdp(_addr_info_t *pai) {
	_bool r = _false;
	if(pai->pdp_base && pai->pdpe) {
		_lm_pdpe_t *pdpe = pai->pdpe;
		if(pdpe->e) {
			if(pdpe->f.avl1 && PG_PMA_FLAG) {
				/* release 4K PD page  */
				_vaddr_t vaddr = PN4K_TO_KVA(pdpe->f.base);
				vmm_free_page(vaddr, 1, VMPS_4K);
				pai->pd_base = pai->pde = NULL;
			}
			pdpe->e = 0; /* clear PDP entry */
		}
		r = _true;
		if(is_page_empty((_vaddr_t)pai->pdp_base, VMPS_4K))
			/* unmap PML4 */
			r = unmap_pml4(pai);
	}
	return r;
}

static _bool unmap_pd(_addr_info_t *pai) {
	_bool r = _false;
	if(pai->pd_base && pai->pde) {
		_lm_pde_t *pde = pai->pde;
		if(pde->e) {
			if(pde->f.ps && pai->page_size == VMPS_2M) {
				/* unmap 2M page */
				if(pde->f.t._2m.avl & PG_PMA_FLAG) {
					/* release 2M page */
					_vaddr_t vaddr = PN2M_TO_KVA(pde->f.t._2m.base);
					vmm_free_page(vaddr, 1, VMPS_2M);
				}
			} else if(!pde->f.ps && pai->page_size == VMPS_4K) {
				/* unmap PT table in 4K page */
				if(pde->f.t._4k.avl & PG_PMA_FLAG) {
					/* release 4K PT page */
					_vaddr_t vaddr = PN4K_TO_KVA(pde->f.t._4k.base);
					vmm_free_page(vaddr, 1, VMPS_4K);
					pai->pt_base = pai->pte = NULL;
				}
			}
			pde->e = 0; /* clear PD entry */
		}
		r = _true;
		if(is_page_empty((_vaddr_t)(pai->pd_base), VMPS_4K))
			/* continue with PDP unmap */
			r = unmap_pdp(pai);
	}
	return r;
}

static _bool unmap_pt(_addr_info_t *pai) {
	_bool r = _false;
	if(pai->pt_base && pai->pte) {
		_lm_pte_t *pte = pai->pte;
		if(pte->e) {
			if(pte->f.avl1 & PG_PMA_FLAG) {
				/* release 4K page */
				_vaddr_t vaddr = PN4K_TO_KVA(pte->f.base);
				vmm_free_page(vaddr, 1, VMPS_4K);
			}
			pte->e = 0; /* clear entry */
		}
		r = _true;
		if(is_page_empty((_vaddr_t)(pai->pt_base), VMPS_4K))
			/* continue with PDE unmap */
			r = unmap_pd(pai);
	}
	return r;
}

static _u32 vmm_unmap(_p_data_t dc, _vaddr_t start, _vaddr_t end, HMUTEX hlock) {
	_u32 r = 0;
	_vmm_dc_t *pdc = (_vmm_dc_t *)dc;
	if(pdc) {
		HMUTEX hm = vmm_lock(dc, hlock);
		_vaddr_t caddr = start;
		_addr_info_t ai;

		while(caddr < end) {
			addr_decode(pdc, caddr, &ai);
			if(_test_addr(&ai)) {
				if(ai.page_size == VMPS_2M) {
					if(unmap_pd(&ai)) {
						caddr += (_vaddr_t)ai.page_addr + PAGE_2M;
						r++;
					} else
						break;
				} else if(ai.page_size == VMPS_4K) {
					if(unmap_pt(&ai)) {
						caddr += (_vaddr_t)ai.page_addr + PAGE_4K;
						r++;
					} else
						break;
				} else 
					/* unsupported page size */
					break;
			} else /* stop */
				break;
		}
		vmm_unlock(dc, hm);
	}
	return r;
}

static _u32 vmm_array(_p_data_t dc, _vaddr_t start, _vaddr_t end, _page_info_t *p_mpi, _u32 nmpi, HMUTEX hlock) {
	_u32 r = 0;
	_vmm_dc_t *pdc = (_vmm_dc_t *)dc;

	if(pdc) {
		HMUTEX hm = vmm_lock(dc, hlock);
		_addr_info_t ai;
		_vaddr_t caddr = start;
		_u32 i = 0;
		while(caddr < end && i < nmpi) {
			addr_decode(pdc, caddr, &ai);
			if(_test_addr(&ai)) {
				p_mpi[i].flags = _get_mapping_flags(&ai);
				p_mpi[i].vaddr = caddr;
				p_mpi[i].vmps = ai.page_size;
				if(ai.page_size == VMPS_2M) {
					p_mpi[i].paddr = PML4_2M_NUM_TO_PAGE(ai.pde->f.t._2m.base);
					caddr += (_vaddr_t)ai.page_addr + PAGE_2M;
				} else if(ai.page_size == VMPS_4K) {
					p_mpi[i].paddr = PML4_4K_NUM_TO_PAGE(ai.pte->f.base);
					caddr += (_vaddr_t)ai.page_addr + PAGE_4K;
				} else
					/* unsupported page size */
					break;
			} else
				/* stop */
				break;
			i++;
		}
		r = i;
		vmm_unlock(dc, hm);
	}
	return r;
}

static void vmm_set_flags(_p_data_t dc, _vaddr_t vaddr_start, _vaddr_t vaddr_end, _u8 flags, HMUTEX hlock) {
	_vmm_dc_t *pdc = (_vmm_dc_t *)dc;

	if(pdc) {
		HMUTEX hm = vmm_lock(dc, hlock);
		_addr_info_t ai;
		_vaddr_t caddr = vaddr_start;
		while(caddr < vaddr_end) {
			addr_decode(pdc, caddr, &ai);
			if(_test_addr(&ai)) {
				if(ai.page_size == VMPS_2M) {
					_lm_pde_t *pde = ai.pde;
					/* set page flags */
					pde->f.p   = (flags & VMMF_PRESENT)?1:0;
					pde->f.rw  = (flags & VMMF_WRITABLE)?1:0;
					pde->f.us  = (flags & VMMF_USER_ACCESS)?1:0;
					pde->f.pwt = 0; /* write back caching policy */
					pde->f.ps  = 1; /* end of page translation (2M) */
					pde->f.pcd = (flags & VMMF_NOT_CACHEABLE)?1:0;
					pde->f.t._2m.avl |= (flags & VMMF_PMA) ? PG_PMA_FLAG : 0;
					caddr += (_vaddr_t)ai.page_addr + PAGE_2M;
				} else if(ai.page_size == VMPS_4K) {
					_lm_pte_t *pte = ai.pte;
					/* set page flags */
					pte->f.p   = (flags & VMMF_PRESENT)?1:0;
					pte->f.rw  = (flags & VMMF_WRITABLE)?1:0;
					pte->f.us  = (flags & VMMF_USER_ACCESS)?1:0;
					pte->f.pwt = 0; /* write back caching policy */
					pte->f.pcd = (flags & VMMF_NOT_CACHEABLE)?1:0;
					pte->f.avl1 |= (flags & VMMF_PMA) ? PG_PMA_FLAG : 0;
					caddr += (_vaddr_t)ai.page_addr + PAGE_4K;
				} else
					break;
			} else
				break;
		}
		vmm_unlock(dc, hm);
	}
}

static void vmm_destroy_mapping(_vmm_dc_t *pdc) {
	HMUTEX hm = vmm_lock((_p_data_t)pdc, 0);
	/*...*/
	vmm_unlock((_p_data_t)pdc, hm);
}

static _i_vmm_t _g_interface_ = {
	.init		= vmm_init,
	.activate	= vmm_activate,
	.map		= vmm_map,
	.unmap		= vmm_unmap,
	.array		= vmm_array,
	.test		= vmm_test,
	.set_flags	= vmm_set_flags,
	.lock		= vmm_lock,
	.unlock		= vmm_unlock
};

static _vx_res_t _mod_ctl_(_u32 cmd, ...) {
	_u32 r = VX_UNSUPPORTED_COMMAND;
	va_list args;

	va_start(args, cmd);

	switch(cmd) {
		case MODCTL_INIT_CONTEXT: {
			_i_repository_t *pi_repo = va_arg(args, _i_repository_t*);
			if(pi_repo) {
				HCONTEXT hc = NULL;
				if(!_gpi_pma_) {
					hc = pi_repo->get_context_by_interface(I_PMA);
					if(hc)
						_gpi_pma_ = HC_INTERFACE(hc);
				}
				if(!_gpi_str_) {
					hc = pi_repo->get_context_by_interface(I_STR);
					if(hc)
						_gpi_str_ = HC_INTERFACE(hc);
				}
				_vmm_dc_t *pdc = va_arg(args, _vmm_dc_t*);
				if(pdc) {
					pdc->hc_mutex = pi_repo->create_context_by_interface(I_MUTEX);
					/*...*/
				}
			}
			r = VX_OK;
		} break;
		case MODCTL_DESTROY_CONTEXT: {
			_i_repository_t *pi_repo = va_arg(args, _i_repository_t*);
			if(pi_repo) {
				_vmm_dc_t *pdc = va_arg(args, _vmm_dc_t*);
				if(pdc) {
					if(pdc->pma)
						vmm_destroy_mapping(pdc);
					pi_repo->release_context(pdc->hc_mutex);
					/*...*/
				}
			}
			r = VX_OK;
		} break;
	}
	va_end(args);

	return r;
}

DEF_VXMOD(
	MOD_VMM,		/* module name */
	I_VMM,			/* interface name */
	&_g_interface_,		/* interface pointer */
	NULL,			/* static data context */
	sizeof(_vmm_dc_t),	/* size of data context (for dynamic allocation) */
	_mod_ctl_,		/* pointer to module controll routine */
	1,0,1,			/* version */
	"PML4 memory mapping"	/* description */
);
