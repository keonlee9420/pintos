#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <stdint.h>
#include "lib/kernel/hash.h"
#include "vm/swap.h"
#include "threads/synch.h"

/* Page loader flag. */
enum page_loader {
  LOAD_FROM_FILE,
  LOAD_FROM_SWAP
};

/* Supplymental page. */
struct s_page
{
  void *upage;                  /* Virtual address. */
  void *kpage;                  /* Kernel virtual address */
  struct hash_elem hash_elem;   /* Hash table element. */
  /* Information for lazy loading. */
  enum page_loader loader;      /* Page loader flag. */
  struct file *file;            /* Target loading file if flag is LOAD_FROM_FILE. */
  struct swap *swap;            /* Target loading swap if flag is LOAD_FROM_SWAP. */
  bool writable;                /* Writable. */
  size_t page_read_bytes;       /* Page read bytes. */
  size_t page_zero_bytes;       /* Page zero bytes. */
};

/* Supplymental page table. */
struct hash s_page_table;      /* Supplymental page table supplements the page table with additional data about each page. */
struct lock s_page_table_lock; /* Supplymental page table lock. */

/* Initialization function. */
void supplymental_init (void);

/* Supplymental page table functions. */
struct s_page *s_page_lookup (void *upage);
bool free_s_page (void *upage);
struct hash_elem *allocate_s_page (void *upage, void*kpage, struct file *file, struct swap *swap, bool writable, size_t page_read_bytes, size_t page_zero_bytes);

#endif /* vm/page.h */