#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
/* Project3 S */
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
/* Project3 E */

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Project3 S */
static bool lazy_load(struct file* file, struct spage* spage);
/* Project3 E */

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */
	/* Project3 S */
	struct spage* spage;
	/* Project3 E */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
	/* Project3 S */
	/* Error out for outbounded access */
	if(is_kernel_vaddr(fault_addr) || fault_addr == NULL)
		thread_exit();

	/* Terminate process if page fault is from write to read-only page */
	if(!not_present)
		thread_exit();

	/* User page fault: follow direction from 
		 corresponding supplemental page table */
	spage = spage_lookup(thread_current()->spt, pg_round_down(fault_addr));	

	/* Stack growth */
	if(spage == NULL)
	{
		void* upage = pg_round_down(fault_addr);
		uint8_t* kpage;
		void* esp = thread_current()->esp == NULL ? 
								f->esp : thread_current()->esp;
		
		if(fault_addr < esp - 32)
		{
			//printf("fault addr: %p\n", fault_addr);
			if(user)
				thread_exit();
			else
			{
				f->eip = (void*)f->eax;
				f->eax = 0xffffffff;
				return;
			}
		}

		spage_create(upage, SPAGE_STACK, NULL, 0, 0, true);
		kpage = frame_allocate(PAL_ZERO);
		
		if(!process_install_page(upage, kpage, true))
		{
			frame_free(kpage);
			thread_exit();
		}
		return;
	}

	/* Handle page fault according to page installation status */
	switch(spage->status)
	{
		/* LOAD: Open file, read file, then map upage */
		case SPAGE_LOAD:
			if(!lazy_load(spage->file, spage))
				thread_exit();
			break;
		/* SWAP: Swap back in */
		case SPAGE_SWAP:
		{
			void* upage = pg_round_down(fault_addr);
			swap_in(upage);
			break;
		}
		/* MMAP: Read own file, map upage */
		case SPAGE_MMAP:
			if(!lazy_load(spage->file, spage))
				thread_exit();
			break;
		/* Stack: this should not happen */
		case SPAGE_STACK:
			NOT_REACHED();
	}
}

/* Read file data into kpage from saved offset, 
	 then map upage with kpage */
static bool 
lazy_load(struct file* file, struct spage* spage)
{
	uint8_t* kpage;
	bool success = false;

	if(file == NULL)
		return false;
		
	kpage = frame_allocate(PAL_ZERO);

	lock_acquire(&filesys_lock);
	if(file_read_at(file, kpage, spage->readbyte, spage->offset) != 
		 (int) spage->readbyte)
		goto load_end;
			
	if(!process_install_page(spage->upage, kpage, spage->writable))
		goto load_end;
	
	success = true;

	load_end:
		lock_release(&filesys_lock);
		if(!success)
			frame_free(kpage);
		return success;
}
