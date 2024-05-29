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
	
	
.is_backspace:
	dec	bx
	shl	bx,1					;move lefe 1 bit == X2 , now bx is the offset of cursor in graphics memory 

	mov	byte	[gs:bx],0x20			;0x20 is the space's ascii
	inc	bx
	mov	byte	[gs:bx],0x07			;0x07 is the attribute of char
	shr	bx,1
	jmp	.set_cursor

.put_other:
	shl 	bx,1					;now bx is the offset of cursor in graphics memory	
	mov	[gs:bx],cl				;
	inc	bx	
	mov	byte 	[gs:bx],0x07			;the attribute of char
	shr	bx,1					;recover the value of cursor
	inc	bx					;next cursor
	cmp	bx,2000
	jl	.set_cursor				;

.is_line_feed:
.is_carriage_return:
	xor	dx,dx					;dx is the high 16 bit
	mov	ax,bx					;ax is the low 16 bit
	mov	si,80
	
	div	si
	sub	bx,dx					;rounding 
	
.is_carriage_return_end:
	add	bx,80
	cmp	bx,2000
.is_line_feed_end:
	jl	.set_cursor


.roll_screen:

	;mov 1~24 to 0~23
	cld	
	mov	ecx,960					;960:	there are 2000-80 = 1920 char need to move , 1920 X 2 = 3820 byte, move 4 byte every time .so 3820 / 4 = 960
	
	mov	esi,0xc00b80a0				; second line
	mov	edi,0xc00b8000				; first line
	rep	movsd

	;set the last line to space
	mov	ebx,3840
	mov	ecx,80
.cls:
	mov	word	[gs:ebx],0x0720			;0x0720 is 2 byte contain ascii and attribute
	add	ebx,2
	loop	.cls
	mov	bx,1920					;make the cursor at the head of last line

.set_cursor:
	;set high 8 bit
	mov	dx,0x03d4
	mov	al,0x0e
	out	dx,al
	mov	dx,0x03d5
	mov	al,bh
	out	dx,al
	;set low 8 bit
	mov	dx,0x03d4
	mov	al,0x0f
	out	dx,al
	mov	dx,0x03d5
	mov	al,bl
	out	dx,al

.put_char_done:
	popad
	ret
	
	
