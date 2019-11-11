#include "vm/page.h"
#include <string.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "vm/frame.h"

/* Hash functions. */
static unsigned hash_func (const struct hash_elem *p_, void *aux UNUSED);
static bool hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
static void spt_destructor(struct hash_elem* hash_elem, void* aux UNUSED);

/* Initialize supplemental page table built by hash table. */
struct hash* 
spt_create(void)
{
	struct hash* spt = malloc(sizeof(struct hash));

	hash_init(spt, hash_func, hash_less, NULL);

	return spt;
}

void 
spt_destroy(void)
{
	struct hash* spt = thread_current()->spt;

	hash_destroy(spt, spt_destructor);
}

/* Create supplemental page for upage. */
struct spage*
spage_create (void *upage, const char* filename, off_t ofs, size_t read_bytes, bool writable)
{
	struct hash* spt = thread_current()->spt;
  struct spage* spage = malloc(sizeof(struct spage));

	if(spage == NULL)
		return NULL;

	spage->upage = upage;
	spage->status = SPAGE_UNLOADED;
	spage->offset = ofs;
	spage->readbyte = read_bytes;
	spage->writable = writable;
	if(filename != NULL)
		strlcpy(spage->filename, filename, 16);

  hash_insert (spt, &spage->elem);

	return spage;
}

/* Map virtual page to physical address entry PTE. */
void 
spage_map(void* upage, uint32_t* pte)
{
	struct spage* spage = spage_lookup(upage);
	spage->pte = pte;
	spage->status = SPAGE_PRESENT;
	frame_map(upage, pte_get_page(*pte));
}

/* Delete and free the supplemental page which is currently allocated for upage.
   Return true if hash_delete is return hash_elem, otherwise return false. */
bool
spage_free (void *upage)
{
	struct hash* spt = thread_current()->spt;
  struct spage *spage = spage_lookup (upage);

	if(spage == NULL)
		return false;

	/* Searches hash for an element equal to element. If one is found, it is removed from
     hash and returned. Otherwise, a null pointer is returned and hash is unchanged. */
	hash_delete (spt, &spage->elem);
	free (spage);
	return true;
}

/* Returns the supplemental page containing the given virtual address,
   or a null pointer if no such page exists. */
struct spage *
spage_lookup (void *upage)
{
  struct spage p;
  struct hash_elem *e;
	struct hash* spt = thread_current()->spt;

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

static void 
spt_destructor(struct hash_elem* hash_elem, void* aux UNUSED)
{
	struct spage* spage = hash_entry(hash_elem, struct spage, elem);
	free(spage);
}
