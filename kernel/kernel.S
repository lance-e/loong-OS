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
	call 	[syscall_table + ebx * 4]
	add 	esp,12					;jmp the three argument
	
	;4. get the return value
	mov	[esp + 8*4],eax
	jmp	intr_exit				; recover the context
	

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
VECTOR 	0x30,ZERO
VECTOR 	0x31,ZERO
VECTOR	0x32,ZERO
VECTOR 	0x33,ZERO
VECTOR 	0x34,ZERO
VECTOR 	0x35,ZERO
VECTOR 	0x36,ZERO
VECTOR  0x37,ZERO
VECTOR  0x38,ZERO
VECTOR  0x39,ZERO
VECTOR  0x3A,ZERO
VECTOR  0x3B,ZERO
VECTOR  0x3C,ZERO
VECTOR  0x3D,ZERO
VECTOR  0x3E,ZERO
VECTOR  0x3F,ZERO
VECTOR  0x40,ZERO
VECTOR  0x41,ZERO
VECTOR  0x42,ZERO
VECTOR  0x43,ZERO
VECTOR  0x44,ZERO
VECTOR  0x45,ZERO
VECTOR  0x46,ZERO
VECTOR  0x47,ZERO
VECTOR  0x48,ZERO
VECTOR  0x49,ZERO
VECTOR  0x4A,ZERO
VECTOR  0x4B,ZERO
VECTOR  0x4C,ZERO
VECTOR  0x4D,ZERO
VECTOR  0x4E,ZERO
VECTOR  0x4F,ZERO
VECTOR  0x50,ZERO
VECTOR  0x51,ZERO
VECTOR  0x52,ZERO
VECTOR  0x53,ZERO
VECTOR  0x54,ZERO
VECTOR  0x55,ZERO
VECTOR  0x56,ZERO
VECTOR  0x57,ZERO
VECTOR  0x58,ZERO
VECTOR  0x59,ZERO
VECTOR  0x5A,ZERO
VECTOR  0x5B,ZERO
VECTOR  0x5C,ZERO
VECTOR  0x5D,ZERO
VECTOR  0x5E,ZERO
VECTOR  0x5F,ZERO
VECTOR  0x61,ZERO
VECTOR  0x62,ZERO
VECTOR  0x63,ZERO
VECTOR  0x64,ZERO
VECTOR  0x65,ZERO
VECTOR  0x66,ZERO
VECTOR  0x67,ZERO
VECTOR  0x68,ZERO
VECTOR  0x69,ZERO
VECTOR  0x6A,ZERO
VECTOR  0x6B,ZERO
VECTOR  0x6C,ZERO
VECTOR  0x6D,ZERO
VECTOR  0x6E,ZERO
VECTOR  0x6F,ZERO
VECTOR  0x70,ZERO
VECTOR  0x71,ZERO
VECTOR  0x72,ZERO
VECTOR  0x73,ZERO
VECTOR  0x74,ZERO
VECTOR  0x75,ZERO
VECTOR  0x76,ZERO
VECTOR  0x77,ZERO
VECTOR  0x78,ZERO
VECTOR  0x79,ZERO
VECTOR  0x7A,ZERO
VECTOR  0x7B,ZERO
VECTOR  0x7C,ZERO
VECTOR  0x7D,ZERO
VECTOR  0x7E,ZERO
VECTOR  0x7F,ZERO
VECTOR  0x80,ZERO
