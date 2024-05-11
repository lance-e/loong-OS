;MBR
SECTION MBR vstart=0x7c00
	mov	ax,cs
	mov 	ds,ax
	mov 	es,ax
	mov 	ss,ax
	mov 	fs,ax
	mov 	sp,0x7c00

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
	
;the tree line is to get the position of cursor
	mov	ah,3
	mov	bh,0
	
	int 	0x10

;begin to print the string,still use the 0x10 interrupt,but the function number is 0x13
	mov 	ax,message
	mov 	bp,ax			;es:bp is the begin of this string
	
	mov 	cx,5			;the length of this string
	mov	ax,0x1301		;AH is to set the function number(0x13) ,AL is to set the method of write string
	mov 	bx,0x2			;BH is to set the number of which page to print string,BL is to set the attribute of string
	
	int 	0x10

	jmp	$			;make this program stay here
	

	message db "1 MBR"
	times 510-($-$$) db 0
	db 	0x55,0xaa

	
