#ifndef FILESYS_FREE_MAP_H
#define FILESYS_FREE_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/block.h"

void free_map_init (void);
void free_map_read (void);
void free_map_create (void);
void free_map_open (void);
void free_map_close (void);

/* Project4 S */
block_sector_t free_map_allocate (void);
void free_map_release (block_sector_t);
/* Project4 E */

#endif /* filesys/free-map.h */
