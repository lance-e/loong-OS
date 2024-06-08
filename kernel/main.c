#include "print.h"
#include "thread.h"
#include "init.h"

void k_thread_a(void*);

int main(void){
	put_str("kernel starting!");
	init_all();
	thread_start("k_thread_a",31,k_thread_a,"argA ");
	
	while(1);
	return 0;
}

void k_thread_a(void* arg){
	char* param= arg;
	while(1){
		put_str(param);
	}
}
