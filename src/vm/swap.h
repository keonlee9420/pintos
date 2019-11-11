#ifndef VM_SWAP_H
#define VM_SWAP_H

void swap_init(void);

void* swap_out(void);
void swap_in(void* upage);

#endif /* vm/swap.h */
