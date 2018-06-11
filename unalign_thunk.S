.globl _unalign
_unalign:
	//add $3, %rsp
	xchg %rdi, %rsi
	call *%rsi
	ret
