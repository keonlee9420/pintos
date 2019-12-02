#include "filesys/cache.h"
#include <stdint.h>
#include <string.h>
#include <bitmap.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "devices/timer.h"
/* For Debugging */
#include <stdio.h>

#define MAX_CACHE_SIZE 64
#define CACHE_FLUSH_INTERVAL 10

static struct list cache_list;
static void* buffer_cache;
static struct bitmap* cache_bmap;
static struct lock cache_lock;
static struct lock cell_lock[MAX_CACHE_SIZE];
static bool cache_runbit;

static struct cache* allocate_cache(block_sector_t sector);
static struct cache* scan_cache(block_sector_t sector);
static void flush_cache(void* aux UNUSED);

void 
cache_init(void)
{
	int i;

	list_init(&cache_list);
	lock_init(&cache_lock);
	buffer_cache = palloc_get_multiple(PAL_ASSERT, MAX_CACHE_SIZE * BLOCK_SECTOR_SIZE / PGSIZE);
	cache_bmap = bitmap_create(MAX_CACHE_SIZE);

	for(i = 0; i < MAX_CACHE_SIZE; i++)
		lock_init(&cell_lock[i]);

	/* Create Flusher thread */
	thread_create("cache_flusher", PRI_DEFAULT, flush_cache, NULL);
}

/* Translate buffer position to actual buffer cache address */
static inline void* 
bufpos_to_addr(size_t bufpos)
{
	return buffer_cache + (BLOCK_SECTOR_SIZE * bufpos);
}

/* Try to read SECTOR in cache into BUFFER by SIZE. 
	 If not exists, cache SECTOR into buffer cache then read */
void 
cache_read(block_sector_t sector, uint8_t* buffer, size_t size, off_t ofs)
{
	struct cache* cache;

	lock_acquire(&cache_lock);
	/* Get buffer cache metadata */
	cache = scan_cache(sector);
	if(cache == NULL)
		cache = allocate_cache(sector);

	lock_acquire(&cell_lock[cache->bufpos]);
	lock_release(&cache_lock);

	/* Read from buffer cache into BUFFER */
	memcpy(buffer, bufpos_to_addr(cache->bufpos) + ofs, size);
	lock_release(&cell_lock[cache->bufpos]);
}

/* Try to write to SECTOR in cache from BUFFER by SIZE. 
	 If not exists, cache SECTOR into buffer cache then write */
void 
cache_write(block_sector_t sector, const uint8_t* buffer, size_t size, off_t ofs)
{
	struct cache* cache;

	lock_acquire(&cache_lock);
	/* Get buffer cache metadata */
	cache = scan_cache(sector);
	if(cache == NULL)
		cache = allocate_cache(sector);

	lock_acquire(&cell_lock[cache->bufpos]);
	lock_release(&cache_lock);

	/* Mark as dirty */
	cache->dirty = true;

	/* Write BUFFER data into buffer cache */
	memcpy(bufpos_to_addr(cache->bufpos) + ofs, buffer, size);	
	lock_release(&cell_lock[cache->bufpos]);
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
	lock_acquire(&cell_lock[cache->bufpos]);
	block_read(fs_device, sector, bufpos_to_addr(cache->bufpos));
	lock_release(&cell_lock[cache->bufpos]);
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
	lock_acquire(&cell_lock[cache->bufpos]);
	if(cache->dirty)
		block_write(fs_device, cache->sector, bufpos_to_addr(cache->bufpos));
	lock_release(&cell_lock[cache->bufpos]);

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

/* Write-Behind Policy */
/* Remove buffer cache header, with writing back
	 Executed when file is closed */
void 
cache_delete(block_sector_t sector)
{
	struct cache* cache;

	lock_acquire(&cache_lock);
	cache = scan_cache(sector);
	if(cache == NULL)
		goto done;

	/* Reset cache bitmap as empty */
	ASSERT(bitmap_test(cache_bmap, cache->bufpos));
	bitmap_reset(cache_bmap, cache->bufpos);

	/* If cache is modified, write back to block */
	if(cache->dirty)
	{
		lock_acquire(&cell_lock[cache->bufpos]);
		block_write(fs_device, cache->sector, bufpos_to_addr(cache->bufpos));
		lock_release(&cell_lock[cache->bufpos]);
	}

	/* Remove cache metadata */
	list_remove(&cache->elem);
	free(cache);

	done:
		lock_release(&cache_lock);
}

/* Flush entire buffer cache into filesys disk 
	 Write back buffer with dirty bit */
void 
cache_writeback(void)
{
	struct cache* cache;
	
	cache_runbit = false;

	lock_acquire(&cache_lock);
	while(!list_empty(&cache_list))
	{
		cache = list_entry(list_pop_front(&cache_list), struct cache, elem);

		/* Reset cache bitmap as empty */
		ASSERT(bitmap_test(cache_bmap, cache->bufpos));
		bitmap_reset(cache_bmap, cache->bufpos);

		/* If cache is modified, write back to block */
		if(cache->dirty)
			block_write(fs_device, cache->sector, bufpos_to_addr(cache->bufpos));

		/* Remove cache metadata */
		list_remove(&cache->elem);
		free(cache);
	}
	lock_release(&cache_lock);
}

/* Flush cache content into file disk periodically. */
static void 
flush_cache(void* aux UNUSED)
{
	struct cache* cache;
	struct list_elem* e;

	cache_runbit = true;

	while(cache_runbit)
	{
		lock_acquire(&cache_lock);
		for(e = list_begin(&cache_list); e != list_end(&cache_list); 
				e = list_next(e))
		{
			cache = list_entry(e, struct cache, elem);
			if(cache->dirty)
			{
				lock_acquire(&cell_lock[cache->bufpos]);
				block_write(fs_device, cache->sector, bufpos_to_addr(cache->bufpos));
				cache->dirty = false;
				lock_release(&cell_lock[cache->bufpos]);
			}
		}
		lock_release(&cache_lock);

		timer_sleep(CACHE_FLUSH_INTERVAL);
	}
}

/* Read-Ahead Policy */
static void fetch_block(void* aux);

/* Install SECTOR data into buffer cache */
void 
cache_install(block_sector_t sector_)
{
	block_sector_t* sector = malloc(sizeof(block_sector_t));
	*sector = sector_;

	if(sector_ == UINT32_MAX)
		return;
	
	thread_create("read_aheader", PRI_DEFAULT, fetch_block, sector);
}

static void 
fetch_block(void* aux)
{
	struct cache* cache;
	block_sector_t* sector = aux;

	lock_acquire(&cache_lock);
	/* Get buffer cache metadata */
	cache = scan_cache(*sector);
	if(cache == NULL)
		cache = allocate_cache(*sector);
	lock_release(&cache_lock);

	free(sector);
}
