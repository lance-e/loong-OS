#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "list.h"
#include "stdint.h"
#include "thread.h"


//struct of semaphore
struct semaphore{
	uint8_t value;
	struct list waiters;
};


//struct of lock
struct lock{
	struct task_struct * holder;				//holder of lock
	struct semaphore semaphore;				//mutex semaphore; semaphore = 1 
	uint32_t holder_repeat_nr;				//the number of holder had apply for lock
};


void sema_init(struct semaphore* psema,uint8_t value);
void lock_init(struct lock* lock);
void sema_down(struct semaphore* psema);
void sema_up(struct semaphore* psema);
void lock_acquire(struct lock* plock);
void lock_release(struct lock* plock);

#endif
