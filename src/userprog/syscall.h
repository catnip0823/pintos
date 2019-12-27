#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

/* struct owned by one process, which stores 
   the information of the file. */
struct file_struct
  {
    struct file *file;       /* store the file name. */
    struct list_elem elem;   /* List element. */
    struct dir *dir;         /* store the directory of the file. */
    int fd;                  /* store the file descripter of the file. */
  };


void syscall_init (void);

#endif /* userprog/syscall.h */
