#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"

struct frame
{
  void *kpage;              /* Kernel virtual address KPAGE which is used as Frame number. */
	struct thread *user;      /* Thread structure who is using(occupying) the frame. */
	void* upage;
  struct list_elem elem;    /* List element for frame table. */
};

/* Initialization function. */
void frame_init (void);

/* Frame functions. */
void* frame_allocate (enum palloc_flags flags);
void frame_free (void *kpage);
void frame_reinsert(void* kpage);
struct frame* frame_select_victim(void);
struct frame* frame_lookup(void* kpage);
void frame_map(void* upage, void* kpage);

#endif /* vm/frame.h */
