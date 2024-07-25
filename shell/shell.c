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

static char cmd_line[MAX_PATH_LEN]  = {0};		//storage the enter command 


char final_path[MAX_PATH_LEN] ;				//clear path  buffer


char cwd_cache[MAX_PATH_LEN] = {0};			//current working directory cache


void print_prompt(void){				//print prompt
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
			continue;
		}
		
		if (!strcmp("ls" , argv[0])){
			buildin_ls(argc , argv);
		}else if (!strcmp("cd" , argv[0])){
			if (buildin_cd(argc , argv) != NULL){
				memset(cwd_cache , 0 , MAX_PATH_LEN);
				strcpy(cwd_cache , final_path);
			}
		}else if (!strcmp("pwd" , argv[0])){
			buildin_pwd(argc , argv);
		}else if (!strcmp("ps" , argv[0])){
			buildin_ps(argc , argv);
		}else if (!strcmp("clear" , argv[0])){
			buildin_clear(argc , argv);
		}else if (!strcmp("mkdir" , argv[0])){
			buildin_mkdir(argc , argv);
		}else if (!strcmp("rmdir" ,argv[0])){
			buildin_rmdir(argc , argv);
		}else if (!strcmp("rm" , argv[0])){
			buildin_rm(argc , argv);
		}else {
			printf("external command\n");
		}
	}
	PANIC("my_shell: should not be here");
}


