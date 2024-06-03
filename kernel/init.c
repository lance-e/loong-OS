#include "init.h"
#include "print.h"
#include "interrupt.h"

//initial all module
void init_all(){
	put_str("init_all\n");
	idt_init();
}
