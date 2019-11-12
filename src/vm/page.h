#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <stdint.h>
#include <hash.h>
#include "threads/synch.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "devices/block.h"

/* Supplemental page table status */
enum spage_state
{
	SPAGE_LOAD, 
	SPAGE_SWAP, 
	SPAGE_MMAP,
	SPAGE_STACK
};

/* Supplemental page. */
struct spage
{
  void *upage;                  /* Virtual page address */
	enum spage_state status;			/* Virtual page status */ 
	struct hash_elem elem;				/* Hash table element */

	/* Present page */
	void* kpage;									/* kpage: Pseudo-Physical page address */

	/* Load page & Memory-mapped file */							
	char filename[16];						/* Executable file name : Load */
	struct file* mapfile;					/* Memory-mapped file : MMap */
	off_t offset;									/* File reading offset */
	size_t readbyte;							/* File reading size */
	bool writable;								/* Page writable condition */

	/* Swapped-out page */
	block_sector_t sector;				/* First sector of swapped-out disk */
};

/* Supplemental page table functions. */
struct hash* spt_create(void);
void spt_destroy(void);

/* Supplemental page table entry functions */
bool spage_free (void *upage);
struct spage* spage_create (void* upage, const char* file, 
														off_t ofs, size_t read_bytes, bool writable);
void spage_map(void* upage, void* kpage);
void spage_swapout(void* upage);
struct spage* spage_lookup(void* upage);

#endif /* vm/page.h */
