	.comm exvar1,160,160
	.comm exvar2,8,8
	.comm exvar3,8,8
	.comm exvar4,8,8
	.globl main
	.type main, @function
main:
	.cfi_startproc
	endbr64
	pushq %rbp
	mov %rsp,%rbp
	subq $16,%rsp
	movq $20,%rbx
	movq %rbx,-0(%rbp)
	movq $10,%rbx
	movq %rbx,-8(%rbp)
	xor %rax,%rax
	xor %rbx,%rbx
	addq -8(%rbp),%rbx
	imulq $8,%rbx
	subq %rbx,%rax
	movq exvar1+0(%rip),%rbx
	movq %rbx,-0(%rbp)
	addq $16,%rsp
	popq %rbp
	ret
	.cfi_endproc
.Flabelmain:

