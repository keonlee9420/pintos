#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <stdint.h>
#include "lib/kernel/hash.h"
#include "vm/swap.h"

/* Page loader flag. */
enum page_loader {
  LOAD_FROM_FILE,
  LOAD_FROM_SWAP
};

/* Supplymental page. */
struct s_page
{
  struct hash_elem hash_elem;   /* Hash table element. */
  void *addr;                   /* Virtual address. */
  /* Information for lazy loading. */
  enum page_loader loader;      /* Page loader flag. */
  struct file *file;            /* Target loading file if flag is LOAD_FROM_FILE. */
  struct swap *swap;            /* Target loading swap if flag is LOAD_FROM_SWAP. */
  size_t page_read_bytes;       /* Page read bytes. */
};

/* Supplymental page table. */
struct hash s_page_table;

/* Initialization function. */
void supplymental_init (void);

/* Supplymental page table functions. */
struct s_page *s_page_lookup (void *vaddr);
bool free_s_page (void *vaddr);
struct hash_elem *allocate_s_page (void *vaddr, enum page_loader loader, struct file *file, struct swap *swap, size_t page_read_bytes);

#endif /* vm/page.h */