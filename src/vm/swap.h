#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <debug.h>
#include <stdint.h>
#include <list.h>
#include <hash.h>

struct swap
{
	void* upage;
	struct block* block;
  struct list_elem elem;
};

void swap_init(void);

#endif /* vm/swap.h */
