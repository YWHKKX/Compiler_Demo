	.globl main
	.type main, @function
main:
	.cfi_startproc
	endbr64
	pushq %rbp
	mov %rsp,%rbp
	subq $40,%rsp
	movq $10,%rbx
	movq %rbx,-0(%rbp)
	movq $5,%rbx
	movq %rbx,-8(%rbp)
	movq $3,%rbx
	movq %rbx,-16(%rbp)
	movq $1,%rbx
	movq %rbx,-24(%rbp)
	xor %rbx,%rbx
	movq -0(%rbp),%rax
	divq -8(%rbp)
	movq %rax,%rbx
	movq %rbx,-32(%rbp)
	movq -32(%rbp),%rax
	addq $40,%rsp
	popq %rbp
	ret
	.cfi_endproc
label1:
