#include "vm/page.h"
#include "threads/malloc.h"

/* Hash functions. */
static unsigned hash_func (const struct hash_elem *p_, void *aux UNUSED);
static bool hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

/* Initialize supplymental page table built by hash table. */
void
supplymental_init ()
{
  hash_init (&s_page_table, hash_func, hash_less, NULL);
}

/* Create and return new supplymental page. */
static struct s_page *
s_page_create (void *vaddr, enum page_loader loader, struct file *file, struct swap *swap, size_t page_read_bytes)
{
  struct s_page *s_page = malloc(sizeof(struct s_page));
  struct hash_elem *hash_elem = malloc(sizeof(struct hash_elem));

  s_page->hash_elem = *hash_elem; // am i right?
  s_page->addr = vaddr;
  s_page->loader = loader;
  s_page->file = file;
  s_page->swap = swap;
  s_page->page_read_bytes = page_read_bytes;
  return s_page;
}

/* Insert new supplymental page into supplymental page table. */
static struct hash_elem *
s_page_insert (struct s_page *p)
{
  /* Searches hash for an element equal to element. If none is found, inserts element into
     hash and returns a null pointer. If the table already contains an element equal to
     element, it is returned without modifying hash. */
  return hash_insert (&s_page_table, &p->hash_elem);
}

/* Returns the supplymental page containing the given virtual address,
   or a null pointer if no such page exists. */
struct s_page *
s_page_lookup (void *vaddr)
{
  struct s_page p;
  struct hash_elem *e;
  p.addr = vaddr;
  e = hash_find (&s_page_table, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct s_page, hash_elem) : NULL;
}

/* Delete and free the supplymental page which is currently allocated for vaddr.
   Return true if hash_delete is return hash_elem, otherwise return false. */
bool
free_s_page (void *vaddr)
{
  struct s_page *s_page;
  struct hash_elem *hash_elem;

  s_page = s_page_lookup (vaddr);
  if (s_page != NULL)
  {
    /* Searches hash for an element equal to element. If one is found, it is removed from
       hash and returned. Otherwise, a null pointer is returned and hash is unchanged. */
    hash_elem = hash_delete (&s_page_table, &s_page->hash_elem);
    if (hash_elem == NULL)
      return false;
    free (hash_elem);
    free (s_page);
  }
  return true;
}

/* Allocate(attach) supplymental page for vaddr.
   Return hash_elem without modifying hash if it's fail, otherwise return NULL. */
struct hash_elem *
allocate_s_page (void *vaddr, enum page_loader loader, struct file *file, struct swap *swap, size_t page_read_bytes)
{
  struct s_page *s_page;
  struct hash_elem *hash_elem;

  s_page = s_page_create (vaddr, loader, file, swap, page_read_bytes);
  hash_elem = s_page_insert (s_page);
  return hash_elem;
}

/* Returns a hash value for page p. */
static unsigned
hash_func (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct s_page *p = hash_entry (p_, struct s_page, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. */
static bool
hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct s_page *a = hash_entry (a_, struct s_page, hash_elem);
  const struct s_page *b = hash_entry (b_, struct s_page, hash_elem);
  return a->addr < b->addr;
}