#include "vm/page.h"
#include <debug.h>
#include <string.h>
#include <bitmap.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"

/* Hash functions. */
static unsigned hash_func (const struct hash_elem *p_, void *aux UNUSED);
static bool hash_less (const struct hash_elem *a_, 
											 const struct hash_elem *b_, void *aux UNUSED);
static void spt_destructor(struct hash_elem* hash_elem, void* aux UNUSED);

/* Initialize supplemental page table built by hash table. */
struct hash* 
spt_create(void)
{
	struct hash* spt = malloc(sizeof(struct hash));

	hash_init(spt, hash_func, hash_less, NULL);

	return spt;
}

/* Destroy all supplemental page table with its elements */
void 
spt_destroy(void)
{
	struct hash* spt = thread_current()->spt;

	/* Delete entire element of hash table */
	hash_destroy(spt, spt_destructor);

	/* Delete hash table */
	free(spt);
}

/* Create supplemental page for upage. */
struct spage*
spage_create (void *upage, int status, struct file* file, 
							off_t ofs, size_t read_bytes, bool writable)
{
	struct hash* spt = thread_current()->spt;
  struct spage* spage = malloc(sizeof(struct spage));

	if(spage == NULL)
		thread_exit();

	ASSERT(spage_lookup(spt, upage) == NULL);

	spage->upage = upage;
	spage->kpage = NULL;
	spage->status = status;
	spage->file = file;
	spage->offset = ofs;
	spage->readbyte = read_bytes;
	spage->writable = writable;
	spage->swapstat = false;

  hash_insert (spt, &spage->elem);

	return spage;
}

/* Map virtual page to physical address entry PTE. */
void 
spage_map(void* upage, void* kpage)
{
	struct hash* spt = thread_current()->spt;
	struct spage* spage = spage_lookup(spt, upage);
	
	ASSERT(pg_ofs(kpage) == 0);
	
	spage->kpage = kpage;
	frame_map(upage, kpage);
}

/* Delete and free the supplemental page which is currently allocated for upage.
   Return true if hash_delete is return hash_elem, otherwise return false. */
bool
spage_free (void *upage)
{
	struct hash* spt = thread_current()->spt;
  struct spage *spage = spage_lookup (spt, upage);
	uint32_t* pd = thread_current()->pagedir;

	if(spage == NULL)
		return false;

	/* Unmap physical frame */
	if(spage->kpage != NULL)
	{
		pagedir_clear_page(pd, upage);
		frame_free(spage->kpage);
	}
	
	/* Free swap disk sector encoded by spage */
	if(spage->swapstat)
		swap_free(spage->sector);

	/* Delete from hash table */
	hash_delete (spt, &spage->elem);
	free (spage);
	return true;
}

/* Returns the supplemental page containing the given virtual address,
   or a null pointer if no such page exists. */
struct spage *
spage_lookup (struct hash* spt, void *upage)
{
  struct spage p;
  struct hash_elem *e;

	ASSERT(pg_ofs(upage) == 0);

  p.upage = upage;
  e = hash_find (spt, &p.elem);
  return e != NULL ? hash_entry (e, struct spage, elem) : NULL;
}

/* Returns a hash value for page p. */
static unsigned
hash_func (const struct hash_elem *p_, void *aux UNUSED)
{
  struct spage *p = hash_entry (p_, struct spage, elem);
  return hash_bytes (&p->upage, sizeof p->upage);
}

/* Returns true if page a precedes page b. */
static bool
hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  struct spage *a = hash_entry (a_, struct spage, elem);
  struct spage *b = hash_entry (b_, struct spage, elem);
  return a->upage < b->upage;
}

/* Free spage element */
static void 
spt_destructor(struct hash_elem* hash_elem, void* aux UNUSED)
{
	struct spage* spage = hash_entry(hash_elem, struct spage, elem);
	if(spage->swapstat)
		swap_free(spage->sector);
	free(spage);
}
