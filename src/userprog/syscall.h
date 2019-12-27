#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

struct file_struct
  {
    struct file *file;
    struct list_elem elem;
    struct dir *dir;
    int fd;
  };


void syscall_init (void);

#endif /* userprog/syscall.h */
