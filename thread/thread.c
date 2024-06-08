#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "interrupt.h"
#include "debug.h"

#define PG_SIZE 4096

struct task_struct* main_thread;			//main thread PCB
struct list thread_ready_list;				//ready queue
struct list thread_all_list;				//all thread queue node
static struct list_elem* thread_tag;			//save the thread node in queue



extern void switch_to(struct task_struct* cur,struct task_struct* next);

//get the current thread's PCB pointer
struct task_struct* running_thread(){
	uint32_t esp;
	asm ("mov %%esp,%0":"=g"(esp));
	return (struct task_struct*)(esp & 0xfffff000);
}





//kernel_thread is to execute function(func_arg)
static void kernel_thread(thread_func* function ,void* func_arg){
	intr_enable();
	function(func_arg);
}


//thread_create :
//initial thread stack ,set the target function which to be executed  and some arguments
void thread_create(struct task_struct* pthread,thread_func function,void* func_arg){
	//reserve the space of intr_stack in kernel stack
	pthread->self_kstack -= sizeof(struct intr_stack);

	//reserve the space of thread_stack in kernel stack
	pthread->self_kstack -= sizeof(struct thread_stack);

	struct thread_stack* kthread_stack = (struct thread_stack*) pthread->self_kstack;
	kthread_stack->eip = kernel_thread;
	kthread_stack->function = function;
	kthread_stack->func_arg = func_arg;
	kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0 ;
}

//initial the basic information of thread
void init_thread(struct task_struct* pthread,char* name,int prio){
	memset(pthread,0,sizeof(*pthread));
	strcpy(pthread->name ,name);
	if (pthread == main_thread){
		pthread->status = TASK_RUNNING;
	}else{
		pthread->status = TASK_READY;
	}
	pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
	pthread->priority = prio;
	pthread->ticks = 0;
	pthread->pgdir= NULL;
	pthread->stack_magic = 0x19870616;
}

//create a thread: priority is 'prio',name is 'name',target function is 'function(func_arg)'
struct task_struct* thread_start(char* name,int prio,thread_func function , void* func_arg){
	struct task_struct* thread = get_kernel_pages(1);
	init_thread(thread,name,prio);
	thread_create(thread,function,func_arg);
	//asm volatile("movl %0 ,%%esp;\
			pop %%ebp; pop %%ebx;pop %%edi; pop %%esi; \
			ret" : : "g" (thread->self_kstack) : "memory");
	//make sure is not in thread_ready_list 
	ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
	//append into ready queue
	list_append(&thread_ready_list,&thread->general_tag);

	//make sue is not in thread_all_list
	ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
	//append into thread all list
	list_append(&thread_all_list,&thread->all_list_tag);

	return thread;
}


//initial main thread:
//don't need apply for address of PCB,because in loader.S had initial the esp = 0xc009f000
//so the main thread's PCB is at 0xc009e000
static void make_main_thread(void){
	main_thread = running_thread();
	init_thread(main_thread,"main",31);

	ASSERT(!elem_find(&thread_all_list,&main_thread->all_list_tag));
	list_append(&thread_all_list,&main_thread->all_list_tag);
}
