#include "ide.h"
#include "debug.h"
#include "stdio-kernel.h"

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
#define BIT_ALT_STAT_BSY 0x80						//hard disk is busy
#define BIT_ALT_STAT_DRDY 0x40						//driver is ready
#define BIT_ALT_STAT_DRQ 0x8						//data transmission

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


uint8_t channel_cnt;							//the channel number calculate by hard disk number
struct ide_channel channels[2];						//two ide channel

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
		channel_no++;						//next channel
	}
	printk("ide_init end\n");
}

