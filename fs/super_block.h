#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H
#include "stdint.h"
//super block
struct super_block {
	uint32_t magic;							//magic number , mean different fs

	uint32_t sec_cnt;						//the all sectors in this partition
	uint32_t inode_cnt;						//the all inode in this partition
	uint32_t part_lba_base;						//the start lba address of in this partition

	uint32_t block_bitmap_lba;					//the start lba address of block bitmap 
	uint32_t block_bitmap_sects;					//the number of sectors 

	uint32_t inode_bitmap_lba;					//the start lba address of inode bitmap
	uint32_t inode_bitmap_sects;					//the number of sectors

	uint32_t inode_table_lba;					//the start lba address of inode table
	uint32_t inode_table_sects;					//the number of sectors

	uint32_t data_start_lba;					//the start lba address of first sector in data area
	uint32_t root_inode_no;						//the inode number of root 
	uint32_t dir_entry_size;					//size of directory

	uint8_t pad[460];						//gather enough 512 byte (1 sector)
} __attribute__ ((packed));

#endif
