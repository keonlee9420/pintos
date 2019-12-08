#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
/* Project4 S */
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
/* Project4 E */

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
		bool deny_write;										/* Not Used: SYnch with struct file */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
		/* Project4 S */
		bool isdir;													/* Directory or file? */
		block_sector_t parent_sector;				/* Parent inode sector */
		/* Project4 E */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry), 0);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (const struct dir *dir) 
{
	if(dir == NULL)
		return NULL;
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode, bool* isdir) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);
	
	/* Project4 S */
	if(!strcmp(name, ".."))
	{
		*inode = inode_open(inode_get_parent(dir_get_inode(dir)));
		*isdir = true;
	}
	else if(!strcmp(name, "."))
	{
		*inode = inode_reopen(dir_get_inode(dir));
		*isdir = true;
	}
  else if (lookup (dir, name, &e, NULL))
	{
    *inode = inode_open (e.inode_sector);
		*isdir = e.isdir;
	}
	/* Project4 E */
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector, bool isdir)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
	/* Project4 S */
  if (*name == '\0' || strlen (name) > NAME_MAX || 
			!strcmp(name, "..") || !strcmp(name, "."))
	/* Project4 E */
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
	/* Project4 S */
	e.isdir = isdir;
	/* Project4 E */
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

	/* Reject root removal */
	if(!strcmp(name, "/"))
		return false;

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

	/* Project4 S */
	/* Check validity of directory entry */
	if(e.isdir)
	{
		struct dir* edir = dir_open(inode);
		char temp[NAME_MAX + 1];
		if(dir_readdir(edir, temp))
		{
			dir_close(edir);
			return false;
		}
	}
	/* Project4 E */

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

/* Project4 S */
/* Change working directory according to given name
	 Change *DIR to one-step before destination file or directory. 
	 Save destination file or directory name into FILENAME */
bool 
dir_chdir(struct dir** dir, const char* name, char* filename)
{
	struct dir* cur_dir = *dir;
	char* name_cp; 
	char* path; 
	char* save_ptr = NULL;
	bool success = false;

	/* Verify input initial directory and input path */
	if(cur_dir == NULL || strlen(name) == 0)
		return false;

	/* Copy entire path */
	name_cp = palloc_get_page(PAL_ZERO);
	if(name_cp == NULL)
		return false;
	strlcpy(name_cp, name, PGSIZE);

	/* Change current directory into root if path is absolute */
	if(name_cp[0] == '/')
	{
		dir_close(cur_dir);
		cur_dir = dir_open_root();
	}

	/* Set initial path */
	path = strtok_r(name_cp, "/", &save_ptr);
	if(strlen(path) > NAME_MAX)
		return false;

	/* Traverse along the path */
	while(path)
	{
		struct dir_entry e;
		char* next = strtok_r(NULL, "/", &save_ptr);

		/* Handle case when PATH is the one to be saved. */
		if(next == NULL)
		{
			strlcpy(filename, path, NAME_MAX + 1);
			success = true;
			break;	
		}

		/* Validate next path */
		if(strlen(next) > NAME_MAX)
			break;

		/* Handle commands: ./.. */
		if(!strcmp(path, "."))
		{
			path = next;
			continue;
		}

		if(!strcmp(path, ".."))
		{
			block_sector_t parent = inode_get_parent(dir_get_inode(cur_dir));
			dir_close(cur_dir);
			cur_dir = dir_open(inode_open(parent));
			path = next;
			continue;
		}

		/* Verify directory PATH (Must exist) */
		if(!lookup(cur_dir, path, &e, NULL) || !e.in_use || !e.isdir)
			break;

		/* Advance */
		dir_close(cur_dir);
		cur_dir = dir_open(inode_open(e.inode_sector));
		path = next;
	}

	*dir = cur_dir;
	palloc_free_page(name_cp);
	return success;
}

struct dir* 
dir_open_cur(void)
{
	struct thread* cur = thread_current();
	if(cur->dir == NULL)
		return dir_open_root();
	else
		return dir_reopen(cur->dir);
}
/* Project4 E */
