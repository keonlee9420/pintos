#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void sys_exit (int status);
void sys_close_all (void);

#endif /* userprog/syscall.h */
