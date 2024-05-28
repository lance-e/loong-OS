TI_GDT	equ	0
RPL0	equ	0
SELECTOR_VIDEO	equ	(0x0003 << 3) + TI_GDT + RPL0

[bits 32]
section	.text
;-----------------------------put char---------------------

global	put_char
put_char:
	pushad						;copy the register environment; there are 8 register
	mov	ax,SELECTOR_VIDEO
	mov	gs,ax

;----------get the cursor position---------------------
	;get high 8 bit
	mov	dx,0x03d4				;operate the address register
	mov	al,0x0e
	out	dx,al
	mov	dx,0x03d5				;operate the data register
	in	al,dx	
	mov	ah,al

	;get low 8 bit
	mov	dx,0x03d4	
	mov	al,0x0f
	out	dx,al
	mov	dx,0x03d5
	in	al,dx
	
;------------get char-----------------------------------
	mov	bx,ax					;save the cursor into bx
	mov	ecx,[esp+36]				;get the char from stack,36 = 4 * 8
	
;-------------judge control char-----------------------
	cmp	cl,0xd
	jz	.is_carriage_return 
	cmp	cl,0xa
	jz	.is_line_feed
	cmp	cl,0x8
	jz	.is_backspace
	jmp	.put_other
	
	
