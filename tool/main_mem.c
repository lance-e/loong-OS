#include "print.h"
#include "init.h"
#include "memory.h"
int main(void){
	put_str("Kernel Starting!\n");
	init_all();

	void* addr = get_kernel_pages(3);
	put_str("\n  I get kernel pages is :");
	put_int((uint32_t)addr);
	put_str("\n");
	void* addr_two = get_kernel_pages(2);
	put_str("\n  I get kernel pages is :");
	put_int((uint32_t)addr_two);
	put_str("\n");

	while(1);
	return 0;
}
