#include "filesys/free-map.h"
#include <bitmap.h>
#include <debug.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
/* Project4 S */
#include "threads/malloc.h"
#include "threads/synch.h"
#include <stdio.h>
/* Project4 E */

static struct file *free_map_file;   /* Free map file. */
static struct bitmap *free_map;      /* Free map, one bit per sector. */
/* Project4 S */
static struct lock free_map_lock;
/* Project4 E */

/* Initializes the free map. */
void
free_map_init (void) 
{
  free_map = bitmap_create (block_size (fs_device));
  if (free_map == NULL)
    PANIC ("bitmap creation failed--file system device is too large");
  bitmap_mark (free_map, FREE_MAP_SECTOR);
  bitmap_mark (free_map, ROOT_DIR_SECTOR);
	/* Project4 S */
	lock_init(&free_map_lock);
	/* Project4 E */
}

/* Project4 S */
/* Allocate a single sector from the free map. 
   Returns SECTOR if successful, -1 otherwise. */
block_sector_t
free_map_allocate (void)
{
  block_sector_t sector = -1;
  block_sector_t* sectors = free_map_allocate_multiple (1);
  if (sectors)
   sector = sectors[0];
  free (sectors);
  return sector;
}

/* Allocates CNT sectors from the free map and stores
   all indices(all aloocated sector numbers) into SECTORS.
   Returns SECTORS if successful, NULL if not enough
   sectors were available or if the free_map file could not be
   written. 
   The caller must free SECTORS after using it. */
block_sector_t*
free_map_allocate_multiple (size_t cnt)
{
  size_t i;
  bool success = true;
  block_sector_t* sectors;

  if (cnt <= 0)
   return NULL;

  sectors = (block_sector_t*)malloc (sizeof (block_sector_t) * cnt);
	lock_acquire(&free_map_lock);
  for (i = 0; i < cnt; i++)
  {
    block_sector_t sector = bitmap_scan_and_flip (free_map, 0, 1, false);
    if (sector != BITMAP_ERROR
      && free_map_file != NULL
      && !bitmap_write (free_map, free_map_file))
    {
      sector = BITMAP_ERROR;
    }
    if (sector != BITMAP_ERROR)
      sectors[i] = sector;
    else
    {
      success = false;
      break;
    }
  }
	lock_release(&free_map_lock);

  /* If fails, release resources and return NULL. */
  if (!success)
  {
    size_t j;
		lock_acquire(&free_map_lock);
    for (j = 0; j <= i; j++)
    {
      bitmap_reset (free_map, sectors[j]);
      free (sectors);
    }
		lock_release(&free_map_lock);
    return NULL;
  }
  return sectors;
}

/* Makes a SECTOR available for use. */
void
free_map_release (block_sector_t sector)
{
	lock_acquire(&free_map_lock);
  ASSERT (bitmap_all (free_map, sector, 1));
  bitmap_reset (free_map, sector);
  bitmap_write (free_map, free_map_file);
	lock_release(&free_map_lock);
}

/* Returns true if sector is marked as allocated, false otherwise. */
bool
free_map_inuse (block_sector_t sector)
{
  return bitmap_all (free_map, sector, 1);
}
/* Project4 E */

/* Opens the free map file and reads it from disk. */
void
free_map_open (void) 
{
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_read (free_map, free_map_file))
    PANIC ("can't read free map");
}

/* Writes the free map to disk and closes the free map file. */
void
free_map_close (void) 
{
  file_close (free_map_file);
}

/* Creates a new free map file on disk and writes the free map to
   it. */
void
free_map_create (void) 
{
  /* Create inode. */
	/* Project4 S */
  if (!inode_create (FREE_MAP_SECTOR, bitmap_file_size (free_map), 0))
    PANIC ("free map creation failed");
	/* Project4 E */

  /* Write bitmap to file. */
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_write (free_map, free_map_file))
    PANIC ("can't write free map");
}
