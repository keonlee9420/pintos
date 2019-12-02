#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "threads/vaddr.h"
#include "filesys/off_t.h"
#include <list.h>
#include <stdbool.h>
#include <debug.h>

/* Maximun size of physical memory for buffer cache in block sector size */
#define MAX_BUFFER_CACHE_IN_BSSIZE 64

/* Maximun size of physical memory for buffer cache in page size */
#define MAX_BUFFER_CACHE_IN_PGSIZE (MAX_BUFFER_CACHE_IN_BSSIZE * BLOCK_SECTOR_SIZE) / PGSIZE

/* Buffer cache flusher time slice */
#define FLUSH_TIME_SLICE 400

struct buffer_cache
{
    block_sector_t sector;    /* Cached disk sector */
    unsigned offset;          /* Offset of buffer cache in physical memory */
    bool dirty_bit;           /* Dirty bit for write-behind */
    bool ref_bit;             /* Reference bit for second chance replacement algorithm */
    struct list_elem elem;    /* List element */
};

void buffer_cache_init (void);
void buffer_cache_close (void);
void cache_read (block_sector_t sector, block_sector_t next_sector, uint8_t* buffer, size_t size, off_t ofs);
void cache_write (block_sector_t sector, block_sector_t next_sector, const uint8_t* buffer, size_t size, off_t ofs);

#endif /* filesys/cache.h */
