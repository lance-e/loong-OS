#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"
#include "list.h"

#define PG_SIZE 4096

struct task_struct* main_thread;			//main thread PCB
struct list thread_ready_list;				//ready queue
struct list thread_all_list;				//all thread queue node
//static struct list_elem* thread_tag;			//save the thread node in queue



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
	pthread->ticks = prio;
	pthread->elapsed_ticks= 0;
	pthread->pgdir= NULL;
	pthread->stack_magic = 0x88888888;			//magic number
}

//create a thread: priority is 'prio',name is 'name',target function is 'function(func_arg)'
struct task_struct* thread_start(char* name,int prio,thread_func function , void* func_arg){
	struct task_struct* thread = get_kernel_pages(1);
	init_thread(thread,name,prio);
	thread_create(thread,function,func_arg);
	

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

//task scheduling
void schedule(){

	ASSERT(intr_get_status() == INTR_OFF);

	struct task_struct* cur = running_thread();
	if (cur->status == TASK_RUNNING){
		ASSERT(!elem_find(&thread_ready_list,&cur->general_tag));
		list_append(&thread_ready_list,&cur->general_tag);
		cur->ticks=cur->priority;
		cur->status = TASK_READY;
	}else{
		//maybe block
		//don't need append to list,because current thread isn't at ready list
	}
	

	ASSERT(!list_empty(&thread_ready_list));

	struct list_elem *thread_tag;
	thread_tag = NULL;
	
	//begin to schedule next thread
	thread_tag = list_pop(&thread_ready_list);
	
	//get the PCB, like the 'running_thread'
	struct task_struct* next = (struct task_struct*)((uint32_t)thread_tag & 0xfffff000);

	next->status = TASK_RUNNING;
	switch_to(cur,next);

}


//block thread
void thread_block(enum task_status stat){
	ASSERT(((stat == TASK_BLOCKED )||(stat ==TASK_WAITING) || (stat == TASK_HANGING)));
	enum intr_status old_status  = intr_disable();
	struct task_struct* cur_thread = running_thread();
	cur_thread->status = stat;
	schedule();
	intr_set_status(old_status);
}

//unblock thread
void thread_unblock(struct task_struct* pthread){
	enum intr_status old_status = intr_disable();
	ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING )||(pthread->status == TASK_HANGING)));
	if (pthread->status != TASK_READY){
		ASSERT(!elem_find(&thread_ready_list,&pthread->general_tag));
		if (elem_find(&thread_ready_list,&pthread->general_tag)){
			PANIC("thread_unblock: blocked thread in ready_list\n");
		}
		list_push(&thread_ready_list,&pthread->general_tag);
		pthread->status = TASK_READY;
	}
	intr_set_status(old_status);
}


//initial thread environment
void thread_init(void){
	put_str("thread_init start\n");
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
	make_main_thread();
	put_str("thread_init done\n");
}