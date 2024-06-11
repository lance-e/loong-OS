#include "console.h"
#include "print.h"
#include "stdint.h"
#include "sync.h"
#include "thread.h"

//console lock
static struct lock console_lock;

//initial console
void console_init(){
	lock_init(&console_lock);
}

//get console
void console_acquire(){
	lock_acquire(&console_lock);
}

//release console 
void console_release(){
	lock_release(&console_lock);
}

//print string in console
void console_put_str(char* str){
	console_acquire();
	put_str(str);
	console_release();
}

//print 16bit int in console
void console_put_int(uint32_t num){
	console_acquire();
	put_int(num);
	console_release();
}


