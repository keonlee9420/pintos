#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

/* Hash functions. */
static unsigned hash_func (const struct hash_elem *p_, void *aux UNUSED);
static bool hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

/* Initialize supplemental page table built by hash table. */
void
supplemental_init ()
{
  hash_init (&s_page_table, hash_func, hash_less, NULL);
  lock_init (&s_page_table_lock);
}

/* Create and return new supplemental page. */
static struct s_page *
s_page_create (void *upage, void*kpage, struct file *file, struct swap *swap, off_t ofs, bool writable, size_t page_read_bytes, size_t page_zero_bytes)
{
  struct s_page *s_page = malloc(sizeof(struct s_page));
  struct hash_elem *hash_elem = malloc(sizeof(struct hash_elem));

  s_page->upage = pg_round_down(upage);
  s_page->kpage = kpage;
  s_page->hash_elem = *hash_elem; // am i right?
  if (file != NULL && swap == NULL)
    s_page->loader = LOAD_FROM_FILE;
  else if (file == NULL && swap != NULL)
    s_page->loader = LOAD_FROM_SWAP;
  s_page->file = file;
  s_page->swap = swap;
  s_page->ofs = ofs;
  s_page->writable = writable;
  s_page->page_read_bytes = page_read_bytes;
  s_page->page_zero_bytes = page_zero_bytes;
  return s_page;
}

/* Insert new supplemental page into supplemental page table. */
static struct hash_elem *
s_page_insert (struct s_page *p)
{
  struct hash_elem *hash_elem;
  /* Searches hash for an element equal to element. If none is found, inserts element into
     hash and returns a null pointer. If the table already contains an element equal to
     element, it is returned without modifying hash. */
  lock_acquire (&s_page_table_lock);
  hash_elem = hash_insert (&s_page_table, &p->hash_elem);
  lock_release (&s_page_table_lock);
  return hash_elem;
}

/* Returns the supplemental page containing the given virtual address,
   or a null pointer if no such page exists. */
struct s_page *
s_page_lookup (void *upage)
{
  struct s_page p;
  struct hash_elem *e;
  p.upage = upage;
  e = hash_find (&s_page_table, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct s_page, hash_elem) : NULL;
}

/* Delete and free the supplemental page which is currently allocated for upage.
   Return true if hash_delete is return hash_elem, otherwise return false. */
bool
free_s_page (void *upage)
{
  struct s_page *s_page;
  struct hash_elem *hash_elem;

  s_page = s_page_lookup (upage);
  if (s_page != NULL)
  {
    /* Searches hash for an element equal to element. If one is found, it is removed from
       hash and returned. Otherwise, a null pointer is returned and hash is unchanged. */
    lock_acquire (&s_page_table_lock);
    hash_elem = hash_delete (&s_page_table, &s_page->hash_elem);
    lock_release (&s_page_table_lock);
    if (hash_elem == NULL)
      return false;
    free (hash_elem);
    free (s_page);
  }
  return true;
}

/* Allocate(attach) supplemental page for upage.
   Return hash_elem without modifying hash if it's fail, otherwise return NULL. */
struct hash_elem *
allocate_s_page (void *upage, void*kpage, struct file *file, struct swap *swap, off_t ofs, bool writable, size_t page_read_bytes, size_t page_zero_bytes)
{
  struct s_page *s_page;
  struct hash_elem *hash_elem;

  s_page = s_page_create (upage, kpage, file, swap, ofs, writable, page_read_bytes, page_zero_bytes);
  hash_elem = s_page_insert (s_page);
  return hash_elem;
}

/* Returns a hash value for page p. */
static unsigned
hash_func (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct s_page *p = hash_entry (p_, struct s_page, hash_elem);
  return hash_bytes (&p->upage, sizeof p->upage);
}

/* Returns true if page a precedes page b. */
static bool
hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct s_page *a = hash_entry (a_, struct s_page, hash_elem);
  const struct s_page *b = hash_entry (b_, struct s_page, hash_elem);
  return a->upage < b->upage;
}