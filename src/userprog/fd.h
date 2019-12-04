#ifndef USERPROG_FD_H
#define USERPROG_FD_H

#include <list.h>

struct fd_data
{
	int fd;
	void* data;
	bool isdir;
	struct list_elem elem;
};

int fd_allocate(void* data, bool isdir);
bool fd_get_data(int fd, void** data);
bool fd_pop(int fd, void** data);
void fd_collapse(void);

#endif /* userprog/fd.h */
