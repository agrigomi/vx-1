#ifndef __I_CPU_H__
#define __I_CPU_H__

#include "mgtype.h"
#include "vxmod.h"

#define I_CPU		"i_cpu"
#define I_CPU_HOST	"i_cpu_host"

/* privilage level */
#define DPL_SYS	0
#define DPL_USR	3

/* CPU data context */
typedef _p_data_t	_cpu_cxt_t;

/*thread entry */
typedef _s32 _context_entry_t(void *p_data); 

typedef struct {
	_u32 	signature;
	_u16	id;
	_u8	flags;
	_u16	ncores;
}_cpu_init_info_t;

typedef struct {
	_bool (*is_boot)(_cpu_cxt_t); /* return 1 if current CPU is boot CPU */
	_u16 (*cpu_id)(_cpu_cxt_t); /* return CPU id (LAPIC id) */
	_u16 (*ccpu_id)(_cpu_cxt_t); /* return current CPU id */
	_u16 (*get_ncores)(_cpu_cxt_t); /* return number of cores */
	HCONTEXT (*get_vmm)(_cpu_cxt_t); /* get VMM context */
	HCONTEXT (*get_blender)(_cpu_cxt_t); /* get blender contrext */
	void (*set_blender)(_cpu_cxt_t, HCONTEXT); /* set blender context */
	_bool (*is_ready)(_cpu_cxt_t); /* return 1 if CPU is completely initialized */
	_bool (*send_init_ipi)(_cpu_cxt_t, _u16 cpu_id, _u32 vector); /* inter CPU interrupt */
	_u32 (*cpu_context_size)(void); /* return size of CPU state */
	_u32 (*fpu_context_size)(void); /* return size of FPU state */
	void (*halt)(void);
	/* create new executable context in 'context_buffer' passed outside */
	void (*create_exec_context)(_cpu_cxt_t, _p_data_t stack, _u32 stack_size, _context_entry_t *p_entry, 
				_u8 dpl, _p_data_t udata, _p_data_t context_buffer);
	void (*switch_exec_context)(void); /* switch executable context */
	void (*set_timer)(_cpu_cxt_t, _u32 countdown);
	_u32 (*get_timer)(_cpu_cxt_t);
	_u64 (*timestamp)(void);
	_bool (*enable_interrupts)(_bool enable); /* return prev flag state */
	void (*enable_cache)(_cpu_cxt_t, _bool enable);
	void (*idle)(_cpu_cxt_t);
	void (*copy_exec_context)(_p_data_t dst, _p_data_t context);
	void (*save_fpu_context)(_p_data_t buffer);
	void (*restore_fpu_context)(_p_data_t buffer);
	void (*init)(_cpu_cxt_t, _cpu_init_info_t *); /* initialize CPU */
	void (*init_info)(_cpu_cxt_t, _cpu_init_info_t *); /* retrive CPU init info */
	void (*start)(_cpu_cxt_t);
	void (*interrupt)(_cpu_cxt_t, _u8 in, _p_data_t context);
	void (*end_of_interrupt)(_cpu_cxt_t);
}_i_cpu_t;

/* IRQ flags */
#define IRQF_DST_LOGICAL	(1<<0)	/* IRQ should be received by more than one CPU */
#define IRQF_POLARITY_LO	(1<<1)  /* input pin polarity */
#define IRQF_MASKED		(1<<2)  /* mask interrupt signal (disable) */
#define IRQF_TRIGGER_LEVEL	(1<<3)  
#define IRQF_LO_PRIORITY	(1<<4)

/* protorype of interrupt callback */
typedef void _int_callback_t(HCONTEXT hc_cpu, void *p_cpu_state, _p_data_t data);

typedef struct {
	HCONTEXT (*create_cpu)(_u32 signature, _u16 id, _u8 flags);
	HCONTEXT (*get_cpu)(_u16 id);
	/*HCONTEXT (*get_cpu_by_index)(_u16 idx); */
	HCONTEXT (*get_current_cpu)(void);
	HCONTEXT (*get_boot_cpu)(void);
	_u16     (*get_cpu_count)(void);
	_int_callback_t *(*get_isr)(_u8 num);
	_p_data_t (*get_isr_data)(_u8 num);
	void     (*set_isr)(_u8 num, _int_callback_t *p_isr, _p_data_t);
	_bool    (*set_irq)(_u8 irq, _u16 dst_cpu, _u8 int_vector, _u32 flags);
	void     (*start)(void);
	_bool    (*is_running)(void);
}_i_cpu_host_t;

#endif

