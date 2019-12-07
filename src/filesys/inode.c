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
#include <limits.h>
/* Project4 E */

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define INDIRECT_MAGIC 0x4d2265d6

/* Project4 S */
/* Max size of on-disk inode in sectors. */
#define INODE_MAX_SECTOR 16524

/* Max number of int-size(4 bytes) elements in a single sector. */
#define INT_PER_SECTOR 128

/* Upper bounds. */
#define DIRECT_LIMIT 12
#define SINGLE_INDIRECT_LIMIT 140

/* On-disk inode (UNIX UFS).
   Must be exactly BLOCK_SECTOR_SIZE bytes long. 
   The capacity of an single inode could be up to 8,460,288 byts long. 
   (8,460,288 byts = INODE_MAX_SECTOR sectors ~= 8MB = 16,384 sectors = 8,388,608 bytes) */
struct inode_disk
  {
    block_sector_t direct_sectors[12];              /* Direct data. */
    block_sector_t single_indirect;                 /* Sector number of single indirect data. */
    block_sector_t double_indirect;                 /* Sector number of double indirect data. */
    block_sector_t parent;							            /* parent directory sector number */
    off_t length;                                   /* File size in bytes. */
    unsigned magic;                                 /* Magic number. */
    uint32_t unused[111];                           /* Not used. */
  };

/* On-disk single indirect inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long.
   The capacity is up to 128 sectors long. */
struct indirect_disk
  {
    block_sector_t sector[INT_PER_SECTOR]; 		     /* Sector numbers of direct data. */
  };

static block_sector_t extend_inode(struct inode_disk* idisk, 
																	 block_sector_t isector, off_t pos);
static block_sector_t get_sector(const struct inode_disk* idisk, off_t pos);
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

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return get_sector (&inode->data, pos);
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;
/* Project4 S */
static struct lock inodes_lock;
/* Project4 E */

/* Initializes the inode module. */
void
inode_init (void) 
{
  ASSERT (sizeof(struct inode_disk) == BLOCK_SECTOR_SIZE);
  ASSERT (sizeof(struct indirect_disk) == BLOCK_SECTOR_SIZE);

  list_init (&open_inodes);
  /* Project4 S */
	lock_init(&inodes_lock);
  /* Project4 E */
}

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

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      /* Project4 S */
			off_t i;

      disk_inode->length = length;
      disk_inode->parent = parent_sector;
      disk_inode->magic = INODE_MAGIC;

      /* Attach each indirect sector to disk inode. */
			for(i = 0; i < DIRECT_LIMIT; i++)
				disk_inode->direct_sectors[i] = UINT_MAX;
      disk_inode->single_indirect = UINT_MAX;
      disk_inode->double_indirect = UINT_MAX;

			lock_acquire(&inodes_lock);
			for(i = 0; i < length; i += BLOCK_SECTOR_SIZE)
				extend_inode(disk_inode, sector, i);

			block_write(fs_device, sector, disk_inode);
			lock_release(&inodes_lock);
			success = true;

			/* Project4 E */
			free (disk_inode);
    }
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
  off_t i;
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

			/* Writeback and delete buffer cache */
			for(ofs = 0; ofs < inode->data.length; ofs += BLOCK_SECTOR_SIZE)
			{
				block_sector_t sector = byte_to_sector(inode, ofs);
				cache_delete(sector);
			}

      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
					/* Deallocate file blocks */
          for (i = 0; i < inode->data.length; i += BLOCK_SECTOR_SIZE)
            free_map_release (get_sector(&inode->data, i));
					/* Deallocate indirect inodes */
					if(inode->data.double_indirect != UINT_MAX)
					{
						struct indirect_disk* inode_d = malloc(BLOCK_SECTOR_SIZE);
						block_read(fs_device, inode->data.double_indirect, inode_d);
						for(i = 0; i < INT_PER_SECTOR; i++)
							if(inode_d->sector[i] != UINT_MAX)
								free_map_release(inode_d->sector[i]);
						free_map_release(inode->data.double_indirect);
						free(inode_d);
					}
					if(inode->data.single_indirect != UINT_MAX)
						free_map_release(inode->data.single_indirect);
					/* Deallocate inode */
          free_map_release (inode->sector);
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
			if(sector_idx == UINT_MAX)
				memset(buffer + bytes_read, 0, chunk_size);
			/* Read sector with buffer cache */
			else
				cache_read(sector_idx, buffer + bytes_read, chunk_size, sector_ofs, next_sector);
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
	if(offset + size > inode->data.length)
		inode->data.length = offset + size;
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

      /* Bytes left in sector. */
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;

      /* Project4 S */
      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < sector_left ? size : sector_left;
      if (chunk_size <= 0)
      	break;

			if(sector_idx == UINT_MAX)
				sector_idx = extend_inode(&inode->data, inode->sector, offset);

			/* Write into buffer cache */
			cache_write(sector_idx, buffer + bytes_written, chunk_size, sector_ofs, next_sector);
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

/* Add index into inode as active section
	 Return allocated sector */
static block_sector_t 
extend_inode(struct inode_disk* idisk, block_sector_t isector, off_t pos)
{
	off_t index = pos / BLOCK_SECTOR_SIZE;
	block_sector_t sector = UINT_MAX;
	char* zeros = calloc(BLOCK_SECTOR_SIZE, 1);

	if(index < DIRECT_LIMIT)
	{
		if(idisk->direct_sectors[index] == UINT_MAX)
		{
			sector = idisk->direct_sectors[index] = free_map_allocate();
			block_write(fs_device, sector, zeros);
		}
	}
	else if(index < SINGLE_INDIRECT_LIMIT)
	{
		size_t idx_single = index - DIRECT_LIMIT;
		/* Set single indirect inode */
		struct indirect_disk* single = malloc(BLOCK_SECTOR_SIZE);
		if(idisk->single_indirect == UINT_MAX)
		{
			idisk->single_indirect = free_map_allocate();
			memset(single, 0xff, BLOCK_SECTOR_SIZE);
		}
		else
			block_read(fs_device, idisk->single_indirect, single);
		/* Extend index */
		if(single->sector[idx_single] == UINT_MAX)
		{
			sector = single->sector[idx_single] = free_map_allocate();
			block_write(fs_device, sector, zeros);
			block_write(fs_device, idisk->single_indirect, single);
		}
		free(single);
	}
	else
	{
		size_t idx_double = (index - SINGLE_INDIRECT_LIMIT) / INT_PER_SECTOR;
		size_t idx_single = (index - SINGLE_INDIRECT_LIMIT) % INT_PER_SECTOR;
		struct indirect_disk* indir_d = malloc(BLOCK_SECTOR_SIZE);
		struct indirect_disk* indir_s = malloc(BLOCK_SECTOR_SIZE);
		/* Access double indirect inode */
		if(idisk->double_indirect == UINT_MAX)
		{
			idisk->double_indirect = free_map_allocate();
			memset(indir_d, 0xff, BLOCK_SECTOR_SIZE);
		}
		else
			block_read(fs_device, idisk->double_indirect, indir_d);
		/* Access second-rank inode */
		if(indir_d->sector[idx_double] == UINT_MAX)
		{
			indir_d->sector[idx_double] = free_map_allocate();
			memset(indir_s, 0xff, BLOCK_SECTOR_SIZE);
		}
		else
			block_read(fs_device, indir_d->sector[idx_double], indir_s);
		/* Extend index */
		if(indir_s->sector[idx_single] == UINT_MAX)
		{
			sector = indir_s->sector[idx_single] = free_map_allocate();
			block_write(fs_device, indir_d->sector[idx_double], indir_s);
			block_write(fs_device, idisk->double_indirect, indir_d);
			block_write(fs_device, sector, zeros);
		}
		free(indir_s);
		free(indir_d);
	}
	/* Free resource then return sector */
	block_write(fs_device, isector, idisk);
	free(zeros);
	return sector;
}

static block_sector_t 
get_sector(const struct inode_disk* idisk, off_t pos)
{
	off_t index = pos / BLOCK_SECTOR_SIZE;

	if(index < DIRECT_LIMIT)
		return idisk->direct_sectors[index];
	else if(index < SINGLE_INDIRECT_LIMIT)
	{
		block_sector_t sector;
		struct indirect_disk* indir_s;
		size_t idx_s = index - DIRECT_LIMIT;

		if(idisk->single_indirect == UINT_MAX)
			return UINT_MAX;

		indir_s = malloc(BLOCK_SECTOR_SIZE);
		block_read(fs_device, idisk->single_indirect, indir_s);
		
		sector = indir_s->sector[idx_s];
		free(indir_s);
		return sector;
	}
	else
	{
		block_sector_t sector = UINT_MAX;
		size_t idx_s = (index - SINGLE_INDIRECT_LIMIT) % INT_PER_SECTOR;
		size_t idx_d = (index - SINGLE_INDIRECT_LIMIT) / INT_PER_SECTOR;
		struct indirect_disk* indir_s = malloc(BLOCK_SECTOR_SIZE);
		struct indirect_disk* indir_d = malloc(BLOCK_SECTOR_SIZE);
		
		if(idisk->double_indirect == UINT_MAX)
			goto done;
		block_read(fs_device, idisk->double_indirect, indir_d);
		
		if(indir_d->sector[idx_d] == UINT_MAX)
			goto done;
		block_read(fs_device, indir_d->sector[idx_d], indir_s);

		sector = indir_s->sector[idx_s];
		done:
			free(indir_s);
			free(indir_d);
			return sector;
	}
}
/* Project4 E */
