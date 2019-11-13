#include "userprog/mmap.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "userprog/fd.h"

mapid_t 
mmap_allocate(int fd, void* addr)
{
	struct list* maplist = &thread_process()->maplist;
	struct mmap* map = malloc(sizeof(struct mmap));
	map->mapid = list_empty(maplist) ? 0 : 
							 list_entry(list_back(maplist), struct mmap, elem)->mapid + 1;
	map->fd = fd;
	map->addr = addr;
	list_push_back(maplist, &map->elem);

	return map->mapid;
}

void* 
mmap_remove(mapid_t mid)
{
	struct mmap* map = mmap_get(mid);
	void* addr;

	if(map == NULL)
		return NULL;

	/* Delete mmap structure */
	addr = map->addr;
	list_remove(&map->elem);
	free(map);	

	return addr;
}

void 
mmap_destroy(void)
{
	struct list* maplist = &thread_process()->maplist;

	while(!list_empty(maplist))
	{
		struct mmap* map = list_entry(list_pop_front(maplist), struct mmap, elem);
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

