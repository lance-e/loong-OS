#include "fs.h"
#include "file.h"
#include "stdio-kernel.h"
#include "thread.h"
#include "bitmap.h"
#include "super_block.h"
#include "memory.h"
#include "inode.h"
#include "string.h"
#include "dir.h"

extern struct partition* cur_part;


//file table
struct file file_table[MAX_FILE_OPEN];

//get a free slot in "file_table"
int32_t get_free_slot_in_global(void){
	uint32_t index =3 ;
	while (index < MAX_FILE_OPEN){
		if (file_table[index].fd_inode== NULL){
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
	if (index == -1){
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
			bitmap_off = part->inode_bitmap.bits + off_size;
			break;
		case BLOCK_BITMAP:
			sec_lba = part->sb->block_bitmap_lba + off_sec;
			bitmap_off = part->block_bitmap.bits + off_size;
			break;
	}
	ide_write(part->my_disk , sec_lba , bitmap_off , 1);
}



//create file
int32_t file_create(struct dir* parent_dir , char* filename , uint8_t flag){
	void* io_buf = sys_malloc(1024);
	if (io_buf == NULL){
		printk("in file_create: sys_malloc for io_buf failed\n");
		return -1;
	}

	uint8_t rollback_step = 0;			//used for roll back
	
	//allocate for new file
	int32_t inode_no =  inode_bitmap_alloc(cur_part);
	if (inode_no == -1){
		printk("in file_create: inode_bitmap_alloc for inode_no failed\n");
		return -1;
	}

	//this node will used in "file_table" , so must allocate in heap memory
	struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode));
	if (new_file_inode == NULL){
		printk("in file_create: sys_malloc for new_file_inode failed\n");
		rollback_step = 1;
		goto rollback;
	}
	//initial
	inode_init(inode_no,new_file_inode);

	int fd_idx = get_free_slot_in_global();
	if (fd_idx == -1){
		printk("exceed max open file\n");
		rollback_step = 2;
		goto rollback;
	}

	file_table[fd_idx].fd_inode = new_file_inode;
	file_table[fd_idx].fd_pos = 0 ;
	file_table[fd_idx].fd_flag = flag;
	file_table[fd_idx].fd_inode->write_deny = false;

	//add this new dir_entry to parent direcotry
	struct dir_entry new_dir_entry ;
	memset(&new_dir_entry , 0 , sizeof(struct dir_entry));
	create_dir_entry(filename , inode_no , FT_REGULAR, &new_dir_entry);


	//!!!synchronous into hard disk
	
	// a. install new directory entry into parent directory
	if (!sync_dir_entry(parent_dir , &new_dir_entry , io_buf)){
		printk("sync dir_entry to disk failed \n");
		rollback_step = 3;
		goto rollback;
	}

	// b. sync file inode
	memset(io_buf , 0 , 1024);	
	inode_sync(cur_part, new_file_inode , io_buf);
	// c. sync parent directory inode
	memset(io_buf , 0 , 1024);
	inode_sync(cur_part , parent_dir->inode , io_buf);
	// d. sync inode bitmap
	bitmap_sync(cur_part , inode_no , INODE_BITMAP);

	// e. add new open file into open_inode list
	list_append(&cur_part->open_inode, &new_file_inode->inode_tag);
	new_file_inode->i_open_cnts = 1;

	sys_free(io_buf);
	return  pcb_fd_install(fd_idx);

	//roll back
rollback:
	switch(rollback_step){
		case 3:
			memset(&file_table[fd_idx] , 0 , sizeof(struct file));
		case 2:
			sys_free(new_file_inode);
		case 1:
			bitmap_set(&cur_part->inode_bitmap , inode_no , 0 );
			break;
	}
	sys_free(io_buf);
	return -1;
}

	



