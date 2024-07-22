#include "fork.h"
#include "memory.h"
#include "string.h"
#include "global.h"
#include "process.h"
#include "bitmap.h"
#include "list.h"
#include "interrupt.h"
#include "debug.h"
#include "file.h"

#define PG_SIZE 4096

extern void intr_exit(void);
extern struct file file_table[MAX_FILE_OPEN];
extern struct list thread_ready_list;
extern struct list thread_all_list;


//copy PCB of parent process to child process
static int32_t copy_pcb_vaddrbitmap_stack0(struct task_struct* child_thread , struct task_struct* parent_thread){
	//1. copy whole page
	memcpy(child_thread , parent_thread , PG_SIZE);	
	//2. fix some place
	child_thread->pid = fork_pid();
	child_thread->elapsed_ticks = 0 ;
	child_thread->status = TASK_READY;
	child_thread->ticks = child_thread->priority;
	child_thread->parent_pid = parent_thread->pid;
	child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
	child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;
	block_desc_init(child_thread->u_block_desc);
	//3. copy parent precess's virtual memory bitmap 
	uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8 , PG_SIZE);
	void* vaddr_btmp = get_kernel_pages(bitmap_pg_cnt);
	memcpy(vaddr_btmp , child_thread->userprog_vaddr.vaddr_bitmap.bits , bitmap_pg_cnt * PG_SIZE);
	child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_btmp;

	//for debug
	ASSERT(strlen(child_thread->name) < 11);
	strcat(child_thread->name , "_fork");

	return 0 ;
}


//copy process body and user stack of parent proces to child process
static void copy_body_stack3(struct task_struct* child_thread , struct task_struct* parent_thread, void* buf_page){
	uint8_t* vaddr_btmp = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
	uint32_t btmp_bytes_len = parent_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len;
	uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;
	uint32_t idx_byte =  0;
	uint32_t idx_bit = 0 ;
	uint32_t prog_vaddr = 0 ;


	//search for pages with existing data from parent process
	while(idx_byte < btmp_bytes_len){
		if (vaddr_btmp[idx_byte]){
			idx_bit = 0 ;
			while(idx_bit < 8){
				if ((BITMAP_MASK << idx_bit) & vaddr_btmp[idx_byte]){
					prog_vaddr = (idx_byte * 8 + idx_bit ) * PG_SIZE + vaddr_start;
					//begin copy data:
					//1. copy to kernel buffer
					memcpy(buf_page , (void*)prog_vaddr , PG_SIZE); 
					//2. switch to child_process's page table
					page_dir_activate(child_thread);
					//3. apply virtual memory in child_process's memory space
					get_a_page_without_opvaddrbitmap(PF_USER , prog_vaddr);
					//4. copy data to child_process
					memcpy((void*)prog_vaddr , buf_page , PG_SIZE);
					//5. switch to parent_process's page table
					page_dir_activate(parent_thread);
				}
				idx_bit++;
			}

		}
		idx_byte++;
	}

}


//create thread_stack for child_process
static int32_t build_child_stack(struct task_struct* child_thread){
	//1. make return_pid to 0
	struct intr_stack* intr_0_stack = (struct intr_stack*)(((uint32_t)child_thread + PG_SIZE) - sizeof(struct intr_stack));
	intr_0_stack->eax =  0;

	//2. build a struct thread_stack for switch to
	uint32_t* ret_addr_in_thread_stack = (uint32_t*)intr_0_stack -1;
	//these are unnecessary ,just for easy look
	uint32_t*  esi_ptr_in_thread_stack = (uint32_t*)intr_0_stack -2;
	uint32_t*  edi_ptr_in_thread_stack = (uint32_t*)intr_0_stack -3;
	uint32_t*  ebx_ptr_in_thread_stack = (uint32_t*)intr_0_stack -4;
	//ebp is top of kernel stack at that time
	uint32_t*  ebp_ptr_in_thread_stack = (uint32_t*)intr_0_stack -5;

	//update to the new return address for switch_to
	*ret_addr_in_thread_stack = (uint32_t)intr_exit;
	
	//unnecessary
	*ebp_ptr_in_thread_stack = *ebx_ptr_in_thread_stack = *edi_ptr_in_thread_stack = *esi_ptr_in_thread_stack = 0; 

	//make parent process's kernel stack as the switch to stack of child process
	child_thread->self_kstack = ebp_ptr_in_thread_stack;


	return 0;
}


//udpate inode open number
static void update_inode_open_cnts(struct task_struct* thread){
	int32_t local_fd = 3 ,global_fd = 0 ;
	while(local_fd < MAX_FILES_OPEN_PER_PROC){
		global_fd = thread->fd_table[local_fd] ;
		ASSERT(global_fd < MAX_FILE_OPEN);
		if (global_fd != -1 ){
			file_table[global_fd].fd_inode->i_open_cnts++;
		}
		local_fd++;
	}
}


//copy resource of parent process to child process
static int32_t copy_process(struct task_struct* child_thread , struct task_struct* parent_thread){
	//kernel buffer
	void* buf_page = get_kernel_pages(1);
	if (buf_page == NULL){
		return -1;
	}

	//1. copy pcb , virtual memory bitmap ,kernel stack
	if (copy_pcb_vaddrbitmap_stack0(child_thread , parent_thread) == -1){
		return -1;
	}

	//2. create page table for child thread
	child_thread->pgdir = create_page_dir();
	if (child_thread->pgdir == NULL){
		return -1;
	}

	//3. copy body of parent process and user stack
	copy_body_stack3(child_thread , parent_thread , buf_page);

	//4. create thread_stack for child_process
	build_child_stack(child_thread);

	//5. update inode open number
	update_inode_open_cnts(child_thread);

	mfree_page(PF_KERNEL , buf_page , 1);
	return 0;
}


//fork
pid_t sys_fork(void){
	struct task_struct* parent_thread = running_thread();
	struct task_struct* child_thread = get_kernel_pages(1);
	if (child_thread == NULL){
		return -1;
	}

	ASSERT(INTR_OFF == intr_get_status() && parent_thread->pgdir != NULL);

	if (copy_process(child_thread ,parent_thread) == -1){
		return -1;
	}

	ASSERT(!elem_find(&thread_ready_list , &child_thread->general_tag));
	list_append(&thread_ready_list , &child_thread->general_tag);
	ASSERT(!elem_find(&thread_all_list , &child_thread->all_list_tag));
	list_append(&thread_all_list , &child_thread->all_list_tag);

	return child_thread->pid;
}
