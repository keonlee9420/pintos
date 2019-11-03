#include "vm/frame.h"
#include "threads/pte.h"
#include "threads/malloc.h"

/* Initialize frame page table. */
void 
framing_init (void)
{
  list_init (&frame_table);
  lock_init (&frame_table_lock);
}

/* Create and return new frame. */
static struct frame *
frame_create (void *upage, void *kpage, struct thread *user)
{
  struct frame *frame = malloc(sizeof(struct frame));
  uint32_t *page_table;
  uint32_t *page_table_entry;
  size_t pde_idx = pd_no (upage);
  size_t pte_idx = pt_no (upage);

  page_table = pde_get_pt (user->pagedir[pde_idx]);
  page_table_entry = (uint32_t *)page_table[pte_idx];

  frame->kpage = kpage;
  frame->upage = upage;
  frame->pte = page_table_entry;
  frame->user = user;

  return frame;
}

/* Insert new frame into frame table. */
static void
frame_insert_table (struct frame *frame)
{
	lock_acquire(&frame_table_lock);
  list_push_front (&frame_table, &frame->elem);
	lock_release(&frame_table_lock);
}

/* Returns the frame containing the given kernel virtual memory KPAGE,
   or a null pointer if no such frame exists. */
struct frame *
frame_lookup (void *kpage)
{
  struct frame *frame = NULL;
  struct list_elem *e;

  for (e = list_begin (&frame_table); e != list_end (&frame_table);
        e = list_next (e))
    {
      frame = list_entry (e, struct frame, elem);
      if (frame->kpage == kpage)
        return frame;
    }

  return frame;
}

/* Delete and free the frame which is currently allocated into KPAGE.*/
void
free_frame (void *kpage)
{
  struct frame *frame;

  frame = frame_lookup (kpage);
  if (frame != NULL)
  {
    list_remove (&frame->elem);
    free(frame);
  }
}

/* Allocate(attach) frame to kernel virtual address KPAGE which might be from user pool. */
void
allocate_frame (void *upage, void *kpage, struct thread *user)
{
  struct frame *frame;

  ASSERT ((user->pagedir)[pd_no (upage)] != 0);

  frame = frame_create (upage, kpage, user);
  frame_insert_table (frame);
}