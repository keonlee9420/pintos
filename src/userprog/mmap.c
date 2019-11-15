#include "userprog/mmap.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "userprog/fd.h"
#include "vm/page.h"
#include <stdio.h>

mapid_t 
mmap_allocate(struct file* file, void* addr)
{
	struct list* maplist = &thread_process()->maplist;
	struct mmap* map = malloc(sizeof(struct mmap));
	map->mapid = list_empty(maplist) ? 0 : 
							 list_entry(list_back(maplist), struct mmap, elem)->mapid + 1;
	map->file = file;
	map->addr = addr;
	list_push_back(maplist, &map->elem);

	return map->mapid;
}

void 
mmap_destroy(void)
{
	struct list* maplist = &thread_process()->maplist;

	while(!list_empty(maplist))
	{
		struct mmap* map = list_entry(list_pop_front(maplist), struct mmap, elem);

		/* Write back to file */
		mmap_writeback(map->file, map->addr);
	
		/* Close file */
		lock_acquire(&filesys_lock);
		file_close(map->file);
		lock_release(&filesys_lock);

		/* Free mmap structure */
		free(map);
	}
}

struct mmap* 
mmap_get(mapid_t mapid)
{
	struct list* maplist = &thread_process()->maplist;
	struct list_elem* e;

	for(e = list_begin(maplist); e != list_end(maplist); 
			e = list_next(e))
	{
		struct mmap* map = list_entry(e, struct mmap, elem);
		if(map->mapid == mapid)
			return map;
	}
	return NULL;
}

void 
mmap_writeback(struct file* file, void* addr)
{
	int fsize;
	struct hash* spt = thread_current()->spt;
	uint32_t* pd = thread_current()->pagedir;

	ASSERT(file != NULL);

	lock_acquire(&filesys_lock);
	fsize = (int)file_length(file);
	lock_release(&filesys_lock);

	while(fsize > 0)
	{
		struct spage* spage = spage_lookup(spt, addr);
		if(pagedir_get_page(pd, addr) != NULL && pagedir_is_dirty(pd, addr))
		{
			lock_acquire(&filesys_lock);
			file_write_at(file, addr, spage->readbyte, spage->offset);
			lock_release(&filesys_lock);
			pagedir_clear_page(pd, addr);
		}
		spage_free(addr);
		
		/* Advance */
		fsize -= PGSIZE;
		addr += PGSIZE;
	}
}

