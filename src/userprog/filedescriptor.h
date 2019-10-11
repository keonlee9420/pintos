#ifndef USERPROG_FILEDESCRIPTOR_H
#define USERPROG_FILEDESCRIPTOR_H

#include <list.h>
#include "filesys/file.h"

/* Mapping structure with file and file descriptor number */
struct file_descriptor
{
	int fd;									/* File descriptor number */
	struct file* file;			/* File structure */
	struct list_elem elem;	/* List element */
};

int fd_open(struct file* file);
struct file* fd_close(int fd);
struct file* fd_convert(int fd);
void fd_collapse(void);

#endif /* userprog/filedescriptor.h */
