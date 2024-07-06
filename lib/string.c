#include "string.h"
#include "global.h"
#include "debug.h"

//set 'size' byte to 'value' from the 'dst_'
void memset(void* dst_ , uint8_t value , uint32_t size){
	ASSERT(dst_ != NULL);
	uint8_t* dst = (uint8_t * ) dst_;
	while(size-- > 0)
		*dst++=value;
}

//copy 'size' byte form 'src_' to 'dst_'
void memcpy(void* dst_, void* src_ ,uint32_t size){
	ASSERT(dst_ != NULL && src_ != NULL);
	uint8_t* dst = (uint8_t* )dst_;
	const uint8_t* src = (uint8_t* )src_;
	while(size-- >0)
		*dst++ = *src++;
}

//compare 'size' byte from a_ and b_
//return == : 0 , > : +1 , < : -1
int memcmp(const void* a_,const void* b_,uint32_t size){
	const char* a = (uint8_t*) a_;
	const char* b = (uint8_t*) b_;
	ASSERT(a != NULL && b != NULL);
	while (size-- > 0){
		if (*a != *b) {
			return *a > *b ?1 : -1;
		}
		*a++;
		*b++;
	}
	return 0;
}

//copy string from str_ to dst_
char* strcpy(char* dst_ , const char* src_){
	ASSERT(dst_ != NULL && src_ != NULL);
	char* r = dst_;
	while (*dst_++ = *src_++);
	return r;
}

//return the length of string
uint32_t strlen(const char* str){
	ASSERT(str != NULL);
	const char* p = str;
	while (*p++);
	return (p - str - 1 );
}

//comapre two string 
//return: ==:0 , > :+1 , < : - 1
int8_t strcmp(const char * a , const char * b){
	ASSERT(a!=NULL && b!= NULL);
	while(*a != 0 && *a ==*b ){
		*a++;
		*b++;
	}
	return (*a < * b) ? -1 : (*a > *b ) ;
}

//search 'ch' from left to right in str ,return the address of first ch
char* strchr(const char* str,const uint8_t ch){
	ASSERT(str != NULL);
	while (*str != 0 ){
		if (*str == ch){
			return (char*)str;
		}
		str++;
	}
	return NULL;
}

//search 'ch' from right to left in str, return the address  of first ch
char* strrchr(const char* str,const uint8_t ch){
	ASSERT(str != NULL);
	const char* last_char = NULL;
	while(*str != 0 ){
		if (*str == ch){
			last_char = str;
		}
		str++;
	}
	return (char*) last_char;
}

//make src_ append to dst_ ,return the address
char* strcat(char* dst_, const char* src_){
	ASSERT(dst_ != NULL && src_ != NULL);
	char * str = dst_;
	while(*str++);
	--str;
	while((*str ++ = *src_++));
	return dst_;
}

//return the frequency of ch in str
uint32_t strchrs(const char* str, uint8_t ch){
	ASSERT(str != NULL);
	uint32_t ch_cnt = 0 ;
	const char * p = str;	
	while (*p != 0 ){
		if (*p == ch){
			ch_cnt++;
		}
		p++;
	}
	return ch_cnt;
}


		
