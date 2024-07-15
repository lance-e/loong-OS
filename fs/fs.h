#ifndef __FS_FS_H
#define __FS_FS_H

#define MAX_FILES_PER_PART 4096					//max file number in per partition
#define BITS_PER_SECTOR 4096					//bits in per sector
#define SECTOR_SIZE 512						//size of sector
#define BLOCK_SIZE SECTOR_SIZE					//size of block

//file type
enum file_types{
	FT_UNKNOWN,						//unsupport
	FT_REGULAR,						//common file
	FT_DIRECTORY						//directory
};

void filesys_init(void);
#endif
