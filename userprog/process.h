#ifndef __USERPROG_PROCESS_H
#define __USERPROG_PROCESS_H
#include "thread.h"

//bottom of stack is 0xc0000000 , but we should point to the start of this page(0x1000 = 4096)
#define USER_STACK3_VADDR (0xc0000000 - 0x1000)

//the start of user process virtual address
#define USER_VADDR_START 0x8048000

#define default_prio 31

void start_process(void* filename_);
void page_dir_activate(struct task_struct* p_thread);
void process_activate(struct task_struct* p_thread);
uint32_t* create_page_dir(void);
void create_user_vaddr_bitmap(struct task_struct* user_prog);
void process_execute(void* filename , char* name);


#endif
