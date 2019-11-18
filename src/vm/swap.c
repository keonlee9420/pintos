#include "vm/swap.h"
#include <bitmap.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include <stdio.h>

static struct lock swap_lock;
static struct bitmap* swap_bitmap;
static struct block* swap_block;

void 
swap_init(void)
{
	swap_block = block_get_role(BLOCK_SWAP);
	lock_init(&swap_lock);
	swap_bitmap = bitmap_create(block_size(swap_block));
}

/* Get victim frame and its kpage & mapped upage,  
	 check dirty bit, */
void* 
swap_out(void)
{
	struct frame* frame = frame_select_victim();
	struct thread* victim = frame->user;
	void* upage_victim = frame->upage;
	void* kpage = frame->kpage;
	uint32_t* pd_victim = victim->pagedir;
	struct spage* spage_victim = spage_lookup(victim->spt, upage_victim);
	
	/* MMAP: Write back to file */
	if(spage_victim->status == SPAGE_MMAP)
	{
		lock_acquire(&filesys_lock);
		file_write_at(spage_victim->file, kpage, 
									spage_victim->readbyte, spage_victim->offset);
		lock_release(&filesys_lock);
	}

	/* If out page is dirty, swap out to disk */
	else if(pagedir_is_dirty(pd_victim, upage_victim) || 
					spage_victim->status == SPAGE_STACK || 
					spage_victim->status == SPAGE_SWAP)
	{
		/* Swap out */
		int i, block_cnt = PGSIZE / BLOCK_SECTOR_SIZE;
		block_sector_t write_sector;

		lock_acquire(&swap_lock);
		/* Get 8 consecutive sector, marking them as used */
		write_sector = bitmap_scan_and_flip(swap_bitmap, 0, block_cnt, false);
		if(write_sector == BITMAP_ERROR)
		{
			lock_release(&swap_lock);
			thread_exit();
		}

		/* Set victim spage status as swapped out */
		spage_victim->sector = write_sector;
		spage_victim->status = SPAGE_SWAP;
		
		/* Write data into swap disk */
		for(i = 0; i < block_cnt; i++)
		{
			const void* buf = kpage + BLOCK_SECTOR_SIZE * i;
			block_write(swap_block, write_sector + i, buf);
		}
		lock_release(&swap_lock);
	}

	spage_victim->kpage = NULL;
	pagedir_clear_page(pd_victim, upage_victim);

	return kpage;
}
	
void 
swap_in(void* upage)
{
	struct spage* spage = spage_lookup(thread_current()->spt, upage);
	block_sector_t sector = spage->sector;
	int i, block_cnt = PGSIZE / BLOCK_SECTOR_SIZE;
	uint8_t* kpage = frame_allocate(PAL_ZERO);

	lock_acquire(&swap_lock);
	ASSERT(bitmap_all(swap_bitmap, sector, block_cnt));
	/* Mark consecutive sectors as freed */
	bitmap_set_multiple(swap_bitmap, sector, block_cnt, false);
	spage->sector = BITMAP_ERROR;

	/* Read block content into kpage */
	for(i = 0; i < block_cnt; i++)
	{
		void* buf = kpage + BLOCK_SECTOR_SIZE * i;
		block_read(swap_block, sector + i, buf);
	}	
	lock_release(&swap_lock);

	/* Map upage with kpage */
	if(!process_install_page(upage, kpage, spage->writable))
	{
		frame_free(kpage);
		thread_exit();
	}
}

void 
swap_free(block_sector_t sector)
{
	int block_cnt = PGSIZE / BLOCK_SECTOR_SIZE;

	/* Return if not recorded page in sector */
	if(sector == BITMAP_ERROR)
		return;

	lock_acquire(&swap_lock);
	bitmap_set_multiple(swap_bitmap, sector, block_cnt, false);
	lock_release(&swap_lock);
}

