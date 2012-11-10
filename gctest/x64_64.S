.text

.global get_stack_pointer
.seh_proc get_stack_pointer

get_stack_pointer:
	.seh_endprologue
	movq %rsp, %rax
	ret
	.seh_endproc
