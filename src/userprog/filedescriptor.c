#include "userprog/filedescriptor.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "userprog/process.h"

/* Create file descriptor struct, 
	 and allocate fd number to added file
	 File descriptor number starts from 3, 
	 since ordinary convention allocates 0 to STDIN, 1 to STDOUT, 2 to STDERR */
int 
fd_open(struct file* file)
{
	/* Allocate fd number */
	struct list* filelist = &thread_process()->filelist;
	int fd = list_empty(filelist) ? 3 : 
					 list_entry(list_rbegin(filelist), 
											struct file_descriptor, elem)->fd + 1;

	/* Create file descriptor struct */
	struct file_descriptor* f = malloc(sizeof(struct file_descriptor));
	f->fd = fd;
	f->file = file;
	list_push_back(filelist, &f->elem);

	return fd;
}

/* Find file descriptor containing FD,
	 then free resource */
struct file* 
fd_close(int fd)
{
	struct list_elem* e;
	struct file_descriptor* f = NULL;
	struct list* filelist = &thread_process()->filelist;
	struct file* file;

	for(e = list_begin(filelist); e != list_end(filelist); 
			e = list_next(e))
	{
		f = list_entry(e, struct file_descriptor, elem);
		if(f->fd == fd)
			break;
		f = NULL;
	}
	
	if(f == NULL)
		return NULL;

	file = f->file;
	list_remove(&f->elem);
	free(f);

	return file;
}

/* Find file which matches to FD */
struct file* 
fd_convert(int fd)
{
	struct list_elem* e;
	struct list* filelist = &thread_process()->filelist;

	for(e = list_begin(filelist); e != list_end(filelist); 
			e = list_next(e))
	{
		struct file_descriptor* f = list_entry(e, struct file_descriptor, elem);
		if(f->fd == fd)
			return f->file;
	}
	return NULL;
}

/* Close all the files opened, 
	 then free all fd resources */
void 
fd_collapse(void)
{
	struct list* filelist = &thread_process()->filelist;
	
	while(!list_empty(filelist))
	{
		struct file_descriptor* f = list_entry(list_pop_front(filelist), 
																					 struct file_descriptor, elem);
		process_acquire_filesys();
		file_close(f->file);
		process_release_filesys();
		free(f);
	}
}
