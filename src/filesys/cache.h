#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <list.h>
#include "devices/block.h"

struct cache
{
	block_sector_t sector;	/* Cached disk sector */
	unsigned bufpos;				/* Cached position in buffer cache */
	bool dirty;							/* Dirty bit */
	struct list_elem elem;	/* List element */
};

void cache_init(void);
void cache_get_empty(void);

#endif /* filesys/cache.h */
