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

	struct dir* p_dir = sys_opendir("/dir1/subdir1");
	if (p_dir){
		printf("/dir1/subdir1 open done\n content:\n");
		char* type = NULL;
		struct dir_entry* dir_e = NULL;
		while ((dir_e = sys_readdir(p_dir))){
			if (dir_e->f_type == FT_REGULAR){
				type = "regular";
			}else {
				type = "directory";
			}
			printf("   %s    %s\n" , type , dir_e->filename);
		}
		if (sys_closedir(p_dir) == 0 ){
			printf("dir close done\n");
		}else {
			printf("dir close failed\n");
		}

	}else {
		printf("/dir1/subdir1 open filed\n");
	}
	
	while(1);
	return 0;
}

