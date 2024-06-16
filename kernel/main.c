#include "print.h"
#include "thread.h"
#include "init.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "syscall-init.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_proc_a(void);
void u_proc_b(void);
int prog_a_pid = 0 , prog_b_pid = 0;

int main(void){
	put_str("kernel starting!\n");
	init_all();

	process_execute(u_proc_a , "user_proc_a");
	process_execute(u_proc_b, "user_proc_b");

	intr_enable();
	console_put_str(" main_pid:0x");
	console_put_int(sys_getpid());
	console_put_char('\n');

	
	thread_start("k_thread_a",31,k_thread_a,"argA ");
	thread_start("k_thread_b",31,k_thread_b,"argB  ");
	
	while(1);
	return 0;
}

void k_thread_a(void* arg){
	char* param = arg;
	console_put_str(" thread_a_pid:0x");
	console_put_int(sys_getpid());
	console_put_char('\n');
	console_put_str(" prog_a_pid:0x");
	console_put_int(prog_a_pid);
	console_put_char('\n');

	while(1);
}
void k_thread_b(void* arg){
	char* param = arg;
	console_put_str(" thread_b_pid:0x");
	console_put_int(sys_getpid());
	console_put_char('\n');
	console_put_str(" prog_b_pid:0x");
	console_put_int(prog_b_pid);
	console_put_char('\n');
	
	while(1);
}
void u_proc_a(void){
	prog_a_pid = getpid();
	while(1);
}

void u_proc_b(void){
	prog_b_pid = getpid();
	while(1);
}
