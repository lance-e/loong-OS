#include "inode.h"
#include "debug.h"
#include "thread.h"
#include "super_block.h"
#include "string.h"
#include "interrupt.h"
#include "file.h"


extern struct partition* cur_part;

//storage the position of inode
struct inode_position{
	bool two_sec;			//is it cross sector
	uint32_t sec_lba;		//sector number where the inode are
	uint32_t off_size;		//byte offset in this sector
};


//get the sector number and byte offset of inode
static void inode_locate(struct partition* part , uint32_t inode_no , struct inode_position* inode_pos){
	ASSERT(inode_no < 4096);	
	uint32_t inode_table_lba = part->sb->inode_table_lba;

	uint32_t inode_size = sizeof(struct inode);
	uint32_t off_size = inode_no * inode_size; 		//byte offset
	uint32_t off_sec = off_size  / 512;			//sector offset
	uint32_t off_size_in_sec = off_size % 512;		//offset in sector
	
	//judge whether cross two sector
	uint32_t left_in_sector = 512 - off_size_in_sec;
	if (left_in_sector > inode_size){
		inode_pos->two_sec =  false;
	}else {
		inode_pos->two_sec =  true;
	}
	inode_pos->sec_lba = inode_table_lba + off_sec;
	inode_pos->off_size = off_size_in_sec;
}

//write inode into partition
void inode_sync(struct partition* part , struct inode* inode , void* io_buf){
	uint8_t inode_no = inode->i_no;	
	struct inode_position inode_pos;
	inode_locate(part, inode_no , &inode_pos);
	ASSERT( inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

	//clear the struct member of inode : inode_tag , i_open_cnts is unused in hard disk 
	struct inode pure_inode;
	memcpy(&pure_inode , inode , sizeof(struct inode));	
	pure_inode.i_open_cnts = 0 ;
	pure_inode.write_deny = false;
	pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

	char* inode_buf = (char*)io_buf;
	if (inode_pos.two_sec){
		//operate in sector , so read the before sector , spell the new data and write 
		ide_read(part->my_disk , inode_pos.sec_lba , inode_buf ,2);
		//spell
		memcpy((inode_buf + inode_pos.off_size) ,&pure_inode , sizeof(struct inode));
		//write the new spelled data
		ide_write(part->my_disk , inode_pos.sec_lba , inode_buf , 2);
	}else {
		//operate in sector , so read the before sector , spell the new data and write 
		ide_read(part->my_disk , inode_pos.sec_lba ,inode_buf , 1);
		//spell
		memcpy((inode_buf + inode_pos.off_size) ,&pure_inode , sizeof(struct inode));
		//write the new spelled data
		ide_write(part->my_disk , inode_pos.sec_lba , inode_buf , 1);
	}
}

//open inode by inode_no
struct inode* inode_open(struct partition* part , uint32_t inode_no){
	//first to find in alread opened inode list  
	struct list_elem* elem = part->open_inode.head.next;
	struct inode* inode_found;
	while (elem != &part->open_inode.tail){
		inode_found = elem2entry(struct inode , inode_tag , elem);
		if (inode_found->i_no == inode_no){
			inode_found->i_open_cnts ++;
			return inode_found;
		}
		elem = elem->next;
	}

	//not find in opened list , so read from hard disk and append to opened list
	struct inode_position inode_pos;
	inode_locate(part , inode_no , &inode_pos);

	//in order to make the new inode shared by all process.So add in kernel space
	//( make user process's page table = NULL , that malloc will create at kernel space
	struct task_struct* cur = running_thread();
	uint32_t* cur_pagedir_bak = cur->pgdir;
	cur->pgdir = NULL;
	inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
	cur->pgdir= cur_pagedir_bak;

	char* inode_buf;
	if (inode_pos.two_sec){
		inode_buf = (char*) sys_malloc(1024);
		ide_read(part->my_disk , inode_pos.sec_lba , inode_buf , 2);
	}else {
		inode_buf = (char*) sys_malloc(512);
		ide_read(part->my_disk , inode_pos.sec_lba , inode_buf , 1);
	}

	memcpy(inode_found , inode_buf + inode_pos.off_size , sizeof(struct inode));
	list_push(&part->open_inode , &inode_found->inode_tag );
	inode_found->i_open_cnts = 1;
	
	sys_free(inode_buf);
	return inode_found;
}
	

//close inode or reduce the number of inode had opened 
void inode_close(struct inode* inode){
	enum intr_status old_status= intr_disable();
	if (--inode->i_open_cnts == 0 ){
		list_remove(&inode->inode_tag);
		struct task_struct* cur = running_thread();
		uint32_t* cur_pgdir_bak = cur->pgdir;
		cur->pgdir = NULL;
		sys_free(inode);
		cur->pgdir = cur_pgdir_bak;
	}
	intr_set_status(old_status);
}

//initial the target inode
void inode_init(uint32_t inode_no , struct inode* new_inode){
	new_inode->i_no = inode_no;
	new_inode->i_size = 0;
	new_inode->i_open_cnts = 0;
	new_inode->write_deny = false;
	uint8_t index = 0;
	while(index < 13){
		new_inode->i_sectors[index] = 0 ;
		index++;
	}
}

//clear inode in partition (actually , this is unnecessary, the data in disk can cover)
void inode_delete(struct partition* part , uint32_t inode_no , void* io_buf ){
	ASSERT(inode_no < 4096);

	struct inode_position inode_pos ;
	inode_locate(part, inode_no , &inode_pos);

	ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

	char* inode_buf = (char*)io_buf;
	if (inode_pos.two_sec){
		ide_read(part->my_disk , inode_pos.sec_lba ,inode_buf ,2 );
		memset((inode_buf + inode_pos.off_size) , 0 , sizeof(struct inode));
		ide_write(part->my_disk , inode_pos.sec_lba , inode_buf , 2);
	}else{
		ide_read(part->my_disk , inode_pos.sec_lba ,inode_buf ,1 );
		memset((inode_buf + inode_pos.off_size) , 0 , sizeof(struct inode));
		ide_write(part->my_disk , inode_pos.sec_lba , inode_buf , 1);
	}

}


//release the inode's block and itself
void inode_release(struct partition* part , uint32_t inode_no ){
	struct inode* inode_to_del = inode_open(part , inode_no);
	ASSERT(inode_to_del->i_no == inode_no);

	//------------ 1. release all used block ---------------------
	
	uint8_t block_idx = 0 , block_cnt = 12;
	uint32_t block_bitmap_idx;
	uint32_t all_blocks[140] = {0};
	//1. first to collect direct block
	while(block_idx < 12){
		all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
		block_idx++;
	}
	//2. second to collect all indirect block and release first level indirect block table's block
	if (inode_to_del->i_sectors[12] != 0){
		//collect all indirect block
		ide_read(part->my_disk , inode_to_del->i_sectors[12] , all_blocks+12 , 1);
		block_cnt = 140;
		//release indirect block table's block
		block_bitmap_idx = inode_to_del->i_sectors[12] - part->sb->data_start_lba;
		bitmap_set(&part->block_bitmap , block_bitmap_idx , 0);
		bitmap_sync(cur_part , block_bitmap_idx , BLOCK_BITMAP);
	}
	//3. third to release all block (have cllect to "all_blocks")
	block_idx = 0 ;
	while (block_idx < block_cnt){
		if (all_blocks[block_idx] != 0 ){
			block_bitmap_idx = 0 ;			//use for next ASSERT
			block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
			ASSERT(block_bitmap_idx > 0);
			bitmap_set(&part->block_bitmap , block_bitmap_idx , 0);
			bitmap_sync(cur_part , block_bitmap_idx , BLOCK_BITMAP);
		}
		block_idx++;
	}

	//--------------- 2. release used inode ----------------------
	bitmap_set(&part->inode_bitmap , inode_no , 0 );
	bitmap_sync(cur_part , inode_no , INODE_BITMAP);

	/*****/// the inode_delect just used to debug , it is unnecessary 
	void* io_buf = sys_malloc(1024);
	inode_delete(part , inode_no , io_buf);
	sys_free(io_buf);
	/******/
	inode_close(inode_to_del);
}
