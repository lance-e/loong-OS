#include   	"interrupt.h"
#include	"stdint.h"
#include	"global.h"
#include	"io.h"


#define PIC_M_CTRL 0x20  			//master chip contrl port
#define PIC_M_DATA 0x21				//master chip data port
#define PIC_S_CTRL 0xa0				//slave chip contrl port 
#define PIC_S_DATA 0xa1				//slave chip data port



//the number of interrupt
#define	IDT_DESC_CNT	0x21				


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
	put_str("int vector : 0x");
	put_int(vec_nr);
	put_char('\n');
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
