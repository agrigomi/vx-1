#ifndef __ADDR_H__
#define __ADDR_H__

#include "pgdef.h"

/* kernel to physical address */
#ifdef _x86_64_
#define k_to_p(vaddr) (vaddr & ~VBASE_64)
#endif
		
#ifdef _x86_
#define k_to_p(vaddr) (vaddr & ~VBASE_32)
#endif

/* physical to kernel address */
#ifdef _x86_64_
#define p_to_k(paddr) ((_u64)paddr | VBASE_64)
#endif

#ifdef _x86_
#define p_to_k(paddr) ((_u64)paddr | VBASE_32)	
#endif

#endif
