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
	SPAGE_PRESENT, 
	SPAGE_UNLOADED, 
	SPAGE_SWAPOUT, 
	SPAGE_MMAP
};

/* Supplemental page. */
struct spage
{
  void *upage;                  /* Virtual page address */
	enum spage_state status;			/* Virtual page status */ 
	struct hash_elem elem;				/* Hash table element */

	/* Present page */
	uint32_t* pte;								/* Page table entry address */

	/* Unloaded page */							
	char filename[16];						/* Executable file name */
	off_t offset;									/* File reading offset */
	size_t readbyte;							/* File reading size */
	bool writable;								/* Page writable condition */

	/* Swapped-out page */
	block_sector_t sector;				/* First sector of swapped-out disk */

	/* Memory-mapping file */
	struct file* mapfile;
};

/* Supplemental page table functions. */
struct hash* spt_create(void);
void spt_destroy(void);

/* Supplemental page table entry functions */
bool spage_free (void *upage);
struct spage* spage_create (void* upage, const char* file, 
														off_t ofs, size_t read_bytes, bool writable);
void spage_map(void* upage, uint32_t* pte);
void spage_swapout(void* upage);
struct spage* spage_lookup(void* upage);

#endif /* vm/page.h */
