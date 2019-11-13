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
	struct thread* victim = frame->user;
	void* upage_victim = frame->upage;
	void* kpage = frame->kpage;
	uint32_t* pd_victim = victim->pagedir;
	struct spage* spage_victim = spage_lookup(victim->spt, upage_victim);

	static block_sector_t next_sector = 0;
	
	/* If out page is dirty, swap out to disk */
	if(pagedir_is_dirty(pd_victim, upage_victim) || spage_victim->status == SPAGE_STACK)
	{
		/* Swap out */
		struct block* block = block_get_role(BLOCK_SWAP);
		int i;
		lock_acquire(&block_lock);
		spage_victim->sector = next_sector;
		spage_victim->status = SPAGE_SWAP;
		for(i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
		{
			const void* buf = kpage + BLOCK_SECTOR_SIZE * i; //upage? kpage?
			block_write(block, next_sector++, buf);
		}
		lock_release(&block_lock);
	}
	spage_victim->kpage = NULL;
	pagedir_clear_page(pd_victim, upage_victim);

	return kpage;
}
	
void 
swap_in(void* upage)
{
	struct block* block = block_get_role(BLOCK_SWAP);
	struct spage* spage = spage_lookup(thread_current()->spt, upage);
	block_sector_t sector = spage->sector;
	int i;
	uint8_t* kpage = frame_allocate(PAL_ZERO);

	for(i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
	{
		void* buf = kpage + BLOCK_SECTOR_SIZE * i;
		block_read(block, sector + i, buf);
	}	

	if(!process_install_page(upage, kpage, spage->writable))
	{
		frame_free(kpage);
		thread_exit();
	}
}
