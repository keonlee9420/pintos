#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <list.h>
#include "filesys/off_t.h"
#include "devices/block.h"

struct cache
{
	block_sector_t sector;	/* Cached disk sector */
	unsigned bufpos;		/* Cached position in buffer cache */
	bool dirty;				/* Dirty bit */
	bool ref;				/* Reference bit */
	struct list_elem elem;	/* List element */
};

void cache_init(void);

void cache_read(block_sector_t sector, uint8_t* buffer, 
								size_t size, off_t ofs, block_sector_t next_sector);
void cache_write(block_sector_t sector, const uint8_t* buffer, 
								 size_t size, off_t ofs, block_sector_t next_sector);
void cache_delete(block_sector_t sector);
void cache_writeback(void);
void cache_install(block_sector_t sector);

#endif /* filesys/cache.h */
