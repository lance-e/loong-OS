#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H
#include "stdint.h" 

#define va_start(ap,v) ap=(va_list)&v 			//make ap point to the first argument
#define va_arg(ap,t) *((t*)(ap += 4))			//make ap point to next arg and return
#define va_end(ap) ap=NULL				//clear the ap

typedef char* va_list ;
uint32_t vsprintf(char* str ,const char* format, va_list ap);
uint32_t printf(const char* format, ...);
uint32_t sprintf(char* buf , const char* format, ...);
#endif
