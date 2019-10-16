#include <stdio.h>
#include "userprog/fd.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/process.h"

/* System for managing data of file descriptor.
Allocate new fd (which is greater than 2) to relevant file.
Enable to manage files as desired way using file descriptor concept. */
int
fd_alloc (struct file *file)
{
  struct process *p = thread_current ()->process;
  struct list *fd_list = &p->fd_list;
  struct fd_data *fd_last;
  struct fd_data *new_fd_data;
  int fd = 3;

  // allocate fd as max + 1 from fd_list
  if (list_size (fd_list))
    {
      // get last fd_data
      fd_last = list_entry (list_back (fd_list), struct fd_data, elem);
      // get fd of fd_last and set fd as next
      fd = fd_last->fd + 1;
    }

  // create and append new_fd_data with current file into fd_list of this process
  new_fd_data = malloc (sizeof (struct fd_data));
  new_fd_data->fd = fd;
  new_fd_data->file = file;
  list_push_back (fd_list, &new_fd_data->elem);

  return fd;
}

// Return file which has fd from fd_list
struct file *
get_file (int fd)
{
  struct process *p = thread_current ()->process;
  struct list *fd_list = &p->fd_list;
  struct fd_data *fd_data = NULL;
  struct list_elem *e;
 
  for (e = list_begin (fd_list); e != list_end (fd_list);
       e = list_next (e))
    {
      fd_data = list_entry (e, struct fd_data, elem);
      if (fd_data->fd == fd)
        return fd_data->file;
    }

	return NULL;
}