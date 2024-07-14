#include "fs.h"
#include "global.h"
#include "super_block.h"
#include "dir.h"
#include "stdio-kernel.h"
#include "memory.h"





//format partition , create file system
static void partition_format(struct disk* hd , struct partition* part){
	uint32_t boot_sector_sects = 1;
	uint32_t super_block_sects = 1;
	uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART , BITS_PER_SECTOR);
	uint32_t inode_table_sects = DIVE_ROUND_UP( 	\
		       	(sizeof(struct inode) * MAX_FILES_PER_PART) , SECTOR_SIZE );
	uint32_t used_sectors = boot_sector_sects +super_block_sects + inode_bitmap_sects + inode_table_sects;
	uint32_t free_sectors = part->sec_cnt - uses_sectors;

	/********************** handle block bitmap sector*******************************/
	uint32_t block_bitmap_sects;
	block_bitmap_sects = DIV_ROUND_UP(free_sects , BIT_PER_SECTOR);
	uint32_t block_bitmap_len = free_sects - block_bitmap_sects;	//the bitmap contain itself
	block_bitmap_sects = DIV_ROUND_UP(block_bitmap_len , BIT_PER_SECTOR);
	/********************************************************************************/

	//initial super block
	struct super_block sb;
	sb.magic = 0x12345678 						//random
	sb.sec_cnt = part->sec_cnt;
	sb.inode_cnt =MAX_FILE_PER_PART;
	sb.part_lba_part = part->start_lba;

	sb.block_bitmap_lba = sb.part_lba_part + 2;			//skip the obr and super_block
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
			sb.block_bitmap_lba, sb.block_bitmap_sectors , sb.inode_bitmap_lba,	\
			st.inode_bitmap_sectors, sb.inode_table_lba , sb.inode_table_sectors,	\
			sb.data_start_lba);
	

	struct disk* hd = part->my_disk;

	/******************** write super block into the 1 sector of this partition ************/

	ide_write(hd , part->start_lba + 1 ,&sb ,1);

	/**************************************************************************************/
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
	uint32_t last_byte = SECTOR_SIZE - (block_bitmap_last_byte %  SECTOR_SIZE);	
	
	//initial the other unrelated bit to 1 (avoid use these corresponding resources)
	memset(&buf[block_bitmap_last_byte] , 0xff , last_size);
	uint8_t index = 0;
	while(index <= block_bitmap_last_bit){
		buf[block_bitmap_last_byte] &= ~(1 << index++);
	}
	
	ide_write(hd , sb.block_bitmap_lba , buf , sb.block_bitmap_sects);

	/****************************************************************************************/

	/******************** initial inode bitmap , and write into the start lba address********/



}
