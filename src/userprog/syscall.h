#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

/* Project2 S */
void syscall_collapse_fd(void);
void syscall_exit(int status);
/* Project2 E */

#endif /* userprog/syscall.h */
