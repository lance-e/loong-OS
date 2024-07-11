#include "stdio.h"
#include "syscall.h"
#include "global.h"
#include "string.h"


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
	char* arg_str;
	while(index_char){
		if (index_char != '%'){
			*(buf_ptr++) = index_char;
			index_char = *(++index_ptr);
			continue;
		}
		index_char = *(++index_ptr); 		//skip '%'
		switch (index_char){
			case 's' :
				arg_str = va_arg(ap, char*);
				strcpy(buf_ptr,arg_str);
				buf_ptr+= strlen(arg_str);
				index_char = *(++index_ptr);
				break;
			case 'c' :
				*(buf_ptr++) = va_arg(ap, char);
				index_char = *(++index_ptr);
				break;
			case 'd' :
				arg_int = va_arg(ap , int);
				if (arg_int < 0){
					arg_int = 0 - arg_int;
					*(buf_ptr++) = '-';
				}
				itoa(arg_int, &buf_ptr , 10);
				index_char = *(++index_ptr);
				break;
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

//sprintf : diffrent from the printf , this is print into buffer
uint32_t sprintf(char* buf , const char* format , ...){
	va_list args;
	uint32_t len;
	va_start(args , format);
	len = vsprintf(buf,format, args);
	va_end(args);
	return len;
}
