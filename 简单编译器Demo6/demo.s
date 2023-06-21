	.globl main
	.type main, @function
main:
	.cfi_startproc
	endbr64
	pushq %rbp
	mov %rsp,%rbp
	subq $312,%rsp
	movq $2,%rbx
	movq %rbx,-288(%rbp)
	movq $1,%rbx
	movq %rbx,-296(%rbp)
	movq $1,%rbx
	movq %rbx,-304(%rbp)
	movq $6,%rbx
	movq %rbx,-104(%rbp)
	xor %rax,%rax
	xor %rbx,%rbx
	addq -288(%rbp),%rbx
	imulq $48,%rbx
	subq %rbx,%rax
	xor %rbx,%rbx
	addq -296(%rbp),%rbx
	imulq $8,%rbx
	subq %rbx,%rax	
xor %rax,%rax
	movq $8,%rbx
	movq %rbx,-0(%rbp,%rax,1)
	addq $312,%rsp
	popq %rbp
	ret
	.cfi_endproc
.Flabelmain:

