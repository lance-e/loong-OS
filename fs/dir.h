#ifndef __FS_DIR_H
#define __FS_DIR_H
#include "stdint.h"
#include "inode.h"
#include "fs.h"


#define MAX_FILE_NAME_LEN 16						//max length of file

//directory struct (unused in hard disk , just use in memory)
struct dir{
	struct inode* inode;
	uint32_t dir_pos;						//the offset of pos (the offset of dir_entry index)
	uint32_t dir_buf[512];						//data cathe in diretory
};

//directory entry struct 
struct dir_entry{
	char filename[MAX_FILE_NAME_LEN];				//file or directory name
	uint32_t i_no;							//inode number
	enum file_types f_type;						//file type
};

#endif