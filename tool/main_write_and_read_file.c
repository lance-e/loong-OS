#include "print.h"
#include "thread.h"
#include "init.h"
#include "string.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "syscall-init.h"
#include "stdio.h"
#include "memory.h"
#include "fs.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_proc_a(void);
void u_proc_b(void);
int prog_a_pid = 0 , prog_b_pid = 0;

int main(void){
	put_str("kernel starting!\n");
	init_all();
	intr_enable();

	process_execute(u_proc_a , "user_proc_a");
	process_execute(u_proc_b, "user_proc_b");
	
	thread_start("k_thread_a",31,k_thread_a,"I am thread_a ");
	thread_start("k_thread_b",31,k_thread_b,"I am thread_b ");
	uint32_t fd = sys_open("/file1" , O_RDWR);
	printf("open /file1 ,fd:%d\n", fd);
	char buf[64] = {0};
	int read_bytes = sys_read(fd ,buf , 18);
	printf("1_ read %d bytes:\n%s\n" ,read_bytes , buf);

	memset(buf , 0 , 64);
	read_bytes = sys_read(fd ,buf , 6);
	printf("2_ read %d bytes:\n%s\n" ,read_bytes , buf);

	memset(buf , 0 , 64);
	read_bytes = sys_read(fd ,buf , 6);
	printf("3_ read %d bytes:\n%s\n" ,read_bytes , buf);


	printf("----------- seek set 0------------\n");
	sys_lseek(fd , 0 , SEEK_SET);
	
	memset(buf , 0 , 64);
	read_bytes = sys_read(fd ,buf , 24);
	printf("4_ read %d bytes:\n%s\n" ,read_bytes , buf);

	sys_close(fd);
	printf("%d close now\n", fd);
	while(1);
	return 0;
}

void k_thread_a(void* arg){
	void* addr1 = sys_malloc(256);
	void* addr2 = sys_malloc(255);
	void* addr3 = sys_malloc(254);
	console_put_str(" thread_a malloc memory : 0x");
	console_put_int((int)addr1);
	console_put_str(" , ");
	console_put_int((int)addr2);
	console_put_str(" , ");
	console_put_int((int)addr3);
	console_put_char('\n');
	int time = 100000;
	while (time -- > 0 );
	sys_free(addr1);
	sys_free(addr2);
	sys_free(addr3);

	while(1);
}
void k_thread_b(void* arg){
	void* addr1 = sys_malloc(256);
	void* addr2 = sys_malloc(255);
	void* addr3 = sys_malloc(254);
	console_put_str(" thread_b malloc memory : 0x");
	console_put_int((int)addr1);
	console_put_str(" , 0x");
	console_put_int((int)addr2);
	console_put_str(" , 0x");
	console_put_int((int)addr3);
	console_put_char('\n');
	int time = 100000;
	while (time -- > 0 );
	sys_free(addr1);
	sys_free(addr2);
	sys_free(addr3);

	while(1);
}
void u_proc_a(void){
	void* addr1 = malloc(256);
	void* addr2 = malloc(255);
	void* addr3 = malloc(254);
	printf(" prog_a malloc addr : 0x%x , 0x%x , 0x%x\n",(int)addr1,(int)addr2,(int)addr3);
	int time = 100000;
	while (time -- > 0 );
	free(addr1);
	free(addr2);
	free(addr3);

	while(1);
}

void u_proc_b(void){
	void* addr1 = malloc(256);
	void* addr2 = malloc(255);
	void* addr3 = malloc(254);
	printf(" prog_b malloc addr : 0x%x , 0x%x , 0x%x\n",(int)addr1,(int)addr2,(int)addr3);
	int time = 100000;
	while (time -- > 0 );
	free(addr1);
	free(addr2);
	free(addr3);

	while(1);
}
