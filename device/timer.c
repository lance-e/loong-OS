#include "timer.h"
#include "io.h"
#include "print.h"

#define IRQ0_FREQUENCY 100
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE INPUT_FREQUENCY / IRQ0_FREQUENCY				//the initial value of counter 0 
#define COUNTER0_PORT 0x40
#define COUNTER0_NO 0 								//No.1 counter 
#define COUNTER0_MODE 2							
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PORT 0x43

//write the control word into the control word register and set the initial value 
static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t rwl, uint8_t counter_mode, uint16_t counter_value){
	//write into control word register
	outb(PIT_CONTROL_PORT,(uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1 ));
	//write the low bit of counter_value
	outb(counter_port,(uint8_t)(counter_value));
	//writhe the hight bit of counter_value
	outb(counter_port,(uint8_t)(counter_value >> 8));
}

//initial PIT8253
void timer_init(){
	put_str("timer_init start \n");
	//set the frequency of counter
	frequency_set(COUNTER0_PORT,COUNTER0_NO,READ_WRITE_LATCH, COUNTER0_MODE, COUNTER0_VALUE);
	put_str("timer_init done \n");
}
		       
			
