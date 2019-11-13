#include "vm/frame.h"
#include "threads/pte.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "vm/swap.h"

/* Frame table list variables */
static struct list frame_table;
static struct lock frame_table_lock;

/* Initialize frame page table. */
void 
frame_init (void)
{
  list_init (&frame_table);
  lock_init (&frame_table_lock);
}

/* Create and return new frame. */
void*
frame_allocate (enum palloc_flags flags)
{
	void* kpage = palloc_get_page(PAL_USER | flags);
	
	if(kpage == NULL)
		kpage = swap_out();		//Need to implement swap here

	/* Create mapped physical page */
  struct frame *frame = malloc(sizeof(struct frame));

  frame->kpage = kpage;

	/* Insert into frame list */
	lock_acquire(&frame_table_lock);
  list_push_back (&frame_table, &frame->elem);
	lock_release(&frame_table_lock);

  return kpage;
}

/* Delete and free the frame which is currently allocated into KPAGE.*/
void
frame_free (void *kpage)
{
  struct frame *frame = frame_lookup (kpage);

  if (frame != NULL)
  {
    lock_acquire (&frame_table_lock);
    list_remove (&frame->elem);
    lock_release (&frame_table_lock);
    free(frame);
		palloc_free_page(kpage);
  }
}

/* Repush frame on top of frame stack for victim selection
	 Reinsertion called when pagedir_get_page succeed */
void 
frame_reinsert(void* kpage)
{
	struct frame* frame = frame_lookup(kpage);
	
	ASSERT(frame != NULL);

	lock_acquire(&frame_table_lock);
	list_remove(&frame->elem);
	list_push_back(&frame_table, &frame->elem);
	lock_release(&frame_table_lock);
}

/* Select victim page to be swapped out */
struct frame* 
frame_select_victim(void)
{
	struct frame* frame;

	ASSERT(!list_empty(&frame_table));
	
	lock_acquire(&frame_table_lock);
	frame = list_entry(list_pop_front(&frame_table), struct frame, elem);
	lock_release(&frame_table_lock);
	
	return frame;
}

/* Returns the frame containing the given kernel virtual memory KPAGE,
   or a null pointer if no such frame exists. */
struct frame *
frame_lookup (void *kpage)
{
  struct list_elem *e;

	ASSERT(pg_ofs(kpage) == 0);

  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
    {
      struct frame* frame = list_entry (e, struct frame, elem);
      if (frame->kpage == kpage)
        return frame;
    }
  return NULL;
}

void 
frame_map(void* upage, void* kpage)
{
	ASSERT(pg_ofs(upage) == 0 && pg_ofs(kpage) == 0);
	struct frame* frame = frame_lookup(kpage);
	frame->upage = upage;
	frame->user = thread_current();
}

