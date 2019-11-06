#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <stdint.h>
#include <hash.h>
#include "threads/synch.h"
#include "vm/swap.h"
#include "filesys/file.h"

/* Supplemental page table status */
enum spage_state
{
	SPAGE_PRESENT, 
	SPAGE_UNLOADED, 
	SPAGE_SWAPOUT
};

/* Supplemental page. */
struct spage
{
  void *upage;                  /* Virtual page address */
	uint32_t pte;									/* Page table element (kpage addr + flags) */
	enum spage_state;							/* Virtual page status */ 
	
	/* Load From File */
	struct file* file;						/* Executable file */
	off_t ofs;										/* Reading offset */

	/* Load from swap */
	struct swap* swap;						/* Swap table element */

	struct hash_elem hash_elem;   /* Hash table element. */
};

/* Supplemental page table functions. */
struct hash* spt_create(void);
void spt_destroy(void);

/* Supplemental page table entry functions */
bool spage_free (void *upage);
bool spage_allocate (void* upage, uint32_t pte);
void spage_out(void* upage);

#endif /* vm/page.h */
