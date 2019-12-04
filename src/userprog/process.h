#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
/* Project2 S */
#include "threads/synch.h"

typedef int pid_t;

struct process
{
	pid_t pid;
	struct thread* parent;

	bool loaded;
	bool isexited;
	int status;

	struct list filelist;
	struct list maplist;

	struct semaphore sema;
	struct list_elem elem;
};

void process_init(void);
/* Project2 E */

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* Project3 S */
bool process_install_page(void* upage, void* kpage, bool writable);
/* Project3 E */

#endif /* userprog/process.h */
