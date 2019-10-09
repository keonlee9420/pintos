#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	int* esp = f->esp;
	switch(*esp)
	{
		case SYS_EXIT:
		{
			printf("%s: exit(%d)\n", thread_name(), *(esp + 1));
			thread_exit();
		}
		case SYS_WRITE:
		{
			char* buffer = *(char**)(esp + 2);
			printf("%s", buffer);
			break;
		}
	}
}
