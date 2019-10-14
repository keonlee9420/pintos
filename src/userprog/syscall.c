#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
static int get_user (const uint8_t *uaddr);
static int get_user_page (void *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static void sys_exit (int status);

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
	int syscall_num = get_user_page (esp);
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
				int status = get_user_page ((int *)(esp + 1));
				sys_exit (status);
				break;
			}
		
		case SYS_WRITE:
			{
				void *buffer = (void *) get_user_page ((void **)(esp + 2));
				printf ("%s", (char *)buffer);
				break;
			}
		default:
			break;
	}	
	/* Project2 E */
}

/* Project2 S */
/* Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
Returns the byte value if successful, -1 if a segfault
occurred. */
static int
get_user (const uint8_t *uaddr)
{
	int result;
	asm ("movl $1f, %0; movzbl %1, %0; 1:"
	: "=&a" (result) : "m" (*uaddr));
	return result;
}

static int
get_user_page (void *uaddr)
{
	int page_value = 0;

	for (int i = 0; i < 4; i++)
	{
		if (!is_user_vaddr (uaddr + i))
			sys_exit (-1);
		int at_value = get_user (uaddr + i);
		if (at_value == -1)
			sys_exit (-1);
		page_value += at_value << (8 * i);
	}
	return page_value;
}

/* Writes BYTE to user address UDST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
	int error_code;
	asm ("movl $1f, %0; movb %b2, %1; 1:"
	: "=&a" (error_code), "=m" (*udst) : "q" (byte));
	return error_code != -1;
}

static void 
sys_exit (int status)
{
	printf ("%s: exit(%d)\n", thread_name (), status);
	thread_exit ();
}
/* Project2 E */
