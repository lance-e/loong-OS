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

	struct stat obj_stat;
	sys_stat("/" , &obj_stat);
	printf("/'s info\n  i_no:%d\n  size:%d\n  filetype:%s\n",obj_stat.st_ino,obj_stat.st_size,obj_stat.st_filetype == 2 ? "directory" : "file");
	sys_stat("/dir1" , &obj_stat);
	printf("/dir1's info\n  i_no:%d\n  size:%d\n  filetype:%s\n",obj_stat.st_ino,obj_stat.st_size,obj_stat.st_filetype == 2 ? "directory" : "file");
	
	while(1);
	return 0;
}

