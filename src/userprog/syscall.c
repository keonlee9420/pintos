#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* Project2 S */
#include <list.h>
#include <string.h>
#include "userprog/process.h"
#include "userprog/fd.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
/* Project2 E */
/* Project3 S */
#include "userprog/mmap.h"
#include "vm/page.h"
/* Project3 E */

static void syscall_handler (struct intr_frame *);

/* Project2 S */
static int read_stack(void* uaddr);

static void sys_halt(void);
static void sys_exit(int status);
static pid_t sys_exec(const char* cmd_line);
static int sys_wait(pid_t pid);
static bool sys_create(const char* file, unsigned initial_size);
static bool sys_remove(const char* file);
static int sys_open(const char* file);
static int sys_filesize(int fd);
static int sys_read(int fd, void* buffer, unsigned size);
static int sys_write(int fd, const void* buffer, unsigned size);
static void sys_seek(int fd, unsigned position);
static unsigned sys_tell(int fd);
static void sys_close(int fd);
/* Project2 E */

/* Project3 S */
static mapid_t sys_mmap(int fd, void* addr);
static void sys_munmap(mapid_t mapping);
/* Project3 E */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Project2 S */
static void
syscall_handler (struct intr_frame *f) 
{
	int* esp = f->esp;
	int syscall_number = read_stack(esp);
	switch(syscall_number)
	{
		case SYS_HALT:
			sys_halt();
		case SYS_EXIT:
		{
			int status = read_stack(++esp);
			sys_exit(status);
		}
		case SYS_EXEC:
		{
			const char* cmdline = (const char*)read_stack(++esp);
			f->eax = sys_exec(cmdline);
			break;
		}
		case SYS_WAIT:
		{
			pid_t pid = read_stack(++esp);
			f->eax = sys_wait(pid);
			break;
		}
		case SYS_CREATE:
		{
			const char* file = (const char*)read_stack(++esp);
			unsigned initial_size = (unsigned)read_stack(++esp);
			f->eax = sys_create(file, initial_size);
			break;
		}
		case SYS_REMOVE:
		{
			const char* file = (const char*)read_stack(++esp);
			f->eax = sys_remove(file);
			break;
		}
		case SYS_OPEN:
		{
			const char* file = (const char*)read_stack(++esp);
			f->eax = sys_open(file);
			break;
		}
		case SYS_FILESIZE:
		{
			int fd = read_stack(++esp);
			f->eax = sys_filesize(fd);
			break;
		}
		case SYS_READ:
		{
			int fd = read_stack(++esp);
			void* buffer = (void*)read_stack(++esp);
			unsigned size = (unsigned)read_stack(++esp);
			f->eax = sys_read(fd, buffer, size);
			break;
		}
		case SYS_WRITE:
		{
			int fd = read_stack(++esp);
			const void* buffer = (const void*)read_stack(++esp);
			unsigned size = (unsigned)read_stack(++esp);
			f->eax = sys_write(fd, buffer, size);
			break;
		}
		case SYS_SEEK:
		{
			int fd = read_stack(++esp);
			unsigned position = read_stack(++esp);
			sys_seek(fd, position);
			break;
		}
		case SYS_TELL:
		{
			int fd = read_stack(++esp);
			f->eax = sys_tell(fd);
			break;
		}
		case SYS_CLOSE:
		{
			int fd = read_stack(++esp);
			sys_close(fd);
			break;
		}
		/* Project3 S */
		case SYS_MMAP:
		{
			int fd = read_stack(++esp);
			void* addr = (void*)read_stack(++esp);
			f->eax = sys_mmap(fd, addr);
			break;
		}
		case SYS_MUNMAP:
		{
			mapid_t mapping = (mapid_t)read_stack(++esp);
			sys_munmap(mapping);
			break;
		}
		/* Project3 E */
		default:
			NOT_REACHED();
	}
}

/* Reads a byte at user virtual address UADDR.
	 UADDR must be below PHYS_BASE.
	 Returns the byte value if successful, -1 if a segfault occurred */
static int 
get_user(const uint8_t* uaddr)
{
	int result;
	asm("movl $1f, %0; movzbl %1, %0; 1:" 
			: "=&a" (result) : "m" (*uaddr));
	return result;
}

/* Read 4 byte sequentially */
static int 
read_stack(void* uaddr)
{
	int result = 0;
	int i;

	for(i = 0; i < 4; i++)
	{
		int tmp;
		if(!is_user_vaddr(uaddr + i) || 
			 (tmp = get_user(uaddr + i)) == -1)
			sys_exit(-1);

		result += tmp << (8 * i);
	}
	
	return result;
}

/* Write BYTE to user address UDST. 
	 UDST must be below PHYS_BASE. 
	 Resutns true if successful, false if a segfault occurred */
static bool 
put_user(uint8_t* udst, uint8_t byte)
{
	int error_code;
	asm ("movl $1f, %0; movb %b2, %1; 1:" 
			 : "=&a" (error_code), "=m" (*udst) : "q" (byte));
	return error_code != -1;
}

/* Validate buffer (usually string), until value meets zero(\0) */
static void 
valid_string(const char* string)
{
	const uint8_t* ptr;
	int result;
	do
	{
		ptr = (const uint8_t*)string++;

		if(!is_user_vaddr(ptr) ||
			 (result = get_user(ptr)) == -1)
			sys_exit(-1);
	}
	while(result);
}

/* Put data into buffer with buffer address check */
static inline bool 
write_buffer(void* buffer, uint8_t byte)
{
	return is_user_vaddr(buffer) && put_user(buffer, byte);
}

static void 
sys_halt(void)
{
	shutdown_power_off();
}

static void 
sys_exit(int status)
{
	struct process* p = thread_current()->process;

	p->status = status;
	thread_exit();
}

static pid_t
sys_exec(const char* cmd_line)
{
	valid_string(cmd_line);
	return process_execute(cmd_line);
}

static int 
sys_wait(pid_t pid)
{
	return process_wait(pid);
}

static bool 
sys_create(const char* file, unsigned initial_size)
{
	bool success;

	valid_string(file);

	lock_acquire(&filesys_lock);
	success = filesys_create(file, initial_size);
	lock_release(&filesys_lock);

	return success;
}

static bool 
sys_remove(const char* file)
{
	bool success;

	valid_string(file);

	lock_acquire(&filesys_lock);
	success = filesys_remove(file);
	lock_release(&filesys_lock);

	return success;
}

static int 
sys_open(const char* filename)
{
	int fd = -1;
	struct file* file;

	valid_string(filename);

	lock_acquire(&filesys_lock);
	if((file = filesys_open(filename)) == NULL)
		goto done;

	fd = fd_allocate(file);

	if(!strcmp(thread_name(), filename))
		file_deny_write(file);

	done:
		lock_release(&filesys_lock);
		return fd;	
}

static int 
sys_filesize(int fd)
{
	int filesize;
	struct file* file = fd_get_file(fd);
	
	if(file == NULL)
		return 0;

	lock_acquire(&filesys_lock);
	filesize = file_length(file);
	lock_release(&filesys_lock);

	return filesize;
}

static int 
sys_read(int fd, void* buffer, unsigned size)
{
	unsigned i;

	switch(fd)
	{
		case STDIN_FILENO:
		{
			uint8_t byte;
			for(i = 0; i < size; i++)
			{
				byte = input_getc();
				if(!write_buffer(buffer + i, byte))
					sys_exit(-1);
			}
			return size;
		}
		case STDOUT_FILENO:
			return 0;
		default:
		{
			unsigned readsize;
			uint8_t* bytes;
			struct file* file = fd_get_file(fd);

			if(file == NULL)
				return 0;

			bytes = malloc(size);

			lock_acquire(&filesys_lock);
			readsize = file_read(file, bytes, size);
			lock_release(&filesys_lock);

			for(i = 0; i < readsize; i++)
				if(!write_buffer(buffer + i, bytes[i]))
				{
					free(bytes);
					sys_exit(-1);
				}

			free(bytes);
			return readsize;
		}
	}
}

static int 
sys_write(int fd, const void* buffer, unsigned size)
{
	switch(fd)
	{
		case STDIN_FILENO:
			return 0;
		case STDOUT_FILENO:
			valid_string(buffer);
			putbuf(buffer, size);
			return size;
		default:
		{
			struct file* file = fd_get_file(fd);
			int writesize;

			if(file == NULL)
				return 0;

			valid_string(buffer);

			lock_acquire(&filesys_lock);
			writesize = file_write(file, buffer, size);
			lock_release(&filesys_lock);
			
			return writesize;
		}
	}
}

static void 
sys_seek(int fd, unsigned position)
{
	struct file* file = fd_get_file(fd);

	if(file == NULL)
		return;

	lock_acquire(&filesys_lock);
	file_seek(file, position);
	lock_release(&filesys_lock);
}

static unsigned 
sys_tell(int fd)
{
	struct file* file = fd_get_file(fd);
	unsigned position;

	if(file == NULL)
		return 0;

	lock_acquire(&filesys_lock);
	position = file_tell(file);
	lock_release(&filesys_lock);

	return position;
}

static void 
sys_close(int fd)
{
	struct file* file = fd_pop(fd);

	if(file != NULL)
	{				
		lock_acquire(&filesys_lock);
		file_close(file);
		lock_release(&filesys_lock);
	}
}
/* Project2 E */

/* Project3 S */
static mapid_t 
sys_mmap(int fd, void* addr)
{
	struct file* file;
	size_t fsize;
	off_t ofs;
	unsigned i;

	/* Check fd validity */
	if(fd == STDOUT_FILENO || fd == STDIN_FILENO)
		return -1;

	/* Check address validity: Non-null, page-aligned */
	if(addr == NULL || pg_round_down(addr) != addr)
		return -1;

	/* Get file from fd */	
	if((file = fd_get_file(fd)) == NULL)
		return -1;	
	
	lock_acquire(&filesys_lock);
	fsize = file_length(file);
	lock_release(&filesys_lock);

	/* Check consecutive page vacancy */
	for(i = 0; i < fsize; i += PGSIZE)
		if(spage_lookup(thread_current()->spt, addr + i) != NULL)
			return -1;
	
	/* Create supplemental page table */
	ofs = 0;	
	while(fsize > 0)
	{
		size_t page_read_bytes = fsize < PGSIZE ? fsize : PGSIZE;
		struct spage* spage = spage_create(addr, NULL, ofs, page_read_bytes, true);
		spage->status = SPAGE_MMAP;
		spage->mapfile = file;

		/* Advance */
		ofs += page_read_bytes;
		fsize -= page_read_bytes;
		addr += PGSIZE;
	}

	/* Create mapid structure */
	return mmap_allocate(fd, addr);
}

static void 
sys_munmap(mapid_t mapping)
{
	void* free_addr = mmap_remove(mapping);
	spage_free(free_addr);
}
/* Project3 E */
