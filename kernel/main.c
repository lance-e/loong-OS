#include "print.h"
#include "thread.h"
#include "init.h"
#include "string.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "syscall-init.h"
#include "stdio.h"
#include "memory.h"
#include "fs.h"
#include "dir.h"

int main(void){
	put_str("kernel starting!\n");
	init_all();
	intr_enable();


	while(1);
	return 0;
}

void init(void){
	uint32_t ret_pid = fork();
	if (ret_pid){
		printf("I am father , my pid :%d , child pid :%d\n" , getpid() ,ret_pid);
	}else{
		printf("I am child , my pid :%d , ret pid :%d \n" , getpid(), ret_pid);
	}
	while(1);
}
