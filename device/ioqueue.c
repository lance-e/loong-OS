#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"


//initial io queue
void ioqueue_init(struct ioqueue* ioq){
	lock_init(&ioq->lock);
	ioq->producer = ioq->consumer = NULL;
	ioq->head = ioq->tail = 0;
}


//return the next position of 'pos' in buffer
static int32_t next_pos(int32_t pos){
	return (pos + 1 ) % bufsize;
}

//judge buffer is it full
bool ioq_full(struct  ioqueue* ioq){
	ASSERT(intr_get_status() == INTR_OFF);
	return next_pos(ioq->head) == ioq->tail;
}

//judge buffer is it empty
bool ioq_empty(struct ioqueue* ioq){
	ASSERT(intr_get_status() == INTR_OFF);
	return ioq->head == ioq->tail;
}

//make current producer of consumer wait in this buffer
static void   ioq_wait(struct task_struct** waiter){
	ASSERT(*waiter == NULL && waiter != NULL);
	*waiter = running_thread();
	thread_block(TASK_BLOCKED);	
}

//wake up waiter
static void  wakeup(struct task_struct** waiter){
	ASSERT(*waiter != NULL);
	thread_unblock(*waiter);
	*waiter =NULL;
}

//consumer get a char from io queue
char ioq_getchar(struct ioqueue* ioq){
	ASSERT(intr_get_status() == INTR_OFF);
	while (ioq_empty(ioq)){
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->consumer);
		lock_release(&ioq->lock);
	}
	char result = ioq->buf[ioq->tail];
	ioq->tail = next_pos(ioq->tail);

	if (ioq->producer != NULL){
		wakeup(&ioq->producer);
	}
	return result;
}


//producer put a char in io queue
void ioq_putchar(struct ioqueue* ioq,char target){
	ASSERT(intr_get_status() == INTR_OFF);
	while (ioq_full(ioq)){
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->producer);
		lock_release(&ioq->lock);
	}
	
	ioq->buf[ioq->head] = target;
	ioq->head = next_pos(ioq->head);

	if (ioq->consumer != NULL){
		wakeup(&ioq->consumer);
	}
}
