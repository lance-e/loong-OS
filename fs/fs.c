#include "fs.h"
#include "stdint.h"
#include "global.h"
#include "super_block.h"
#include "dir.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "ide.h"
#include "debug.h"
#include "string.h"


extern uint8_t channel_cnt;
extern struct list partition_list;
extern struct ide_channel channels[2];



//format partition , create file system
static void partition_format(struct disk* _hd , struct partition* part){
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
						partition_format(hd , part);
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
}


