#include "userprog/fd.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/process.h"

/* Allocate new file descriptor number 
	 New fd must be greater than 2 */
int 
fd_allocate(struct file* file)
{
	struct list* filelist = &thread_process()->filelist;
	struct fd_data* fd_new;
	int fd = list_empty(filelist) ? 3 : 
					 list_entry(list_back(filelist), struct fd_data, elem)->fd + 1;

	fd_new = malloc(sizeof(struct fd_data));
	fd_new->fd = fd;
	fd_new->file = file;
	list_push_back(filelist, &fd_new->elem);
	
	return fd;
}

struct file* 
fd_get_file(int fd)
{
	struct list* filelist = &thread_process()->filelist;
	struct list_elem* e;

	for(e = list_begin(filelist); e != list_end(filelist); 
			e = list_next(e))
	{
		struct fd_data* fd_data = list_entry(e, struct fd_data, elem);
		if(fd_data->fd == fd)
			return fd_data->file;
	}
	return NULL;
}

struct file* 
fd_pop(int fd)
{
	struct list* filelist = &thread_process()->filelist;
	struct list_elem* e;
	struct file* file;
	
	for(e = list_begin(filelist); e != list_end(filelist); 
			e = list_next(e))
	{
		struct fd_data* fd_data = list_entry(e, struct fd_data, elem);
		if(fd_data->fd == fd)
		{
			file = fd_data->file;
			list_remove(e);
			free(fd_data);
			return file;
		}
	}
	return NULL;
}

void 
fd_collapse(void)
{
	struct list* filelist = &thread_process()->filelist;

	while(!list_empty(filelist))
	{
		struct fd_data* fd_data = list_entry(list_pop_front(filelist), 
																				 struct fd_data, elem);
		lock_acquire(&filesys_lock);
		file_close(fd_data->file);
		lock_release(&filesys_lock);
		free(fd_data);
	}
}


