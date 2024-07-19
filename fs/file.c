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
#include "interrupt.h"
#include "global.h"
#include "debug.h"

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
	struct task_struct* cur = running_thread();
	uint32_t* pg_dir_bak = cur->pgdir;
	cur->pgdir =  NULL;

	struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode));
	if (new_file_inode == NULL){
		printk("in file_create: sys_malloc for new_file_inode failed\n");
		rollback_step = 1;
		goto rollback;
	}
	cur->pgdir = pg_dir_bak;

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
	inode_sync(cur_part , parent_dir->inode , io_buf);
	// c. sync parent directory inode
	memset(io_buf , 0 , 1024);	
	inode_sync(cur_part, new_file_inode , io_buf);
	// d. sync inode bitmap
	bitmap_sync(cur_part , inode_no , INODE_BITMAP);

	// e. add new open file into open_inode list
	list_push(&cur_part->open_inode, &new_file_inode->inode_tag);
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
			cur->pgdir = pg_dir_bak;
		case 1:
			bitmap_set(&cur_part->inode_bitmap , inode_no , 0 );
			break;
	}
	sys_free(io_buf);
	return -1;
}

	
//open file
int32_t file_open(uint32_t inode_no , uint8_t flag){
	int32_t fd_index = get_free_slot_in_global();
	if (fd_index == -1){
		printk("exceed max open files\n");
		return -1;
	}
	file_table[fd_index].fd_inode = inode_open(cur_part , inode_no);
	file_table[fd_index].fd_pos = 0;
	file_table[fd_index].fd_flag = flag;
	bool* write_deny = &file_table[fd_index].fd_inode->write_deny;

	if (flag & O_WRONLY || flag & O_RDWR ){
		enum intr_status old_status = intr_disable();
		if (!(*write_deny)){		
			*write_deny = true;
			intr_set_status(old_status);
		}else {		//the file is being written by someone else
			intr_set_status(old_status);
			printk("file can't be write now , try again later\n");
			return -1;
		}
	}
	//if read or create , don't need care "write_deny"
	return pcb_fd_install(fd_index);

}


//close file
int32_t file_close(struct file* file){
	if (file == NULL){
		return -1;
	}
	file->fd_inode->write_deny = false;
	inode_close(file->fd_inode);
	file->fd_inode = NULL;
	return 0;
}

//write "count" data of buffer into file
int32_t file_write(struct file* file , const void* buf , uint32_t count){
	if ((file->fd_inode->i_size + count ) > (BLOCK_SIZE * 140 )){
		//512 * 140 = 71680 , only support 140 block
		printk("exceed max file_size 71680 bytes , write file failed\n");		
		return -1;
	}
	uint8_t* io_buf = sys_malloc(512);
	if (io_buf == NULL){
		printk("file_write: sys_malloc for io_buf failed\n");
		return -1;
	}

	//used for record all block address of file
	uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);
	if (all_blocks == NULL){
		printk("file_write: sys_malloc for all_blocks failed\n");
		return -1;
	}

	int32_t block_lba = -1;			//block address
	uint32_t block_bitmap_idx = 0;		//the index of block_bitmap (used in block_bitmap_sync)
	int32_t indirect_block_table;		//first level indirect block table address
	uint32_t block_idx;			//block index

	const uint8_t* src = buf;		//data of will write
	uint32_t bytes_written = 0;		//record the written data size
	uint32_t size_left = count;		//record the not written data size
	uint32_t sec_idx ;			//sector index	
	uint32_t sec_lba ;			//sector address
	uint32_t sec_off_bytes;			//sector byte offset
	uint32_t sec_left_bytes;		//sector left byte
	uint32_t chunk_size;			//the size of data per write 
	
	//judge is this file first time to write (mean no block , need to allocate a  block)
	if (file->fd_inode->i_sectors[0] == 0 ){
		block_lba = block_bitmap_alloc(cur_part);
		if (block_lba == -1){
			printk("file_write: block_bitmap_alloc for block_lba failed\n");
			return -1;
		}
		file->fd_inode->i_sectors[0] = block_lba;

		//sync block bitmap in disk
		block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
		ASSERT(block_bitmap_idx != 0 );
		bitmap_sync(cur_part , block_bitmap_idx , BLOCK_BITMAP);
	}

	//the number of block already occupied before write
	uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;
	//the number of blocks to be occupied after write 
	uint32_t file_will_used_blocks = (file->fd_inode->i_size + count ) / BLOCK_SIZE + 1;

	ASSERT( file_will_used_blocks < 140);
	//the increment
	uint32_t add_blocks = file_will_used_blocks -  file_has_used_blocks;

	//-------------  collect all block address into "all_blocks" ---------------

	if (add_blocks == 0 ){		//no increment
		if (file_will_used_blocks <= 12){
			block_idx = file_has_used_blocks - 1 ;
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
		}else {
			//if had used the indirect block before write , need read the indirect block first
			ASSERT(file->fd_inode->i_sectors[12] != 0);
			indirect_block_table  =  file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk , indirect_block_table , all_blocks+12 , 1);
		}
		
	}else {		//have increment
		if (file_will_used_blocks <= 12){ 
			//1.get sector with remaining space for continued use , write into "all_blocks"
			block_idx = file_has_used_blocks - 1;
			ASSERT(file->fd_inode->i_sectors[block_idx] != 0 );
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

			//2.allocate sectors will use in future , write into "all_blocks"
			block_idx = file_has_used_blocks;
			while (block_idx < file_will_used_blocks){
				block_lba = block_bitmap_alloc(cur_part);
				if (block_lba == -1){
					printk("file_write: block_bitmap_alloc for situation 1 failed\n");
					return -1;
				}
				ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
				file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;

				//sync block bitmap in disk
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				ASSERT(block_bitmap_idx != 0 );
				bitmap_sync(cur_part , block_bitmap_idx , BLOCK_BITMAP);

				block_idx++;
			}
		}else if (file_will_used_blocks > 12 && file_has_used_blocks <= 12){
			//1.get sector with remaining space for continued use , write into "all_blocks"
			block_idx = file_has_used_blocks - 1;
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

			//2.create the first level indirect block table
			block_lba = block_bitmap_alloc(cur_part);
			if (block_lba == -1){
				printk("file_write: block_bitmap_alloc for situation 2 failed\n");
				return -1;
			}
			ASSERT(file->fd_inode->i_sectors[12] == 0 );
			file->fd_inode->i_sectors[12] = indirect_block_table = block_lba;

			//3.allocate sectors will use in future , write into "all_blocks"
			block_idx = file_has_used_blocks;
			while (block_idx < file_will_used_blocks){
				block_lba = block_bitmap_alloc(cur_part);
				if (block_lba == -1){
					printk("file_write: block_bitmap_alloc for situation 2 failed\n");
					return -1;
				}
				if (block_idx < 12){
					ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
					file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
				}else {
					//indirect block just write into all_blocks, all ok that sync
					all_blocks[block_idx] = block_lba;
				}

				//sync block bitmap in disk
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				bitmap_sync(cur_part , block_bitmap_idx , BLOCK_BITMAP);

				block_idx++;
			}
			//sync all indirect block in disk
			ide_write(cur_part->my_disk , indirect_block_table , all_blocks+12 , 1);
		}else if (file_has_used_blocks > 12){
			ASSERT(file->fd_inode->i_sectors[12] != 0);
			//1.get the address 
			indirect_block_table = file->fd_inode->i_sectors[12];
			//2.get the all indirect block into "all_blocks"
			ide_read(cur_part->my_disk , indirect_block_table , all_blocks+12 , 1);
			//3.allocate sectors will use in future , write into "all_blocks"
			block_idx = file_has_used_blocks;
			while (block_idx < file_will_used_blocks){
				block_lba = block_bitmap_alloc(cur_part);
				if (block_lba == -1){
					printk("file_write: block_bitmap_alloc for situation 3 failed\n");
					return -1;
				}
				all_blocks[block_idx] = block_lba;

				//sync block bitmap in disk
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				bitmap_sync(cur_part , block_bitmap_idx , BLOCK_BITMAP);

				block_idx++;
			}
			//sync all indirect block in disk
			ide_write(cur_part->my_disk , indirect_block_table , all_blocks+12 , 1);
		}
	}

	//------------------------- begin write data --------------------------------

	bool first_write_block = true;		//show is it has remaining space
	file->fd_pos = file->fd_inode->i_size -1;	

	while (bytes_written < count){
		memset(io_buf , 0 , BLOCK_SIZE);
		sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
		sec_lba = all_blocks[sec_idx];
		sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
		sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

		//judge the size of data in this writing
		chunk_size = size_left > sec_left_bytes ? sec_left_bytes : size_left; 
		
		if (first_write_block){
			ide_read(cur_part->my_disk , sec_lba , io_buf , 1);
			first_write_block = false;
		}

		//combinate old data and new data
		memcpy(io_buf + sec_off_bytes , (uint8_t*)src , chunk_size);
		ide_write(cur_part->my_disk , sec_lba , io_buf , 1);
		printk("file write at lba 0x%x\n" , sec_lba);
		src += chunk_size;	//point to next data
		file->fd_inode->i_size += chunk_size; 	//update the size of file
		file->fd_pos += chunk_size;
		bytes_written += chunk_size;
		size_left -= chunk_size;
	}
	
	//synchronous inode in disk

	//buffer may need 2 sector , but "io_buf" just 512 byte , so allocate a new buffer
	uint8_t* sync_buf = sys_malloc(1024);
	if (sync_buf == NULL){
		printk("file_write: sys_malloc for sync_buf failed\n");
		return -1;
	}else {
		inode_sync(cur_part , file->fd_inode , sync_buf);
		sys_free(sync_buf);
	}
	sys_free(all_blocks);
	sys_free(io_buf);
	return bytes_written;
}



//read file
int32_t file_read(struct file* file , void* buf , uint32_t count){
	uint8_t* buf_dst = (uint8_t*)buf;
	uint32_t size = count , size_left = size;

	//if count more than the left size of file , so use the left size as the new targete count
	if ((file->fd_pos + count ) > file->fd_inode->i_size){
		size = file->fd_inode->i_size - file->fd_pos;
		size_left = size;
		if (size == 0 ){		//file is full
			return -1;
		}
	}
	
	uint8_t* io_buf = sys_malloc(512);
	if (io_buf == NULL){
		printk("file_read: sys_malloc for io_buf failed\n");
		return -1;
	}

	//used for record all block address of file
	uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);
	if (all_blocks == NULL){
		printk("file_read: sys_malloc for all_blocks failed\n");
		return -1;
	} 

	uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;
	uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;
	uint32_t read_block = block_read_end_idx - block_read_start_idx;
	
	ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);

	int32_t indirect_block_table;		//first level indirect block table address
	uint32_t block_idx;			//block index
	
	//-------------  collect all block address into "all_blocks" ---------------
	
	if (read_block == 0 ){
		ASSERT(block_read_start_idx == block_read_end_idx);
		if (block_read_end_idx < 12){
			block_idx = block_read_start_idx;
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
		}else{		//need to read all indirect block 
			indirect_block_table = file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk , indirect_block_table , all_blocks+12 , 1);
		}
	}else {
		if (block_read_end_idx < 12){
			block_idx = block_read_start_idx;
			while(block_idx <= block_read_end_idx){
				all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
				block_idx++;
			}
		}else if (block_read_start_idx < 12 && block_read_end_idx > 12){
			block_idx = block_read_start_idx;
			while(block_idx <= 12 ){
				all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
				block_idx++;
			}
			ASSERT(file->fd_inode->i_sectors[12] != 0);
			//read all indirect block
			indirect_block_table = file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk , indirect_block_table , all_blocks+12 , 1);
		}else {
			ASSERT(file->fd_inode->i_sectors[12] != 0);
			//read all indirect block
			indirect_block_table = file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk , indirect_block_table , all_blocks+12 , 1);

		}

	}

	//------------------------- begin read data --------------------------------
	uint32_t sec_idx ;			//sector index	
	uint32_t sec_lba ;			//sector address
	uint32_t sec_off_bytes;			//sector byte offset
	uint32_t sec_left_bytes;		//sector left byte
	uint32_t chunk_size;			//the size of data per write 
	uint32_t bytes_read  = 0 ;		//record the read data size

	while (bytes_read < count){

		memset(io_buf , 0 , BLOCK_SIZE);

		sec_idx = file->fd_pos / BLOCK_SIZE;		//different from write ,here use fd_pos
		sec_lba = all_blocks[sec_idx];
		sec_off_bytes = file->fd_pos  % BLOCK_SIZE;
		sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

		//judge the size of data in this writing
		chunk_size = size_left > sec_left_bytes ? sec_left_bytes : size_left; 
		
		ide_read(cur_part->my_disk , sec_lba , io_buf , 1);

		//combinate old data and new data
		memcpy(buf_dst , io_buf + sec_off_bytes , chunk_size);
	
		buf_dst  += chunk_size;	//point to next data
		file->fd_pos += chunk_size;
		bytes_read += chunk_size;
		size_left -= chunk_size;
	}
	
	
	sys_free(all_blocks);
	sys_free(io_buf);
	return bytes_read;	
}
