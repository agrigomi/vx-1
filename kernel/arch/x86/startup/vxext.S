.global _start
.section ".text"
.code64

_start:
	nop
	push	%rsi
	movq	16(%rsp), %rsi
	pushq	%rsi
	callq	_vx_ext_init_
	addq	$8, %rsp
	pop	%rsi
	ret
	
.end

