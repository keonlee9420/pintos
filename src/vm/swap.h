#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"

void swap_init(void);

void* swap_out(void);
void swap_in(void* upage);
void swap_free(block_sector_t sector);

#endif /* vm/swap.h */
