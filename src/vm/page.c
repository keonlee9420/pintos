#include "vm/page.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/pte.h"

/* Hash functions. */
static unsigned hash_func (const struct hash_elem *p_, void *aux UNUSED);
static bool hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
static void spt_destructor(struct hash_elem* hash_elem, void* aux UNUSED);

static struct spage* lookup_spage(struct hash* spt, void* upage);


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

/* Allocate(attach) supplemental page for upage.
   Return hash_elem without modifying hash if it's fail, otherwise return NULL. */
bool
spage_allocate (void *upage, uint32_t pte)
{
	struct hash* spt = thread_current()->spt;
  struct spage* spage = malloc(sizeof(struct spage));

	if(spage == NULL)
		return false;

	spage->upage = upage;
	spage->pte = pte;

  hash_insert (spt, &spage->hash_elem);

	return true;
}

/* Delete and free the supplemental page which is currently allocated for upage.
   Return true if hash_delete is return hash_elem, otherwise return false. */
bool
spage_free (void *upage)
{
	struct hash* spt = thread_current()->spt;
  struct spage *spage = lookup_spage (spt, upage);

	if(spage == NULL)
		return false;

	/* Searches hash for an element equal to element. If one is found, it is removed from
     hash and returned. Otherwise, a null pointer is returned and hash is unchanged. */
	hash_delete (spt, &spage->hash_elem);
	free (spage);
	return true;
}

void 
spage_out(void* upage)
{
	struct hash* spt = thread_current()->spt;
	struct spage* spage = lookup_spage(spt, upage);

	spage->pte &= ~PTE_P;
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
  e = hash_find (spt, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct spage, hash_elem) : NULL;
}

/* Returns a hash value for page p. */
static unsigned
hash_func (const struct hash_elem *p_, void *aux UNUSED)
{
  struct spage *p = hash_entry (p_, struct spage, hash_elem);
  return hash_bytes (&p->upage, sizeof p->upage);
}

/* Returns true if page a precedes page b. */
static bool
hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  struct spage *a = hash_entry (a_, struct spage, hash_elem);
  struct spage *b = hash_entry (b_, struct spage, hash_elem);
  return a->upage < b->upage;
}

static void 
spt_destructor(struct hash_elem* hash_elem, void* aux UNUSED)
{
	struct spage* spage = hash_entry(hash_elem, struct spage, hash_elem);
	free(spage);
}
