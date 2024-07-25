#include "buildin_cmd.h"
#include "fs.h"
#include "string.h"
#include "dir.h"
#include "debug.h"
#include "syscall.h"


static void wash_path(char* old_abs_path , char* new_abs_path){
	ASSERT(old_abs_path[0] =='/');
	char name[MAX_FILE_NAME_LEN] = {0};
	char* sub_path = old_abs_path;
	sub_path = path_parse(old_abs_path , name);
	if (name[0] == 0){
		new_abs_path[0] = '/';
		new_abs_path[1] = 0;
		return;
	}
	new_abs_path[0] = 0;
	strcat(new_abs_path , "/");
	while(name[0] ){
		if (!strcmp(".." , name)){
			char* slash_ptr = strrchr(new_abs_path , '/');
			if (slash_ptr != new_abs_path){
				*slash_ptr = 0;
			}else {
				*(slash_ptr+1) = 0;
			}
		}else if (strcmp("." , name)){
			if (strcmp(new_abs_path , "/")){
				strcat(new_abs_path , "/");
			}
			strcat(new_abs_path , name);
		}

		memset(name , 0 , MAX_FILE_NAME_LEN);
		if (sub_path){
			sub_path = path_parse(sub_path , name);
		}

	}
}


void make_clear_abs_path(char* path , char* final_path){
	char abs_path[MAX_PATH_LEN] = {0};
	if (path[0] != '/'){
		memset(abs_path , 0 , MAX_PATH_LEN);
		if (getcwd(abs_path , MAX_PATH_LEN) != NULL){
			if (!((abs_path[0] == '/' ) && (abs_path[1] == 0))){
				strcat(abs_path , "/");
			}
		}
	}
	strcat(abs_path , path);
	wash_path(abs_path , final_path);
}
