#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"

//judge use which pool
enum pool_flags{
	PF_KERNEL =1 ,
	PF_USER = 2;
};

#define PG_P_1 1 
#define PG_P_0 0 
#define PG_RW_R 0 
#define PG_RW_W 2 
#define PG_US_S 0
#define PG_US_U 4

//virtual address pool
struct virtual_addr{
	struct bitmap vaddr_bitmap;
	uint32_t vaddr_start;
};

extern struct pool kernel_pool ,user_pool;
void mem_init(void);

#endif
