#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
/* Project2 S */
#include "lib/user/syscall.h"
#include "lib/kernel/list.h"
#include "threads/synch.h"
/* Project2 E */

/* Project2 S */
enum process_status
	{
		PROCESS_CREATED,
		PROCESS_LOADED,
		PROCESS_SUCCESS,
		PROCESS_ERROR
	};
#define PROCESS_ALLOCATE_ERROR (NULL) // Error value for allocation of process
struct process
	{
		pid_t pid;
		enum process_status status;
		struct list_elem elem; //used for children list of parent 
																				// of this process if it exists.			
		struct list children; //list of children processes 
																				// of this process if they exist.
		struct semaphore sema; //sema for waiting child's load or child's exit
	};
/* Project2 E */

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
