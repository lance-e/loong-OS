#include "fs.h"
#include "global.h"
#include "super_block.h"
#include "dir.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "ide.h"
#include "debug.h"
#include "string.h"
#include "file.h"
#include "thread.h"
#include "console.h"


					
extern uint8_t channel_cnt;
extern struct list partition_list;
extern struct ide_channel channels[2];
extern struct file file_table[MAX_FILE_OPEN];
extern struct dir root_dir;


struct partition* cur_part;		//default operation partition 
					

//get the partition named "part_name" from partition_list , and assign to "cur_part"
static bool mount_partition(struct list_elem* pelem , int arg){
	char* part_name = (char*) arg;
	struct partition* part = elem2entry(struct partition , part_tag , pelem);
	if (!(strcmp(part->name , part_name))){			//0 :mean equal
		cur_part = part;
		struct disk* hd = cur_part->my_disk;

		/*********** read the super_block from hard disk into memory **************/

		//sb_buf: storage the super_block read from hard disk
		struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);

		//sb: in memory
		cur_part->sb = (struct super_block*)sys_malloc(SECTOR_SIZE);
		if (cur_part->sb == NULL){
			PANIC("alloc memory failed!");
		}

		//read super_block from hard disk
		memset(sb_buf , 0  , SECTOR_SIZE);
		ide_read(hd , cur_part->start_lba + 1 , sb_buf , 1);

		//copy the information of sb_buf into 'sb'
		//(copy can filterate the unused message)
		memcpy(cur_part->sb , sb_buf , sizeof(struct super_block));	

		/*********** read the block bitmap from hard disk into memory **************/

		cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
		if (cur_part->block_bitmap.bits == NULL){
			PANIC("alloc memory failed!");
		}
		cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE; 

		//read block block from hard disk
		ide_read(hd , sb_buf->block_bitmap_lba , cur_part->block_bitmap.bits , sb_buf->block_bitmap_sects);

		/********** read the inode bitmap from hard disk into memory **************/

		cur_part->inode_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
		if (cur_part->inode_bitmap.bits == NULL){
			PANIC("alloc memory failed!");
		}
		cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;

		//read inode bitmap from hard disk
		ide_read(hd , sb_buf->inode_bitmap_lba , cur_part->inode_bitmap.bits , sb_buf->inode_bitmap_sects);

		/****************/

		list_init(&cur_part->open_inode);
		printk("mount %s done!\n" , part->name);
		
		return true;					//just for "list_traversal": ture that stop

	}
	return false;
}
				


//format partition , create file system
static void partition_format( struct partition* part){
	uint32_t boot_sector_sects = 1;
	uint32_t super_block_sects = 1;
	uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART , BITS_PER_SECTOR);
	uint32_t inode_table_sects = DIV_ROUND_UP( 	\
		       	(sizeof(struct inode) * MAX_FILES_PER_PART) , SECTOR_SIZE );
	uint32_t used_sectors = boot_sector_sects +super_block_sects + inode_bitmap_sects + inode_table_sects;
	uint32_t free_sectors = part->sec_cnt - used_sectors;

	/********************** handle block bitmap sector*******************************/
	uint32_t block_bitmap_sects;
	block_bitmap_sects = DIV_ROUND_UP(free_sectors , BITS_PER_SECTOR);
	uint32_t block_bitmap_bit_len = free_sectors - block_bitmap_sects;	//the bitmap contain itself
	block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len , BITS_PER_SECTOR);
	/********************************************************************************/

	//initial super block
	struct super_block sb;
	sb.magic = 0x12345678; 						//random
	sb.sec_cnt = part->sec_cnt;
	sb.inode_cnt =MAX_FILES_PER_PART;
	sb.part_lba_base = part->start_lba;

	sb.block_bitmap_lba = sb.part_lba_base + 2;			//skip the obr and super_block
	sb.block_bitmap_sects = block_bitmap_sects;

	sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
	sb.inode_bitmap_sects = inode_bitmap_sects;

	sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
	sb.inode_table_sects = inode_table_sects;

	sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
	sb.root_inode_no = 0;
	sb.dir_entry_size  = sizeof(struct dir_entry);

	printk("%s info:\n", part->name);
	printk(" magic:0x%x\n  part_lba_base:0x%x\n	\
			all_sectors:0x%x\n	\
			inode_cnt:0x%x\n	\
			block_bitmap_lba:0x%x\n	\
			block_bitmap_sectors:0x%x\n	\
			inode_bitmap_lba:0x%x\n	\
			inode_bitmap_sectors:0x%x\n	\
			inode_table_lba:0x%x\n	\
			inode_table_sectors:0x%x\n	\
			data_start_lba:0x%x\n"	\
			,sb.magic, sb.part_lba_base, sb.sec_cnt , sb.inode_cnt , 	\
			sb.block_bitmap_lba, sb.block_bitmap_sects , sb.inode_bitmap_lba,	\
			sb.inode_bitmap_sects, sb.inode_table_lba , sb.inode_table_sects,	\
			sb.data_start_lba);
	

	struct disk* hd = part->my_disk;

	/******************** write super block into the 1 sector of this partition ************/

	ide_write(hd , part->start_lba + 1 ,&sb ,1);

	printk("  super_block_lba:0x%x\n" , part->start_lba + 1);


	//get the large size of meta data , as the buffer size
	uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects ? sb.block_bitmap_sects : sb.inode_bitmap_sects);
	buf_size = (buf_size >= sb.inode_table_sects ? buf_size : sb.inode_table_sects) * SECTOR_SIZE;
	uint8_t* buf = (uint8_t*)sys_malloc(buf_size);

	/****************** initial block bitmap , and write into the start lba address *******/

	buf[0] |= 0x01;							//the 0 block is offer to root
	uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
	uint32_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
	
	//other unrelated byte in the last sector less than a sector size
	uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte %  SECTOR_SIZE);	
	
	//initial the other unrelated bit to 1 (avoid use these corresponding resources)
	memset(&buf[block_bitmap_last_byte] , 0xff , last_size);
	uint8_t index = 0;
	while(index <= block_bitmap_last_bit){
		buf[block_bitmap_last_byte] &= ~(1 << index++);
	}
	
	ide_write(hd , sb.block_bitmap_lba , buf , sb.block_bitmap_sects);


	/******************** initial inode bitmap , and write into the start lba address********/

	//clear buffer
	memset(buf , 0 , buf_size);
	buf[0] |= 0x01;
	//inode bitmap sectors = 1, don't have the rest of last sectors,so don't need to handle
	
	ide_write(hd , sb.inode_bitmap_lba , buf , sb.inode_bitmap_sects);

	/******************** initial inode table , and write into the start lab address*********/

	//clear buffer
	memset(buf , 0 , buf_size);
	buf[0] |= 0x01;
	struct inode* i = (struct inode*)buf;
	i->i_size = sb.dir_entry_size * 2;	// "." and ".." 
	i->i_no = 0;				//root occupied  inode 0
	i->i_sectors[0] = sb.data_start_lba;

	ide_write(hd , sb.inode_table_lba , buf , sb.inode_table_sects);

	/*********************** write "." and ".." into root directory *************************/

	memset(buf ,0 , buf_size);
	struct dir_entry* p_de = (struct dir_entry*) buf;

	//initial current directory :"."
	memcpy(p_de->filename, ".", 1);	
	p_de->i_no = 0;
	p_de->f_type = FT_DIRECTORY;
	p_de++;
	
	//initial father direcotry of current direcotry :".."
	memcpy(p_de->filename , ".." , 2);
	p_de->i_no = 0 ;
	p_de->f_type = FT_DIRECTORY;

	//sb.data_start_lba had allocate to root directory , there are dir_entry of root directory
	ide_write(hd ,sb.data_start_lba , buf , 1);

	printk("  root_dir_lba:0x%x\n" , sb.data_start_lba);
	printk("%s format done\n" ,part->name);

	sys_free(buf);
}

//search fs in hd , if not found that format partition and create fs
void filesys_init(void){
	uint8_t channel_no = 0 , dev_no , part_index = 0;

	struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);

	if (sb_buf == NULL){
		PANIC("alloc memory faild!");
	}

	printk("searching filesystem......\n");
	while(channel_no < channel_cnt){
		dev_no = 0 ;
		while(dev_no < 2 ){
			if(dev_no == 0){		//skip hd60M.img
				dev_no++;
				continue;
			}
			struct disk* hd = &channels[channel_no].devices[dev_no];
			struct partition* part = hd->prim_parts;
			while(part_index < 12){
				if (part_index == 4){  	//handle logic partition
					part = hd->logic_parts;
				}

				if(part->sec_cnt != 0 ){
					memset(sb_buf , 0 , SECTOR_SIZE);
					//read out super block , judge the magic number
					ide_read(hd , part->start_lba + 1, sb_buf , 1);

					if (sb_buf->magic == 0x12345678 ){
						printk("%s has filesystem\n",part->name);
					}else {
						printk("formatting %s's partition %s......\n",hd->name, part->name);
						partition_format(part);
					}
				}
				part_index ++;
				part++;			//next partition
			}
			dev_no++;			//next hard disk
		}
		channel_no++;				//next channel
	}
	sys_free(sb_buf);

	//make sure default operate partition
	char default_part[8] = "sdb1";
	//mount partition
	list_traversal(&partition_list , mount_partition ,(int)default_part);

	//open root dir
	open_root_dir(cur_part);
	//inital file_table
	uint32_t fd_index = 0 ;
	while(fd_index  < MAX_FILE_OPEN){
		file_table[fd_index++].fd_inode = NULL;
	}
}



//parse the path :
//eg: "///a/b" , name_store : a , return pathname: /b. 
static char* path_parse(char* pathname , char* name_store){
	if (pathname[0] == '/' ){
		while(*(++pathname) == '/');		//skip lisk this: "////a/b"
	}
	while(*pathname != '/' && *pathname!= 0 ){
		*name_store++ = *pathname++;
	}
	if (pathname[0] == 0){
		return NULL;
	}
	return pathname;
}

//return the depth of path
int32_t path_depth_cnt(char* pathname){
	ASSERT(pathname != NULL);
	char* p = pathname;
	char name[MAX_FILE_NAME_LEN];

	uint32_t depth = 0 ;
	p = path_parse(p , name);
	while (name[0]){
		depth++;
		memset(name , 0 , MAX_FILE_NAME_LEN);
		if (p){					//p != NULL, continue
			p = path_parse( p, name);
		}
	}
	return depth;
}


//search file
static int search_file(const char* pathname , struct path_search_record* searched_record){
	//if search root dir , than direct return
	if (!strcmp(pathname , "/" ) || !strcmp(pathname ,"/.") || !strcmp(pathname , "/..")){
		searched_record->searched_path[0] = 0;			
		searched_record->parent_dir =  &root_dir;
		searched_record->file_type = FT_DIRECTORY;
		return 0 ;
	}
	uint32_t path_len = strlen(pathname);
	ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);

	char* sub_path = (char*) pathname;
	struct dir* parent_dir = &root_dir;
	struct dir_entry dir_e;
	char name[MAX_PATH_LEN] = {0};			//record parsed name after path_parse
	
	searched_record->parent_dir = parent_dir;
	searched_record->file_type = FT_UNKNOWN;
	uint32_t parent_inode_no = 0 ;			//inode number of parent directory
								
	sub_path = path_parse(sub_path, name);
	while (name[0]){
		ASSERT(strlen(searched_record->searched_path) < 512);

		strcat(searched_record->searched_path , "/");
		strcat(searched_record->searched_path , name);
	
		if (search_dir_entry(cur_part , parent_dir , name ,&dir_e)){

			memset(name , 0 , MAX_FILE_NAME_LEN);

			if (sub_path){
				sub_path = path_parse(sub_path , name);
			}
			if (dir_e.f_type == FT_DIRECTORY){		//directory
				parent_inode_no = parent_dir->inode->i_no;
				dir_close(parent_dir);	
				parent_dir = dir_open(cur_part , dir_e.i_no);
				searched_record->parent_dir = parent_dir;
				continue;
			}else if (dir_e.f_type == FT_REGULAR){		//file
				searched_record->file_type = FT_REGULAR;
				return dir_e.i_no;
			}
		}else {	 //not found
			//don't close the parent_dir , will used in create
			return -1;
		}
	}


	//only find directory, so return it's parent directory
	dir_close(searched_record->parent_dir);
	searched_record->parent_dir = dir_open(cur_part , parent_inode_no);
	searched_record->file_type = FT_DIRECTORY;

	return dir_e.i_no;
}

//open or create file . success return file descriptor , fail return -1
int32_t sys_open(const char* pathname ,uint8_t flags){
	if (pathname[strlen(pathname) - 1] == '/' ){
		printk("can't open a directory %s\n", pathname);
		return -1;
	}

	ASSERT(flags <= 7);
	int32_t fd = -1;

	struct path_search_record searched_record ; 
	memset(&searched_record ,  0 , sizeof(struct path_search_record));
	
	int32_t path_depth = path_depth_cnt((char*) pathname);
	
	
	//determine if the file exist
	int inode_no =  search_file(pathname , &searched_record);
	bool found = inode_no == -1 ? false : true;

	if (searched_record.file_type == FT_DIRECTORY){
		printk("can't open a directory with open() , use opendir() to instead\n");
		dir_close(searched_record.parent_dir);
		return -1;
	}

	int32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
	if (path_searched_depth != path_depth){
		printk("cannot access %s: Not a directory , subpath %s is't exist\n",	\
				pathname , searched_record.searched_path);
		dir_close(searched_record.parent_dir);
		return -1;
	}

	if (!found && !(flags & O_CREAT)) {
		printk("in path %s , file %s isn't exist \n" , pathname , 	\
				(strrchr(searched_record.searched_path, '/') + 1));
		dir_close(searched_record.parent_dir);
		return -1;
	}else if (found && flags & O_CREAT){
		printk("%s has already exist!\n" , pathname);
		dir_close(searched_record.parent_dir);
		return -1;
	}

	switch(flags & O_CREAT){
		case O_CREAT:
			printk("creating file\n");
			fd = file_create(searched_record.parent_dir , (strrchr(pathname, '/' ) + 1) , flags);
			dir_close(searched_record.parent_dir);
			break;
		default:
			//other case :O_RDONLY , O_WRONLY ,O_RDWR
			fd = file_open(inode_no , flags);
			
	}

	//thie fd is the pcb's fd_table index , not the index of global file_table
	return fd;
}	
	

//transform file descriptor into index of file_table
static uint32_t fd_local2global(uint32_t fd_local){
	struct task_struct* cur = running_thread();
	int32_t global_fd = cur->fd_table[fd_local];
	ASSERT( global_fd >= 0 && global_fd < MAX_FILE_OPEN);
	return (uint32_t)global_fd;
}


//close file
int32_t  sys_close(int32_t fd){
	int32_t ret = -1;
	if (fd > 2 ){
		uint32_t global_fd = fd_local2global(fd);
		ret = file_close(&file_table[global_fd]);
		running_thread()->fd_table[fd] = -1;
	}
	return ret;
}

//write "count" of  data from "buf" into file
int32_t sys_write(int32_t fd ,const void* buf , uint32_t count){
	if (fd < 0 ){
		printk("sys_write: fd error\n");		
		return -1;
	}
	if (fd == stdout_no){
		char tmp_buf[1024] = {0};
		memcpy(tmp_buf , (char*)buf , count);
		console_put_str(tmp_buf);
		return count;
	}
	uint32_t global_fd = fd_local2global(fd);
	struct file* wr_file = &file_table[global_fd];
	if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR){
		uint32_t bytes_written = file_write(wr_file , buf ,count);
		return bytes_written;
	}else{
		console_put_str("sys_write: not allowed to write file without flag O_WRONLY or O_RDWR\n");
		return -1;
	}

}	

//read "count" of data from file into "buf"
int32_t sys_read(int32_t fd ,void* buf , uint32_t count){
	if (fd < 0){
		printk("sys_read: fd error\n");
		return -1;
	}
	ASSERT(buf != NULL);
	uint32_t global_fd = fd_local2global(fd);
	return file_read(&file_table[global_fd] , buf , count);

}

//reset the offset pointer for file read and write operate
int32_t sys_lseek(int32_t fd , int32_t offset , uint8_t whence){
	if (fd < 0 ){
		printk("sys_lseek: fd error\n");
		return -1;
	}
	ASSERT(whence > 0 && whence <4 );
	uint32_t global_fd = fd_local2global(fd);
	struct file* file = &file_table[global_fd];
	int32_t new_pos = 0 ;
	int32_t file_size = (int32_t)file->fd_inode->i_size;
	switch (whence){
		case SEEK_SET:
			new_pos = offset;
			break;
		case SEEK_CUR:
			new_pos = (int32_t)file->fd_pos + offset;
			break;
		case SEEK_END:			//in this case , offset must less than 0
			new_pos = file_size + offset;
	}
	if (new_pos < 0 || new_pos > (file_size + 1)){
		return -1;	
	}
	file->fd_pos = new_pos;
	return file->fd_pos;
}


//delete file (not directory)
int32_t sys_unlink(const char* pathname){
	//1. to inspect is it exist 
	struct path_search_record search_record;
	memset(&search_record , 0 , sizeof(struct path_search_record));
	int inode_no = search_file(pathname , &search_record);
	ASSERT(inode_no != 0 );
	if (inode_no == -1){
		printk("file %s not found\n" , pathname);
		dir_close(search_record.parent_dir);
		return -1;
	}else if (search_record.file_type == FT_DIRECTORY){
		printk("can't delete directory with sys_unlink() , you should use sys_rmdir() to instead\n");
		dir_close(search_record.parent_dir);
		return -1;
	}

	//2. to inspect is it using
	uint32_t file_idx = 0 ;
	while (file_idx < MAX_FILE_OPEN){
		if (file_table[file_idx].fd_inode != NULL && file_table[file_idx].fd_inode->i_no == (uint32_t)inode_no){
			break;
		}
		file_idx++;
	}

	//mean this file are using 
	if (file_idx < MAX_FILE_OPEN){
		printk("file %s is in use , not allow to delete\n", pathname);
		dir_close(search_record.parent_dir);
		return -1;
	}

	ASSERT(file_idx == MAX_FILE_OPEN);
	void* io_buf = sys_malloc(SECTOR_SIZE + SECTOR_SIZE);
	struct dir* parent_dir = search_record.parent_dir;
	//delete
	delete_dir_entry(cur_part , parent_dir , inode_no , io_buf);
	inode_release(cur_part , inode_no);
	sys_free(io_buf);
	dir_close(search_record.parent_dir);
	return 0;
}

//create directory
int32_t sys_mkdir(const char* pathname){
	uint8_t rollback_step = 0 ;
	void* io_buf = sys_malloc(SECTOR_SIZE * 2);
	if (io_buf == NULL){
		printk("sys_mkdir: sys_malloc for io_buf failed\n");
		return -1;
	}

	//------------------ 1.to find is it exist -------------------------
	
	struct path_search_record searched_record;
	memset(&searched_record , 0 , sizeof(struct path_search_record));
	int inode_no = -1;
	inode_no = search_file(pathname , &searched_record);
	if (inode_no != -1){
		printk("sys_mkdir: file or directory %s exist\n", pathname);
		rollback_step = 1;
		goto rollback;
	}else {
		//not found , but still to judge the subpath is it exist
		uint32_t pathname_depth = path_depth_cnt((char*)pathname);
		uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
		if (pathname_depth != path_searched_depth){
			printk("sys_mkdir: can't access %s : Not a directory ,subpath %s is't exist\n",pathname , searched_record.searched_path);
			rollback_step = 1;
			goto rollback;
		}
	}

	struct dir* parent_dir = searched_record.parent_dir;
	char* dir_name = strrchr(searched_record.searched_path , '/') + 1;

	//------------------  2.create inode for new directory ---------------
	
	inode_no = inode_bitmap_alloc(cur_part);
	if (inode_no == -1){
		printk("sys_mkdir: inode_bitmap_alloc for inode_no failed\n");
		rollback_step = 1;
		goto rollback;
	}
	struct inode new_dir_inode;
	inode_init(inode_no , &new_dir_inode);  //initial
	

	//-------  3.allocate a new block for direcotry entry to storage "." ,".." -----------
	uint32_t block_bitmap_idx = 0 ;
	int32_t block_lba = -1;
	block_lba = block_bitmap_alloc(cur_part);
	if (block_lba == -1){
		printk("sys_mkdir: block_bitmap_alloc for block_lba failed\n");
		rollback_step = 2;
		goto rollback;
	}
	new_dir_inode.i_sectors[0] = block_lba; 	//first directory entry
	//sync in disk
	block_bitmap_idx = block_lba -  cur_part->sb->data_start_lba;
	bitmap_sync(cur_part , block_bitmap_idx , BLOCK_BITMAP);

	//-------- 4.write "." and ".." into first direcotry entry ----------
	memset(io_buf , 0 , SECTOR_SIZE * 2);
	struct dir_entry* p_de = (struct dir_entry*)io_buf;

	memcpy(p_de->filename , "." , 1);
	p_de->i_no = inode_no;
	p_de->f_type = FT_DIRECTORY;
	p_de++;

	memcpy(p_de->filename , "..", 2);
	p_de->i_no = parent_dir->inode->i_no;
	p_de->f_type = FT_DIRECTORY;

	ide_write(cur_part->my_disk , new_dir_inode.i_sectors[0] , io_buf , 1);
	new_dir_inode.i_size = 2 * cur_part->sb->dir_entry_size;

	//-----  5.add direcotry entry of new directory in it's parent direcotry --------
	struct dir_entry new_dir_entry;
	memset(&new_dir_entry , 0 , sizeof(struct dir_entry));
	create_dir_entry(dir_name , inode_no , FT_DIRECTORY , &new_dir_entry);
	memset(io_buf , 0 , SECTOR_SIZE * 2);
	//sync dir_entry in disk
	if (!sync_dir_entry(parent_dir , &new_dir_entry , io_buf)){
		printk("sys_mkdir: sync_dir_entry for new_dir_entry failed\n");
		rollback_step = 2;
		goto rollback;
	}


	//-------------- 6.synchronous all new resours in disk -----------------
	//sync parent dir inode
	memset(io_buf , 0, SECTOR_SIZE * 2);
	inode_sync(cur_part , parent_dir->inode , io_buf);
	//sync new dir inode
	memset(io_buf , 0, SECTOR_SIZE * 2);
	inode_sync(cur_part , &new_dir_inode , io_buf);
	//sync inode block 
	bitmap_sync(cur_part , inode_no , INODE_BITMAP);

	sys_free(io_buf);
	dir_close(searched_record.parent_dir);
	return 0;

rollback:
	switch (rollback_step){
		case 2:
			//rollback the allocate inode 
			bitmap_set(&cur_part->inode_bitmap , inode_no , 0 );

		case 1:
			dir_close(searched_record.parent_dir);
			break;
	}
	sys_free(io_buf);
	return -1;
}

//open directory
struct dir* sys_opendir(const char* name){
	ASSERT(strlen(name) < MAX_PATH_LEN);
	if (name[0]== '/' && (name[1] == 0 || name[1] == '.')){
		return &root_dir;
	}

	//to judge is it exist
	struct path_search_record searched_record;
	struct dir* ret = NULL;
	memset(&searched_record , 0 , sizeof(struct path_search_record));
	int inode_no = -1;
	inode_no = search_file(name , &searched_record);
	if (inode_no == -1){
		printk("In %s , sub path %s not exist \n",name , searched_record.searched_path); 
	}else{
		if (searched_record.file_type == FT_REGULAR){
			printk("%s is regular file\n" , name);
		}else if (searched_record.file_type == FT_DIRECTORY){
			ret = dir_open(cur_part , inode_no);
		}
	}
	dir_close(searched_record.parent_dir);
	return ret;
}

//close directory
int32_t sys_closedir(struct dir* dir){
	int32_t ret = -1;
	if (dir != NULL){
		dir_close(dir);
		ret = 0 ;
	}
	return ret;
}

//read a directory entry at direcotry
struct dir_entry* sys_readdir(struct dir* dir){
	ASSERT(dir != NULL);
	return dir_read(dir);
}


//set directory's pointer "dir_pos" to 0
void sys_rewinddir(struct dir* dir){
	dir->dir_pos = 0 ;
}


//delete empty directory
int32_t sys_rmdir(const char* pathname){
	//first to judge is this path exist
	struct path_search_record searched_record;
	memset(&searched_record , 0 , sizeof(struct path_search_record));
	int inode_no = search_file(pathname , &searched_record);
	ASSERT(inode_no != 0 );
	int32_t ret = -1 ;
	if (inode_no == -1){
		printk("In %s , sub path %s not exist\n", pathname , searched_record.searched_path);
	}else{
		if (searched_record.file_type == FT_REGULAR){
			printk("%s is a regular file\n", pathname);
		}else {
			struct dir* dir = dir_open(cur_part , inode_no);
			if (!dir_is_empty(dir)){
				printk("directory %s is not empty, it is not allowed to delete a nonempty direcotry\n" , pathname);
			}else {
				if (!dir_remove(searched_record.parent_dir , dir)){
					ret = 0 ;
				}
			}
			dir_close(dir);
		}
	}
	dir_close(searched_record.parent_dir);
	return ret;
}


//get parent directory's inode number
static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_nr , void* io_buf){
	struct inode* child_inode = inode_open(cur_part , child_inode_nr );
	uint32_t block_lba = child_inode->i_sectors[0];
	ASSERT(block_lba >= cur_part->sb->data_start_lba );
	inode_close(child_inode);
	ide_read(cur_part->my_disk , block_lba , io_buf , 1);
	struct dir_entry* dir_e = (struct dir_entry*)io_buf; 
	ASSERT(dir_e[1].i_no < 4096 && dir_e[1].f_type == FT_DIRECTORY);
	return dir_e[1].i_no;
}



//find child directory entry's name . from "p_inode_nr" to find "c_inode_nr"
static int get_child_dir_name(uint32_t p_inode_nr , uint32_t c_inode_nr , char* path , void* io_buf){
	struct inode* parent_dir_inode = inode_open(cur_part , p_inode_nr);
	uint8_t block_idx = 0 ;
	uint32_t all_blocks[140] = {0};
	uint32_t block_cnt = 12;
	
	//get all blocks

	while(block_idx < 12){
		all_blocks[block_idx] = parent_dir_inode->i_sectors[block_idx];
		block_idx++;
	}
	if (parent_dir_inode->i_sectors[12]){
		ide_read(cur_part->my_disk , parent_dir_inode->i_sectors[12] , all_blocks + 12, 1);
		block_cnt = 140;
	}
	inode_close(parent_dir_inode);

	struct dir_entry* dir_e = (struct dir_entry*)io_buf;
	uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
	uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;
	block_idx = 0;

	//traversal all block
	while(block_idx < block_cnt){
		if (all_blocks[block_idx]){
			ide_read(cur_part->my_disk , all_blocks[block_idx] , io_buf , 1);
			uint8_t dir_e_idx = 0 ;
			while (dir_e_idx < dir_entrys_per_sec){
				if ((dir_e + dir_e_idx)->i_no == c_inode_nr){
					strcat(path , "/");
					strcat(path , (dir_e+dir_e_idx)->filename);
					return 0;
				}
				dir_e_idx++;
			}
		}
		block_idx++;
	}
	return -1;
}





//get current working directory
char* sys_getcwd(char* buf , uint32_t size){
	//if user process offer a NULL to buf , syscall must malloc for buf
	ASSERT(buf != NULL);
	void* io_buf = sys_malloc(SECTOR_SIZE);
	if (io_buf == NULL){
		return NULL;
	}

	struct task_struct* cur_thread  = running_thread();
	int32_t parent_inode_nr = 0 ;
	int32_t child_inode_nr = cur_thread->cwd_inode_nr;
	ASSERT(child_inode_nr >= 0 && child_inode_nr < 4096);
	
	if (child_inode_nr == 0){
		buf[0] = '/';
		buf[1] = 0;
		return buf;
	}
	memset(buf , 0 ,size);
	char full_path_reverse[MAX_PATH_LEN] = {0}; 	//full path buffer
	
	//from low to high : to find the final parent directory 
	while((child_inode_nr)){
		parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr , io_buf);
		if (get_child_dir_name(parent_inode_nr , child_inode_nr , full_path_reverse, io_buf)== -1){
			sys_free(io_buf);
			return NULL;
		}
		child_inode_nr = parent_inode_nr;
	}
	ASSERT(strlen(full_path_reverse) <= size);

	//now get full path , but the order is reverse
	//---- begin remake full path
	char* last_slash;
	while((last_slash = strrchr(full_path_reverse , '/'))){
		uint16_t len =strlen(buf);
		strcpy(buf + len , last_slash);
		*last_slash = 0 ;
	}
	sys_free(io_buf);
	return buf;
}


//change current working directory to target path
int32_t sys_chdir(const char* path){
	int32_t ret = -1;
	struct path_search_record searched_record;
	memset(&searched_record , 0 , sizeof(struct path_search_record));
	int inode_no = search_file(path , &searched_record);
	if (inode_no != -1 ){
		if (searched_record.file_type == FT_DIRECTORY){
			running_thread()->cwd_inode_nr = inode_no;
			ret = 0;
		}else {
			printk("sys_chdir: %s is regular file or other!\n" , path);
		}
	}
	dir_close(searched_record.parent_dir);
	return ret;
}


//get status of file
int32_t sys_stat(const char* path , struct stat* buf){
	if (!strcmp(path , "/" ) || !strcmp(path , "/.") || !strcmp(path , "/..")){
		buf->st_filetype = FT_DIRECTORY;
		buf->st_ino = 0 ;
		buf->st_size = root_dir.inode->i_size;
		return 0 ;
	}

	int32_t ret = -1;
	struct path_search_record searched_record;
	memset(&searched_record , 0 , 1);
	int inode_no = search_file(path , &searched_record);
	if (inode_no != -1){
		struct inode* obj_inode = inode_open(cur_part , inode_no);
		buf->st_size = obj_inode->i_size;
		inode_close(obj_inode);
		buf->st_filetype = searched_record.file_type;
		buf->st_ino = inode_no;
		ret = 0 ;
	}else {
		printk("sys_stat: can't search %s\n" , path);
	}
	return ret;
}
