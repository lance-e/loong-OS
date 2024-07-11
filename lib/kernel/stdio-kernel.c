#include "stdio-kernel.h"
#include "stdio.h"
#include "global.h"
#include "console.h"

//print for kernel
void printk(const char* format,...){
	va_list args;
	va_start(args,format);
	char buf[1024] = {0};
	vsprintf(buf, format , args);
	va_end(args);
	console_put_str(buf);
}

