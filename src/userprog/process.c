#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
/* Project2 S */
#include "threads/malloc.h"
#include "userprog/fd.h"
/* Project2 E */
/* Project3 S */
#include "userprog/mmap.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
/* Project3 E */

static thread_func start_process NO_RETURN;
static bool load (void* args, void (**eip) (void), void **esp);
/* Project2 S */
static void pass_argument(const char* cmdline, void** esp);
static void parse_title(char* title, const char* file_name);
static void setup_child(bool success);
static bool create_child(tid_t child_tid);
static struct process* find_process(pid_t child_pid);

/* List for all processes */
static struct list proc_list;
static struct lock proclist_lock;

/* Initialize process system */
void 
process_init(void)
{
	list_init(&proc_list);
	lock_init(&proclist_lock);
}
/* Project2 E */

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
	/* Project4 S */
	struct dir** dir;
	void* arg;
	/* Project4 E */
	/* Project2 S */
	char file_title[16];
	
	parse_title(file_title, file_name);
	/* Project2 E */

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  arg = palloc_get_page (0);
  if (arg == NULL)
    return TID_ERROR;
	
	/* Project4 S */
	dir = arg;
	*dir = thread_current()->dir;
	fn_copy = arg + 4;
	
  strlcpy (fn_copy, file_name, PGSIZE - 4);

  /* Create a new thread to execute FILE_NAME. */
	/* Project2 S */
  tid = thread_create (file_title, PRI_DEFAULT, start_process, arg);
	
	/* Return -1 if child thread not created */
	if (tid == TID_ERROR)
	{
    palloc_free_page (arg); 
		return -1;
	}

	/* Wait for child loading, 
		 Return -1 when child loading failed */
	if(!create_child(tid))
		return -1;
	/* Project2 E */
	
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
	void* args = file_name_;
  char *file_name = file_name_ + 4;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
	/* Project2 S */
  if((success = load (args, &if_.eip, &if_.esp)))
		pass_argument(file_name, &if_.esp);

  palloc_free_page (args);
	setup_child(success);
	/* Project2 E */

  /* If load failed, quit. */
  if (!success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
	struct process* p;
	int status;

	/* Return -1 if 
		 1; Thread creation or load failed
		 2. Process struct not exist (called wait more than once)
		 3. Current process in not a parent of child_tid */
  if(child_tid == -1 || 
		 (p = find_process(child_tid)) == NULL || 
		 p->parent != thread_current())
		return -1;

	/* Wait until child wakes parent up */
	while(!p->isexited)
		sema_down(&p->sema);

	/* Free process resource */
	status  = p->status;
	lock_acquire(&proclist_lock);
	list_remove(&p->elem);
	lock_release(&proclist_lock);
	free(p);

	return status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
	struct file* file;
	/* Project2 S */
	struct process* proc = cur->process;

	/* Project3 S */
	if(proc != NULL)
	{
		printf("%s: exit(%d)\n", thread_name(), proc->status);
		/* Free mmap resource */
		mmap_destroy();
		/* Collapse fd structs */
		fd_collapse();
	}

	/* Close Load file for execution */
	file = thread_current()->loadfile;
	if(file != NULL)
		file_close(file);
	/* Project3 E */

	/* Project4 S */
	if(cur->dir != NULL)
		dir_close(cur->dir);
	/* Project4 E */

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

	if(proc != NULL)
	{
		/* Mark as exited */
		proc->isexited = true;
		/* Signal to parent */
		sema_up(&proc->sema);
	}
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (void* args, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
	struct dir** dir = args;
	bool isdir;
	const char* file_name = args + 4;
	/* Project2 S */
	char file_title[16];

	parse_title(file_title, file_name);
	/* Project2 E */

	/* Project4 S */
	if(*dir != NULL)
		t->dir = dir_reopen(*dir);
	/* Project4 E */

	/* Project3 S */
	/* Allocate supplemental page table */
	t->spt = spt_create();
	if(t->spt == NULL)
		goto done;
	/* Project3 E */

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
	/* Project2 S */
	file = filesys_open (file_title, &isdir);
	/* Project2 E */
	/* Project3 S */
	thread_current()->loadfile = file;
	/* Project3 E */
  if (file == NULL || isdir) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
	return success;
}

/* load() helpers. */

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file* file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
	off_t offset;

  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

	/* Project3 S */
	offset = ofs;
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
			size_t page_zero_bytes = PGSIZE - page_read_bytes;			
			
			/* Create supplemental page table containing upage 
				 Store information needed for later loading */
			spage_create(upage, SPAGE_LOAD, file, 
									 offset, page_read_bytes, writable); 

      /* Advance. */
			offset += page_read_bytes;
			/* Project3 E */

      read_bytes -= page_read_bytes;
			zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

	/* Project3 S */
	void* upage = PHYS_BASE - PGSIZE;

	/* Create UPAGE information, then map to frame */
	spage_create(upage, SPAGE_STACK, NULL, 0, 0, true);
  kpage = frame_allocate(PAL_ZERO);

	success = process_install_page (upage, kpage, true);
  if (success)
    *esp = PHYS_BASE;
  else
    frame_free (kpage);
	/* Project3 E */
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
process_install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();
  
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

/* Project2 S */
/* Parse command line, 
	 then stack up according to 80x86 convention */
static void 
pass_argument(const char* cmdline, void** esp)
{
	void** argv = palloc_get_page(0);
	int argc = 0;
	void* tmp_esp = PHYS_BASE;
	char* saver; 
	char* token;

	/* Stack up arguments */
	for(token = strtok_r((char*)cmdline, " ", &saver); token; 
			token = strtok_r(NULL, " ", &saver))
	{
		size_t tok_size = strlen(token) + 1;
		tmp_esp -= tok_size;
		strlcpy(tmp_esp, token, tok_size);
		argv[argc++] = tmp_esp;
	}
	argv[argc] = NULL;

	/* Word alignment */
	tmp_esp -= 4 - (int)(PHYS_BASE - tmp_esp) % 4;

	/* Stack up argv pointers */
	tmp_esp -= 4 * (argc + 1);
	memcpy(tmp_esp, argv, 4 * (argc + 1));
	palloc_free_page(argv);

	/* Stack up argv, argc */
	argv = tmp_esp - 4;
	*argv = tmp_esp;

	tmp_esp -= 8;
	memcpy(tmp_esp, &argc, 4);

	/* Save return address into ESP */
	*esp = tmp_esp - 4;
}

static void 
parse_title(char* title, const char* file_name)
{
	char* file_copy = palloc_get_page(0);
	char* saver;
	strlcpy(file_copy, file_name, PGSIZE);
	strtok_r(file_copy, " ", &saver);
	strlcpy(title, file_copy, 16);
	palloc_free_page(file_copy);
}

/* Create child process, then wait for child loading */
static bool 
create_child(tid_t child_tid)
{
	bool success;

	/* Create child process struct */
	struct process* p = malloc(sizeof(struct process));
	p->pid = child_tid;
	p->parent = thread_current();
	p->isexited = false;
	p->status = -1;
	list_init(&p->filelist);
	sema_init(&p->sema, 0);

	/* Project3 S */
	list_init(&p->maplist);
	/* Project3 E */

	lock_acquire(&proclist_lock);
	list_push_front(&proc_list, &p->elem);
	lock_release(&proclist_lock);

	sema_down(&p->sema);

	/* Free resources if load failed */
	if(!(success = p->loaded))
	{
		lock_acquire(&proclist_lock);
		list_remove(&p->elem);
		lock_release(&proclist_lock);
		free(p);
	}

	return success;
}

/* Setup current process, then wake parent up */
static void 
setup_child(bool success)
{
	struct process* proc;
	/* Wair for own process struct created */
	while((proc = find_process(thread_tid())) == NULL)
		thread_yield();

	/* Save load condition and connect process with thread */
	proc->loaded = success;
	thread_current()->process = proc;

	/* Wake parent up */
	sema_up(&proc->sema);
}

/* Find process struct with pid */
static struct process* 
find_process(pid_t pid)
{
	struct list_elem* e;
	struct process* p;

	/* Find process list */
	lock_acquire(&proclist_lock);
	for(e = list_begin(&proc_list); e != list_end(&proc_list); 
			e = list_next(e))
	{
		p = list_entry(e, struct process, elem);
		if(p->pid == pid)
		{
			lock_release(&proclist_lock);
			return p;
		}
	}
	lock_release(&proclist_lock);

	return NULL;
}
