#ifndef __SEGMENT__
#define __SEGMENT__

#define MAX_GDT_RECORDS 10

#define GRAN_G	 	(_u8)(1 << 7)
#define GRAN_D	 	(_u8)(1 << 6)
#define GRAN_L	 	(_u8)(1 << 5)
#define GRAN_AVL 	(_u8)(1 << 4)
#define GRAN_LIMIT	(_u8)0xf

#define GRAN_16		GRAN_D|GRAN_LIMIT
#define GRAN_32		GRAN_G|GRAN_D|GRAN_LIMIT
#define GRAN_64 	GRAN_G|GRAN_L|GRAN_LIMIT

#define NULL_SEGMENT		0
#define FLAT_CODE_SEGMENT	1
#define FLAT_DATA_SEGMENT	2
#define FLAT_STACK_SEGMENT	3
#define FLAT_CODE_SEGMENT_16	4
#define FLAT_DATA_SEGMENT_16	5
#define FLAT_STACK_SEGMENT_16	6
#define FLAT_CODE_SEGMENT_64	7
#define FLAT_DATA_SEGMENT_64	8
#define FREE_SEGMENT_INDEX	9

#endif
