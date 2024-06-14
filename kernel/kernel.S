[bits 32]

%define	ERROR_CODE	nop				;if the cpu automic to push the error code so that don't do anything

%define	ZERO	push	0				;if the cpu wasn't automic push, so push 0 by ourself

extern idt_table					;in c file

section	.data
global	intr_entry_table
intr_entry_table:

%macro	VECTOR	2
section	.text
intr%1entry:						;%1 is the first param

	%2						;%2 is the second param
	;save the environment
	push 	ds
	push 	es
	push	fs
	push	gs
	pushad	

	mov	al,0x20					;interrupt end command:EOI
	out	0xa0,al
	out	0x20,al

	push	%1					;push the interrupt vector number
	
	call	[idt_table+ %1 * 4 ]			;call the interrupt function in c file
	jmp 	intr_exit
	


section	.data
	dd	intr%1entry				;in order to make the all element of intr_entry_table is the address of  every handle function
%endmacro

section .text
global intr_exit
intr_exit:
	add	esp,4					;skip the param(interrupt vector number)
	popad
	pop	gs
	pop	fs
	pop	es
	pop	ds
	add	esp,4					;skip the error code
	iretd


VECTOR	0x00,ZERO
VECTOR	0x01,ZERO
VECTOR	0x02,ZERO
VECTOR	0x03,ZERO
VECTOR	0x04,ZERO
VECTOR	0x05,ZERO
VECTOR	0x06,ZERO
VECTOR	0x07,ZERO
VECTOR	0x08,ERROR_CODE
VECTOR	0x09,ZERO
VECTOR	0x0a,ERROR_CODE
VECTOR	0x0b,ERROR_CODE
VECTOR	0x0c,ERROR_CODE
VECTOR	0x0d,ERROR_CODE
VECTOR	0x0e,ERROR_CODE
VECTOR	0x0f,ZERO
VECTOR	0x10,ZERO
VECTOR	0x11,ERROR_CODE
VECTOR	0x12,ZERO
VECTOR	0x13,ZERO
VECTOR	0x14,ZERO
VECTOR	0x15,ZERO
VECTOR	0x16,ZERO
VECTOR	0x17,ZERO
VECTOR	0x18,ZERO
VECTOR	0x19,ZERO
VECTOR	0x1a,ZERO
VECTOR	0x1b,ZERO
VECTOR	0x1c,ZERO
VECTOR	0x1d,ZERO
VECTOR	0x1e,ERROR_CODE
VECTOR	0x1f,ZERO
VECTOR	0x20,ZERO					;clock interrupt
VECTOR  0x21,ZERO					;keyboard interrupt
VECTOR  0x22,ZERO
VECTOR  0x23,ZERO
VECTOR  0x24,ZERO
VECTOR  0x25,ZERO
VECTOR  0x26,ZERO
VECTOR  0x27,ZERO
VECTOR  0x28,ZERO
VECTOR  0x29,ZERO
VECTOR  0x2a,ZERO
VECTOR  0x2b,ZERO
VECTOR  0x2c,ZERO
VECTOR  0x2d,ZERO
VECTOR  0x2e,ZERO
VECTOR  0x2f,ZERO



;;;;;;;;;;;;;;;;;;;; 0x80 interrupt ;;;;;;;;;;;;;;;;;;;;;;
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
	;1.save the context
	push 	0 

	push	ds
	push	es
	push	fs	
	push	gs
	pushad
	

	push	0x80

	;2.push params 
	push	edx					;3th arg
	push	ecx					;2nd arg
	push	ebx					;1st arg

	;3. call the handler
	call 	[sycall_table + ebx * 4]
	add 	esp,12					;jmp the three argument
	
	;4. get the return value
	mov	[esp + 8*4],eax
	jmp	intr_exita				; recover the context
	
