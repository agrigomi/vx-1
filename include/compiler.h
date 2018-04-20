#ifndef __COMPILER_H__
#define __COMPILER_H__

#define _UNUSED_ 	__attribute__((__unused__))
#define _USED_		__attribute__((used))
#define _OPTIMIZE_NONE_	__attribute__((optimize("O0")))
#define _OPTIMIZE_SIZE_	__attribute__((optimize("Os")))
#define _OPTIMIZE_ALL_	__attribute__((optimize("O3")))

#endif
