#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
typedef void* intr_handler;
void idt_init(void);


//define the two status of interrupt
//INTR_OFF = 0 ,turn off interrupt
//INTR_ON = 1 , turn on interrupt
enum intr_status{
	INTR_OFF,
	INTR_ON
};
enum intr_status intr_get_status(void);
enum intr_status intr_set_status(enum intr_status);
enum intr_status intr_enable(void);
enum intr_status intr_disable(void);

#endif
