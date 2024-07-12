#include "ide.h"
#include "debug.h"
#include "stdio-kernel.h"
#include "timer.h"
#include "stdio.h"


//define the port of all register of device
#define reg_data(channel) (channel->port_base + 0 )
#define reg_error(channel) (channel->port_base + 1 )
#define reg_sect_cnt(channel) (channel->port_base + 2 )
#define reg_lba_l(channel) (channel->port_base + 3 )
#define reg_lba_m(channel) (channel->port_base + 4 )
#define reg_lba_h(channel) (channel->port_base + 5 )
#define reg_dev(channel) (channel->port_base + 6 )
#define reg_status(channel) (channel->port_base + 7 )
#define reg_cmd(channel) (reg_status(channel))
#define reg_alt_status(channel) (channel->port_base + 0x206 )
#define reg_ctl(channel) reg_alt_status(channel)


//some key bit of 'reg_alt_status' register 
#define BIT_STAT_BSY 0x80						//hard disk is busy
#define BIT_STAT_DRDY 0x40						//driver is ready
#define BIT_STAT_DRQ 0x8						//data transmission

//some key bit of 'device' register
#define BIT_DEV_MBS 0xa0						//5 bit and 7 bit is lock to 1
#define BIT_DEV_LBA 0x40
#define BIT_DEV_DEV 0x10

// some operate hard disk command
#define CMD_IDENTIFY 0xec						//identify command
#define CMD_READ_SECTOR 0x20						//read sector command
#define CMD_WRITE_SECTOR 0x30						//write sector command
									

//define the max sector number (for debug)
#define max_lba (( 80 * 1024 * 1024 / 512) - 1)				//only support 80mb hard disk


//record the total extention partition (used in partition_scan)
int32_t ext_lba_base = 0 ;

uint8_t p_no = 0 , l_no = 0 ;						//record the index of main partition and logic partition

struct list partition_list ;						//partition list

uint8_t channel_cnt;							//the channel number calculate by hard disk number
struct ide_channel channels[2];						//two ide channel
									
//partition table entry (16 byte)
struct partition_table_entry{
	uint8_t bootable;
	uint8_t start_head;
	uint8_t start_sec;
	uint8_t start_chs;
	uint8_t fs_type;
	uint8_t end_head;
	uint8_t end_sec;
	uint8_t end_chs;

	uint32_t start_lba;
	uint32_t sec_cnt;
} __attribute__((packed));						//make sure this struct is 16 byte


//boot sector (mbr / ebr)
strucjt boot_sector{
	uint8_t other[446];						//boot code
	struct partition_table_partition partition_bable[4];		//the partition table have 4 entry,total 64 byte
	uint16_t signature;						//the end flag is 0x55 , 0xaa
} __attribute__((packed));



//initial hard disk data struct
void ide_init(){
	printk("ide_init start\n");
	uint8_t hd_cnt = *((uint8_t*)(0x475));				//get the number of hard disk 
	ASSERT(hd_cnt > 0);	
	channel_cnt = DIV_ROUND_UP( hd_cnt , 2 );
	struct ide_channel* channel;
	uint8_t channel_no = 0 ;
	//handle the hard disk in every channel
	while( channel_no < channel_cnt){
		channel = &channels[channel_no];
		sprintf(channel->name , "ide%d", channel_no);
		switch (channel_no){
			case 0 :
				channel->port_base = 0x1f0;
				channel->irq_no = 0x20 + 14;
				break;
			case 1 :
				channel->prot_base = 0x170;
				channel->irq_no = 0x20 + 15;
				break;
		}
		channel->expecting_intr = false;
		lock_init(&channel->lock);
		//initial to 0 . because of driver will do 'sema_down' to block thread
		//until hard word have done , the interrupt handler will do 'sema_up' to wake up thread
		sema_init(&channel->disk_done, 0);			

		register_handler(channel->irq_no , intr_hd_handler);

		channel_no++;						//next channel
	}
	
	printk("ide_init end\n");
}


//select the hard disk to read and write
static void select_disk(struct disk* hd){
	uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
	if (hd->dev_no == 1){
		reg_device |= BIT_DEV_DEV;
	}
	outb(reg_dev(hd->my_channel) , reg_device);
}

//write the start sector address and the number of sector into disk controller
static void select_sector(struct disk* hd ,uint32_t lba, uint8_t sec_cnt){
	ASSERT( lba <= max_lba);
	struct ide_channel* channel = hd->my_channel;

	//write the number of sector 
	outb(reg_sect_cnt(channel) , sec_cnt); 				//if sec_cnt == 0 , mean write 256 sector
	//write the lba address (sector number)
	outb(reg_lba_l(channel) , lba);
	outb(reg_lba_m(channel) , lba >> 8);
	outb(reg_lba_h(channel) , lba >> 16);

	outb(reg_dev(channel) , BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0 ) | lba >> 24 );
}

//send command to channel
static void cmd_out(struct ide_channel* channel , uint8_t cmd ){
	channel->expecting_intr = true;
	outb(reg_cmd(channel) , cmd );
}

// read 'sec_cnt' sector data from hard disk into buffer
static void read_from_sector(struct disk* hd , void* buf , uint8_t sec_cnt){
	uint32_t size_in_byte;
	if (sec_cnt == 0 ){
		size_in_byte = 256 * 512 ;
	}else {
		size_in_byte = sec_cnt * 512;
	}
	insw(reg_data(channel) , buf , size_in_byte / 2 );
}

// write 'sec_cnt' sector data from buffer into hard disk
static void write_to_sector(struct disk* hd , void* buf , uint8-t sec_cnt){
	uint32_t size_in_byte;
	if (sec_cnt == 0 ){
		size_in_byte = 256 * 512 ;
	}else {
		size_in_byte = sec_cnt * 512 ;
	}
	outsw(reg_data(channel) , buf ,size_in_byte / 2);
}


//wait for 30 second
static bool busy_wait(struct disk* hd){
	struct ide_channel* channel = hd->channel;
	uint16_t time_limit = 30 * 1000;				//30 second
	while(time_limit -= 10 >= 0 ){
		if (!(inb(reg_status(channel)) & BIT_STAT_BSY)) {
			return (inb(reg_status(channel)) & BIT_STAT_DRQ);
		}else {
			mtime_sleep(10);
		}
	}
	return false;
}


//read 'sec_cnt' sector from hard disk into buffer
void ide_read(struct disk* hd , uint32_t lba , void* buf, uint32_t  sec_cnt){
	ASSERT(lba <= max_lba);
	ASSERT(sec_cnt > 0 );
	lock_acquire( &hd->channel->lock);

	// 1.select hard disk
	select_disk(hd);

	uint32_t secs_op;						//the number of sector every operate
	uint32_t secs_done = 0;						//the number of sector have done
	while(secs_done < sec_cnt){
		if ((secs_done + 256 ) <= sec_cnt){
			secs_op = 256 ;
		}else {
			secs_op = sec_cnt - secs_done;
		}

		// 2.write the number of sector and the start sector number
		select_sector(hd , lba + secs_done, secs_op);

		// 3.write the command into 'reg_cmd' register
		cmd_out(hd->my_channel , CMD_READ_SECTOR);


		/***************** the time to block itself  *************************
		 *  only can block itself when hard disk are working.
		 *  now the hard disk begin busy , 
		 *  block himself , wait for the interrupt handler to wake up itself
		 */
		sema_down(&hd->my_channel->disk_done);
		
		 /*  ******************************************************************/

		// 4.judge the status of hard disk whether readable or not
		if (!busy_wait(hd)){
			char error[64];
			sprintf(error , "%s read sector %d failed !!!!!\n",hd->name , lba);
			PANIC(error);
		}

		//5.read data into buf
		read_from_sector(hd , (void*)((uint32_t)buf + secs_done * 512) , secs_op);
		secs_done += secs_op;
	}
	lock_release(&hd->my_channel->lock);
}


//write 'sec_cnt' sector from buffer into hard disk
void ide_write(struct disk* hd , uint32_t lba , void* buf , uint32_t sec_cnt){
	ASSERT(lba <= max_lba);
	ASSERT(sec_cnt > 0);
	lock_acquire(&hd->my_channel->lock);

	// 1.select hard disk
	select_disk(hd);
	uint32_t secs_op;
	uint32_t secs_done = 0 ;
	while(secs_done < sec_cnt){
		if ((secs_done + 256 ) <= sec_cnt){
			secs_op = 256;
		}else {
			secs_op = sec_cnt - secs_done;
		}

		// 2.write the number of sector and the start sector number
		select_sector(hd , lba + secs_done , secs_op);

		// 3.write the command into 'reg_cmd' register
		cmd_out(hd->ma_channel , CMD_WRITE_SECTOR);

		// 4.judge the status of hard disk whether readable or not
		if (!busy_wait(hd)){
			char error[64];
			sprintf(error , "%s write sector %d failed !!!!!\n",hd->name , lba);
			PANIC(error);
		}

		// 5.read data into buf
		write_to_sector(hd , (void*)((uint32_t)buf + secs_done * 512) , secs_op);
		
		//block itself when hard disk response
		sema_down(&hd->my_channel->disk_done);
		secs_done +=secs_op;
	}
	lock_release(&hd->my_channel->lock);
}


//hard disk interrupt handler
void intr_hd_hander(uint8_t irq_no){
	ASSERT(irq_no == 0x2e || irq_no == 0x2f);
	uint8_t ch_no =irq_no - 0x2e ;
	struct ide_channel* channel = &channels[ch_no];
	ASSERT(channel->irq_no == irq_no);
	if (channel->expecting_intr){
		channel->expecting_intr = false;
		sema_up(&channel->disk_done);					//wake up driver
		//read the status register , clear this interrupt
		inb(reg_status(channel));
	}
}

	
//swap the position of 'len' byte in 'dst', and save in buffer
static void swap_pairs_bytes(const char* dst, char* buf , uint32_t len){
	uint8_t index ; 
	for (index = 0 ; index < len ; index += 2){
		buf[index + 1] = *dst++;
		buf[index] = *det++;
	}
	buf[index] = '\0';
}

//get the hard disk parameter
static void indentify_disk(struct disk* hd ){
}



