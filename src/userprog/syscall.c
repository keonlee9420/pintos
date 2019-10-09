#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
	/* Project2 S */
	int *esp = f->esp;
	int syscall_num = *esp;
  // printf ("SYSCALL NUM: %d\n", syscall_num);
	
	switch (syscall_num)
	{
		case SYS_HALT:
			{
				shutdown_power_off ();
				break;
			}
		case SYS_EXIT:
			{
				int status = *(int *)(esp + 1);
				printf ("%s: exit(%d)\n", thread_name (), status);
				thread_exit ();
				break;
			}

		case SYS_WRITE:
			{
				void *buffer = *(void **)(esp + 2);
				printf ("%s", (char *)buffer);
				break;
			}
		default:
			break;
	}	
	/* Project2 E */
}
