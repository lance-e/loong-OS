#include "stdio.h"
#include "syscall.h"
#include "global.h"
#include "string.h"

#define va_start(ap,v) ap=(va_list)&v 			//make ap point to the first argument
#define va_arg(ap,t) *((t*)(ap += 4))			//make ap point to next arg and return
#define va_end(ap) ap=NULL				//clear the ap

//transform integer to ascii
static void itoa(uint32_t value , char** buf_ptr_addr, uint8_t base){
	uint32_t m = value % base;			// get the lowest bit
	uint32_t i = value / base;			// round 
	if (i) {
		itoa(i,buf_ptr_addr,base);
	}
	if ( m < 10){
		*((*buf_ptr_addr)++) = m + '0';
	}else{
		*((*buf_ptr_addr)++) = m - 10 + 'A';
	}
}

//vsprintf : input the param ap into the string 'str' ,and return the length of this string
uint32_t vsprintf(char* str,const char* format, va_list ap){
	char* buf_ptr = str;
	const char* index_ptr = format;
	char index_char = *index_ptr;
	uint32_t arg_int;
	while(index_char){
		if (index_char != '%'){
			*(buf_ptr++) = index_char;
			index_char = *(++index_ptr);
			continue;
		}
		index_char = *(++index_ptr); 		//skip '%'
		switch (index_char){
			case 'x' :
				arg_int = va_arg(ap, int);
				itoa(arg_int , &buf_ptr, 16);
				index_char = * (++ index_ptr);	//skip the format char and update the index_char;
				break;
		}
	}
	return strlen(str);
}

//printf : format print string
uint32_t printf( const char* format, ...){
	va_list args;
	uint32_t len;
	va_start(args, format);				//make the args point to 'format'
	char buf[1024] = {0};				// save the finally string
	len = vsprintf(buf,format,args);
	va_end(args);
	write(buf);
	return len;
}

