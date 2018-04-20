#ifndef __MP_SCAN_H__
#define __MP_SCAN_H__

#include "mgtype.h"
#include "addr.h"

#define MPF_SIGNATURE	(_str_t)"_MP_"
#define MPC_SIGNATURE	(_str_t)"PCMP"

#define FS_SIGNATURE  0x5f504d5f
#define CT_SIGNATURE  0x504d4350

#define MPF_IDENT	(('_'<<24) | ('P'<<16) | ('M'<<8) | '_')

/* MPC types of records */
#define	MPC_PROCESSOR		0
#define	MPC_BUS			1
#define	MPC_IOAPIC		2
#define	MPC_INTSRC		3
#define	MPC_LINTSRC		4

#define CPU_ENABLED             1       /* Processor is available */
#define CPU_BOOTPROCESSOR       2       /* Processor is the BP */

/* List of Bus Type string values, Intel MP Spec. */
#define BUSTYPE_EISA	"EISA"
#define BUSTYPE_ISA	"ISA"
#define BUSTYPE_INTERN	"INTERN"	/* Internal BUS */
#define BUSTYPE_MCA	"MCA"
#define BUSTYPE_VL	"VL"		/* Local bus */
#define BUSTYPE_PCI	"PCI"
#define BUSTYPE_PCMCIA	"PCMCIA"
#define BUSTYPE_CBUS	"CBUS"
#define BUSTYPE_CBUSII	"CBUSII"
#define BUSTYPE_FUTURE	"FUTURE"
#define BUSTYPE_MBI	"MBI"
#define BUSTYPE_MBII	"MBII"
#define BUSTYPE_MPI	"MPI"
#define BUSTYPE_MPSA	"MPSA"
#define BUSTYPE_NUBUS	"NUBUS"
#define BUSTYPE_TC	"TC"
#define BUSTYPE_VME	"VME"
#define BUSTYPE_XPRESS	"XPRESS"

/* CPU creation flags */
#define CCPUF_ENABLED	(1<<0)
#define CCPUF_BOOT	(1<<1)
#define CCPUF_SOCKET	(1<<2)

enum mpc_bus_type {
	MP_BUS_ISA = 1,
	MP_BUS_EISA,
	MP_BUS_PCI,
	MP_BUS_MCA,
};

typedef struct { /* MP Floating Pointer Structure */
	_s8	sign[4];	/* MP signature ('_MP_') */
	_u32	cfg_ptr;	/* configuration table pointer */
	_u8	len;		/* This is a 1 byte value specifying the length of this structure  */
				/*   in 16 byte paragraphs.  This should be 1. */
	_u8	msp_version;	/* version of multiprocessing specification (1 = 1.1; 4 = 1.4) */
	_u8	checksum;
	_u8	features1;	/* feature flags */
	_u8	features2;	/* feature flags */
	_u8	features3;	/* feature flags (reserved) */
	_u8	features4;	/* feature flags (reserved) */
	_u8	features5;	/* feature flags (reserved) */
}__attribute__((packed)) _mp_t;

typedef struct { /* MP Configuration Table */
	_s8	sign[4];	/* MPC signature ('PCMP') */
	_u16	len;		/* sizeof MPC table in bytes */
	_u8	msp_revision;	/* specification revision number */
	_u8	checksum;
	_s8	oem_id[8];
	_s8	product_id[12];
	_u32	oem_ptr;	/* pointer to OEM-defined configuration table (optional)	*/
	_u16	oem_sz;		/* size of OEM table (optional)	*/
	_u16	oem_count;	/* The number of entries following this base header table in memory. */
	_u32	lapic_ptr;	/* the physical address of local APIC for each processor */
	_u32	reserved;
}__attribute__((packed)) _mpc_t;

typedef struct { /* CPU (type 0) */
	_u8	lapic_id;	/* This is the unique APIC ID number for the processor.				*/
	_u8	lapic_version;	/* This is bits 0-7 of the Local APIC version number register.			*/
	_u8	flags;		/* bit 0:									*/
				/*  This bit indicates whether the processor is enabled. 			*/
				/*   If this bit is zero, the OS should not attempt to initialize this processor.*/
				/* bit 1:									*/
				/*   This bit indicates that the processor entry refers to the bootstrap 	*/
				/*   processor if set.								*/
	_u32	sign;		/* This is the CPU signature as would be returned by the CPUID instruction. 	*/
				/*   If the processor does not support the CPUID instruction, 			*/
				/*   the BIOS fills this value according to the values in the specification.	*/
	_u32	fflags;		/* This is the feature flags as would be returned by the CPUID instruction. 	*/
				/*   If the processor does not support the CPUID instruction, 			*/
				/*  the BIOS fills this value according to values in the specification.		*/
	_u32	reserved[2];
}__attribute__((packed)) _mpc_cpu_t;

typedef struct { /* BUS (type 1) */
	_u8	id;
	_u8	type[6];
}__attribute__((packed)) _mpc_bus_t;

typedef struct { /* IOAPIC (type 2) */
	_u8	id;		/* This is the ID of this IO APIC.						*/
	_u8	version;	/* This is bits 0-7 of the IO APIC's version register.				*/
	_u8	flags;		/* bit 0:									*/
				/*  This bit indicates whether this IO APIC is enabled. 			*/
				/*  If this bit is zero, the OS should not attempt to access this IO APIC.	*/
	_u32	ptr;		/* This contains the physical base address where this IO APIC is mapped.	*/
}__attribute__((packed)) _mpc_ioapic_t;

typedef struct {
	_u8	irq_type;
	_u16	irq_flags;
	_u8	src_bus;
	_u8	src_bus_irq;
	_u8	dst_apic;
	_u8	dst_irq;
}__attribute__((packed)) _mpc_intsrc_t;

typedef struct {
	_u8	irq_type;
	_u16	irq_flags;
	_u8	src_bus;
	_u8	src_bus_irq;
	_u8	dst_apic;
	_u8	dst_apic_lint;
}__attribute__((packed)) _mpc_lintsrc_t;

typedef struct {
	_u8	type;	/* record type */
	
	union {
		_mpc_cpu_t	cpu;
		_mpc_bus_t	bus;
		_mpc_ioapic_t	ioapic;
		_mpc_intsrc_t	intsrc;
		_mpc_lintsrc_t	lintsrc;
	} data;
}__attribute__((packed)) _mpc_record_t;

/* Multiprocessor floating pointer structure lookup */
_mp_t *mp_find_table(void);
/* extract pointer to MP configuration table */
_mpc_t *mp_get_mpc(_mp_t *p_mpf);
/* return the number of available processors */
_u16 mp_cpu_count(_mp_t *p_mpf);
/* return pointer to CPU record by zero based index */
_mpc_cpu_t *mp_get_cpu(_mp_t *p_mpf, _u16 cpu_idx);
/* return pointer to IO APIC */
_mpc_ioapic_t *mp_get_ioapic(_mp_t *p_mpf, _u16 ioapic_idx);
/* return pointer to BUS record by index */
_mpc_bus_t *mp_get_bus(_mp_t *p_mpf, _u16 bus_idx);
/* return pointer to INTSRC by index */
_mpc_intsrc_t *mp_get_intsrc(_mp_t *p_mpf, _u16 intsrc_idx);
/* return pointer to INTSRC by index */
_mpc_lintsrc_t *mp_get_lintsrc(_mp_t *p_mpf, _u16 lintsrc_idx);

#endif
