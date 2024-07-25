#ifndef __LIB_USER_ASSERT_H
#define __LIB_USER_ASSERT_H

void user_spin(char* filename,int line , const char* func ,const char* condition);


//------------------------------------ __VA_ARGS__---------------------------------
// stand for the all argument of ...
// ... is mean the argument is changalbe 

#define panic(...) user_spin(__FILE__,__LINE__,__func__,__VA_ARGS__)

//------------------------------------------------------------------------------

#ifdef NDEBUG
#define assert(CONDITION) ((void)0)
#else 
#define assert(CONDITION) if (CONDITION){}else {panic(#CONDITION);}

#endif   /*NDEBUG*/

#endif   /*__KERNEL_DEBUG_H*/
