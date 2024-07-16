#ifndef __FS_FILE_H
#define __FS_FILE_H
#include "stdint.h"
#include "ide.h"
int32_t get_free_slot_in_global(void);
int32_t pcb_fd_install(int32_t global_fd_index);
int32_t inode_bitmap_alloc(struct partition* part);
void bitmap_sync(struct partition* part , uint32_t bit_idx , uint8_t btmp);

#endif
