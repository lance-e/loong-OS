%include	"boot.inc"
section	loader	vstart=LOADER_BASE_ADDR
LOADER_STAcK_TOP	equ	LOADER_BASE_ADDR
jmp	loader_start

GDT_BASE:	dd	0x00000000			;this is 0th descriptor
		dd	0x00000000
CODE_DESC:	dd	0x0000FFFF
		dd	DESC_CODE_HIGH4
DATA_STACK_DESC: dd	0x0000FFFF
		dd	DESC_DATA_HIGH4
VIDEO_DESC:	dd	0x80000007			;limit=(0xbffff - 0xb8000) / 4k = 0x7
		dd	DESC_VIDEO_HIGH4
GDT_SIZE	equ	$ - GDT_BASE
GDT_LIMIT	equ	GDT_SIZE - 1

times	60	dq	0				;reserve 60 descriptor (8 byte);

SELECTOR_CODE	equ	(0x0001 << 3 ) + TI_GDT + RPL0
SELECTOR_DATA	equ	(0x0002 << 3 ) + TI_GDT + RPL0
SELECTOR_VIDEO	equ	(0x0003 << 3 ) + TI_GDT + RPL0

gdt_ptr		dw	GDT_LIMIT
		dd	GDT_BASE
loadermsg	db	"going to the loader"
	
loader_start:
	;print the string

	mov	sp,LOADER_BASE_ADDR
	mov	bp,loadermsg
	mov	cx,19
	mov	ax,0x1301
	mov	bx,0x001f
	mov	dx,0x1800
	int	0x10

	;enter to the pretect mode
	;1.open the A20 address line
	;2.load the gdt
	;3.set the cr0's pe to 1
	
	in	al,0x92
	or	al,0000_0010B
	out	0x92,al

	ldtr	[gdt_ptr]
	
	mov	eax,cr0
	or	eax,0x00000001
	mov	cr0,eax

	;over
	jmp	dword	SELECTOR_CODE:p_mode_start

[bits 32]
p_mode_start:
	mov	ax,SELECTOR_DATA
	mov	ds,ax
	mov	es,ax
	mov	ss,ax
	mov	esp,LOADER_STACK_TOP
	mov	ax,SELECTOR_VIDEO
	mov	gs,ax
	
	mov	byte	[gs:160], 'P'

	jmp	$
