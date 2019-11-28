#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "threads/vaddr.h"
#include <list.h>
#include "devices/block.h"
#include <stdbool.h>

/* Maximun size of physical memory for buffer cache in block sector size */
#define MAX_BUFFER_CACHE_IN_BSSIZE 64

/* Maximun size of physical memory for buffer cache in page size */
#define MAX_BUFFER_CACHE_IN_PGSIZE (MAX_BUFFER_CACHE_IN_BSSIZE * BLOCK_SECTOR_SIZE) / PGSIZE

struct buffer_cache
{
    block_sector_t sector;    /* Cached disk sector */
    unsigned offset;          /* Offset of buffer cache in physical memory */
    bool dirty_bit;           /* Dirty bit for write-behind */
    bool ref_bit;             /* Reference bit for second chance replacement algorithm */
    struct list_elem elem;    /* List element */
};

void buffer_cache_init ();
struct buffer_cache* look_up_cache (block_sector_t sector);

#endif /* filesys/cache.h */
