#ifndef __FS_DIR_H
#define __FS_DIR_H
#include "stdint.h"
#include "inode.h"
#include "fs.h"
#include "global.h"
#include "ide.h"


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

void open_root_dir(struct partition* part);
struct dir* dir_open(struct partition* part , uint32_t ionde_no);
bool search_dir_entry(struct partition* part , struct dir* pdir , const char* name ,struct dir_entry* dir_e);
void dir_close(struct dir* dir);
void create_dir_entry(char* filename , uint32_t inode_no , uint8_t file_type , struct dir_entry* p_de);
bool sync_dir_entry(struct dir* parent_dir , struct dir_entry* p_de , void* io_buf);
bool delete_dir_entry(struct partition* part , struct dir* pdir , uint32_t inode_no ,void* io_buf);
#endif
