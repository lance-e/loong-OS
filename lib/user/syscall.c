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
	return (void*)_syscall1(SYS_MALLOC,size);
}

void free(void* ptr){
	_syscall1(SYS_FREE,ptr);
}

pid_t fork(){
	return _syscall0(SYS_FORK);
}

int32_t read(int32_t fd , void* buf , uint32_t count){
	return _syscall3(SYS_READ , fd , buf , count);
}

void putchar(char char_ascii){
	_syscall1(SYS_PUTCHAR , char_ascii);
}

void clear(void){
	_syscall0(SYS_CLEAR);
}
	
char* getcwd(char* buf , uint32_t size){
	return (char*) _syscall2(SYS_GETCWD , buf ,size);
}

int32_t open(char* pathname , uint8_t flag){
	return _syscall2(SYS_OPEN , pathname , flag);
}

int32_t close(int32_t fd){
	return _syscall1(SYS_CLOSE , fd);
}

int32_t lseek(int32_t fd , int32_t offset , uint8_t whence){
	return _syscall3(SYS_LSEEK , fd ,offset , whence);
}

int32_t unlink(const char* pathname ){
	return _syscall1(SYS_UNLINK ,pathname);
}

int32_t mkdir(const char* pathname){
	return _syscall1(SYS_MKDIR , pathname);
}

struct dir* opendir(const char* name){
	return (struct dir*)_syscall1(SYS_OPENDIR , name);
}

int32_t closedir(struct dir* dir){
	return _syscall1(SYS_CLOSEDIR , dir);
}

int32_t rmdir(const char* pathname){
	return _syscall1(SYS_RMDIR , pathname);
}

struct dir_entry* readdir(struct dir* dir){
	return (struct dir_entry*)_syscall1(SYS_READDIR , dir);
}

void rewinddir(struct dir* dir){
	_syscall1(SYS_REWINDDIR , dir);
}

int32_t stat(const char* path , struct stat* buf){
	return _syscall2(SYS_STAT , path , buf);
}

int32_t chdir(const char* path){
	return _syscall1(SYS_CHDIR , path);
}

void ps(void){
	_syscall0(SYS_PS);
}

int32_t execv(const char* path , const char* argv[]){
	return _syscall2(SYS_EXECV , path , argv);
}

