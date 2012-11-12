.text

.global _get_stack_pointer

_get_stack_pointer:
	movl %esp, %eax
	ret
