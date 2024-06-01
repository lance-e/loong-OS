[bits 32]

%define	ERROR_CODE	nop				;if the cpu automic to push the error code so that don't do anything

%define	ZERO	push	0				;if the cpu wasn't automic push, so push 0 by ourself

extern	put_str

section	.data
intr_str	db	"interrupt occur!",0xa,0

global	intr_entry_table
intr_entry_table:

%macro	VECTOR	2
section	.text
intr%1entry:						;%1 is the first param

	%2						;%2 is the second param
	push	intr_str
	call	put_str
	add	esp,4
	

	mov	al,0x20					;interrupt end command:EOI
	out	0xa0,al
	out	0x20,al

	add	esp,4
	iret
section	.data
	dd	intr%1entry				;in order to make the all element of intr_entry_table is the address of  every handle function

%endmacro


VECTOR	0x00,ZERO
VECTOR	0x01,ZERO
VECTOR	0x02,ZERO
VECTOR	0x03,ZERO
VECTOR	0x04,ZERO
VECTOR	0x05,ZERO
VECTOR	0x06,ZERO
VECTOR	0x07,ZERO
VECTOR	0x08,ZERO
VECTOR	0x09,ZERO
VECTOR	0x0a,ZERO
VECTOR	0x0b,ZERO
VECTOR	0x0c,ZERO
VECTOR	0x0d,ZERO
VECTOR	0x0e,ZERO
VECTOR	0x0f,ZERO
VECTOR	0x10,ZERO
VECTOR	0x11,ZERO
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
VECTOR	0x20,ZERO

