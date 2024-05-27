%include	"boot.inc"
section	loader	vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP	equ	LOADER_BASE_ADDR

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

total_mem_bytes	dd	0				;!!!save the total memory capacity

gdt_ptr		dw	GDT_LIMIT			;the pointer of GDT
		dd	GDT_BASE
	
ards_buf	times	244	db	0		;in order to memory alignment 
ards_nr		dw	0				;record the number of ARDS
loader_start:
	;get the physic memory
	xor	ebx,ebx					;EBX:set the ebx = 0 
	mov	edx,0x534d4150				;EDX:the ascii of SMAP
	mov	di,ards_buf				;DI:set the buffer of ARDS

;---------------------int 0x15 eax=0xe820------------------------
.e820_mem_get_loop:
	mov	eax,0x0000e820				;EAX:set the function number is 0xe820 per loop,because of the EAX will update to the 0x534d4150 after eachinterruption 
	mov	ecx,20					;ECX:set the size of ARDS
	int	0x15				
	jc	.e820_failed_so_try_e801	
	
	add	di,cx					;add 20 byte that make the di point to the next ARDS
	inc	word	[ards_nr]			;record the number of the ARDS
	cmp	ebx,0					;if the ebx = 0 and the cf != 1 mean that the ARDS are all return.this one is the last one
	jnz	.e820_mem_get_loop

	;calculate the memory capacity 
	mov	cx,[ards_nr]
	mov	ebx,ards_buf
	xor	edx,edx
.find_max_mem_area:
	mov	eax,[ebx]				;base_add_low
	add	eax,[ebx+8]				;length_low
	add	ebx,20					;point to the next ARDS
	cmp	edx,eax
	jge	.next_ards
	mov	edx,eax					;edx = the all memory
.next_ards:
	loop	.find_max_mem_area
	jmp	.mem_get_ok

;-----------------------int 0x15 ax=0xe801---------------------
.e820_failed_so_try_e801:
	mov	ax,0xe801
	int	0x15
	jc	.e801_failed_so_try88
	
	;calculate the memory capacity
	;1.the memory less than 15mb
	mov	cx,0x400
	mul	cx
	shl	edx,16
	and	eax,0x0000FFFF
	or 	edx,eax
	add	edx,0x100000
	mov	esi,edx
	;2.the memory more than 16mb
	xor	eax,eax
	mov	ax,bx
	mov	ecx,0x10000
	mul	ecx
	add	esi,eax
	mov	edx,esi					;edx = the all memory
	jmp	.mem_get_ok

;------------------------int 0x15 ah=0x88-------------------------
.e801_failed_so_try88:
	mov	ah,0x88
	int	0x15
	jc	.error_hlt
	and	eax,0x0000FFFF
	mov	cx,0x400
	mul	cx
	shl	edx,16
	or	edx,eax
	add	edx,0x100000
	
.mem_get_ok:
	mov	[total_mem_bytes],edx
	
	
	;enter to the pretect mode
	;1.open the A20 address line
	;2.load the gdt
	;3.set the cr0's pe to 1
	
	in	al,0x92
	or	al,0000_0010B
	out	0x92,al

	lgdt	[gdt_ptr]
	
	mov	eax,cr0
	or	eax,0x00000001
	mov	cr0,eax

	;over
	jmp	dword	SELECTOR_CODE:p_mode_start
.error_hlt:
	jmp	$

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

;--------------------------load kernel--------------------------------

	mov	eax,KERNEL_START_SECTOR			;the sector of kernel	
	mov	ebx,KERNEL_BIN_BASE_ADDR		;the target address of load the kernel
	mov	ecx,200					;the number of kernel's sector
	
	call	rd_disk_m_32


;------------------------turn on the page machanism-----------------------------
	call	setup_page
	
	sgdt	[gdt_ptr]
	
	mov	ebx,[gdt_ptr+2]				;the video descriptor
	or	dword	[ebx+0x18+4],0xc0000000
	add	dword 	[gdt_ptr+2],0xc0000000
	add	esp,0xc0000000
	
	mov	eax,PAGE_DIR_TABLE_POS			;set cr3 = the position of page directory 
	mov	cr3,eax
	
	mov	eax,cr0
	or 	eax,0x80000000				;set the pg position of cr0
	mov	cr0,eax

	lgdt 	[gdt_ptr]				;reload the new gdt

;---------------enter kernel--------------------------------

	jmp	SELECTOR_CODE:enter_kernel		;refresh assembly line,just in case
enter_kernel:	
	call	kernel_init
	mov	esp,0xc009f000
	mov	byte	[gs:160], 'K'
	jmp	KERNEL_ENTRY_POINT



;---------------setup page ----------------------------
setup_page:
	mov	ecx,4096				;the size of page directory is 4kb , so set the ecx= 4096
	mov	esi,0
	;clear the memory
.clear_page_dir:
	mov	byte	[PAGE_DIR_TABLE_POS+esi],0
	inc	esi
	loop	.clear_page_dir				

	;create the page directory entry
.create_pde:	
	mov	eax,PAGE_DIR_TABLE_POS
	add 	eax,0x1000
	mov	ebx,eax					;ebx is the base address of pte
	or	eax,PG_US_U | PG_RW_W | PG_P		;eax=0x101111 ,this is position and attribute of first page table
	mov	[PAGE_DIR_TABLE_POS + 0x0],eax
	mov	[PAGE_DIR_TABLE_POS + 0xc00],eax	;0xc00 is the boundary between user program  and operation system
	sub	eax,0x1000
	mov	[PAGE_DIR_TABLE_POS+4092],eax
	
	;create the page table entry
	mov	ecx,256
	mov	esi,0
	mov	edx,PG_US_U | PG_RW_W | PG_P		;edx = 0x111, the attribute is 7 , the position is 0 
.create_pte:
	mov	[ebx+esi*4],edx
	add	edx,4096				;4k
	inc	esi
	loop	.create_pte
	

	;create the kernel page directory entry
	mov	eax,PAGE_DIR_TABLE_POS
	add	eax,0x2000				;eax is the position of second page table
	or	eax,PG_US_U | PG_RW_W | PG_P
	mov	ebx,PAGE_DIR_TABLE_POS
	mov	ecx,254					;the number of all pde between 769 ~ 1022
	mov	esi,769
.create_kernel_pde:
	mov	[ebx+esi*4],eax
	inc	esi
	add	eax,0x1000
	loop	.create_kernel_pde
	ret

;--------------------copy the segment of kernel.bin to the address of compile kernel -------------

kernel_init:
	xor	eax,eax
	xor	ebx,ebx					;ebx:record the address of program header table(e_phoff)
	xor	ecx,ecx					;cx:record the number of program header in program header table(e_phnum)
	xor	edx,edx					;dx:record the size of program header(e_phentsize)
	
	mov	dx,[KERNEL_BIN_BASE_ADDR+42]		;42 is the e_phentsize's offset
	mov	ebx,[KERNEL_BIN_BASE_ADDR+28]		;28 is the e_phoff's offset. Here is mean the first program header
	add	ebx,KERNEL_BIN_BASE_ADDR
	mov	cx,[KERNEL_BIN_BASE_ADDR+44]		;44 is the e_phnum's offset

.each_segment:
	cmp	byte	[ebx+0],PT_NULL			;compare between  p_type and PT_NULL,
	je	.PTNULL					;if == is that the program header isn't use, so jump
	
	;push the params for function memcpy

	;first param:size
	push	dword	[ebx+16]			;16 is the p_filesz's offset

	;scond param:source address 
	mov	eax,[ebx+4]				;4 is the p_offset's offset
	add 	eax,KERNEL_BIN_BASE_ADDR		;add the kernel.bin's load address.
	push 	eax					;Now the eax is the physic memory of this section,

	;third param;target address
	push	dword	[ebx+8]				;8 is the p_vaddr's offset
	
	call 	mem_cpy
	add	esp,12					;clear the three params in the stack

.PTNULL:
	add	ebx,edx					;edx is the size of program header .So here is make the ebx point to the next program header
	loop	.each_segment
	ret

;-------------------------mem_cpy(dst,src,size)---------------------------;
;			(byte by byte copy)				  ;
;-------------------------------------------------------------------------;
mem_cpy:
	;save the environment
	cld
	push	ebp
	mov	ebp,esp
	push	ecx					
	
	mov	edi,[ebp+8]				;dst
	mov	esi,[ebp+12]				;src
	mov	ecx,[ebp+16]				;size
	rep	movsb					;byte by byte copy
	
	;recovery the environment 
	pop	ecx
	pop	ebp
	ret
			
rd_disk_m_32:
	;copy the value
	mov	esi,eax
	mov	edi,ecx

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
	mov	[ebx],ax
	
	add	ebx,2
	loop	.loop_read_data
	
	;read over
	ret


