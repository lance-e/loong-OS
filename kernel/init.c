#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"

//initial all module
void init_all(){
	put_str("init_all\n");
	idt_init();
	mem_init();
	thread_init();
	timer_init();
	console_init();
	keyboard_init();
	tss_init();
}
