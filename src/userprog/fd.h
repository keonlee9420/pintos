#ifndef USERPROG_FD_H
#define USERPROG_FD_H

#include <list.h>
#include "filesys/file.h"

/* process*/
struct fd_data
{
  int fd;                 /* File descriptor */
  struct file* file;      /* File lineked to fd */
  struct list_elem elem;  /* Element of fd_list for each process */
};

int fd_alloc (struct file *file);
struct file *get_file (int fd);

#endif /* userprog/fd.h */
