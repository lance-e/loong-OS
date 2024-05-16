%include	"boot.inc"
SECTION MBR vstart=0x7c00
	mov	ax,cs
	mov 	ds,ax
	mov 	es,ax
	mov 	ss,ax
	mov 	fs,ax
	mov 	sp,0x7c00
	
	mov 	ax,0xb800 		;add to operate the graphics memory,and 0xb8000 is the begining of the graphics memory 
	mov	gs,ax			

;clear the screen: use the 0x10 interrupt
;input:
;AH = 0x06(function number)
;AL = the upScroll's row number
;BH = the attribute of the upScroll
;(CL,CH) = (x,y) in the upper left corner of windows
;(DL,DH) = (x,y) int the lower right corner of windows

	mov 	ax,0x0600
	mov 	bx,0x0700
	mov 	cx,0			;the upper left corner (0,0)
	mov 	dx,0x184f		;the lower right corner (24,79)  (0x18 = 24,0x4f=79)
	
	int 	0x10
	
	mov	byte	[gs:0x00],'L'
	mov	byte	[gs:0x01],0xA4
	mov	byte	[gs:0x02],'O'
	mov	byte	[gs:0x03],0xA4
	mov	byte	[gs:0x04],'O'
	mov	byte	[gs:0x05],0xA4
	mov	byte	[gs:0x06],'N'
	mov	byte	[gs:0x07],0xA4
	mov	byte	[gs:0x08],'G'
	mov	byte	[gs:0x09],0xA4
	mov	byte	[gs:0x0A],'-'
	mov	byte	[gs:0x0B],0xA4
	mov	byte	[gs:0x0C],'O'
	mov	byte	[gs:0x0D],0xA4
	mov	byte	[gs:0x0E],'S'
	mov	byte	[gs:0x0F],0xA4

	;begin to load the loader
	;operate the io
	mov	eax,LOADER_START_SECTOR
	mov	bx,LOADER_BASE_ADDR
	mov	cx,1
	call	rd_disk_m_16
	jmp	LOADER_BASE_ADDR
rd_disk_m_16:
	;copy the value
	mov	esi,eax
	mov	di,cx

	mov	al,cl
	mov	dx,0x1F2
	out	dx,al
	
	mov	eax,esi				;recover the value of ax

	mov	dx,0x1F3
	out	dx,al
	
	mov	cl,0x8
	shr	eax,cl
	mov	dx,0x1F4
	out	dx,al

	shr	eax,cl
	mov	dx,0x1F5
	out	dx,al

	shr	eax,cl
	and	al,0x0f
	or 	al,0xe0			;11100000 = 0xe0,
	mov	dx,0x1F6
	out	dx,al
	mov	ax,0x20		
	mov	dx,0x1F7
	out	dx,al

   .not_ready:
	nop
	in	al,dx
	and	al,0x88
	cmp	al,0x08
	jnz	.not_ready

	;begine to loop read
	;set the source 
	mov	ax,di
	mov	dx,256
	mul	dx
	mov	cx,ax
	mov	dx,0x1F0
   .loop_read_data:
	in	ax,dx
	mov	[bx],ax
	
	add	bx,2
	loop	.loop_read_data
	
	;read over
	ret

	times 510-($-$$) db 0
	db 	0x55,0xaa

	
