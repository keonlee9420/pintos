#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <stdint.h>
#include "lib/kernel/hash.h"

/* Supplymental page */
struct s_page
{
  struct hash_elem hash_elem;   /* Hash table element. */
  void *addr;                   /* Virtual address. */
};

/* Supplymental page table */
struct hash s_page_table;

/* Initialization function */
void supplymental_init (void);

/* Supplymental page table functions */
struct s_page *s_page_lookup (void *vaddr);
bool free_s_page (void *vaddr);
struct hash_elem *allocate_s_page (void *vaddr);

#endif /* vm/page.h */