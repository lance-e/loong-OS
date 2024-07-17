#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "stdint.h"
#include "list.h"
#include "memory.h"


#define MAX_FILES_OPEN_PER_PROC 8			// the max number of open file in per process

typedef int16_t pid_t;
typedef void thread_func(void*);


//status of process or thread
enum task_status{
	TASK_RUNNING,
	TASK_READY,
	TASK_BLOCKED,
	TASK_WAITING,
	TASK_HANGING,
	TASK_DIED
};


/***************************	intr_stack	************************
 * used to save the context of process or thread when interrrupt happen
 * this stack is at the top of page
 * ********************************************************************/
struct intr_stack{
	uint32_t vec_no;				//kernel.S: VECTOR has push %1,the number of interrupt
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t esp_dummy;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;

	//there will push when cpu enter high privilege level from low privilege level
	uint32_t error_code;
	void (*eip) (void);
	uint32_t cs;
	uint32_t eflags;
	void* esp;
	uint32_t ss;
};

/*****************************	thread_stack	********************************
 * stack of thread ,used to save the function which will execute in this thread
 * just for used in switch_to to save the environment of thread.
 * ****************************************************************************/
struct thread_stack{
	uint32_t ebp;
	uint32_t ebx;
	uint32_t edi;
	uint32_t esi;

	//when frist excute, eip point to the target function
	//other time , eip point to the return address of switch_to 
	void (*eip) (thread_func* func , void* func_arg);
	
	//there are used for first dispatched to cpu
	void (*unused_retaddr);			//unused_retaddr just for occupy a position  in stack
	thread_func* function;			//function name
	void* func_arg;				//function arguments
};


//the PCB 
struct task_struct {
	uint32_t* self_kstack;
	pid_t pid;
	enum task_status status;
	char name[16];
	uint8_t priority;
	uint8_t ticks;				//the time ticks of run in cpu

	uint32_t elapsed_ticks;			//record the all ticks after run in cpu
						
	int32_t fd_table[MAX_FILES_OPEN_PER_PROC]; //file descriptor array

	struct list_elem general_tag;		//the node of general list

	struct list_elem all_list_tag;		//the node of thread list
	
	uint32_t* pgdir;			//the virtual address of process's page table,!:thread:NULL(thread don't have page table)

	struct virtual_addr userprog_vaddr;	// virtual address pool of user process 
						
	struct mem_block_desc u_block_desc[DESC_CNT]; //memory block descriptor of user process

	uint32_t stack_magic;			//edge of stack
};


struct task_struct* running_thread(void);
void thread_create(struct task_struct* pthread,thread_func function , void* func_arg);
void init_thread(struct task_struct* pthread,char* name,int prio);
struct task_struct* thread_start(char* name, int prio, thread_func function , void* func_arg);
void thread_init(void);
void schedule(void);
void thread_yield(void);
void thread_block(enum task_status stat);
void thread_unblock(struct task_struct* pthread);

#endif
