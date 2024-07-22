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
#include "shell.h"
#include "debug.h"

int main(void){
	put_str("kernel starting!\n");
	init_all();
	intr_enable();
	cls_screen();
	console_put_str("[lance@localhost /]$ ");
	while(1);
	return 0;
}

void init(void){
	uint32_t ret_pid = fork();
	if (ret_pid){
		while(1);
	}else{
		my_shell();
	}
	PANIC("init: should not be here");
}
