#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
/* Project2 S */
#include "lib/user/syscall.h"
#include "lib/kernel/list.h"
#include "threads/synch.h"
/* Project2 E */

/* Project2 S */
#define PROCESS_ALLOCATE_ERROR (NULL) // Error value for allocation of process

enum process_status
	{
		PROCESS_CREATED,
		PROCESS_LOADED,
		PROCESS_SUCCESS,
		PROCESS_ERROR
	};

struct process
	{
		pid_t pid;
		enum process_status status; 	// Process status based on its phase
		int exit_status; 							// Exit status of exit call from process
		struct list_elem elem; 				// Element used for children list of parent of this process if it exists			
		struct list children; 				// List of children processes of this process if they exist
		struct list fd_list;					// List of fd_data of relevant files owned by this process
		struct semaphore sema; 				// Sema for waiting child's load or child's exit
	};
/* Project2 E */

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
