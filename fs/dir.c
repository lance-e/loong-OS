#include "dir.h"
#include "inode.h"
#include "memory.h"
#include "string.h"
#include "stdio-kernel.h"
#include "super_block.h"
#include "debug.h"
#include "file.h"


extern struct partition* cur_part;

struct dir root_dir;				//root direcotory

//open root directory
void open_root_dir(struct partition* part){
	root_dir.inode = inode_open(part , part->sb->root_inode_no);
	root_dir.dir_pos = 0;
}


//open dir by "inode_no"
struct dir* dir_open(struct partition* part , uint32_t inode_no){
	struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
	pdir->inode = inode_open(part ,inode_no);
	pdir->dir_pos =  0;
	return pdir;
}


//search the "name" in direcotory "pdir" at partition "part" , and save at dir_entry "dir_e"
bool search_dir_entry(struct partition* part , struct dir* pdir , const char* name , struct dir_entry* dir_e){
	uint32_t block_cnt = 140;		//12 + 128
	uint32_t* all_blocks = (uint32_t*)sys_malloc(48 + 512);		//storage all block  pointer
	if (all_blocks == 0 ){
		printk("search_dir_entry : sysmalloc for all_blocks failed\n");
		return false;
	}

	uint32_t block_idx =  0 ;
	while (block_idx < 12){
		all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
		block_idx ++;
	}

	block_idx = 0 ;

	if (pdir->inode->i_sectors[12] != 0){			//indirect block exists
		ide_read(part->my_disk , pdir->inode->i_sectors[12] , all_blocks+12 , 1);
	}

	//now all_block had storage all sectors used in this file / directory
	
	uint8_t* buf = (uint8_t *) sys_malloc(SECTOR_SIZE);
	struct dir_entry* p_de = (struct dir_entry*)buf;
	uint32_t dir_entry_size = part->sb->dir_entry_size;	
	uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;	//the number of dir_entry in a sector

	//begin to search in all blocks
	while(block_idx < block_cnt){
		if (all_blocks[block_idx] == 0){		//empty
			block_idx++;
			continue;
		}
		//not empty , read this sector
		ide_read(part->my_disk , all_blocks[block_idx] , buf , 1);

		uint32_t dir_entry_idx = 0;
		//traversal all the directory entry in this sector
		while(dir_entry_idx < dir_entry_cnt){
			if (!strcmp(p_de->filename , name)){
				memcpy(dir_e , p_de , dir_entry_size);
				sys_free(buf);
				sys_free(all_blocks);
				return true;
			}
			dir_entry_idx++;
			p_de++;
		}	
		block_idx++;
		p_de = (struct dir_entry*)buf;
		memset(buf , 0 , SECTOR_SIZE);
	}
	sys_free(buf);
	sys_free(all_blocks);
	return false;
}
	

//close dir
void dir_close(struct dir* dir){
	if (dir == &root_dir){
		return ;
	}
	inode_close(dir->inode);
	sys_free(dir);
}

//initial directory entry in memory
void create_dir_entry(char* filename , uint32_t inode_no ,uint8_t file_type, struct dir_entry* p_de){
	ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);
	memcpy(p_de->filename , filename , strlen(filename));
	p_de->i_no = inode_no;
	p_de->f_type = file_type;
}

//write directory entry into parent directory
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de ,void* io_buf){
	struct inode* dir_inode = parent_dir->inode;
	uint32_t dir_size = dir_inode->i_size;
	uint32_t dir_entry_size  = cur_part->sb->dir_entry_size;

	ASSERT(dir_size % dir_entry_size == 0 );

	uint32_t dir_entrys_per_sec = (512 / dir_entry_size);

	int32_t block_lba = -1; 		//record block lba address

	uint8_t block_idx = 0;
	uint32_t all_blocks[140] = {0};		// storage 12 direct block and 128 indirect block 

	//12 direct block save in "all_blocks"
	while(block_idx < 12){
		all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
		block_idx++;
	}

	//dir_e : use to tarversal the directory entry in sector
	struct dir_entry* dir_e = (struct dir_entry*)io_buf;

	int32_t block_bitmap_idx = -1;

	//begin to traversal all block to find free space 
	block_idx = 0;
	while (block_idx < 140){
		block_bitmap_idx = -1;
		if (all_blocks[block_idx]==0){
			block_lba  =  block_bitmap_alloc(cur_part);
			if (block_lba == -1){
				printk("alloc block bitmap for sync_dir_entry failed\n");
				return false;
			}
			//sync after allocate block
			block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
			ASSERT(block_bitmap_idx != -1);
			bitmap_sync(cur_part , block_bitmap_idx , BLOCK_BITMAP);
			
			block_bitmap_idx = - 1;
			if (block_idx < 12){		//direct block
				dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;	
			}else if(block_idx == 12){	//not allocate first level indirect block table (12 is mean the 0 th indirect block)
				dir_inode->i_sectors[12]  = block_lba;	//give to block table  
				
				//allocate a new block for indirect block
				block_lba = -1;
				block_lba = block_bitmap_alloc(cur_part);
				if (block_lba == -1){	//allocate failed , restore before data
					block_bitmap_idx = dir_inode->i_sectors[12] - 	\
							   cur_part->sb->data_start_lba;
					bitmap_set(&cur_part->block_bitmap , block_bitmap_idx , 0 );
					dir_inode->i_sectors[12] = 0 ;
					printk("alloc block bitmap for sync_dir_entry failed \n");
					return false;
				}
				//sync after allocate block
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				ASSERT(block_bitmap_idx != -1);
				bitmap_sync(cur_part , block_bitmap_idx , BLOCK_BITMAP);

				all_blocks[block_idx] = block_lba;
				//write the address of 0th indirect block into indirect block table
				ide_write(cur_part->my_disk , dir_inode->i_sectors[12] , all_blocks + 12 , 1);

			}else {
				all_blocks[block_idx] = block_lba;
				//write the address of indirect block into indirect block table
				ide_write(cur_part->my_disk , dir_inode->i_sectors[12] , all_blocks + 12, 1);
			}

			//write the new directory entry "p_de" into the new allocate block
			memset(io_buf , 0 , 512);	//clear
			memcpy(io_buf , p_de , dir_entry_size);
			ide_write(cur_part->my_disk,   all_blocks[block_idx] , io_buf , 1);
			dir_inode->i_size += dir_entry_size;
			return true;

		}
		//block already exist ,read into memory ,and find empty directory entry in it
		ide_read(cur_part->my_disk , all_blocks[block_idx] , io_buf , 1);
		
		uint8_t dir_entry_idx = 0 ;
		while(dir_entry_idx < dir_entrys_per_sec){
			if((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN){
				memcpy(dir_e + dir_entry_idx , p_de, dir_entry_size);
				ide_write(cur_part->my_disk , all_blocks[block_idx] , io_buf , 1);
				dir_inode->i_size += dir_entry_size;
				return true;
			}				
			dir_entry_idx++;
		}
		block_idx++;
	}
	printk("directory is fall!\n");
	return false;
}

//delete directory entry which inode_no is "inode_no" from parent directory "pdir"
bool delete_dir_entry(struct partition* part , struct dir* pdir , uint32_t inode_no ,void* io_buf){
	struct inode* dir_inode = pdir->inode;	
	uint32_t block_idx = 0 ;
	uint32_t all_blocks[140] = {0};
	
	//-------------  collect all block address into "all_blocks" ---------------
	
	while (block_idx < 12){
		all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
		block_idx++;
	}

	if (pdir->inode->i_sectors[12]){
		ide_read(part->my_disk , pdir->inode->i_sectors[12] , all_blocks+12 , 1);
	}

	uint32_t dir_entry_size = part->sb->dir_entry_size;
	uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;
	struct dir_entry* dir_e = (struct dir_entry*)io_buf;
	struct dir_entry* dir_entry_found = NULL;
	uint32_t dir_entry_idx , dir_entry_cnt ;	//idx used to traversal , cnt used to judge is it need release  block
	bool is_dir_first_block = false;		// first block in a directory
	
	//------------  traversal all block , to get directory entry --------------
	block_idx = 0 ;
	while(block_idx < 140){
		is_dir_first_block = false;
		if (all_blocks[block_idx] == 0){
			block_idx++;
			continue;
		}
		dir_entry_idx = dir_entry_cnt = 0 ;
		//read sector
		memset(io_buf , 0 , SECTOR_SIZE);
		ide_read(part->my_disk , all_blocks[block_idx] , io_buf , 1);
		//traversal all directory entry in a sector
		while (dir_entry_idx < dir_entrys_per_sec){
			if ((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN){
				if (!strcmp((dir_e + dir_entry_idx)->filename , ".")){
					is_dir_first_block = true;
				}else if (strcmp((dir_e + dir_entry_idx)->filename , "." ) && strcmp((dir_e + dir_entry_idx)->filename , ".." )){
					dir_entry_cnt++;	//count the number of dir_entry
					if ((dir_e + dir_entry_idx)->i_no == inode_no){
						ASSERT(dir_entry_found == NULL); //inode_no only one
						dir_entry_found = dir_e + dir_entry_idx;
					}
				}
			}
			dir_entry_idx++;
		}

		//if not found target directory entry , traversal next block
		if (dir_entry_found == NULL){
			block_idx ++;
			continue;
		}

		//if found target directory entry
		ASSERT(dir_entry_cnt >= 1);

		if (dir_entry_cnt == 1 && !is_dir_first_block){ 	//clear dir_e and release block
			// 1. delete in block bitmap
			uint32_t block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
			bitmap_set(&part->block_bitmap , block_bitmap_idx , 0 );
			bitmap_sync(cur_part , block_bitmap_idx , BLOCK_BITMAP);

			// 2.delete in inode's i_sectors 
			if (block_idx < 12){
				all_blocks[block_idx] = 0;
			}else {
				uint32_t indirect_block_idx = 12 ;
				uint32_t indirect_block_cnt = 0 ; // if only a indirect block ,will release the indirect block table's block
				while(indirect_block_idx < 140){
					if (all_blocks[indirect_block_idx] != 0){
						indirect_block_cnt ++;
					}
				}
				ASSERT(indirect_block_cnt >= 1);
				if (indirect_block_cnt > 1){
					all_blocks[indirect_block_idx]  = 0 ;
					ide_write(part->my_disk , dir_inode->i_sectors[12] , all_blocks+12 , 1);
				}else { 
					all_blocks[indirect_block_idx]  = 0 ;
					ide_write(part->my_disk , dir_inode->i_sectors[12] , all_blocks+12 , 1);
					uint32_t block_bitmap_idx = all_blocks[12] - part->sb->data_start_lba;
					bitmap_set(&part->block_bitmap , block_bitmap_idx , 0 );
					bitmap_sync(cur_part , block_bitmap_idx , BLOCK_BITMAP);
					dir_inode->i_sectors[12] = 0;
				}
			}
		}else { //only clear directory entry
			memset(dir_entry_found , 0 , dir_entry_size);
			ide_write(part->my_disk , all_blocks[block_idx] , io_buf , 1);
		}

		//update inode and sync in disk
		ASSERT(dir_inode->i_size >= dir_entry_size);
		dir_inode->i_size -= dir_entry_size;
		memset(io_buf , 0 , SECTOR_SIZE * 2);
		inode_sync(cur_part , dir_inode , io_buf);

		return true;

	}
	//not found target direcotry entry in all sector
	return false;
}


//read directory: read a directory entry
struct dir_entry* dir_read(struct dir* dir){
	struct dir_entry* dir_e = (struct dir_entry*)dir->dir_buf;	
	struct inode* dir_inode = dir->inode;
	uint32_t all_blocks[140] = {0};
	uint32_t block_cnt = 12;
	uint32_t block_idx = 0;
	uint32_t dir_entry_idx = 0 ;

	//get all blocks 
	while(block_idx < 12){
		all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
		block_idx++;
	}
	if(dir_inode->i_sectors[12] != 0){
		ide_read(cur_part->my_disk , dir_inode->i_sectors[12] , all_blocks+12 , 1);
		block_cnt = 140;
	}
	block_idx = 0 ;
	
	uint32_t cur_dir_entry_pos = 0 ;	//record current offset of directory entry
	uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
	uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;
	
	//traversal all blocks
	while (dir->dir_pos < dir_inode->i_size){
		if (dir->dir_pos >= dir_inode->i_size){
			return NULL;
		}
		if (all_blocks[block_idx] == 0 ){
			block_idx++;
			continue;
		}
		memset(dir_e , 0 , SECTOR_SIZE);
		ide_read(cur_part->my_disk , all_blocks[block_idx]  , dir_e , 1);
		dir_entry_idx = 0 ;
		//traversal all directory entry in this sector
		while(dir_entry_idx < dir_entrys_per_sec){
			if((dir_e + dir_entry_idx)->f_type){
				if (cur_dir_entry_pos < dir->dir_pos){
					cur_dir_entry_pos += dir_entry_size;
					dir_entry_idx ++;
					continue;
				}
				ASSERT(cur_dir_entry_pos == dir->dir_pos);
				dir->dir_pos += dir_entry_size;
				return dir_e + dir_entry_idx;
			}
			dir_entry_idx++;
		}
		block_idx++;
	}
	return NULL;
}

//judge the directory is it empty
bool dir_is_empty(struct dir* dir){
	struct inode* dir_inode = dir->inode;
	return (dir_inode->i_size == 2 * cur_part->sb->dir_entry_size);
}


//remove directory from parent directory
int32_t dir_remove(struct dir* parent_dir , struct dir* child_dir){
	struct inode* child_dir_inode = child_dir->inode;	
	uint32_t block_idx =  1;
	while (block_idx < 13){ 	//only have a block at sector[0] , other must 0 
		ASSERT(child_dir->inode->i_sectors[block_idx] == 0);
		block_idx++;
	}
	void* io_buf = sys_malloc(SECTOR_SIZE * 2);
	if (io_buf == NULL){
		printk("dir_remove: sys_malloc for io_buf failed\n");
		return -1;
	}
	//delete directory entry
	delete_dir_entry(cur_part , parent_dir , child_dir_inode->i_no , io_buf);
	//release inode
	inode_release(cur_part , child_dir_inode->i_no);
	sys_free(io_buf);
	return 0;
}
