#include "fs.h"
#include "stdio-kernel.h"
#include "thread.h"
#include "bitmap.h"



//file table
struct file file_table[MAX_FILE_OPEN];

//get a free slot in "file_table"
int32_t get_free_slot_in_global(void){
	uint32_t index =3 ;
	while (index < MAX_FILE_OPEN){
		if (file_table[index].fd_index == NULL){
			break;
		}
		index ++;
	}
	if (index == MAX_FILE_OPEN){
		printk("exceed max open files\n");
		return -1;
	}
	return index;
}
		
//install the fd into thread/process 's fd_table
int32_t pcb_fd_install(int32_t global_fd_index){
	struct task_struct* cur = running_thread();
	uint8_t local_fd_index =  3 ;		//skip the stdin stdout stderr
	while (local_fd_index < MAX_FILES_OPEN_PER_PROC){
		if (cur->fd_table[local_fd_index] == -1){
			cur->fd_table[local_fd_index] = global_fd_index;
			break;
		}
		local_fd_index++;
	}
	if (local_fd_index == MAX_FILES_OPEN_PER_PROC){
		printk("exceed max open files_per_proc\n");
		return -1;
	}
	return local_fd_index;
}


//allocate inode bitmap
int32_t inode_bitmap_alloc(struct partition* part){
	int32_t index = bitmap_scan(&part->inode_bitmap, 1);
	if (index == -1){
		return -1;
	}
	bitmap_set(&part->inode_bitmap , index , 1);
	return index;
}

//allocate  data block  (we set block == sector == 512byte)
int32_t block_bitmap_alloc(struct partition* part){
	int32_t index = bitmap_scan(&part->block_bitmap , 1);
	if (index = -1){
		return -1;
	}
	bitmap_set(&part->block_bitmap , index , 1);
	return (part->sb->data_start_lba + index);
}

//synchronous the 512 byte in memory where 'bit_idx' at  into hard disk
void bitmap_sync(struct partition* part , uint32_t bit_idx, uint8_t btmp){
	uint32_t off_sec =  bit_idx / 4096;		//sector offset(8 * 512 )
	uint32_t off_size = off_sec * BLOCK_SIZE;	//byte offset
	uint32_t sec_lba ;
	uint8_t* bitmap_off;
	switch(btmp){
		case INODE_BITMAP:
			sec_lba = part->sb->inode_bitmap_lba + off_sec;
			bitmap_off = part->sb->inode_bitmap.bits + off_size;
			break;
		case BLOCK_BITMAP:
			sec_lba = part->sb->block_bitmap_lba + off_sec;
			bitmap_off = part->sb->block_bitmap.bits + off_size;
			break;
	}
	ide_write(part->my_disk , sec_lba , bitmap_off , 1);
}




	



