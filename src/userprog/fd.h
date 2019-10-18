#ifndef USERPROG_FD_H
#define USERPROG_FD_H

#include <list.h>
#include "filesys/file.h"

struct fd_data
{
	int fd;
	struct file* file;
	struct list_elem elem;
};

int fd_allocate(struct file* file);
struct file* fd_get_file(int fd);
struct file* fd_pop(int fd);
void fd_collapse(void);

#endif /* userprog/fd.h */
