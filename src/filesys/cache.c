#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/palloc.h"
#include <bitmap.h>
#include <string.h>

struct list buffer_cache_list;    /* List for buffer cache element */
void* cache_zone;                 /* Start point of buffer cache in physical memory */
struct bitmap* cache_bmap;        /* Bitmap structure for buffer cache management */

/* Individual locks for each cache slot */
static struct lock cache_system_lock;
static struct lock cache_locks[MAX_BUFFER_CACHE_IN_BSSIZE];

static unsigned get_free_buffer ();
static void* get_physical_addr (unsigned offset);
static void setup_cache (struct buffer_cache* bc, block_sector_t sector, unsigned offset);
static struct buffer_cache* caching (block_sector_t sector);
static struct buffer_cache* evict (block_sector_t sector);

/* Initiate buffer cache */
void
buffer_cache_init ()
{
  list_init (&buffer_cache_list);
  cache_zone = palloc_get_multiple (PAL_ASSERT, MAX_BUFFER_CACHE_IN_PGSIZE);
  cache_bmap = bitmap_create (MAX_BUFFER_CACHE_IN_BSSIZE);
  lock_init (&cache_system_lock);
  /* Individual lock init */
  for (int i = 0; i < MAX_BUFFER_CACHE_IN_BSSIZE; i++)
  {
    lock_init (&cache_locks[i]);
  }
}

/* Get free buffer and return offset */
static unsigned
get_free_buffer ()
{
  return bitmap_scan_and_flip (cache_bmap, 0, 1, true);
}

/* Return actual(physical) address of buffer cache */
static void*
get_physical_addr (unsigned offset)
{
  return (void*) (cache_zone + (BLOCK_SECTOR_SIZE * offset));
}

/* Setup buffer cache with given members and read block data on it */
static void
setup_cache (struct buffer_cache* bc, block_sector_t sector, unsigned offset)
{
  bc->offset = offset;
  bc->sector = sector;
  bc->dirty_bit = false;
  bc->ref_bit = true;

  block_read (fs_device, sector, get_physical_addr(bc->offset));
}

/* Select victim cache and return it after reset with new sector data.
    The replacement is along with second chance algorithm. */
static struct buffer_cache*
evict (block_sector_t sector)
{
  struct list_elem* e = list_begin (&buffer_cache_list);
  struct buffer_cache* bc = NULL;

  while (true)
  {
    bc = list_entry (e, struct buffer_cache, elem);

    if (bc->ref_bit)
    {
      bc->ref_bit = false;
      e = (e == list_end (&buffer_cache_list)) ? list_begin (&buffer_cache_list) : list_next (e);
      continue;
    }

    /* write-behind */
    if (bc->dirty_bit)
      block_write (fs_device, bc->sector, get_physical_addr(bc->offset));

    /* Setup victim cache with new sector data */
    setup_cache (bc, sector, bc->offset);

    break;
  }

  return bc;
}

/* Caching block setcot into buffer cache */
static struct buffer_cache*
caching (block_sector_t sector)
{
  struct buffer_cache* bc;
  bool isFull = bitmap_all (&cache_bmap, 0, bitmap_size (cache_bmap));

  if (isFull)
    bc = evict (sector);
  else
  {
    bc = malloc (BLOCK_SECTOR_SIZE);
    setup_cache (bc, sector, get_free_buffer ());
    list_push_back (&buffer_cache_list, &bc->elem);
  }

  return bc;
}

/* Find cache with sector data.
    Cache new sector data if there is no previous one. 
    Return the found/new cache. */
struct buffer_cache*
look_up_cache (block_sector_t sector)
{
  struct list_elem* e;
  struct buffer_cache* bc = NULL;

  for (e = list_begin (&buffer_cache_list); e != list_end (&buffer_cache_list);
      e = list_next (e))
  {
    bc = list_entry (e, struct buffer_cache, elem);
    if (bc->sector == sector)
      break;
  }

  /* If there is no previous cache, then cache it */
  if (bc == NULL)
  {
    lock_acquire (&cache_system_lock);
    bc = caching (sector);
    lock_release (&cache_system_lock);
  }

  return bc;
}