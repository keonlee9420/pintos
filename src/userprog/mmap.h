#ifndef USERPROG_MMAP_H
#define USERPROG_MMAP_H

#include <list.h>

typedef int mapid_t;

struct mmap
{
	mapid_t mapid;
	int fd;
	void* addr;
	struct list_elem elem;
};

mapid_t mmap_allocate(int fd, void* addr);
void* mmap_remove(mapid_t mapid);
void mmap_destroy(void);
struct mmap* mmap_get(mapid_t mapid);

#endif /* userprog/mmap.h */
