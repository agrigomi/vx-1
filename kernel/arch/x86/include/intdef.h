#ifndef __INTDEF_H__
#define __INTDEF_H__

/* IRQ */
#define IRQ_KBD		1	/* keyboard IRQ */
#define IRQ_SLAVE_PIC	2
#define IRQ_FLOPPY	6
#define IRQ_RTC		8
#define IRQ_IDE_CH1	14
#define IRQ_IDE_CH2	15

/* exceptions */
#define EX_DE		0	/* divide by zero */
#define EX_DB		1	/* debug */
#define EX_NMI		2	/* Non-maskable-interrupt */
#define EX_BP		3	/* breakpoint */
#define EX_OF		4	/* overflow */
#define EX_BR		5	/* Bound-Range */
#define EX_DU		6	/* Invalid opcode */
#define EX_NM		7	/* device not available */
#define EX_DF		8	/* double fault */
#define EX_TS		10	/* invalid TSS */
#define EX_NP		11	/* segment not present */
#define EX_SS		12	/* stack */
#define EX_GP		13	/* general protection */
#define EX_PF		14	/* page fault */
#define EX_MF		16	/* floating point exception */
#define EX_AC		17	/* alignment check */
#define EX_MC		18	/* machine check */
#define EX_XF		19	/* SIMF floating point */
#define EX_SX		30	/* security exception */

/* interrupt vectors */
#define INT_KBD		0x45	/* keyboard */
#define INT_FLOPPY	0x50
#define INT_IDE_CH1	0x51	/* first IDE channel IRQ14 */
#define INT_IDE_CH2	0x52	/* second IDE channel IRQ15 */
#define INT_AHCI	0x60	/* AHCI controllers 0x60 to 0x6f */
#define INT_USB		0x70	/* USB host controller 0x70 to 0x7f */
#define INT_SYSCALL	0x80	/* SYSCALL */
#define INT_SLAVE_PIC	0xfb
#define INT_RTC		0xfc	/* RTC */
#define INT_LAPIC_SRI	0xfd	/* LAPIC SRI vector */
#define INT_LAPIC_TIMER	0xfe	/* LOCAL APIC timer */
#define INT_TASK_SWITCH	0xff	/* switch context */

#endif

