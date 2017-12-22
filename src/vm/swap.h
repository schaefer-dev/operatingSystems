#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"
#include "vm/frame.h"


void vm_swap_init (void);
block_sector_t vm_swap_page(void *phys_addr);
void vm_swap_back(block_sector_t swap_sector, void *phys_addr);
void vm_swap_free(block_sector_t swap_sector);


#endif
