#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/synch.h"

struct frame
{
  void *kpage;              /* Kernel virtual address KPAGE which is used as Frame number */
  void *upage;              /* User virtual address UPAGE which is using(occupying) the frame */
  uint32_t *pte;            /* Page table entry which is matched to the frame */
	struct thread *user;      /* Thread structure who is using(occupying) the frame */
  struct list_elem elem;    /* List element for frame table */
};

struct list frame_table;      /* Frame table containing one entry for each frame that contains a user page */
struct lock frame_table_lock; /* Frame table lock */

/* Initialization function */
void framing_init (void);

/* Frame functions */
void allocate_frame (void *upage, void *kpage, struct thread *user);

#endif /* vm/frame.h */