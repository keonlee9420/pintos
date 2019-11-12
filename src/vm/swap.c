#include "vm/swap.h"
#include <stdio.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "devices/block.h"

static struct lock block_lock;

void 
swap_init(void)
{
	lock_init(&block_lock);
}

/* Get victim frame and its kpage & mapped upage,  
	 check dirty bit,  */
void* 
swap_out(void)
{
	struct frame* frame = frame_select_victim();
	void* upage = frame->upage;
	void* kpage = frame->kpage;
	uint32_t* pd = frame->user->pagedir;
	struct spage* spage = spage_lookup(upage);

	static block_sector_t next_sector = 0;
	
	printf("next sector: %d\n", next_sector);
	/* If out page is dirty, swap out to disk */
	if(pagedir_is_dirty(pd, upage))
	{
		/* Swap out */
		struct block* block = block_get_role(BLOCK_SWAP);
		int i;
		lock_acquire(&block_lock);
		spage->sector = next_sector;
		spage->status = SPAGE_SWAP;
		for(i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
		{
			const void* buf = upage + BLOCK_SECTOR_SIZE * i;
			block_write(block, next_sector++, buf);
		}
		lock_release(&block_lock);
	}
	spage->kpage = NULL;
	pagedir_clear_page(pd, upage);

	return kpage;
}
	
void 
swap_in(void* upage)
{
	struct block* block = block_get_role(BLOCK_SWAP);
	struct spage* spage = spage_lookup(upage);
	block_sector_t sector = spage->sector;
	int i;
	uint8_t* kpage = frame_allocate(PAL_ZERO);
	
	if(kpage == NULL)
		kpage = swap_out();

	for(i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
	{
		void* buf = upage + BLOCK_SECTOR_SIZE * i;
		block_read(block, sector + i, buf);
	}	

	if(!process_install_page(upage, kpage, spage->writable))
	{
		frame_free(kpage);
		thread_exit();
	}

	spage->status = SPAGE_PRESENT;
}
