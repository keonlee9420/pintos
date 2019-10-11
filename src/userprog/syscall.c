#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* Project2 S */
#include <list.h>
#include <string.h>
#include "userprog/process.h"
#include "userprog/filedescriptor.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
/* Project2 E */

static void syscall_handler (struct intr_frame *);

/* Project2 S */
static int read_user(void* uaddr);

static void sc_halt(void);
static void sc_exit(int status);
static pid_t sc_exec(const char* cmd_line);
static int sc_wait(pid_t pid);
static bool sc_create(const char* file, unsigned initial_size);
static bool sc_remove(const char* file);
static int sc_open(const char* file);
static int sc_filesize(int fd);
static int sc_read(int fd, void* buffer, unsigned size);
static int sc_write(int fd, const void* buffer, unsigned size);
static void sc_seek(int fd, unsigned position);
static unsigned sc_tell(int fd);
static void sc_close(int fd);
/* Project2 E */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Project2 S */
static void
syscall_handler (struct intr_frame *f) 
{
  void* esp = f->esp;
	int syscall_number = read_user(esp);
	switch(syscall_number)
	{
		case SYS_HALT:
			sc_halt();
		case SYS_EXIT:
			sc_exit(read_user(esp + 4));
		case SYS_EXEC:
			f->eax = sc_exec((const char*)read_user(esp + 4));
			break;
		case SYS_WAIT:
			f->eax = sc_wait((pid_t)read_user(esp + 4));
			break;
		case SYS_CREATE:
			f->eax = sc_create((const char*)read_user(esp + 4), 
												 (unsigned)read_user(esp + 8));
			break;
		case SYS_REMOVE:
			f->eax = sc_remove((const char*)read_user(esp + 4));
			break;
		case SYS_OPEN:
			f->eax = sc_open((const char*)read_user(esp + 4));
			break;
		case SYS_FILESIZE:
			f->eax = sc_filesize(read_user(esp + 4));
			break;
		case SYS_READ:
			f->eax = sc_read(read_user(esp + 4), 
											 (void*)read_user(esp + 8), 
											 (unsigned)read_user(esp + 12));
			break;
		case SYS_WRITE:
			f->eax = sc_write(read_user(esp + 4), 
												(const void*)read_user(esp + 8), 
												(unsigned)read_user(esp + 12));
			break;
		case SYS_SEEK:
			sc_seek(read_user(esp + 4), 
							(unsigned)read_user(esp + 8));
			break;
		case SYS_TELL:
			f->eax = sc_tell(read_user(esp + 4));
			break;
		case SYS_CLOSE:
			sc_close(read_user(esp + 4));
			break;
		default:
			printf("Unvalid system call\n");
	}
}

/*  */
static int 
get_user(const uint8_t* uaddr)
{
	int result;
	asm("movl $1f, %0; movzbl %1, %0; 1:" 
			: "=&a" (result) : "m" (*uaddr));
	return result;
}

static bool 
put_user(uint8_t* udst, uint8_t byte)
{
	int error_code;
	asm("movl $1f, %0; movb %b2, %1; 1:" 
			: "=&a" (error_code), "=m" (*udst) : "q" (byte));
	return error_code != -1;
}

static int 
read_user(void* uaddr)
{
	unsigned i;
	int result = 0;

	if(!is_user_vaddr(uaddr + 4))
		sc_exit(-1);

	for(i = 0; i < 4; i++)
	{
		int value = get_user(uaddr + i);
		if(value == -1)
			sc_exit(-1);
		result += value << (8 * i);
	}
	return result;
}

static void 
validate_string(const char* str)
{
	int result;
	const uint8_t* ptr;
	do
	{
		ptr = (const uint8_t*)str++;

		if(!is_user_vaddr(ptr))
			sc_exit(-1);
			
		result = get_user(ptr);
		if(result == -1)
			sc_exit(-1);

	}while(result);
}

static void 
sc_halt(void)
{
	shutdown_power_off();
	NOT_REACHED();
}

static void 
sc_exit(int status)
{
	struct process* p = thread_process();
	p->status = status;
	printf("%s: exit(%d)\n", thread_name(), status);
	thread_exit();
	NOT_REACHED();
}

static pid_t 
sc_exec(const char* cmd_line)
{
	validate_string(cmd_line);
	return process_execute(cmd_line);
}

static int 
sc_wait(pid_t pid)
{
	return process_wait(pid);
}

static bool 
sc_create(const char* file, unsigned initial_size)
{
	validate_string(file);
	return filesys_create(file, initial_size);
}

static bool 
sc_remove(const char* file)
{
	validate_string(file);
	return filesys_remove(file);
}

static int 
sc_open(const char* file)
{
	struct file* file_opened;

	validate_string(file);

	if((file_opened = filesys_open(file)) == NULL)
		return -1;

	if(!strcmp(thread_name(), file))
		file_deny_write(file_opened);

	return fd_open(file_opened);
}

static int 
sc_filesize(int fd)
{
	struct file* file = fd_convert(fd);
	if(file == NULL)
		return 0;
	return (int)file_length(file);
}

static int 
sc_read(int fd, void* buffer, unsigned size)
{
	unsigned i;

	switch(fd)
	{
		case STDIN_FILENO:
		{
			uint8_t data;
			for(i = 0; i < size; i++)
			{
				data = input_getc();
				if(!is_user_vaddr(buffer) || !put_user(buffer++, data))
					sc_exit(-1);
			}
			return size;
		}
		case STDOUT_FILENO:
			return 0;
		default:
		{
			struct file* file = fd_convert(fd);
			if(file == NULL)
				return 0;

			uint8_t* data = malloc(size);
			unsigned readsize = file_read(file, data, size);
			for(i = 0; i < readsize; i++)
			{
				if(!is_user_vaddr(buffer) || !put_user(buffer++, data[i]))
				{
					free(data);
					sc_exit(-1);
				}
			}
			free(data);
			return readsize;
		}
	}
}

static int 
sc_write(int fd, const void* buffer, unsigned size)
{
	struct file* file;
	validate_string(buffer);
	switch(fd)
	{
		case STDIN_FILENO:
			return 0;
		case STDOUT_FILENO:
			putbuf(buffer, size);
			return size;
			
		default:
			if((file = fd_convert(fd)) == NULL)
				return 0;
			return file_write(file, buffer, size);
	}
}

static void 
sc_seek(int fd, unsigned position)
{
	struct file* file = fd_convert(fd);
	if(file == NULL)
		return;
	file_seek(file, position);
}

static unsigned 
sc_tell(int fd)
{
	struct file* file = fd_convert(fd);
	if(file == NULL)
		return 0;
	return file_tell(file);
}

static void 
sc_close(int fd)
{
	struct file* file = fd_close(fd);
	if(file != NULL)
		file_close(file);
}

