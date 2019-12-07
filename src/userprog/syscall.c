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
#include "userprog/pagedir.h"
#include "vm/page.h"
/* Project3 E */
/* Project4 S */
#include "filesys/directory.h"
#include "filesys/inode.h"
/* Project4 E */

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
/* Project4 S */
static bool sys_chdir(const char* dir);
static bool sys_mkdir(const char* dir);
static bool sys_readdir(int fd, char* name);
static bool sys_isdir(int fd);
static int sys_inumber(int fd);
/* Project4 E */

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
	thread_current()->esp = esp;
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
		/* Project4 S */
		case SYS_CHDIR:
		{
			const char* dir = (const char*)read_stack(++esp);
			f->eax = sys_chdir(dir);
			break;
		}
		case SYS_MKDIR:
		{
			const char* dir = (const char*)read_stack(++esp);
			f->eax = sys_mkdir(dir);
			break;
		}
		case SYS_READDIR:
		{
			int fd = read_stack(++esp);
			char* name = (char*)read_stack(++esp);
			f->eax = sys_readdir(fd, name);
			break;
		}
		case SYS_ISDIR:
		{
			int fd = read_stack(++esp);
			f->eax = sys_isdir(fd);
			break;
		}
		case SYS_INUMBER:
		{
			int fd = read_stack(++esp);
			f->eax = sys_inumber(fd);
			break;
		}
		default:
			NOT_REACHED();
	}
	thread_current()->esp = NULL;
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
	valid_string(file);

	return filesys_create(file, initial_size, false);
}

static bool 
sys_remove(const char* file)
{
	valid_string(file);
	return filesys_remove(file);
}

static int 
sys_open(const char* filename)
{
	void* data;
	bool isdir;

	valid_string(filename);

	if((data = filesys_open(filename, &isdir)) == NULL)
		return -1;

	if(!isdir && !strcmp(thread_name(), filename))
		file_deny_write(data);

	return fd_allocate(data, isdir);
}

static int 
sys_filesize(int fd)
{
	void* file = NULL;
	if(fd_get_data(fd, &file))
		return 0;

	return file_length(file);
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
			return -1;
		default:
		{
			unsigned readsize;
			uint8_t* bytes;
			void* file;

			if(fd_get_data(fd, &file))
				return -1;

			bytes = malloc(size);
			readsize = file_read(file, bytes, size);

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
			return -1;
		case STDOUT_FILENO:
			valid_string(buffer);
			putbuf(buffer, size);
			return size;
		default:
		{
			void* file = NULL;
			if(fd_get_data(fd, &file))
				return -1;

			valid_string(buffer);
			return file_write(file, buffer, size);
		}
	}
}

static void 
sys_seek(int fd, unsigned position)
{
	void* file = NULL;
	if(fd_get_data(fd, &file))
		return;

	file_seek(file, position);
}

static unsigned 
sys_tell(int fd)
{
	void* file = NULL;
	if(fd_get_data(fd, &file))
		return 0;

	return file_tell(file);
}

static void 
sys_close(int fd)
{
	void* data = NULL;
	bool dir = fd_pop(fd, &data);

	if(data == NULL)
		return;

	if(dir)
		dir_close(data);
	else
		file_close(data);
}
/* Project2 E */

/* Project3 S */
static mapid_t 
sys_mmap(int fd, void* addr)
{
	size_t fsize;
	off_t ofs;
	unsigned i;
	void* file = NULL;
	void* upage = addr;

	/* Check address validity: Non-null, page-aligned */
	if(addr == NULL || pg_round_down(addr) != addr)
		return -1;

	/* Get file from fd, checking fd validity */	
	if(fd_get_data(fd, &file))
		return -1;	
	
	file = file_reopen(file);
	fsize = file_length(file);

	/* Return if mapping zero byte */
	if(fsize == 0)
		return -1;

	/* Check consecutive page vacancy */
	for(i = 0; i < fsize; i += PGSIZE)
		if(spage_lookup(thread_current()->spt, addr + i) != NULL)
			return -1;
	
	/* Create supplemental page table */
	ofs = 0;
	while(fsize > 0)
	{
		size_t page_read_bytes = fsize < PGSIZE ? fsize : PGSIZE;

		spage_create(upage, SPAGE_MMAP, file, 
								 ofs, page_read_bytes, true);

		/* Advance */
		ofs += page_read_bytes;
		fsize -= page_read_bytes;
		upage += PGSIZE;
	}

	/* Create mapid structure */
	return mmap_allocate(file, addr);
}

static void 
sys_munmap(mapid_t mapping)
{
	struct mmap* map = mmap_get(mapping);

	/* Return if no mapping with MAPPING */
	if(map == NULL)
		return;

	/* Write back to file */
	mmap_writeback(map->file, map->addr);

	/* Close file */
	file_close(map->file);

	/* Remove mmap element */
	list_remove(&map->elem);
	free(map);
}
/* Project3 E */
/* Project4 S */
static bool 
sys_chdir(const char* dir)
{
	struct thread* t = thread_current();
	struct dir* cur_dir;
	char dirname[NAME_MAX + 1];
	struct inode* inode = NULL;
	bool success;
	bool isdir;

	valid_string(dir);

	/* If dir indicates only root '/', 
		 direct root directory then exit */
	if(!strcmp(dir, "/"))
	{
		dir_close(t->dir);
		t->dir = dir_open_root();
		return true;
	}
	
	cur_dir = dir_open_cur();
	success = dir_chdir(&cur_dir, dir, dirname) 
						&& dir_lookup(cur_dir, dirname, &inode, &isdir)
						&& isdir;

	dir_close(cur_dir);
	if(success)
	{
		dir_close(t->dir);
		t->dir = dir_open(inode);
	}

	return success;
}

static bool 
sys_mkdir(const char* dir)
{
	valid_string(dir);
	return filesys_create(dir, 512, true);
}

static bool 
sys_readdir(int fd, char* name)
{
	void* dir = NULL;
	
	if(!fd_get_data(fd, &dir))
		return false;	

	return dir_readdir(dir, name);
}

static bool 
sys_isdir(int fd)
{
	void* dir = NULL;
	return fd_get_data(fd, &dir) && dir != NULL;
}

static int 
sys_inumber(int fd)
{
	void* data = NULL;
	bool isdir = fd_get_data(fd, &data);

	if(data == NULL)
		return -1;	

	if(isdir)
		return inode_get_inumber(dir_get_inode(data));
	else
		return inode_get_inumber(file_get_inode(data));
}
/* Project4 E */
