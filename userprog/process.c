#include "process.h"
#include "thread.h"
#include "global.h"
#include "print.h"
#include "list.h"
#include "debug.h"
#include "string.h"
#include "interrupt.h"
#include "tss.h"

#define PG_SIZE 4096

// will use to return 0 privilege level
extern void intr_exit(void);
extern struct list thread_ready_list;
extern struct list thread_all_list;

//create the context of process , will invoke in 'kernel_thread'
void start_process(void* filename_) {
	void* function = filename_;
	struct task_struct* cur = running_thread();
	cur->self_kstack += sizeof(struct thread_stack);			//jump thread_stack,now self_kstack point to the top of intr_stack(lowest)
	struct intr_stack* proc_stack = (struct intr_stack*)(cur->self_kstack);
	proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0 ;
	proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
	proc_stack->gs = 0 ;						// user mode don't use
	proc_stack->fs = proc_stack->es = proc_stack->ds = SELECTOR_U_DATA;
	proc_stack->eip = function;					// address of target process
	proc_stack->cs = SELECTOR_U_CODE;
	proc_stack->eflags = ( EFLAGS_IOPL_0 | EFLAGS_IF_1  |  EFLAGS_MBS );	
	proc_stack->esp = (void*) ((uint32_t)get_a_page(PF_USER,USER_STACK3_VADDR )+PG_SIZE);
	proc_stack->ss = SELECTOR_U_DATA;
	asm volatile ("movl %0 , %%esp ; jmp intr_exit" : : "g" (proc_stack) : "memory" );
}

//activate page table
void page_dir_activate(struct task_struct* p_thread){
	uint32_t pagedir_phy_addr = 0x100000;
	if (p_thread->pgdir != NULL ){
		pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
	}
	//update cr3
	asm volatile(" movl %0 , %%cr3" : : "r" (pagedir_phy_addr) : "memory");
}

//activate process :
// 1. activate page table;
// 2. update esp0 of tss to user process's 0 level stack
void process_activate(struct task_struct* p_thread){
	ASSERT(p_thread != NULL);
	
	//first step;
	page_dir_activate(p_thread);

	//second step:
	if (p_thread->pgdir){
		update_tss_esp(p_thread);
	}
}


//create page directory table
uint32_t* create_page_dir(void){

	//user can't get the user program 's page directory table , so create at kernel 
	uint32_t* page_dir_vaddr = get_kernel_pages(1);
	
	if (page_dir_vaddr == NULL){
		put_str("create_page_dir : get_kernel_pages failed \n");
		return NULL;
	}
	
	/********************** all process share kernel **************************************
	 *
	 *	make all process share kernel is to copy the page directory table of
	 *	kernel to every process
	 *
	 *	0x300 * 4 : 0x300 is the 768th pde(begin of kernel pde) , 4 is 4 byte per pde
	 *	0xfffff000 : the last pde ,is the address of page directory table
	 *	1024 : 1 GB of kernel is 256 th pde, so all is 256 * 4 = 1024 */

	memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300 * 4),(uint32_t*)(0xfffff000 + 0x300*4),1024 );

	/****************************************************************************************/

	/****************************  update address of page directory ***********************
	 *
	 * 	the last pde is the physic address of page directory table
	 * 	so update the last pde to the new physic address       */

	uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)(page_dir_vaddr)); 
	page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1 ;

	/*************************************************************************************/

	return page_dir_vaddr;
}

//chreate user process's virtual address bitmap
void create_user_vaddr_bitmap(struct task_struct* user_prog){
	//0x804800
	user_prog->userprog_vaddr.vaddr_start  = USER_VADDR_START ;
	
	// the length of bitmap
	uint32_t bitmap_bytes_len =(uint32_t)((0xc0000000 - USER_VADDR_START ) / PG_SIZE / 8);
	//get the number of page 
	uint32_t pg_cnt = DIV_ROUND_UP(bitmap_bytes_len, PG_SIZE);
	
	//apply for page, return the start address  of page 
	user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(pg_cnt);
	user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = bitmap_bytes_len;

	bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}


// create user process
void process_execute(void* filename , char* name) {
	// get a pcb
	struct task_struct* thread = get_kernel_pages(1);
	init_thread(thread,name,default_prio);
	create_user_vaddr_bitmap(thread);
	thread_create(thread, start_process,filename);
	thread->pgdir = create_page_dir();

	enum intr_status old_status = intr_disable();

	ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
	list_append(&thread_ready_list,&thread->general_tag);
	ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
	list_append(&thread_all_list,&thread->all_list_tag);

	intr_set_status(old_status);
}
	
