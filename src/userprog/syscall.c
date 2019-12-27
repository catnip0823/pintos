#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"

/* The max length of a command line. */
#define MAX_CMD_LEN 50

/* The lock to be used in system call. */
struct lock syscall_critical_section;

/* Function as the system call handler. */
static void syscall_handler (struct intr_frame *);

/* Function to check the validity of the pointer. */
void check_valid_pointer(void *pointer);
const char *check_physical_pointer(void *pointer);
void check_pointer(void* pointer, size_t size);

// proj4 helper functions
struct file* fd_to_file(int fd);
struct file_struct* file_to_struct(struct file* file);
struct dir* file_to_dir(struct file* file);

/* Function to each of the system call. */
void syscall_halt (void);
void syscall_exit (int status);
int syscall_exec (const char *cmd_line);
int syscall_wait (int pid);
bool syscall_create (const char *file, unsigned initial_size);
bool syscall_remove (const char *file);
int syscall_open (const char *file);
int syscall_filesize (int fd);
int syscall_read (int fd, void *buffer, unsigned size);
int syscall_write (int fd, const void *buffer, unsigned size);
void syscall_seek (int fd, unsigned position);
unsigned syscall_tell (int fd);
void syscall_close (int fd);

bool syscall_chdir (const char *dir);
bool syscall_mkdir (const char *dir);
bool syscall_readdir (int fd, char *name);
bool syscall_isdir (int fd);
int syscall_inumber (int fd);

/* Function to initialize the system call. */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&syscall_critical_section);
}


/* Function as system call handler. 
   Determine the type of the system call and 
   call corrosponding function. */
static void
syscall_handler (struct intr_frame *f UNUSED){
  /* Check whether the pointer is valid. */
  check_pointer((void *)f->esp, 4);

  /* Varibales to represent the argument. */
  int arg1;
  int arg2;
  int arg3;
  char *file;

  /* Determine the type of system call. */
  switch(*(int*) f->esp){
    case SYS_HALT:
      syscall_halt();
      break;
    case SYS_EXIT:
      /* Check validity of argument. */
      check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      syscall_exit((int)arg1);
      break;
    case SYS_EXEC:
      /* Check validity of argument. */
      check_valid_pointer((void*)((int*)f->esp + 1));
      check_valid_pointer((void*)*((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      f->eax = syscall_exec((int)arg1);
      break;
    case SYS_WAIT:
      /* Check validity of argument. */
      check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      f->eax = syscall_wait((int)arg1);
      break;
    case SYS_CREATE:
      /* Check validity of arguments. */
      check_valid_pointer((void*)((int*)f->esp + 1));
      check_valid_pointer((void*)((int*)f->esp + 2));
      arg1 = *((int*)f->esp+1);
      arg2 = *((int*)f->esp+2);
      file = (char*)arg1;
      file = check_physical_pointer((void*)file);
      f->eax = syscall_create(file, (unsigned int)arg2);
      break;
    case SYS_REMOVE:
      /* Check validity of arguments. */
      check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      file = (char*)arg1;
      file = check_physical_pointer((void*)file);
      f->eax = syscall_remove(file);
      break;
    case SYS_OPEN:
      /* Check validity of arguments. */
      check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      file = (char*)arg1;
      file = check_physical_pointer((void*)file);
      f->eax = syscall_open(file);
      break;
    case SYS_FILESIZE:
      /* Check validity of argument. */
      check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      f->eax = syscall_filesize((int)arg1);
      break;
    case SYS_READ:
      /* Check validity of arguments. */
      check_valid_pointer((void *)((int*)f->esp+1));
      check_valid_pointer((void *)((int*)f->esp+2));
      check_valid_pointer((void *)((int*)f->esp+3));
      arg1 = *((int*)f->esp+1);
      arg2 = *((int*)f->esp+2);
      arg3 = *((int*)f->esp+3);
      arg2 = check_physical_pointer((void*)arg2);
      f->eax = syscall_read((int)arg1, (void*)arg2, (unsigned int)arg3);
      break;
    case SYS_WRITE:
      /* Check validity of arguments. */
      check_valid_pointer((void *)((int*)f->esp+1));
      check_valid_pointer((void *)((int*)f->esp+2));
      check_valid_pointer((void *)((int*)f->esp+3));
      arg1 = *((int*)f->esp+1);
      arg2 = *((int*)f->esp+2);
      arg3 = *((int*)f->esp+3);
      arg2 = check_physical_pointer((void*)arg2);
      f->eax = syscall_write((int)arg1, (void*)arg2, (unsigned int)arg3);
      break;
    case SYS_SEEK:
      /* Check validity of arguments. */
      check_valid_pointer((void *)((int*)f->esp+1));
      check_valid_pointer((void *)((int*)f->esp+2));
      arg1 = *((int*)f->esp+1);
      arg2 = *((int*)f->esp+2);
      syscall_seek((int)arg1, (unsigned int)arg2);
      break;
    case SYS_TELL:
      /* Check validity of arguments. */
      check_valid_pointer((void *)((int*)f->esp+1));
      arg1 = *((int*)f->esp+1);
      f->eax = syscall_tell((int)arg1);
      break;
    case SYS_CLOSE:
      /* Check validity of arguments. */
      check_valid_pointer((void *)((int*)f->esp+1));
      arg1 = *((int*)f->esp+1);
      syscall_close((int)arg1);
      break;
    case SYS_CHDIR:
      check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      file = (char*)arg1;
      file = check_physical_pointer((void*)file);
      f->eax = syscall_chdir(file);
      break;
    case SYS_MKDIR:
      check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      file = (char*)arg1;
      file = check_physical_pointer((void*)file);
      f->eax = syscall_mkdir(file);
      break;
    case SYS_READDIR:
      check_valid_pointer((void *)((int*)f->esp+1));
      check_valid_pointer((void *)((int*)f->esp+2));
      arg1 = *((int*)f->esp+1);
      arg2 = *((int*)f->esp+2);
      arg2 = check_physical_pointer((void*)arg2);
      f->eax = syscall_readdir((int)arg1, (char*)arg2);
      break;
    case SYS_ISDIR:
      check_valid_pointer((void *)((int*)f->esp+1));
      arg1 = *((int*)f->esp+1);
      f->eax = syscall_isdir((int)arg1);
      break;
    case SYS_INUMBER:
      check_valid_pointer((void *)((int*)f->esp+1));
      arg1 = *((int*)f->esp+1);
      f->eax = syscall_inumber((int)arg1);
      break;
    default:
      syscall_exit(-1);
  }
}


/* Check the validity of the given pointer, including
   whether it is null, or beyond PHY_BASE, or is kernel
   addr, or invalid page */
void 
check_valid_pointer(void *pointer){
  if (pointer == NULL)
    syscall_exit(-1);
  if (is_user_vaddr(pointer) == false)
    syscall_exit(-1);
  if (is_kernel_vaddr(pointer))
    syscall_exit(-1);
  if (!pagedir_get_page(thread_current()->pagedir, pointer))
    syscall_exit(-1);
}


/* Check the validity of physical pointers, just
   like the function above, and return the char 
   pointer of the page directory */
const char*
check_physical_pointer(void *pointer){
  if (pointer == NULL)
    syscall_exit(-1);
  if (is_user_vaddr(pointer) == false)
    syscall_exit(-1);
  if (is_kernel_vaddr(pointer))
    syscall_exit(-1);
  void * ret_value = (void *)pagedir_get_page(
                     thread_current()->pagedir, pointer);
  if (!ret_value)
    syscall_exit(-1);
  return (const char *)ret_value;
}


/* Check the pointer with the given size,
   intend to deal with the boundary case.
   Implement by calling previous function. */
void 
check_pointer(void* pointer, size_t size){
  check_valid_pointer((uint8_t*)pointer + size - 1);
}


/* Function for system call wait, achieved by
   calling the function of shutdown_power_off */
void 
syscall_halt (void){
  shutdown_power_off();
}


/* Function to exit a process, first leave the terminate
   message into the thread structure and call thread_exit
   to exit the current thread. */
void 
syscall_exit(int status){
  thread_current()->process_terminate_message = status;
  thread_exit();
}


/* Function for execute the command in system call.
   by calling process_execute, also use a lock
   to deal with the synchronization. */
int 
syscall_exec (const char *cmd_line){
  lock_acquire(&syscall_critical_section);
  int ret_value = process_execute(cmd_line);
  lock_release(&syscall_critical_section);
  return ret_value;
}


/* System call for wait a process, by calling the
   function of process_wait(), return the return
   value returned by process_wait(). */
int 
syscall_wait (int pid){
  int ret_value = process_wait(pid);
  return ret_value;
}


/* System call for create the file, with give filename
   and initial file size. This function returns the
   return value returned by filesys_create. */
bool 
syscall_create (const char *file, unsigned initial_size){
  lock_acquire(&syscall_critical_section);
  bool ret_value = filesys_create(file, initial_size);
  lock_release(&syscall_critical_section);
  return ret_value;
}


/* System call for remove the file, given the
   filename, implement by calling the function
   in file system, return its return value. */
bool 
syscall_remove (const char *file){
  lock_acquire(&syscall_critical_section);
  bool ret_value = filesys_remove(file);
  lock_release(&syscall_critical_section);
  return ret_value;
}


/* System call for opening the file, give its
   filename as argument, return the current fd.
   Return -1 if fails to open the file. */
int 
syscall_open (const char *file){
  lock_acquire(&syscall_critical_section);
  struct file *new_file_open = filesys_open(file);
  /* If failed to open the file, then return -1 as error. */
  if (!new_file_open){
    lock_release(&syscall_critical_section);
    return -1;
  }
  struct file_struct *new_item = malloc(sizeof(struct file_struct));
  new_item->file = new_file_open;
  int ret_value = thread_current()->fd;
  thread_current()->fd++;
  new_item->fd = ret_value;

  struct inode *inode_open = file_get_inode(new_file_open);
  if (!inode_open)
    new_item->dir = NULL;
  else if (!inode_open->data.dir_or_file)
    new_item->dir = NULL;
  else
    new_item->dir = dir_open(inode_open);

  list_push_back(&thread_current()->files_per_process, &new_item->elem);
  lock_release(&syscall_critical_section);
  return ret_value;
}


/* System call to get the file size, given its fd.
   Check the validity of the argument first, then
   call the function of file_length and return. */
int 
syscall_filesize (int fd){  
  lock_acquire(&syscall_critical_section);
  struct file *current_file = fd_to_file(fd);
  if (!current_file){
    /* If givn fd of current thread is empty */
    lock_release(&syscall_critical_section);
    return -1;
  }
  int ret_value = (int)file_length(current_file);
  lock_release(&syscall_critical_section);
  return ret_value;
}


/* System call for read from the buffer, given the
   fd, size and pointer to the buffer. First check
   the validity of them then calling the function in
   file system, returns the interger of read status. */
int 
syscall_read (int fd, void *buffer, unsigned size){
  /* If fd == 0, it means out put to console. */
  if (fd == 0)
    return (int)input_getc();
  /* If fd == 1, then return 0 immediately. */
  if (fd == 1)
    return 0;
  lock_acquire(&syscall_critical_section);
  struct file *current_file = fd_to_file(fd);
  if (!current_file){
    /* If givn fd of current thread is empty */
    lock_release(&syscall_critical_section);
    return -1;
  }
  int ret_value = (int)file_read(current_file, buffer, size);
  lock_release(&syscall_critical_section);
  return ret_value;
}


/* System call to write to the file, given fd, a
   pointer to the buffer and the size. Check the 
   argument and call function in file system. */
int
syscall_write (int fd, const void *buffer, unsigned size){
  /* If it is to write to console. */
  if (fd == 1){
    putbuf(buffer, size);
    return size;
  }
  /* If fd == 0, then return 0. */
  if (fd == 0)
    return 0;
  lock_acquire(&syscall_critical_section);
  struct file *current_file = fd_to_file(fd);
  if (!current_file){
    /* If givn fd of current thread is empty */
    lock_release(&syscall_critical_section);
    return -1;
  }
  int ret_value = (int)file_write(current_file, buffer, size);
  lock_release(&syscall_critical_section);
  return ret_value;
}


/* System call for changes the next byte to be read
   or written in open file fd to position, expressed
   in bytes from the beginning of the file. */
void
syscall_seek (int fd, unsigned position){
  lock_acquire(&syscall_critical_section);
  struct file *current_file = fd_to_file(fd);
  if (!current_file){
    /* If givn fd of current thread is empty */
    lock_release(&syscall_critical_section);
    return;
  }
  file_seek(current_file, position);
  lock_release(&syscall_critical_section);
  return;
}


/* Returns the position of the next byte to be 
   read or written in open file fd, expressed 
   in bytes from the beginning of the file. */
unsigned 
syscall_tell (int fd){
  lock_acquire(&syscall_critical_section);
  struct file *current_file = fd_to_file(fd);
  if (!current_file){
    /* If givn fd of current thread is empty */
    lock_release(&syscall_critical_section);
    return;
  }
  unsigned ret_value = (unsigned)file_tell(current_file);
  lock_release(&syscall_critical_section);
  return ret_value;
}


/* Closes file descriptor fd. Exiting or terminating a 
   process implicitly closes all its open file descriptors,
   as if by calling this function for each one. */
void 
syscall_close (int fd){
  lock_acquire(&syscall_critical_section);
  struct file *current_file = fd_to_file(fd);
  if (!current_file){
    /* If givn fd of current thread is empty */
    lock_release(&syscall_critical_section);
    return -1;
  }

  struct file_struct *pf = file_to_struct(current_file);
  struct dir *current_dir = pf->dir;
  if (current_dir)
    dir_close(current_dir);
  file_close(current_file);
  list_remove(&pf->elem);
  lock_release(&syscall_critical_section);
}


bool
syscall_chdir(const char *dir){
  lock_acquire(&syscall_critical_section);
  if (dir[0] == '/'){
    if (!thread_current()->cwd)
      thread_current()->cwd = dir_open_root();
    lock_release(&syscall_critical_section);
    return true;
  }

  struct dir* temp_dir;
  if (!thread_current()->cwd)
    temp_dir = dir_open_root();
  else
    temp_dir = thread_current()->cwd;
  struct inode *inode;
  if (!dir_lookup(temp_dir, dir, &inode)){
    lock_release(&syscall_critical_section);
    return false;
  }
  dir_close(temp_dir);
  thread_current()->cwd = dir_open(inode);
  lock_release(&syscall_critical_section);
  if (temp_dir)
    return true;
  return false;
}


bool
syscall_mkdir(const char *dir){
  lock_acquire(&syscall_critical_section);
  block_sector_t sector;
  if(!free_map_allocate(1, &sector)){
    lock_release(&syscall_critical_section);
    return false;
  }

  char *copy_path = (char *)malloc(sizeof(char)*(strlen(dir)+1));
  memcpy(copy_path, dir, strlen (dir) + 1);
  
  char *save_ptr;
  char *curr_val = "";
  char *token = strtok_r(copy_path, "/", &save_ptr);
  while(token){
    curr_val = token;
    token = strtok_r(NULL, "/", &save_ptr);
  }

  struct dir *get_dir = find_leaf(dir);
  bool ret_value = dir_create(sector, 1, get_dir);
  if (!dir_add (get_dir, curr_val, sector))
    ret_value = false;
  lock_release(&syscall_critical_section);
  return ret_value;
}


bool
syscall_readdir(int fd, char *name){
  lock_acquire(&syscall_critical_section);
  struct file *current_file = fd_to_file(fd);
  if (!current_file){
    /* If givn fd of current thread is empty */
    lock_release(&syscall_critical_section);
    return false;
  }
  struct dir *current_dir = file_to_dir(current_file);
  bool ret_value = dir_readdir(current_dir, name);
  lock_release(&syscall_critical_section);
  return ret_value;
}


bool
syscall_isdir(int fd){
  lock_acquire(&syscall_critical_section);
  struct file *current_file = fd_to_file(fd);
  if (!current_file){
    /* If givn fd of current thread is empty */
    lock_release(&syscall_critical_section);
    return false;
  }
  struct inode *inode = file_get_inode(current_file);
  bool ret_value = inode->data.dir_or_file;
  lock_release(&syscall_critical_section);
  return ret_value;
}


int
syscall_inumber(int fd){
  lock_acquire(&syscall_critical_section);
  struct file *current_file = fd_to_file(fd);
  if (!current_file){
    /* If givn fd of current thread is empty */
    lock_release(&syscall_critical_section);
    return -1;
  }
  struct inode *inode = file_get_inode(current_file);
  int ret_value = inode_get_inumber(inode);
  lock_release(&syscall_critical_section);
  return ret_value;
}



// proj4 helper functions
struct file* fd_to_file(int fd){
  struct list_elem * e;
  for (e = list_begin(&thread_current()->files_per_process);
       e != list_end(&thread_current()->files_per_process);
       e = list_next(e)){
    struct file_struct *curr = list_entry(e, struct file_struct, elem);
    if (curr->fd == fd)
      return curr->file;
  }
  return NULL;
}

struct file_struct* file_to_struct(struct file* file){
  struct list_elem * e;
  for (e = list_begin(&thread_current()->files_per_process);
       e != list_end(&thread_current()->files_per_process);
       e = list_next(e)){
    struct file_struct *curr = list_entry(e, struct file_struct, elem);
    if (curr->file == file)
      return curr;
  }
  return NULL;
}

struct dir* file_to_dir(struct file* file){
  struct list_elem * e;
  for (e = list_begin(&thread_current()->files_per_process);
       e != list_end(&thread_current()->files_per_process);
       e = list_next(e)){
    struct file_struct *curr = list_entry(e, struct file_struct, elem);
    if (curr->file == file)
      return curr->dir;
  }
  return NULL;
}