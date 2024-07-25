#include "shell.h"
#include "stdio.h"
#include "debug.h"
#include "syscall.h"
#include "string.h"
#include "file.h"
#include "fs.h"
#include "buildin_cmd.h"



#define cmd_len 128
#define MAX_ARG_NR 16

char* argv [MAX_ARG_NR];
int32_t argc = -1;

//storage the enter command 
static char cmd_line[MAX_PATH_LEN]  = {0};
//
char final_path[MAX_PATH_LEN]  = {0};

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
			case 'l' - 'a':		//clear screen
				*pos = 0 ;
				clear();
				print_prompt();
				printf("%s" ,buf);
				break;
			case 'u' - 'a':
				while(buf != pos){
					putchar('\b');
					*(pos--) = 0 ;
				}
				break;

			default:
				putchar(*pos);
				pos++;
		}	
	}
	printf("readline: can't find enter_key in the cmd_line,max num of char is 128\n");
}


//parse command "cmd_str" take "token" as a interval , and storage into "argv"
static int32_t cmd_parse(char* cmd_str , char** argv ,char token){
	ASSERT(cmd_str != NULL);	
	int32_t arg_idx = 0 ;
	while(arg_idx < MAX_ARG_NR){
		argv[arg_idx] = NULL;
		arg_idx ++;
	}
	char* next = cmd_str;
	int32_t argc = 0;
	//handle whole command line
	while(*next){
		//remove the space
		while(*next == token){
			next++;
		}
		//handle the last argument is space
		if (*next == 0 ){
			break;
		}
		argv[argc] = next;

		//handle the command and parameter
		while(*next && *next != token){
			next++;
		}
		//make token into 0
		if (*next){
			*next++ = 0;
		}
		if (argc > MAX_ARG_NR){
			return -1;
		}
		argc++;
	}
	return argc;
}


//shell
void my_shell(void){
	cwd_cache[0] = '/';
	cwd_cache[1] = 0;
	while(1){
		print_prompt();
		memset(final_path, 0 , MAX_PATH_LEN);
		memset(cmd_line , 0 , MAX_PATH_LEN);
		readline(cmd_line , MAX_PATH_LEN);
		if (cmd_line[0]  == 0){
			continue;
		}
		argc = -1;
		argc = cmd_parse(cmd_line , argv , ' ');
		if (argc == -1){
			printf("num of arguments exceed %d\n" ,MAX_ARG_NR);
		}
		int32_t arg_idx = 0 ;
		char buf[MAX_PATH_LEN] = {0};
		while(arg_idx < argc){
			make_clear_abs_path(argv[arg_idx] , buf);
			printf("%s -> %s\n" , argv[arg_idx] , buf);
			arg_idx++;
		}
		printf("\n");
	}
	PANIC("my_shell: should not be here");
}


