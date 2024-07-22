#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "global.h"
#include "string.h"
#include "debug.h"
#include "sync.h"
#include "thread.h"
#include "interrupt.h"

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



//memory pool
struct pool{
	struct bitmap pool_bitmap;			//to manage the physic memory
	uint32_t phy_addr_start;			//the start of physic memory to manage
	uint32_t pool_size;				//the size of pool
	struct lock lock;				//lock when apply for address

};
struct pool kernel_pool,user_pool;
struct virtual_addr kernel_vaddr;


//memory repository
struct arena {
	struct mem_block_desc* desc;
	
	// if large =true , cnt is the number of page 
	// if large =false, cnt is the number of memory block
	uint32_t cnt;					

	bool large;
};
struct mem_block_desc k_block_descs[DESC_CNT];



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
	// execute *pte must after pde had create,otherwise will Page_Fault
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


//use for fork:
//make a map of 'vaddr' and physic address in 'pf' pool ,just can map only a page
// (it is like malloc_page , but don't need apply virtual address again and set virtual adress bitmap);
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr){
	struct pool* mem_pool = (pf & PF_KERNEL)? &kernel_pool : &user_pool;
	lock_acquire(&mem_pool->lock);

	// apply for physic address
	void* page_phyaddr = palloc(mem_pool);
	if (page_phyaddr == NULL){
		lock_release(&mem_pool->lock);
		return NULL;
	}
	// make map
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


//prepare for malloc :initial the memory block descriptor  
void block_desc_init(struct mem_block_desc* desc_array){
	uint16_t index ,size = 16;
	for (index = 0 ; index < DESC_CNT ; index ++ ){
		desc_array[index].block_size = size;

		//initial the number of memory block in arena
		desc_array[index].block_per_arena = (PG_SIZE - sizeof(struct arena)) / size;

		list_init(&desc_array[index].free_list);

		size *= 2;
	}
}

//return the address of 'index' in the arena
static struct mem_block* arena2block(struct arena* a , uint32_t index){
	return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + index * a->desc->block_size);
}

//return the address of which arena 'b' at
static struct arena* block2arena(struct mem_block* b){
	return (struct arena*)((uint32_t)b & 0xfffff000);
}

//malloc 'size' memory from heap
void* sys_malloc(uint32_t size){
	enum pool_flags PF;
	struct pool* mem_pool;
	uint32_t pool_size;
	struct mem_block_desc* descs;
	struct task_struct* cur_thread = running_thread();

	//judge use which memory pool
	if (cur_thread->pgdir == NULL ){				//kernel memory pool
		PF = PF_KERNEL;
		pool_size = kernel_pool.pool_size;
		mem_pool = &kernel_pool;
		descs = k_block_descs;
	}else {								//user process memory pool
		PF = PF_USER;
		pool_size = user_pool.pool_size;
		mem_pool = &user_pool;
		descs = cur_thread->u_block_desc; 
	}

	//judge if there is enough capacity
	if (!(size > 0 && size < pool_size)){
		return NULL;
	}
	struct arena* a;
	struct mem_block* b;
	lock_acquire(&mem_pool->lock);
	
	//begin malloc
	if (size > 1024){
		uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena) , PG_SIZE);

		a = malloc_page(PF,page_cnt);

		if (a != NULL){
			memset(a, 0 , page_cnt * PG_SIZE);		//clear the malloc memory
			a->desc = NULL;
			a->cnt = page_cnt;
			a->large = true;
			lock_release(&mem_pool->lock);
			//+1 is mean skip a size of struct arena(meta message)
			return (void*)(a + 1);				
		}else {
			lock_release(&mem_pool->lock);
			return NULL;
		}

	}else {								

		//get the kind of block index
		uint8_t index;
		for (index = 0 ; index < DESC_CNT;  index++){
			if (size <= descs[index].block_size){
				break;
			}
		}
		
		// if the 'free_list' already have no available 'mem_block', now create new arena
		if (list_empty(&descs[index].free_list)){
			a = malloc_page(PF , 1);
			if (a == NULL){
				lock_release(&mem_pool->lock);
				return NULL;
			}
			memset(a , 0 , PG_SIZE);

			a->desc = &descs[index];
			a->large = false;
			a->cnt = descs[index].block_per_arena;
			
			uint32_t block_index;

			enum intr_status old_status = intr_disable();

			//begin to split the memory of arena
			//and add to the 'free_list' of memory block descriptor
			for (block_index = 0 ;block_index < descs[index].block_per_arena;block_index ++){
				b = arena2block(a, block_index);
				ASSERT(!elem_find(&a->desc->free_list , &b->free_elem));
				list_append( &a->desc->free_list , &b->free_elem);
			}
			intr_set_status(old_status);
		}
		
		//begin to malloc memory block
		b = (struct mem_block*)(list_pop(&(descs[index].free_list)) );
		memset(b , 0 , descs[index].block_size);

		a = block2arena(b);
		a->cnt --;
		lock_release(&mem_pool->lock);
		return (void*)b;
	}

}

//free memory 'ptr'
void sys_free(void* ptr){
	ASSERT( ptr != NULL);
	if (ptr != NULL){
		enum pool_flags pf;
		struct pool* mem_pool;

		//judge thread of process
		if (running_thread()->pgdir == NULL){
			ASSERT((uint32_t)ptr >= K_HEAP_START);
			pf = PF_KERNEL;
			mem_pool = &kernel_pool;
		}else {
			pf = PF_USER;
			mem_pool = &user_pool;
		}

		lock_acquire(&mem_pool->lock);
		struct mem_block* b = ptr;
		struct arena* a = block2arena(b);

		ASSERT(a->large == 1 || a->large == 0);
		if (a->desc == NULL && a->large == true ){
			mfree_page(pf , a , a->cnt);
		}else {
			//recycle the memory block into free_list
			list_append(&a->desc->free_list , &b->free_elem);

			//judge the all block in arena is it all free,
			//if all free that free the whole arena
			if (++a->cnt == a->desc->block_per_arena){
				uint32_t index;
				for (index = 0 ; index < a->desc->block_per_arena ; index++){
					struct mem_block* b = arena2block(a , index);
					ASSERT(elem_find(&a->desc->free_list ,  &b->free_elem));
					list_remove(&b->free_elem);
				}
				mfree_page( pf , a , 1);
			}
		}
		lock_release(&mem_pool->lock);
	}
}



//free physic memory to physic memory pool
void pfree(uint32_t pg_phy_addr){
	struct pool* mem_pool;
	uint32_t index = 0 ;
	if (pg_phy_addr >= user_pool.phy_addr_start) {				//user memory pool
		mem_pool = &user_pool;
		index = (pg_phy_addr - user_pool.phy_addr_start ) / PG_SIZE;
	}else {									//kernel memory pool
		mem_pool = &kernel_pool;
		index = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
	}
	bitmap_set(&mem_pool->pool_bitmap , index , 0 );
}

//remove the map of 'vaddr', just to remove the pte of 'vaddr'
static void page_table_pte_remove(uint32_t vaddr){
	uint32_t* pte = pte_ptr(vaddr);
	*pte &= PG_P_0 ;
	asm volatile ("invlpg %0": : "m" (vaddr) : "memory");
}

//free the 'pg_cnt' consecutive pages virtual memory from virtual memory pool
static void vaddr_remove(enum pool_flags pf , void* _vaddr , uint32_t pg_cnt){
	uint32_t bit_start_index = 0 , vaddr = (uint32_t)_vaddr , count = 0 ;
	if (pf == PF_KERNEL){				//kernel virtual memory pool
		bit_start_index = (vaddr - kernel_vaddr.vaddr_start ) / PG_SIZE;
		while(count < pg_cnt) {
			bitmap_set(&kernel_vaddr.vaddr_bitmap , bit_start_index+ count++, 0);
		}
	}else {						//user virtual memory pool
		struct task_struct * cur_thread = running_thread();
		bit_start_index =( vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
		while (count < pg_cnt ) {
			bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap,bit_start_index + count++ , 0 );
		}
	}
}



//free 'pg_cnt' page memory
/***********************   mfree_page 	    ******************
 * 	1.vaddr_remove :		free virtual memory into virtual memory pool
 * 	2.pfree: 			free physic memory into  physic memory pool
 * 	3.page_table_pte_remove: 	remvoe the map
 ******************************************************************/
void mfree_page(enum  pool_flags pf , void* _vaddr , uint32_t pg_cnt){
	//struct pool* mem_pool;
	uint32_t phy_addr ;
	uint32_t vaddr = (uint32_t)_vaddr, count = 0;

	ASSERT(pg_cnt > 0 && vaddr % PG_SIZE == 0 );

	phy_addr = addr_v2p(vaddr); 		//get the physic memory by the virtual address;
	
	//make sure the target page is exist at the out of low 1mb , 1kb pdt and 1kb pt
	ASSERT( (phy_addr % PG_SIZE ) == 0 && phy_addr >= 0x102000);

	//judge use which memory pool
	if (phy_addr >= user_pool.phy_addr_start){			//in user memory pool
		vaddr -= PG_SIZE;	
		while(count < pg_cnt){
			vaddr += PG_SIZE;
			phy_addr = addr_v2p(vaddr);
			ASSERT((phy_addr % PG_SIZE ) == 0 && phy_addr >= user_pool.phy_addr_start);
			pfree(phy_addr);
			page_table_pte_remove(vaddr);
			count++;
		}
	}else {
		vaddr -= PG_SIZE;
		while(count < pg_cnt){
			vaddr += PG_SIZE;
			phy_addr = addr_v2p(vaddr);
			ASSERT((phy_addr % PG_SIZE ) == 0 &&	\
			phy_addr >= kernel_pool.phy_addr_start &&	\
			phy_addr < user_pool.phy_addr_start);
			pfree(phy_addr);
			page_table_pte_remove(vaddr);
			count++;
		}
	}
	//clear the target bit of virtual memory's bitmap
	vaddr_remove(pf , _vaddr , pg_cnt);
}





void mem_init(){
	put_str("mem_init start\n");
	uint32_t mem_bytes_total = (* (uint32_t*)(0xb00));				//set at loader.S 
	mem_pool_init(mem_bytes_total);
	block_desc_init(k_block_descs);
	put_str("mem_init done\n");
}

