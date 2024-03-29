#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
/* Project4 S */
#include <limits.h>
#include "threads/malloc.h"
#include "filesys/cache.h"
/* Project4 E */

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

	/* Project4 S */
	cache_init();
	/* Project4 E */

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
	/* Project4 S */
	cache_writeback();
	/* Project4 E */
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool isdir) 
{
  block_sector_t inode_sector = 0;
  /* Project4 S */
	char filename[NAME_MAX + 1];
  struct dir *dir = dir_open_cur ();
	block_sector_t parent_sector = inode_get_inumber(dir_get_inode(dir));
  bool success = (dir != NULL
									&& dir_chdir(&dir, name, filename)
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, parent_sector)
                  && dir_add (dir, filename, inode_sector, isdir));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  /* Project4 E */
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
void*
filesys_open (const char *name, bool* isdir)
{
	char filename[NAME_MAX + 1];
  struct dir *dir;
  struct inode *inode = NULL;

	/* Project4 S */
	/* Handle opening root directory */
	if(!strcmp(name, "/"))
	{
		*isdir = true;
		return dir_open_root();
	}

	dir = dir_open_cur();
	/* Travel file path */
	if(!dir_chdir(&dir, name, filename))
		return NULL;

  if (dir != NULL)
    dir_lookup (dir, filename, &inode, isdir);
  dir_close (dir);

	if(*isdir)
		return dir_open(inode);
	else
  	return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
	char filename[NAME_MAX + 1];
	/* Project4 S */
  struct dir *dir;
  bool success;
	
	if(!strcmp(name, "/"))
		return false;
	
	dir = dir_open_cur();
	success = dir != NULL
						&& dir_chdir(&dir, name, filename) 
						&& dir_remove (dir, filename);
	/* Project4 E */ 
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 0))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
