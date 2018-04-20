.global _start
.extern _vx_core_init_
.extern CORE_ADDR
.extern CORE_VADDR
.extern CORE_ARCH
.extern CORE_VERSION
.section ".text"
.code64

_start:
	nop
	cli
/*
	jmp	__l1
__g_core_arch__		:	.byte	CORE_ARCH
__g_core_addr__		:	.long	CORE_ADDR 
__g_cire_vaddr__	:	.quad	CORE_VADDR
__g_core_version__	:	.int	CORE_VERSION
__l1:
*/
	pushq	8(%rsp) /* expect _core_startup_t* parameter */
	callq	_vx_core_init_
	addq	$8, %rsp
	ret
	
.global __g_core_arch__
.global __g_core_addr__
.global __g_core_vaddr__
.global __g_cire_version__
.end
