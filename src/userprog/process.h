#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

/* Project2 S */
typedef int pid_t;

struct process
{
	pid_t pid;											/* Process pid */
	struct thread* parent;					/* Parent thread of process */

	bool success;										/* Load status */
	bool isexited;									/* Exit determinator */
	int status;											/* Exit status */

	struct list filelist;						/* List of file descriptor */

	struct semaphore semaphore;			/* Synch for load/wait syscall */

	struct list_elem elem;					/* Proc_list element */
};

void process_init(void);
void process_acquire_filesys(void);
void process_release_filesys(void);
/* Project2 E */

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
