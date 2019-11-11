#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdio.h>

void syscall_init (void);

/* Project3 S */
typedef uint32_t mapid_t;

void mmap_destroy(void);
struct mapid* mmap_get(mapid_t mapid);
/* Project3 E */

#endif /* userprog/syscall.h */
