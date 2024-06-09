[bits 32]
section .text
global switch_to
switch_to:
	;save context of current thread
	;<- in stack :here is the return address
	push	esi
	push 	edi
	push	ebx
	push	ebp

	mov	eax,[esp+20]				;get the cur(current thread)
	mov	[eax],esp				;save the esp(stack of current thread)

	;resume context of next thread
	mov	eax,[esp+24]
	mov	esp,[eax]				;reload new esp(stack of next thread)
	
	pop	ebp
	pop	ebx
	pop	edi
	pop	esi
	ret						;here is return address in new stack

