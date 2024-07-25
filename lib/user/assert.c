#include "assert.h"
#include "print.h"


//print the filename ,line ,function name ,condition 
void user_spin(char* filename, 		\
		int line,		\
		const char* func,	\
		const char* condition){
	put_str("\n\n\n!!!!! error !!!!!\n");
	printf("filename: %s\nline: %d\nfunction: %s\ncondition: %s\n", filename,line, func, condition);
	while(1);
}

