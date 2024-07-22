#include "syscall.h"

/* no argument */
#define _syscall0(NUMBER) 								\
({											\
	int result;									\
	asm volatile ("int $0x80 " : "=a" (result) : "a" (NUMBER) : "memory"); 		\
	result;										\
})


/* one argument */
#define _syscall1(NUMBER,ARG1)								\
({											\
 	int result;									\
	asm volatile ("int $0x80"							\
	: "=a" (result) : "a"(NUMBER) , "b" (ARG1) : "memory");				\
	result;										\
})

/* two argument */
#define _syscall2(NUMBER,ARG1, ARG2) 							\
({											\
 	int result;									\
	asm volatile ("int $0x80 "							\
	: "=a" (result) : "a"(NUMBER) , "b" (ARG1) , "c" (ARG2) :"memory");		\
	result;										\
})

/* three argument */
#define _syscall3(NUMBER ,ARG1, ARG2, ARG3)						\
({											\
 	int result;									\
	asm volatile ("int $0x80 "							\
	: "=a" (result) : "a"(NUMBER) , "b" (ARG1) , "c" (ARG2) ,"d" (ARG3) :"memory");	\
	result;										\
})



uint32_t getpid(){
	return _syscall0(SYS_GETPID);
}

uint32_t write(int32_t fd , const void* buf , uint32_t count){
	return _syscall3(SYS_WRITE, fd , buf , count);
}

void* malloc(uint32_t size){
	return _syscall1(SYS_MALLOC,size);
}

void free(void* ptr){
	return _syscall1(SYS_FREE,ptr);
}

pid_t fork(){
	return _syscall0(SYS_FORK);
}

	
