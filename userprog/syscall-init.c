#include "syscall-init.h"
#include "syscall.h"
#include "thread.h"
#include "print.h"

#define syscall_nr 32
typedef void* syscall;
syscall syscall_table[syscall_nr]; 			//the member of array is function pointer

// return the pid of current id
uint32_t sys_getpid(void){
	return running_thread()->pid;
}

void syscall_init(void){
	put_str("syscall_init start\n");
	syscall_table[SYS_GETPID] = sys_getpid;
	put_str("syscall_init done\n");
}

