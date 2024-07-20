#ifndef __FS_FS_H
#define __FS_FS_H
#include "stdint.h"

#define MAX_FILES_PER_PART 4096					//max file number in per partition
#define BITS_PER_SECTOR 4096					//bits in per sector
#define SECTOR_SIZE 512						//size of sector
#define BLOCK_SIZE SECTOR_SIZE					//size of block
#define MAX_PATH_LEN 512

//file type
enum file_types{
	FT_UNKNOWN,						//unsupport
	FT_REGULAR,						//common file
	FT_DIRECTORY						//directory
};

enum oflags{
	O_RDONLY,						//read only , 000b
	O_WRONLY,						//write only, 001b
	O_RDWR,							//read and write , 010b
	O_CREAT = 4						//creat, 100b
};

//offset of file read and write position
enum whence{
	SEEK_SET = 1,
	SEEK_CUR ,
	SEEK_END
};

//record the parent directory
struct path_search_record{
	char searched_path[MAX_PATH_LEN];			//parent path
	struct dir* parent_dir;					//parent directory
	enum file_types file_type;				//file type
};

void filesys_init(void);
int32_t path_depth_cnt(char* pathname);
int32_t sys_open(const char* pathname ,uint8_t flags);
int32_t sys_close(int32_t fd);
int32_t sys_write(int32_t fd ,const void* buf , uint32_t count);
int32_t sys_read(int32_t fd ,void* buf , uint32_t count);
int32_t sys_lseek(int32_t fd , int32_t offset , uint8_t whence);
int32_t sys_unlink(const char* pathname);
int32_t sys_mkdir(const char* pathname);

#endif
