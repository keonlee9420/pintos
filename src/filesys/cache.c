#include "filesys/cache.h"
#include <string.h>
#include <bitmap.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
/* For Debugging */
#include <stdio.h>

static struct list cache_list;
static void* buffer_cache;
static struct bitmap* cache_bmap;
static struct lock cache_lock;

#define MAX_CACHE_SIZE 64

static struct cache* allocate_cache(block_sector_t sector);
static struct cache* scan_cache(block_sector_t sector);

void 
cache_init(void)
{
	list_init(&cache_list);
	lock_init(&cache_lock);
	buffer_cache = palloc_get_multiple(PAL_ASSERT, MAX_CACHE_SIZE * BLOCK_SECTOR_SIZE / PGSIZE);
	cache_bmap = bitmap_create(MAX_CACHE_SIZE);
}

/* Translate buffer position to actual buffer cache address */
static void* 
bufpos_to_addr(size_t bufpos)
{
	return buffer_cache + (BLOCK_SECTOR_SIZE * bufpos);
}

/* Try to read SECTOR in cache into BUFFER by SIZE. 
	 If not exists, cache SECTOR into buffer cache then read */
void 
cache_read(block_sector_t sector, uint8_t* buffer, size_t size)
{
	struct cache* cache;

	lock_acquire(&cache_lock);

	/* Get buffer cache metadata */
	cache = scan_cache(sector);
	if(cache == NULL)
		cache = allocate_cache(sector);

	/* Read from buffer cache into BUFFER */
	memcpy(buffer, bufpos_to_addr(cache->bufpos), size);

	lock_release(&cache_lock);
}

/* Try to write to SECTOR in cache from BUFFER by SIZE. 
	 If not exists, cache SECTOR into buffer cache then write */
void 
cache_write(block_sector_t sector, const uint8_t* buffer, size_t size)
{
	struct cache* cache;

	lock_acquire(&cache_lock);

	/* Get buffer cache metadata */
	cache = scan_cache(sector);
	if(cache == NULL)
		cache = allocate_cache(sector);

	/* Write BUFFER data into buffer cache */
	memcpy(bufpos_to_addr(cache->bufpos), buffer, size);	

	/* Mark as dirty */
	cache->dirty = true;

	lock_release(&cache_lock);
}

static struct cache* evict_cache(void);

/* Scan cache to search empty space.
	 Evict if cache is full, then allocate cache
	 Write sector data into buffer cache at the end */
static struct cache* 
allocate_cache(block_sector_t sector)
{
	struct cache* cache;
	size_t cache_pos;	

	ASSERT(lock_held_by_current_thread(&cache_lock));
	
	cache_pos = bitmap_scan_and_flip(cache_bmap, 0, 1, false);
	
	/* Evict if cache is full */
	if(cache_pos == BITMAP_ERROR)
		cache = evict_cache();
	else
	{
		cache = malloc(sizeof(struct cache));
		cache->bufpos = cache_pos;
	}
	cache->dirty = false;
	cache->sector = sector;
	block_read(fs_device, sector, bufpos_to_addr(cache->bufpos));
	list_push_back(&cache_list, &cache->elem);

	return cache;
}

/* Return position of block evicted.
	 Write back to disk when block content is dirty. */
static struct cache* 
evict_cache(void)
{
	struct cache* cache;	

	ASSERT(bitmap_all(cache_bmap, 0, MAX_CACHE_SIZE));
	ASSERT(list_size(&cache_list) == MAX_CACHE_SIZE);
	ASSERT(lock_held_by_current_thread(&cache_lock));

	/* 이 부분에서 evict policy 바꾸면서 해봐도 될 꺼같음 */
	cache = list_entry(list_pop_front(&cache_list), struct cache, elem);
	if(cache->dirty)
		block_write(fs_device, cache->sector, bufpos_to_addr(cache->bufpos));

	return cache;
}

/* Scan buffer cache header list
	 Return cache struct if sector is in cache, NULL otherwise */
static struct cache* 
scan_cache(block_sector_t sector)
{
	struct cache* cache;
	struct list_elem* e;

	ASSERT(lock_held_by_current_thread(&cache_lock));

	for(e = list_begin(&cache_list); e != list_end(&cache_list); 
			e = list_next(e))
	{
		cache = list_entry(e, struct cache, elem);
		if(cache->sector == sector)
			return cache;
	}

	return NULL;
}


