#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
/* Project2 S */
#include "devices/input.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "lib/string.h"
#include "lib/user/syscall.h"
#include "lib/kernel/console.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "userprog/fd.h"
/* Project2 E */

static void syscall_handler (struct intr_frame *);
/* Project2 S */
static int get_user (const uint8_t *uaddr);
static int get_user_page (void *uaddr);
static void valid_user_string (const char *string);
static bool put_user (uint8_t *udst, uint8_t byte);
static void sys_halt (void);
static int sys_exec (const char *cmd_line);
static int sys_wait (pid_t pid);
static bool sys_create (const char *file, unsigned initial_size);
static bool sys_remove (const char *file);
static int sys_open (const char *filename);
static int sys_filesize (int fd);
static int sys_read (int fd, void *buffer, unsigned size);
static int sys_write (int fd, const void *buffer, unsigned size);
static void sys_seek (int fd, unsigned position);
static unsigned sys_tell (int fd);
static void sys_close (int fd);
/* Project2 E */

/* Project2 S */
struct lock filesys_lock;
/* Project2 E */

void
syscall_init (void) 
{
	/* Project2 S */
	// init the lock for filesys 
  lock_init (&filesys_lock);
  /* Project2 E */
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
	/* Project2 S */
	int *esp = f->esp;
	int syscall_num = get_user_page (esp);
	//printf ("SYSCALL NUM: %d\n", syscall_num);
	
	switch (syscall_num)
	{
		case SYS_HALT:
			{
				sys_halt ();
				break;
			}
		case SYS_EXIT:
			{
				int status = get_user_page (esp + 1);
				sys_exit (status);
				break;
			}
		case SYS_EXEC:
			{
				const char *cmd_line = (const char *)get_user_page (esp + 1);
				pid_t pid = sys_exec (cmd_line);
				f->eax = pid;
				break;
			}	
		case SYS_WAIT:
			{
				pid_t pid = get_user_page (esp + 1);
				int status = sys_wait (pid);
				f->eax = status;
				break;
			}	
		case SYS_CREATE:
			{
				const char *file = (char *)get_user_page (esp + 1);
				unsigned initial_size = (unsigned)get_user_page (esp + 2);
				bool status = sys_create (file, initial_size);
				f->eax = status;
				break;
			}
		case SYS_REMOVE:
			{
				const char *file = (char *)get_user_page (esp + 1);
				bool status = sys_remove (file);
				f->eax = status;
				break;
			}
		case SYS_OPEN:
			{
				const char *filename = (char *)get_user_page (esp + 1);
				int fd = sys_open (filename);
				f->eax = fd;
				break;
			}
		case SYS_FILESIZE:
			{
				int fd = get_user_page (esp + 1);
				int filesize = sys_filesize (fd);
				f->eax = filesize;
				break;
			}
		case SYS_READ:
			{
				int fd = get_user_page (esp + 1);
				void *buffer = (void *)get_user_page (esp + 2);
				unsigned size = (unsigned)get_user_page (esp + 3);
				int read_length = sys_read (fd, buffer, size);
				f->eax = read_length;
				break;
			}
		case SYS_WRITE:
			{
				int fd = get_user_page (esp + 1);
				const void *buffer = (const void *)get_user_page (esp + 2);
				unsigned size = (unsigned)get_user_page (esp + 3);
				int write_length = sys_write (fd, buffer, size);
				f->eax = write_length;
				break;
			}
		case SYS_SEEK:
			{
				int fd = get_user_page (esp + 1);
				unsigned position = (unsigned)get_user_page (esp + 2);
				sys_seek (fd, position);
				break;
			}
		case SYS_TELL:
			{
				int fd = get_user_page (esp + 1);
				unsigned next_position = sys_tell (fd);
				f->eax = next_position;
				break;
			}
		case SYS_CLOSE:
			{
				int fd = get_user_page (esp + 1);
				sys_close (fd);
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

static void
valid_user_string (const char *string)
{
	int i = 0;
	while (1)
	{
		if (!is_user_vaddr ((void *)string + i) 
																|| get_user ((void *)string + i) == -1)
			sys_exit (-1);
		char char_at = (char)get_user ((void *)string + i++);
		if (char_at == '\0')
			return;
	} 
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
sys_halt (void)
{
	shutdown_power_off ();
}

void 
sys_exit (int status)
{
	struct process *p;
  struct fd_data *fd_data;
  struct list_elem *e;

	// print msg for exit status
	printf ("%s: exit(%d)\n", thread_name (), status);

	// save exit status at exit_status of process
	p = thread_current ()->process;
	p->exit_status = status;
	p->status = status ? PROCESS_SUCCESS : PROCESS_ERROR;

	// close all open file if exist
	while (!list_empty (&p->fd_list))
	{
		e = list_pop_front (&p->fd_list);
		fd_data = list_entry (e, struct fd_data, elem);
		sys_close (fd_data->fd);
	}

	thread_exit ();
}

static int
sys_exec (const char *cmd_line)
{
	valid_user_string (cmd_line);
	return process_execute (cmd_line);
}

static int
sys_wait (pid_t pid)
{
	return process_wait (pid);
}

static bool 
sys_create (const char *file, unsigned initial_size)
{
	valid_user_string (file);

	// filesys in critical section
	lock_acquire (&filesys_lock);
	bool status = filesys_create (file, (off_t)initial_size);
	lock_release (&filesys_lock);
	return status; 
}

static bool 
sys_remove (const char *file)
{
	valid_user_string (file);

	// filesys in critical section
	lock_acquire (&filesys_lock);
	bool status = filesys_remove (file);
	lock_release (&filesys_lock);
	return status;
}

static int 
sys_open (const char *filename)
{
	int fd;
	struct file *file;

	valid_user_string (filename);

	// filesys in critical section
	lock_acquire (&filesys_lock);
	file = filesys_open (filename);
	fd = file ? fd_alloc (file) : -1;
	
	// denying writes to executables
  if(strcmp(thread_current ()->name, filename) == 0)
    file_deny_write(file);

	lock_release (&filesys_lock);

	return fd;
}

static int 
sys_filesize (int fd)
{
	struct file *file = fd_get_file (fd);
	if (file == NULL)
		return 0;

	// filesys in critical section
	lock_acquire (&filesys_lock);
	int filesize = (int)file_length (file);
	lock_release (&filesys_lock);
	return filesize;
}

static bool
put_on_valid_buffer (void *buffer, uint8_t byte)
{
	return is_user_vaddr (buffer) && put_user (buffer, byte);
}

static int
sys_read (int fd, void *buffer, unsigned size)
{
	struct file *file;
	unsigned read_length = 0;

	// return if fd = STDOUT_FILENO 1
	if (fd == STDOUT_FILENO)
		return 0;

	// filesys in critical section
	lock_acquire (&filesys_lock);

	// fd = STDIN_FILENO 0 or elses
	if (fd == STDIN_FILENO)
		{
			uint8_t byte;
			// validation checking
			for (unsigned i = 0; i < size; i++)
				{
					// get i'th byte from key
					byte = input_getc ();
					// validate buffer and put byte on it
					bool success = put_on_valid_buffer (buffer + i, byte);
					// if fail, then releas filesys_lock and call sys_exit with status -1
					if (!success)
						{
							lock_release (&filesys_lock);
							sys_exit (-1);
						}
				}
			read_length = size;
		}
	else
		{
			// if there is no such file with fd
			if ((file = fd_get_file (fd)) == NULL)
				{
					lock_release (&filesys_lock);
					return 0;
				}

			// validation checking
			uint8_t *byte = malloc (size);
			// read file to byte first
			read_length = file_read (file, byte, size);
			// and then do validation checking
			for (unsigned i = 0; i < read_length; i++)
				{
					// validate buffer and put byte on it
					bool success = put_on_valid_buffer (buffer + i, byte[i]);
					// if fail, then releas resources and call sys_exit with status -1
					if (!success)
						{
							free (byte);
							lock_release (&filesys_lock);
							sys_exit (-1);
						}
				}
			free (byte);
		}
	
	lock_release (&filesys_lock);

	return (int)read_length;
}

static int
sys_write (int fd, const void *buffer, unsigned size)
{
	struct file *file;
	unsigned write_length = 0;

	// return if fd = STDIN_FILENO 0
	if (fd == STDIN_FILENO)
		return 0;

	// validate buffer
	valid_user_string ((const char *)buffer);

	// filesys in critical section
	lock_acquire (&filesys_lock);

	// fd = STDOUT_FILENO 1 or elses
	if (fd == STDOUT_FILENO)
		{
			putbuf (buffer, size);
			write_length = size;
		}
	else
		{
			// if there is no such file with fd
			if ((file = fd_get_file (fd)) == NULL)
				{
					lock_release (&filesys_lock);
					return 0;
				}
			write_length = file_write (file, buffer, size);
		}

	lock_release (&filesys_lock);

	return (int)write_length;
}

static void
sys_seek (int fd, unsigned position)
{
	struct file *file = fd_get_file (fd);

	// filesys in critical section
	lock_acquire (&filesys_lock);
	file_seek (file, (off_t)position);
	lock_release (&filesys_lock);
}

static unsigned
sys_tell (int fd)
{
	struct file *file = fd_get_file (fd);
	unsigned position;

	// filesys in critical section
	lock_acquire (&filesys_lock);
	position = (unsigned)file_tell (file);
	lock_release (&filesys_lock);
	return position;
}

static void
sys_close (int fd)
{
	struct file *file;

	// filesys in critical section
	lock_acquire (&filesys_lock);
	if ((file = fd_pop_file (fd)) != NULL)
		{
			file_close (file);
		}
	lock_release (&filesys_lock);
}

/* Project2 E */
