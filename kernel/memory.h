#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
#include "list.h"

//judge use which pool
enum pool_flags{
	PF_KERNEL =1 ,
	PF_USER = 2
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

//memory block
struct mem_block{
	struct list_elem free_elem;
};

//memory block descriptor
struct mem_block_desc{
	uint32_t block_size;				//size of memory block
	uint32_t block_per_arena;			//the number of memory block in per arena
	struct list free_list;
};

// the kind of mem_block_desc: 16k 32k 64k 128k 256k 512k 1024k 
#define DESC_CNT 7 


extern struct pool kernel_pool ,user_pool;
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt);
void* get_kernel_pages(uint32_t pg_cnt);
void* get_user_pages(uint32_t pg_cnt);
void* get_a_page(enum pool_flags pf,uint32_t  vaddr);
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void block_desc_init(struct mem_block_desc* desc_array);
void* sys_malloc(uint32_t size);

void sys_free(void* ptr);
void pfree(uint32_t phy_addr);
void mfree_page(enum pool_flags pf ,void* _vaddr , uint32_t pg_cnt);

void mem_init(void);

#endif
