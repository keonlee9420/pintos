#include "userprog/fd.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/directory.h"

/* Allocate new file descriptor number 
	 New fd must be greater than 2 */
int 
fd_allocate(void* data, bool isdir)
{
	struct list* filelist = &thread_process()->filelist;
	struct fd_data* fd_new;
	int fd = list_empty(filelist) ? 3 : 
					 list_entry(list_back(filelist), struct fd_data, elem)->fd + 1;

	fd_new = malloc(sizeof(struct fd_data));
	fd_new->fd = fd;
	fd_new->data = data;
	fd_new->isdir = isdir;
	list_push_back(filelist, &fd_new->elem);
	
	return fd;
}

bool 
fd_get_data(int fd, void** data)
{
	struct list* filelist = &thread_process()->filelist;
	struct list_elem* e;

	for(e = list_begin(filelist); e != list_end(filelist); 
			e = list_next(e))
	{
		struct fd_data* fd_data = list_entry(e, struct fd_data, elem);
		if(fd_data->fd == fd)
		{
			*data = fd_data->data;
			return fd_data->isdir;
		}
	}
	return true;
}

bool 
fd_pop(int fd, void** data)
{
	struct list* filelist = &thread_process()->filelist;
	struct list_elem* e;
	bool isdir;
	
	for(e = list_begin(filelist); e != list_end(filelist); 
			e = list_next(e))
	{
		struct fd_data* fd_data = list_entry(e, struct fd_data, elem);
		if(fd_data->fd == fd)
		{
			*data = fd_data->data;
			isdir = fd_data->isdir;
			list_remove(e);
			free(fd_data);
			return isdir;
		}
	}
	return true;
}

void 
fd_collapse(void)
{
	struct list* filelist = &thread_process()->filelist;

	while(!list_empty(filelist))
	{
		struct fd_data* fd_data = list_entry(list_pop_front(filelist), 
																				 struct fd_data, elem);
		if(fd_data->isdir)
			dir_close(fd_data->data);
		else
			file_close(fd_data->data);
		free(fd_data);
	}
}

