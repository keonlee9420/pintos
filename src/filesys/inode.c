#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
/* Project4 S */
#include "filesys/cache.h"
#include "threads/synch.h"
#include <stdio.h>
/* Project4 E */

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Project4 S */
/* Max size of on-disk inode in sectors. */
#define INODE_TOTAL_SECTOR_NUM 16524
const int inode_total_sector_num = 16524;
const int block_sector_size = 512;

/* Max number of int-size(4 bytes) elements in a single sector. */
#define INT_PER_SECTOR 128
const int int_per_sector = 128;

/* Upper bounds. */
#define INODE_END_OF_DIRECT 12
#define INODE_END_OF_SINGLE 140
const int inode_end_of_direct = 12;
const int inode_end_of_single = 140;


/* On-disk inode (UNIX UFS).
   Must be exactly BLOCK_SECTOR_SIZE bytes long. 
   The capacity of an single inode could be up to 8,460,288 byts long. 
   (8,460,288 byts = INODE_TOTAL_SECTOR_NUM sectors ~= 8MB = 16,384 sectors = 8,388,608 bytes) */
struct inode_disk
  {
    block_sector_t direct_sectors[12];              /* Direct data. */
    block_sector_t single_indirect;                 /* Sector number of single indirect data. */
    block_sector_t double_indirect;                 /* Sector number of double indirect data. */
    block_sector_t parent;							            /* parent directory sector number */
    block_sector_t size;                            /* File size in sectors. */
    off_t length;                                   /* File size in bytes. */
    unsigned magic;                                 /* Magic number. */
    uint32_t unused[110];                           /* Not used. */
  };

/* On-disk single indirect inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long.
   The capacity is up to 128 sectors long. */
struct inode_disk_single
  {
    block_sector_t direct_sectors[INT_PER_SECTOR];      /* Sector numbers of direct data. */
  };

/* On-disk doulbe indirect inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long.
   The capacity is up to 128 * 128 sectors long. */
struct inode_disk_double
  {
    block_sector_t single_indirects[INT_PER_SECTOR];    /* Sector numbers of single indirect inode. */
  };

static void inode_disk_create (struct inode_disk* disk_inode, block_sector_t sectors, block_sector_t* allocated_sectors, block_sector_t reserved_sector);
static void inode_disk_extend (struct inode_disk* disk_inode, block_sector_t max_capacity);
static block_sector_t index_to_sector (const struct inode_disk* inode_disk, off_t indexed_sector);
static int check_extensible (off_t offset, off_t size);
/* Project4 E */

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
		/* Project4 S */
		struct lock lock;										/* Inode usage synchronization */
		/* Project4 E */
    struct inode_disk data;             /* Inode content. */
  };

/* Get actual sector number of indexed sector. */
static block_sector_t
index_to_sector (const struct inode_disk* inode_disk, off_t index)
{
  ASSERT (inode_disk != NULL);

  block_sector_t sector = -1;

  size_t i;
  int single_indirect_index = 0;
  for (i = 0; i < inode_disk->size; i++)
  {
    if (i < INODE_END_OF_DIRECT)
    {
      if ((off_t)i == index)
      {
        sector = inode_disk->direct_sectors[i];
        break;
      }
    }
    else if (i >= INODE_END_OF_DIRECT && i < INODE_END_OF_SINGLE)
    {
      if ((off_t)i == index)
      {
        struct inode_disk_single* inode_s = malloc (sizeof (struct inode_disk_single));
        block_read (fs_device, inode_disk->single_indirect, inode_s);
        sector = inode_s->direct_sectors[i - inode_end_of_direct];
        free (inode_s);
        break;
      }
    }
    else
    {
      if ((off_t)i == index)
      {
        struct inode_disk_single* inode_s = malloc (sizeof (struct inode_disk_single));
        struct inode_disk_double* inode_d = malloc (sizeof (struct inode_disk_double));
        block_read (fs_device, inode_disk->double_indirect, inode_d);
        block_read (fs_device, inode_d->single_indirects[single_indirect_index], inode_s);
        sector = inode_s->direct_sectors[i - inode_end_of_single + (single_indirect_index * int_per_sector)];
        free (inode_s);
        free (inode_d);
        break;
      }

      if ((((int)i - inode_end_of_single + 1) / int_per_sector)  == (single_indirect_index + 1))
      {
        /* Advacne. */
        single_indirect_index++;
      }      
    }    
  }
  return sector;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return index_to_sector (&inode->data, pos / BLOCK_SECTOR_SIZE);
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;
/* Project4 S */
static struct lock inodes_lock;
static struct lock free_map_lock;
/* Project4 E */

/* Initializes the inode module. */
void
inode_init (void) 
{
  ASSERT (sizeof(struct inode_disk) == BLOCK_SECTOR_SIZE);
  ASSERT (sizeof(struct inode_disk_single) == BLOCK_SECTOR_SIZE);
  ASSERT (sizeof(struct inode_disk_double) == BLOCK_SECTOR_SIZE);

  list_init (&open_inodes);
  /* Project4 S */
	lock_init(&inodes_lock);
	lock_init(&free_map_lock);
  /* Project4 E */
}

/* Project4 S */
/* Create sectors into inode disk. */
static void
inode_disk_create (struct inode_disk* disk_inode, block_sector_t sectors, block_sector_t* allocated_sectors, block_sector_t reserved_sector)
{
  ASSERT (disk_inode != NULL);
  ASSERT (sectors > 0 && sectors <= INODE_TOTAL_SECTOR_NUM);
  ASSERT (allocated_sectors != NULL);
  
  static char zeros[BLOCK_SECTOR_SIZE];

  /* Allocate memory for single and double indirect of DISK_INODE. */
  struct inode_disk_single* single_indirect = malloc (sizeof (struct inode_disk_single));
  struct inode_disk_double* double_indirect = malloc (sizeof (struct inode_disk_double));

  size_t i;
  int single_indirect_index = 0;
  const size_t END_OF_INODE = sectors - 1;
  for (i = 0; i < sectors; i++)
  {
    if (i < INODE_END_OF_DIRECT)
    {
      /* Allocate into direct data. */
      disk_inode->direct_sectors[i] = allocated_sectors[i];
      block_write (fs_device, allocated_sectors[i], zeros);
    }
    else if (i >= INODE_END_OF_DIRECT && i < INODE_END_OF_SINGLE)
    {
      /* Allocate into single indirect data. */
      single_indirect->direct_sectors[i - inode_end_of_direct] = allocated_sectors[i];
      block_write (fs_device, allocated_sectors[i], zeros);

      /* Block write single indirect at the end. */
      if (i == END_OF_INODE || (i == inode_end_of_single - 1))
      {
        block_write (fs_device, disk_inode->single_indirect, single_indirect);

        /* Empty single indirect. */
        memset (&single_indirect->direct_sectors, -1, sizeof(single_indirect->direct_sectors));
      }
    }
    else
    {
      /* Allocate into double indirect data. */
      int index = i - inode_end_of_single + (single_indirect_index * int_per_sector);
      single_indirect->direct_sectors[index] = allocated_sectors[i];
      block_write (fs_device, allocated_sectors[i], zeros);

      /* Block write after the current single indirect full. */
      if ((i == END_OF_INODE) || ((((int)i - inode_end_of_single + 1) / int_per_sector)  == (single_indirect_index + 1)))
      {
        /* Block write and attach to double indirect. */
        double_indirect->single_indirects[single_indirect_index] = free_map_allocate (reserved_sector);
        block_write (fs_device, double_indirect->single_indirects[single_indirect_index], single_indirect);
        
        /* Empty single indirect. */
        memset (&single_indirect->direct_sectors, -1, sizeof(single_indirect->direct_sectors));

        /* Advacne. */
        single_indirect_index++;

        if (i == END_OF_INODE)
        {
          /* Block write double indirect at the end. */
          block_write (fs_device, disk_inode->double_indirect, double_indirect);
        }
      }      
    }    
  }

  /* Free resources */
  free (single_indirect);
  free (double_indirect);
}
/* Project4 E */

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, block_sector_t parent_sector)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  lock_acquire (&free_map_lock);
  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
			/* Project4 S */
      disk_inode->size = sectors;
			disk_inode->parent = parent_sector;
      disk_inode->magic = INODE_MAGIC;

      /* Attach each indirect sector to disk inode. */
      disk_inode->single_indirect = free_map_allocate (sector);
      disk_inode->double_indirect = free_map_allocate (sector);

      block_sector_t* allocated_sectors;
      if ((allocated_sectors = free_map_allocate_multiple (sectors, sector)))
        {
          if (sectors > 0) 
            inode_disk_create (disk_inode, sectors, allocated_sectors, sector);
          block_write (fs_device, sector, disk_inode);
          success = true; 
        } 
      /* If given file size is zero. */
      else if (length == 0)
        {
          block_write (fs_device, sector, disk_inode);
          success = true; 
        }
      free (allocated_sectors);
      /* Project4 E */
      free (disk_inode);
    }
  lock_release (&free_map_lock);

  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
	/* Project4 S */
	lock_acquire(&inodes_lock);
	/* Project4 E */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
					/* Project4 S */
					lock_release(&inodes_lock);
					/* Project4 E */
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
	/* Project4 S */
	lock_init(&inode->lock);
	lock_release(&inodes_lock);
	/* Project4 E */
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
	/* Project4 S */
	{
		if(inode->removed)
			return NULL;

		lock_acquire(&inode->lock);
    inode->open_cnt++;
		lock_release(&inode->lock);
	}
	/* Project4 E */
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
	if(inode == NULL)
		return -1;
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
	/* Project4 S */
  size_t i;
	off_t ofs;
	/* Project4 E */

  /* Ignore null pointer. */
  if (inode == NULL)
    return;

	/* Project4 S */
	lock_acquire(&inodes_lock);
  /* Release resources if this was the last opener. */
	lock_acquire(&inode->lock);
  if (--inode->open_cnt == 0)
    {
			lock_release(&inode->lock);
			/* Project4 S */
      
			/* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector);
          for (i = 0; i < bytes_to_sectors (inode->data.length); i++)
            free_map_release (index_to_sector (&inode->data, i));
        }

			/* Project4 S */
			/* Writeback and delete buffer cache */
			for(ofs = 0; ofs < inode->data.length; ofs += BLOCK_SECTOR_SIZE)
			{
				block_sector_t sector = byte_to_sector(inode, ofs);
				cache_delete(sector);
			}
			/* Project4 E */

      free (inode); 
    }
	/* Project4 S */
	else
		lock_release(&inode->lock);
	lock_release(&inodes_lock);
	/* Project4 E */
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  lock_acquire(&inode->lock);
  inode->removed = true;
  lock_release(&inode->lock);
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

	/* Project4 S */
	lock_acquire(&inode->lock);
	/* Project4 E */
  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

			/* Project4 S */
			/* Get sector number of next block */
			block_sector_t next_sector = byte_to_sector(inode, offset + BLOCK_SECTOR_SIZE);
			/* Project4 E */

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

			/* Project4 S */
			/* Read sector with buffer cache */
			cache_read(sector_idx, buffer + bytes_read, 
								 chunk_size, sector_ofs, next_sector);
			/* Project4 E */

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
	/* Project4 S */
	lock_release(&inode->lock);
	/* Project4 E */

  return bytes_read;
}

/* Project4 S */
/* Extend inode to MAX_CAPACITY. */
static void
inode_disk_extend (struct inode_disk* disk_inode, block_sector_t max_capacity)
{
  ASSERT (disk_inode != NULL);
  ASSERT (max_capacity > 0 && max_capacity <= INODE_TOTAL_SECTOR_NUM);
  
  static char zeros[BLOCK_SECTOR_SIZE];

  /* Allocate memory for single and double indirect of DISK_INODE. */
  struct inode_disk_single* single_indirect = malloc (sizeof (struct inode_disk_single));
  struct inode_disk_double* double_indirect = malloc (sizeof (struct inode_disk_double));

  size_t i;
  int single_indirect_index = 0;
  const size_t END_OF_INODE = max_capacity - 1;
  for (i = 0; i < max_capacity; i++)
  {
    if (i < INODE_END_OF_DIRECT)
    {
      /* Load existing sector. */
      disk_inode->direct_sectors[i] = index_to_sector (disk_inode, i);
      if (disk_inode->direct_sectors[i] == -1)
      {
        /* Allocate if empty. */
        disk_inode->direct_sectors[i] = free_map_allocate (-1);
        block_write (fs_device, disk_inode->direct_sectors[i], zeros);
      }
    }
    else if (i >= INODE_END_OF_DIRECT && i < INODE_END_OF_SINGLE)
    {
      /* Load existing sector. */
      single_indirect->direct_sectors[i - inode_end_of_direct] = index_to_sector (disk_inode, i);
      if (single_indirect->direct_sectors[i - inode_end_of_direct] == -1)
      {
        /* Allocate if empty. */
        single_indirect->direct_sectors[i - inode_end_of_direct] = free_map_allocate (-1);
        block_write (fs_device, single_indirect->direct_sectors[i - inode_end_of_direct], zeros);
      }

      /* Block write single indirect at the end. */
      if (i == END_OF_INODE || (i == inode_end_of_single - 1))
      {
        block_write (fs_device, disk_inode->single_indirect, single_indirect);

        /* Empty single indirect. */
        memset (&single_indirect->direct_sectors, -1, sizeof(single_indirect->direct_sectors));
      }
    }
    else
    {
      int index = i - inode_end_of_single + (single_indirect_index * int_per_sector);
      /* Load existing sector. */
      single_indirect->direct_sectors[index] = index_to_sector (disk_inode, i);
      if (single_indirect->direct_sectors[index] == -1)
      {
        /* Allocate if empty. */
        single_indirect->direct_sectors[index] = free_map_allocate (-1);
        block_write (fs_device, single_indirect->direct_sectors[index], zeros);
      }

      /* Block write after the current single indirect full. */
      if ((i == END_OF_INODE) || ((((int)i - inode_end_of_single + 1) / int_per_sector)  == (single_indirect_index + 1)))
      {
        /* Block write and attach to double indirect. */
        double_indirect->single_indirects[single_indirect_index] = free_map_allocate (-1);
        block_write (fs_device, double_indirect->single_indirects[single_indirect_index], single_indirect);
        
        /* Empty single indirect. */
        memset (&single_indirect->direct_sectors, -1, sizeof(single_indirect->direct_sectors));

        /* Advacne. */
        single_indirect_index++;

        if (i == END_OF_INODE)
        {
          /* Block write double indirect at the end. */
          block_write (fs_device, disk_inode->double_indirect, double_indirect);
        }
      }      
    }    
  }

  /* Free resources */
  free (single_indirect);
  free (double_indirect);
}

/* Check whether it's possible to extend the inode. 
   Return MAX_CAPACITY when the inode can be extended from OFFSET in SIZE long. 
   Return -1, otherwise. */
static int
check_extensible (off_t offset, off_t size)
{
  int over_ofs = (offset + size) % block_sector_size;
  int over = (offset + size) / block_sector_size;
  int max_capacity = over_ofs ? over + 1 : over;
  if (max_capacity > inode_total_sector_num)
    return -1;
  return max_capacity;
}
/* Project4 E */

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

	/* Project4 S */
	lock_acquire(&inode->lock);
	/* Project4 E */
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

			/* Project4 S */
			/* Get sector number of next block */
			block_sector_t next_sector = byte_to_sector(inode, offset + BLOCK_SECTOR_SIZE);
			/* Project4 E */

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
      {
        int max_capacity;
        /* Check whether it's possible to extend the inode. */
        if ((max_capacity = check_extensible (offset, size)) > 0)
        {
          /* Extend the inode. */
          inode_disk_extend (&inode->data, max_capacity);
  
          /* Update meta data. */
          inode->data.size = max_capacity;
          inode->data.length += size;
          block_write (fs_device, inode->sector , &inode->data);
          continue;
        }
        else
          break;
      } 

			/* Project4 S */
			/* Write into buffer cache */
			cache_write(sector_idx, buffer + bytes_written, 
									chunk_size, sector_ofs, next_sector);
			/* Project4 E */

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
	/* Project4 S */
	lock_release(&inode->lock);
	/* Project4 E */

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
	/* Project4 S */
	lock_acquire(&inode->lock);
  inode->deny_write_cnt++;
	lock_release(&inode->lock);
	/* Project4 E */
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
	/* Project4 S */
	lock_acquire(&inode->lock);
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
	lock_release(&inode->lock);
	/* Project4 E */
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

/* Project4 S */
block_sector_t 
inode_get_parent(const struct inode* inode)
{
	return inode->data.parent;
}
/* Project4 E */
