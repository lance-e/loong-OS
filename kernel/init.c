#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"

//initial all module
void init_all(){
	put_str("init_all\n");
	idt_init();
	mem_init();
	thread_init();
	timer_init();
}
