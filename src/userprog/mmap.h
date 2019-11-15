#ifndef USERPROG_MMAP_H
#define USERPROG_MMAP_H

#include <list.h>
#include "filesys/file.h"

typedef int mapid_t;

struct mmap
{
	mapid_t mapid;
	struct file* file;
	void* addr;
	struct list_elem elem;
};

mapid_t mmap_allocate(struct file* file, void* addr);
void mmap_destroy(void);
struct mmap* mmap_get(mapid_t mapid);
void mmap_writeback(struct file* file, void* addr);

#endif /* userprog/mmap.h */
