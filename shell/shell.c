#include "shell.h"
#include "stdio.h"
#include "debug.h"
#include "syscall.h"
#include "string.h"
#include "file.h"



#define cmd_len 128
#define MAX_ARG_NR 16


//storage the enter command 
static char cmd_line[cmd_len]  = {0};

//current working directory cache
char cwd_cache[64] = {0};

//print prompt
void print_prompt(void){
	printf("[lance@localhost %s]$ ",cwd_cache);
}

//read count byte from keyboard buffer into "buf"
static  void readline(char* buf , int32_t count){
	ASSERT(buf != NULL && count > 0 );
	char* pos = buf;
	while (read(stdin_no , pos , 1) != -1 && (pos - buf )< count ){
		switch(*pos){
			case '\n':
			case '\r':
				*pos = 0;
				putchar('\n');
				return ;
			case '\b':
				if (buf[0] != '\b'){
					--pos; 
					putchar('\b');
				}
				break;
			default:
				putchar(*pos);
				pos++;
		}	
	}
	printf("readline: can't find enter_key in the cmd_line,max num of char is 128\n");
}


//shell
void my_shell(void){
	cwd_cache[0] = '/';
	while(1){
		print_prompt();
		memset(cmd_line , 0 , cmd_len);
		readline(cmd_line , cmd_len);
		if (cmd_line[0]  == 0){
			continue;
		}
	}
	PANIC("my_shell: should not be here");
}
