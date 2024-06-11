#include "print.h"
#include "thread.h"
#include "init.h"
#include "interrupt.h"

void k_thread_a(void*);
void k_thread_b(void*);


int main(void){
	put_str("kernel starting!\n");
	init_all();
	thread_start("k_thread_a",31,k_thread_a,"argA ");
	thread_start("k_thread_b",8,k_thread_b,"argB ");

	intr_enable();
	while(1){
		intr_disable();
		put_str("Main ");
		intr_enable();
	};
	return 0;
}

void k_thread_a(void* arg){
	char* param= arg;
	while(1){
		intr_disable();
		put_str(param);
		intr_enable();
	}
}
void k_thread_b(void* arg){
	char* param = arg;
	while(1){
		intr_disable();
		put_str(param);
		intr_enable();
	}
}
