#include "malloc.h"
#include "vxmod.h"
#include "i_repository.h"
#include "startup_context.h"
#include "i_memory.h"
#include "atom.h"

#define MEGABYTE	(1024*1024)

extern void _vx_core_init_(_core_startup_t *p_sc);


HCONTEXT _ghc_vmm_ = NULL;

void test(void) {
	/* TEST */
	if(!_ghc_vmm_) {
		_ghc_vmm_ = __g_p_i_repository__->create_context_by_interface(I_VMM);
		_i_vmm_t *pi_vmm = HC_INTERFACE(_ghc_vmm_);
		_p_data_t pd_vmm = HC_DATA(_ghc_vmm_);
		__asm__ __volatile__("":::"memory");

		if(pi_vmm && pd_vmm) {
			_page_info_t pgi;
			pgi.vaddr = 0xfffabcdef;
			pgi.paddr = 0;
			pgi.vmps = VMPS_4K;
			pgi.flags = 0;
			pi_vmm->map(pd_vmm, &pgi, 1, 0);
			/*...*/
			__asm__ __volatile__("nop");
		}
	}
}

int test_mb(int a, int b) {

	a ^= b ^= a ^= b;
	a = (a = a+b) - (b = a - b);
	__asm__ __volatile__("":::"memory");
	/*__WMB__();*/
	return a;
}

int main(int arhc, char *argv[]) {
	_core_startup_t csc;

	csc.vbase = 0;
	csc.pt_page_size = 4096;
	csc.mmap[0].address = (_u64)malloc(4*MEGABYTE);
	csc.mmap[0].size    = 4*MEGABYTE;
	csc.mmap[0].type    = MEM_TYPE_FREE;

	csc.mmap[1].address = (_u64)malloc(4*MEGABYTE);
	csc.mmap[1].size    = 4*MEGABYTE;
	csc.mmap[1].type    = MEM_TYPE_FREE;

	csc.mmap[2].address = (_u64)malloc(8*MEGABYTE);
	csc.mmap[2].size    = 8*MEGABYTE;
	csc.mmap[2].type    = MEM_TYPE_FREE;

	csc.mmap[3].address = (_u64)malloc(16*MEGABYTE);
	csc.mmap[3].size    = 16*MEGABYTE;
	csc.mmap[3].type    = MEM_TYPE_FREE;
	
	csc.mm_cnt = 4;
	csc.rm_cnt = 0;

	_vx_core_init_(&csc);

	test();
	test_mb(5,10);
	return 0;
}

