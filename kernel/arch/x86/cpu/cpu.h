#ifndef __CPU_H__
#define __CPU_H__

void cpu_vendor_id(_s8 *out);/* require min 12 byte by output */
_u16 cpu_logical_count(void);
_u8  cpu_apic_id(void);
_u8 cpu_get_cores(void);

#endif
