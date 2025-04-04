#include "print.h"
#include "thread.h"
#include "init.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "process.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_proc_a(void);
void u_proc_b(void);
int test_var_a = 0 , test_var_b = 0;

int main(void){
	put_str("kernel starting!\n");
	init_all();
	thread_start("k_thread_a",31,k_thread_a,"argA ");
	thread_start("k_thread_b",31,k_thread_b,"argB  ");
	process_execute(u_proc_a , "user_proc_a");
	process_execute(u_proc_b, "user_proc_b");

	intr_enable();
	while(1);
	return 0;
}

void k_thread_a(void* arg){
	while(1){
		console_put_str("v_a:0x");
		console_put_int(test_var_a);
	}
}
void k_thread_b(void* arg){
	while(1){
		console_put_str("v_b:0x");
		console_put_int(test_var_b);
	}
}
void u_proc_a(void){
	while(1){
		test_var_a++;
	}
}

void u_proc_b(void){
	while(1){
		test_var_b++;
	}
}
