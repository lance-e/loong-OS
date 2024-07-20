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

	printf("/dir1 content before delete /dir1/subdir1:\n");
	struct dir* dir = sys_opendir("/dir1/");
	char* type = NULL;
	struct dir_entry* dir_e = NULL;
	while ((dir_e = sys_readdir(dir))){
		if (dir_e->f_type == FT_REGULAR){
			type = "regular";
		}else {
			type = "directory";
		}
		printf("   %s    %s\n" , type , dir_e->filename);
	}

	printf("try to delete nonempty directory /dir1/subdir1 \n");
	if (sys_rmdir("/dir1/subdir1") == -1){
		printf("sys_rmdir: /dir1/subdir1 failed\n");
	}
	if (sys_unlink("/dir1/subdir1/file2") == -1){
		printf("sys_unlink: /dir1/subdir1 failed\n");
	}
	printf("try to delete nonempty directory /dir1/subdir1 again \n");
	if (sys_rmdir("/dir1/subdir1") == -1){
		printf("sys_rmdir: /dir1/subdir1 failed\n");
	}


	printf("/dir1 content after delete /dir1/subdir1:\n");
	sys_rewinddir(dir);
	while ((dir_e = sys_readdir(dir))){
		if (dir_e->f_type == FT_REGULAR){
			type = "regular";
		}else {
			type = "directory";
		}
		printf("   %s    %s\n" , type , dir_e->filename);
	}
	while(1);
	return 0;
}

