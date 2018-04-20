/* -*- linux-c -*- ------------------------------------------------------- *
 *
 *   Copyright (C) 1991, 1992 Linus Torvalds
 *   Copyright 2007-2008 rPath, Inc. - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author H. Peter Anvin
 *
 *   This file is part of the Linux kernel, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */

/*
 * Enable A20 gate (return -1 on failure)
 */
#include "bootx86.h"
#include "code16gcc.h"

static void io_delay(void) {
	const _u16 DELAY_PORT = 0x80;
	__asm__ __volatile__("outb %%al,%0" : : "dN" (DELAY_PORT));
}

static void outb(_u8 v, _u16 port) {
	__asm__ __volatile__("outb %0,%1" : : "a" (v), "dN" (port));
}

static _u8 inb(_u16 port) {
	_u8 v;
	__asm__ __volatile__("inb %1,%0" : "=a" (v) : "dN" (port));
	return v;
}

static void set_fs(_u16 seg) {
	__asm__ __volatile__("movw %0,%%fs" : : "rm" (seg));
}

static void set_gs(_u16 seg) {
	__asm__ __volatile__("movw %0,%%gs" : : "rm" (seg));
}

static _u32 rdfs32(addr_t addr) {
	_u32 v;
	__asm__ __volatile__("movl %%fs:%1,%0" : "=r" (v) : "m" (*(_u32 *)addr));
	return v;
}

static void wrfs32(_u32 v, addr_t addr) {
	__asm__ __volatile__("movl %1,%%fs:%0" : "+m" (*(_u32 *)addr) : "ri" (v));
}

static _u32 rdgs32(addr_t addr) {
	_u32 v;
	__asm__ __volatile__("movl %%gs:%1,%0" : "=r" (v) : "m" (*(_u32 *)addr));
	return v;
}

#define MAX_8042_LOOPS	100000
#define MAX_8042_FF	32

static int empty_8042(void)
{
	_u8 status;
	int loops = MAX_8042_LOOPS;
	int ffs   = MAX_8042_FF;

	while (loops--) {
		io_delay();

		status = inb(0x64);
		if (status == 0xff) {
			/* FF is a plausible, but very unlikely status */
			if (!--ffs)
				return -1; /* Assume no KBC present */
		}
		if (status & 1) {
			/* Read and discard input data */
			io_delay();
			(void)inb(0x60);
		} else if (!(status & 2)) {
			/* Buffers empty, finished! */
			return 0;
		}
	}

	return -1;
}

/* Returns nonzero if the A20 line is enabled.  The memory address
   used as a test is the int $0x80 vector, which should be safe. */

#define A20_TEST_ADDR	(4*0x80)
#define A20_TEST_SHORT  32
#define A20_TEST_LONG	2097152	/* 2^21 */


static int a20_test(int loops)
{
	int ok = 0;
	int saved, ctr;

	set_fs(0x0000);
	set_gs(0xffff);

	saved = ctr = rdfs32(A20_TEST_ADDR);

	while (loops--) {
		wrfs32(++ctr, A20_TEST_ADDR);
		io_delay();	/* Serialize and make delay constant */
		ok = rdgs32(A20_TEST_ADDR+0x10) ^ ctr;
		if (ok)
			break;
	}

	wrfs32(saved, A20_TEST_ADDR);
	return ok;
}

/* Quick test to see if A20 is already enabled */
static int a20_test_short(void)
{
	return a20_test(A20_TEST_SHORT);
}

/* Longer test that actually waits for A20 to come on line; this
   is useful when dealing with the KBC or other slow external circuitry. */
static int a20_test_long(void)
{
	return a20_test(A20_TEST_LONG);
}

static void enable_a20_kbc(void)
{
	empty_8042();

	outb(0xd1, 0x64);	/* Command write */
	empty_8042();

	outb(0xdf, 0x60);	/* A20 on */
	empty_8042();

	outb(0xff, 0x64);	/* Null command, but UHCI wants it */
	empty_8042();
}

static void enable_a20_fast(void)
{
	_u8 port_a;

	port_a = inb(0x92);	/* Configuration port A */
	port_a |=  0x02;	/* Enable A20 */
	port_a &= ~0x01;	/* Do not reset machine */
	outb(port_a, 0x92);
}

/*
 * Actual routine to enable A20; return 0 on ok, -1 on failure
 */

#define A20_ENABLE_LOOPS 255	/* Number of times to try */

int enable_a20(void)
{
       int loops = A20_ENABLE_LOOPS;
       int kbc_err;

       while (loops--) {
	       /* First, check to see if A20 is already enabled
		  (legacy free, etc.) */
	       if (a20_test_short())
		       return 0;
	       
	       /* Next, try the BIOS (INT 0x15, AX=0x2401) */
	       if (a20_test_short())
		       return 0;
	       
	       /* Try enabling A20 through the keyboard controller */
	       kbc_err = empty_8042();

	       if (a20_test_short())
		       return 0; /* BIOS worked, but with delayed reaction */
	
	       if (!kbc_err) {
		       enable_a20_kbc();
		       if (a20_test_long())
			       return 0;
	       }
	       
	       /* Finally, try enabling the "fast A20 gate" */
	       enable_a20_fast();
	       if (a20_test_long())
		       return 0;
       }
       
       return -1;
}
