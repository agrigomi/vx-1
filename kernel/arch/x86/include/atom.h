#ifndef __ATOM_H__
#define __ATOM_H__

#define __EXCHANGE_L__(v1, v2) \
	__asm__ __volatile__ ("lock xchgl	%%eax, %[state]\n" \
				: [state] "=m" (v2), "=a"(v1) \
				:"a" (v1));
/* write memory barrier */
#define __WMB__() \
	__asm__ __volatile__ ("sfence":::"memory")
/* load memory barrier */
#define __LMB__()\
	__asm__ __volatile__ ("lfence":::"memory")
/* write/load memory barrier */
#define __MB__() \
	__asm__ __volatile__ ("mfence":::"memory")

#endif

