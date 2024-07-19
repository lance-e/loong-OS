#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "stdint.h"
#include "list.h"
#include "global.h"
#include "ide.h"

//inode struct
struct inode{
	uint32_t i_no;							//the inode number
	uint32_t i_size;						//size of file or directory

	uint32_t i_open_cnts;						//the times of opened
	bool write_deny;						//can't write file concurrence(test this before process write )
	uint32_t i_sectors[13];						//0~11 is direct block pointer,12 is indirect block pointer
	struct list_elem inode_tag;				
};

void inode_sync(struct partition* part, struct inode* inode , void* io_buf);
struct inode* inode_open(struct partition* part , uint32_t inode_no);
void inode_close(struct inode* inode);
void inode_init(uint32_t inode_no , struct inode* new_inode);
void inode_delete(struct partition* part , uint32_t inode_no , void* io_buf);
void inode_release(struct partition* part , uint32_t inode_no);
#endif 
