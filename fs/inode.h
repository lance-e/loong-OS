#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "stdint.h"
#include "list.h"

//inode struct
struct inode{
	uint32_t i_no;							//the inode number
	uint32_t i_size;						//size of file or directory

	uint32_t i_open_cnts;						//the times of opened
	bool write_deny;						//can't write file concurrence(test this before process write )
	uint32_t i_sectors[13];						//0~11 is direct block pointer,12 is indirect block pointer
	struct list_elem inode_tag;				
};

#endif 
