#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "global.h"
#include "string.h"
#include "debug.h"
#include "sync.h"

#define PG_SIZE 4096

/********************************************	address of bitmap**************************************************
 * 0xc009f000 is the top of kernel main thread stack , 0xc009e000 is the pcb of kernel main thread 
 * page frame is 4kb , size of bitmap is a page frame can contain 128mb(4k * 8 * 4k),
 * the address of bitmap is begin at 0xc009a000,end at 0xc009e000, so there are 4 page frame
 * the bitmap can contain 512mb*/

#define MEM_BITMAP_BASE 0xc009a000 

 /* ****************************************************************************************************************/


/* 0xc0000000 is the begin of kernel in virtual address
 * 0x00100000 is mean skip the low 1mb memory , make the virtual address logically continuous*/

#define K_HEAP_START 0xc0100000


#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)	//get the hight 10 bit
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)	//get the middle 10 bit


struct pool{
	struct bitmap pool_bitmap;			//to manage the physic memory
	uint32_t phy_addr_start;			//the start of physic memory to manage
	uint32_t pool_size;				//the size of pool
	struct lock lock;				//lock when apply for address

};
struct pool kernel_pool,user_pool;
struct virtual_addr kernel_vaddr;

//apply for 'pg_cnt' virtual page in the 'pf' virtual memory pool
//success :return the begin of address , failed: return NULL
static void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt){
	int vaddr_start =  0 , bit_idx_start = -1;
	uint32_t cnt = 0;
	if (pf == PF_KERNEL){
		//kernel address pool
		bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap,pg_cnt);
		if (bit_idx_start == -1 ){
			return NULL;
		}
		while (cnt < pg_cnt){
			bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start+cnt++,1);
		}
		vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
	}else {
		//user address pool
		struct task_struct* cur = running_thread();
		bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap,pg_cnt);
		if (bit_idx_start == -1 ){
			return NULL;
		}
		while (cnt < pg_cnt){
			bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx_start+cnt++,1);
		}
		vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
		// (0xc0000000 - PG_SIZE ) has been allocate for user process's 3 level stack
		ASSERT((uint32_t) vaddr_start < (0xc0000000 - PG_SIZE));
	}
	return (void*) vaddr_start;
}



//get the PTE pointer of vaddr;!!!!!
uint32_t* pte_ptr(uint32_t vaddr){
       uint32_t* pte = (uint32_t*) (0xffc00000 + ((vaddr & 0xffc00000) >> 10) +PTE_IDX(vaddr) * 4 );
       return pte;
}

//get the PDE pointer of vaddr;!!!!!
uint32_t* pde_ptr(uint32_t vaddr){
	uint32_t* pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
	return pde;
}

//get the physic page from m_pool
static void* palloc(struct pool* m_pool){
	int bit_idx = bitmap_scan(&m_pool->pool_bitmap,1);
	if (bit_idx == -1 ){
		return NULL;
	}
	bitmap_set(&m_pool->pool_bitmap,bit_idx,1);
	uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
	return (void*)page_phyaddr;
}

//make the map of _vaddr and _page_phyaddr in page table
static void page_table_add(void* _vaddr, void* _page_phyaddr){
	uint32_t vaddr = (uint32_t) _vaddr,page_phyaddr = (uint32_t) _page_phyaddr;
	uint32_t* pde = pde_ptr(vaddr);
	uint32_t* pte = pte_ptr(vaddr);
	// execute *pte must after pde had create,otherwise wile Page_Fault
	if(*pde & 0x00000001){
		//make sure pte is not exist 
		ASSERT(!(*pte & 0x00000001));
		if (!(*pte & 0x00000001)){
			*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
		}else {
			PANIC("pte repeat");
			*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
		}
	}else {
		//pde wasn't exist ,so create pde first
		uint32_t pde_phyaddr = (uint32_t) palloc(&kernel_pool);
		*pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
		memset((void*)((int)pte & 0xfffff000) , 0 , PG_SIZE);
		
		ASSERT(!(*pte & 0x00000001));
		*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
	}
}


//malloc 'pg_cnt' page memory
/***********************   malloc_page 	******************
 * 	1.vaddr_get :		apply for virtual memory in virtual memory pool
 * 	2.palloc: 		apply for physic memory in physic memory pool
 * 	3.page_table_add: 	make the map
 ******************************************************************/
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt){
	ASSERT(pg_cnt > 0 && pg_cnt < 3840);		//here set max is 15*1024*1024 /4096 = 3840 page
	void * vaddr_start = vaddr_get(pf,pg_cnt);
	if (vaddr_start == NULL ){
		return NULL;
	}

	uint32_t vaddr = (uint32_t)vaddr_start , cnt = pg_cnt;
	struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

	//physic memory isn't continuous
	while (cnt-- > 0){
		void * page_phyaddr = palloc(mem_pool);
		if (page_phyaddr == NULL){
			//physic memory will roll back , will write in future
			return NULL;
		}
		page_table_add((void*)vaddr,page_phyaddr);
		vaddr += PG_SIZE;
	}
	return vaddr_start;
}

//apply for 'pg_cnt'  page in kernel physic memory pool
void* get_kernel_pages(uint32_t pg_cnt){
	void* vaddr = malloc_page(PF_KERNEL,pg_cnt);
	if (vaddr != NULL){
		memset(vaddr , 0 , pg_cnt * PG_SIZE);
	}
	return vaddr;
}

//apply for 'pg_cnt'  page in user physic memory pool
void* get_user_pages(uint32_t pg_cnt){
	lock_acquire(&user_pool.lock);
	void* vaddr = malloc_page(PF_USER,pg_cnt);
	if (vaddr != NULL){
		memset(vaddr , 0 , pg_cnt * PG_SIZE);
	}
	lock_release(&user_pool.lock);
	return vaddr;
}


//make a map of 'vaddr' and physic address in 'pf' pool ,just can map only a page
// (it is like malloc_page , but don't need apply virtual address again);
void* get_a_page(enum pool_flags pf, uint32_t vaddr){
	struct pool* mem_pool = (pf & PF_KERNEL)? &kernel_pool : &user_pool;
	lock_acquire(&mem_pool->lock);

	// first : set virtual address's bitmap to 1
	struct task_struct* cur = running_thread();
	int32_t bit_idx = -1;
	if (cur->pgdir != NULL &&  pf == PF_USER){
		bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
		ASSERT(bit_idx > 0);
		bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx, 1);
	}else if (cur->pgdir == NULL && pf == PF_KERNEL){
		bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
		ASSERT(bit_idx > 0 );
		bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx , 1);
	}else {
		PANIC("get_a_page: not allow kernel alloc userspace or user alloc kernelsapce by get_a_page\n");
	}

	// second : apply for physic address
	void* page_phyaddr = palloc(mem_pool);
	if (page_phyaddr == NULL){
		lock_release(&mem_pool->lock);
		return NULL;
	}
	// third : make map
	page_table_add((void*)vaddr,page_phyaddr);
	lock_release(&mem_pool->lock);
	return (void*)vaddr;
}

//get the physic address of 'vaddr'
uint32_t addr_v2p (uint32_t vaddr){
	uint32_t* pte = pte_ptr(vaddr);
	return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}
	

//initial memory pool
static void mem_pool_init(uint32_t all_mem){
	put_str("  mem_pool_init start\n");

	uint32_t page_table_size = PG_SIZE * 256;

	uint32_t used_mem = page_table_size + 0x100000; 

	uint32_t free_mem = all_mem - used_mem;
	uint16_t all_free_pages = free_mem / PG_SIZE;

	uint16_t kernel_free_pages = all_free_pages /2 ;
	uint16_t user_free_pages = all_free_pages - kernel_free_pages;

	// a byte can express 8 pages
	uint32_t kbm_length = kernel_free_pages / 8 ;	
	uint32_t ubm_length = user_free_pages / 8;

	//kernel pool start address
	uint32_t kp_start = used_mem;
	//user pool start address
	uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;

	//initial
	kernel_pool.phy_addr_start = kp_start;
	kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
	kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
	kernel_pool.pool_bitmap.bits = (void*) MEM_BITMAP_BASE;				//0x9a000
	lock_init(&kernel_pool.lock);
	user_pool.phy_addr_start = up_start;
	user_pool.pool_size = user_free_pages * PG_SIZE;
	user_pool.pool_bitmap.btmp_bytes_len  =ubm_length;
	user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);		//follow on kernel pool bitmap
	lock_init(&user_pool.lock);

	/*******************print the memory pool information*****************/
	put_str("     kernel_pool_bitmap_start:");
	put_int((int)kernel_pool.pool_bitmap.bits);
	put_str("  kernel_pool_phy_address_start:");
	put_int(kernel_pool.phy_addr_start);
	put_str("\n");

	put_str("     user_pool_bitmap_start:");
	put_int((int)user_pool.pool_bitmap.bits);
	put_str("  user_pool_phy_address_start:");
	put_int(user_pool.phy_addr_start);
	put_str("\n");

	//set bitmap to 0
	bitmap_init(&kernel_pool.pool_bitmap);
	bitmap_init(&user_pool.pool_bitmap);

	/***************** initial the kernel virtual memory pool************/
	//initial the bitmap of kernel virtual address
	//used for maintain the kernel virtual address , so the size is equal to kernel pool 
	kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
	kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);


	kernel_vaddr.vaddr_start = K_HEAP_START;
	bitmap_init(&kernel_vaddr.vaddr_bitmap);

	
	put_str("  mem_pool_init done \n");

}


void mem_init(){
	put_str("mem_init start\n");
	uint32_t mem_bytes_total = (* (uint32_t*)(0xb00));				//set at loader.S 
	mem_pool_init(mem_bytes_total);
	put_str("mem_init done\n");
}

