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



	