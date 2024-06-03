#include "print.h"
#include "init.h"
int main(void){
	put_str("Kernel Starting!\n");
	init_all();
	asm volatile ("sti");
	while(1);
	return 0;
}
