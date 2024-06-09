TI_GDT	equ	0
RPL0	equ	0
SELECTOR_VIDEO	equ	(0x0003 << 3) + TI_GDT + RPL0

section	.data
put_int_buffer	dq	0				;8 byte buffer
[bits 32]
section	.text
;-----------------------------put char---------------------
global put_char

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
	mov	ecx,[esp+36]				;get the char from stack,36 = 4 * 8,(8 is all register had pushed by 'pushad')
	
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

global set_cursor
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
	

;------------------------put_str----------------------------------
global	put_str
put_str:
	push	ebx
	push	ecx
	xor	ecx,ecx
	mov	ebx,[esp+12]				;get the position of string ,stack had the value of ebx and ecx (8 byte) and the address of function to return (4 byte)	
		
.goon:
	mov	cl,[ebx]
	cmp	cl,0					;0 is '\0'
	je	.put_over
	push	ecx					;push the param of put_char 
	call	put_char				;
	add	esp,4
	inc	ebx
	jmp	.goon

.put_over:
	pop	ecx
	pop	ebx
	ret

;-------------------------put_int-----------------------------
global	put_int
put_int:
	pushad
	mov	ebp,esp
	mov	eax,[ebp+4*9]				;the address of return and value of 8 register
	mov	edx,eax
	mov	edi,7					;the offset in buffer
	mov	ecx,8
	mov	ebx,put_int_buffer
	
.based16_4bits:
	
	and	edx,0x0000000F
	
	cmp	edx,9
	jg	.is_A2F					; >
	add	edx,'0'
	jmp	.store

.is_A2F:
	sub	edx,10					;sub 10: is the offset to A
	add	edx,'A'					;get the ascii
	
.store:
	mov	[ebx+edi],dl				;ebx is the base address of buffer,edi is the offset of buffer,dl is the ascii of char
	dec	edi

	shr	eax,4					;deal next number
	mov	edx,eax
	loop	.based16_4bits
	
.ready_to_print:
	inc	edi
.skip_prefix_0:
	cmp	di,8
	je	.full0

.go_on_skip:
	mov	cl,[put_int_buffer+edi]
	inc	edi					;next char
	cmp	cl,'0'
	je	.skip_prefix_0
	dec	edi					;due to the char isn't '0' , so resume the edi that point to the char and print
	jmp	.put_each_num

.full0:
	mov	cl,'0'	
.put_each_num:
	push	ecx
	call	put_char
	add	esp,4					;clear the param stack in put_char
	inc	edi					;next char
	mov	cl,[put_int_buffer+edi]
	cmp	edi,8
	jl	.put_each_num
	popad
	ret
	

global set_cursor
set_cursor:
	pushad
	; 32 bytes of 8 registers and 4 bytes of return address
	mov bx, [esp+36]
	; set high 8 bits
	mov dx, 0x03d4
	mov al, 0x0e
	out dx, al

	mov dx, 0x03d5
	mov al, bh
	out dx, al

	; set low 8 bits
	mov dx, 0x03d4
	mov al, 0x0f
	out dx, al

	mov dx, 0x03d5
	mov al, bl
	out dx, al

	popad
	ret
