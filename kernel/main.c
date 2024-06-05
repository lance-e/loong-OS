#include "print.h"
#include "init.h"
#include "debug.h"
#include "string.h"
int main(void){
	put_str("Kernel Starting!\n");
	init_all();
	ASSERT(strcmp("ABC","ABD"));
	while(1);
	return 0;
}
