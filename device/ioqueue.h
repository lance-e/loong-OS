#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "sync.h"
#include "thread.h"

#define bufsize 64

//circular queue
struct ioqueue{
	struct lock lock;
	struct task_struct* producer;
	struct task_struct* consumer;
	char buf[bufsize];
	int32_t head;						//head of queue: write data
	int32_t tail;						//tail of queue: read data
};

void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char target );

#endif
