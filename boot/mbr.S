;MBR
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
	
	mov	byte	[gs:0x00],'1'
	mov	byte	[gs:0x01],0xA4
	mov	byte	[gs:0x02],' '
	mov	byte	[gs:0x03],0xA4
	mov	byte	[gs:0x04],'M'
	mov	byte	[gs:0x05],0xA4
	mov	byte	[gs:0x06],'B'
	mov	byte	[gs:0x07],0xA4
	mov	byte	[gs:0x08],'R'
	mov	byte	[gs:0x09],0xA4
	jmp	$			;make this program stay here
	

	times 510-($-$$) db 0
	db 	0x55,0xaa

	
