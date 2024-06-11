#include   	"interrupt.h"
#include	"stdint.h"
#include	"global.h"
#include	"io.h"
#include 	"print.h"


#define PIC_M_CTRL 0x20  			//master chip contrl port
#define PIC_M_DATA 0x21				//master chip data port
#define PIC_S_CTRL 0xa0				//slave chip contrl port 
#define PIC_S_DATA 0xa1				//slave chip data port


//the number of interrupt
#define	IDT_DESC_CNT	0x21				

#define EFLAGS_IF 0x00000200
#define GET_EFLAGS(EFLAGS_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAGS_VAR))



// struct of interrupt gate describtor
struct 	gate_desc{
	uint16_t	func_offset_low_word;
	uint16_t	selector;
	uint8_t		dcount;
	uint8_t		attribute;
	uint16_t	func_offset_high_word;
};

static	void make_idt_desc(struct gate_desc* p_gdesc,uint8_t attr,intr_handler function);
//interrupt descriptor table
static	struct 	gate_desc	idt[IDT_DESC_CNT];

//save the name 
char* intr_name[IDT_DESC_CNT];

//save the interrupt function
intr_handler idt_table[IDT_DESC_CNT];

extern intr_handler intr_entry_table[IDT_DESC_CNT];

static void pic_init(void){
	//initial main chip
	outb(PIC_M_CTRL , 0x11);			//ICW1
	outb(PIC_M_DATA , 0x20);			//ICW2
	outb(PIC_M_DATA , 0x04);			//ICW3
	outb(PIC_M_DATA , 0x01);			//ICW4

	//initial slave chip
	outb(PIC_S_CTRL , 0x11);			//ICW1
	outb(PIC_S_DATA , 0x28);			//ICW2
	outb(PIC_S_DATA , 0x02);			//ICW3
	outb(PIC_S_DATA , 0x01);			//ICW4

	//open the IR0 in the main chip , mean that just accept the  clock interrupt
	outb(PIC_M_DATA , 0xfe);
	outb(PIC_S_DATA , 0xff);

	put_str(" pic_init done\n");
}


static	void 	make_idt_desc(struct gate_desc* p_gdesc,uint8_t	attr,intr_handler function){
	p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
	p_gdesc->selector = SELECTOR_K_CODE;
	p_gdesc->dcount = 0 ;
	p_gdesc->attribute = attr;
	p_gdesc->func_offset_high_word=((uint32_t)function & 0xFFFF0000)>> 16;
}

static	void	idt_desc_init(void){
	int 	i;
	for (i = 0 ; i < IDT_DESC_CNT;i++){
		make_idt_desc(&idt[i],IDT_DESC_ATTR_DPL0,intr_entry_table[i]);
	}
	put_str(" idt_desc_init	done\n");
}

// general  interrupt function 
static void general_intr_handler(uint8_t vec_nr){
	//IRQ7 and IRQ15 will make spurious interrupt ,don't need handle
	//0x2f is the slave chip 's last IRQ ,is reserved items
	if (vec_nr == 0x27 || vec_nr == 0x2f){
		return ;
	}
	
	//print a block of blank
	set_cursor(0);

	int cursor_pos =  0 ;
	while (cursor_pos++ < 320){
		put_char(' ');
	}

	set_cursor(0);
	put_str("!!!!!!!!          excetion message begin         !!!!!!!!\n");
	set_cursor(88);
	put_str(intr_name[vec_nr]);
	if (vec_nr == 14){
		// PageFault
		int page_fault_vaddr = 0 ;
		asm ("movl %%cr2 , %0" : "=r" (page_fault_vaddr));
		put_str("\npage fault address is ");put_int(page_fault_vaddr);
	}
	put_str("\n!!!!!!!!         exccetion message end            !!!!!!!\n");
	while(1);
}	


static void exception_init(void){
	int i ;
	for (i =  0 ; i < IDT_DESC_CNT; i++){
		idt_table[i] = general_intr_handler; 			//here set the interrupt function is the default general_intr_handler
		intr_name[i] = "unknown";
	}
	//initial exception name
	intr_name[0] = "#DE Divide Error";
	intr_name[1] = "#DB Debug Exception";
	intr_name[2] = "NMI Interrupt";
	intr_name[3] = "#BP Breakpoint Exception";
	intr_name[4] = "#OF overflow Exception";
	intr_name[5] = "#BR BOUND Range Exceeded Exception";
	intr_name[6] = "#UD Invalid Opcode Exception";
	intr_name[7] = "#NM Device Not Availible Exception";
	intr_name[8] = "#DF Double Fault Exception";
	intr_name[9] = "Coprocessor Segment Overrun";
	intr_name[10] = "#TS Invalid TSS Exception ";
	intr_name[11] = "#NP Segment Not Present";
	intr_name[12] = "#SS Stack Fault Exception";
	intr_name[13] = "#GP General Protection Exception";
	intr_name[14] = "#PF Page-Fault Exception";
	//intr_name[15] ; 15 is the reserved items,not used
	intr_name[16] = "#MF x87 FPU Floationg-Point Error";
	intr_name[17] = "#AC Alignment Check Exception";
	intr_name[18] = "#MC Machine-Check Exception";
	intr_name[19] = "#XF SIMD Floating-Point Exception";
}

//register interrupt function in the 'vector_no' of interrupt function array
void register_handler(uint8_t vector_no,intr_handler function){
	idt_table[vector_no]  = function;
}

void	idt_init(){
	put_str("idt_init start\n");
	idt_desc_init();		//initial interrupt descriptor table	
	exception_init();
	pic_init();			//initial 8259A

	
	//load idt
	uint64_t idt_operand = ((sizeof(idt)-1) | ((uint64_t)(uint32_t)idt << 16 ));
	asm volatile("lidt %0": : "m"(idt_operand));
	put_str("idt_init done\n");
}

//enable interrupt and return the status before open
enum intr_status intr_enable(){
	if (INTR_ON == intr_get_status()){
		return INTR_ON;
	}else {
		asm volatile ("sti");
		return INTR_OFF;
	}
}

//disable interrupt and return the status before close
enum intr_status intr_disable(){
	if (INTR_ON == intr_get_status()){
		asm volatile ("cli" : : : "memory");
		return INTR_ON;
	}else {
		return INTR_OFF;
	}
}

//set the status
enum intr_status intr_set_status(enum intr_status status){
	return status & INTR_ON ? intr_enable() : intr_disable();
}

//get the status now
enum intr_status intr_get_status(){
	uint32_t eflags = 0 ;
	GET_EFLAGS(eflags);
	return (EFLAGS_IF & eflags ) ? INTR_ON : INTR_OFF;
}
