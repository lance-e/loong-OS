#include "print.h"
#include "thread.h"
#include "init.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"

void k_thread_a(void*);
void k_thread_b(void*);


int main(void){
	put_str("kernel starting!\n");
	init_all();
	thread_start("consumer_a",31,k_thread_a," A_");
	thread_start("consumer_b",31,k_thread_b," B_");

	intr_enable();
	while(1);
	return 0;
}

void k_thread_a(void* arg){
	while(1){
		enum intr_status old_status = intr_disable();
		if (!ioq_empty(&kbd_buf)){
			console_put_str(arg);
			char byte = ioq_getchar(&kbd_buf);
			console_put_char(byte);
		}
		intr_set_status(old_status);
	}
}
void k_thread_b(void* arg){
	while(1){
		enum intr_status old_status = intr_disable();
		if (!ioq_empty(&kbd_buf)){
			console_put_str(arg);
			char byte = ioq_getchar(&kbd_buf);
			console_put_char(byte);
		}
		intr_set_status(old_status);
	}
}
