#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

/* The max length of a command line. */
#define MAX_CMD_LEN 50

/* The lock to be used in system call. */
struct lock syscall_critical_section;

/* Function as the system call handler. */
static void syscall_handler (struct intr_frame *);

/* Function to check the validity of the pointer. */
void check_valid_pointer(void *pointer);
const char *check_physical_pointer(void *pointer);
int get_value(uint8_t * ptr);
void check_str(char* ptr);
void check_pointer(void* pointer, size_t size);
char *check_str_pro (const char *us);
int copy_to (uint8_t *dst, const uint8_t *usrc);

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
  // printf("%d\n", *(int*) f->esp);

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
  	// printf("hhhh\n");
      check_valid_pointer((void*)((int*)f->esp + 1));
      check_valid_pointer((void*)*((int*)f->esp + 1));
      // printf("hhhh\n");
      arg1 = *((int*)f->esp+1);
      // arg1 = check_physical_pointer((void*)arg1);
      //check_str((char*)arg1);
      //char *str = check_str_pro(*(char**)(f->esp+4));
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
    default:
  	  syscall_exit(-1);
  }
}


int 
get_value(uint8_t * ptr){
  if (!is_user_vaddr(ptr))
    return -1;
  int retval;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (retval) : "m" (*ptr));
  return retval;
}

/*
bool check_str(void* ptr){
  char c;
  c = get_value((uint8_t*)ptr);
  while (c != '\0' && c != -1){
    ptr++;
    c = get_value((uint8_t*)ptr);
  }
  if (c == '\0')
    return true;
  return false;
}*/

void 
check_str(char* ptr){
  int t = 0;
  while(t < MAX_CMD_LEN){
    if(is_user_vaddr(ptr) == false){
	    ASSERT(5 == 1);
	    syscall_exit(-1);}
    if (*ptr == '\0')
      return;
    t++;
    ptr++;
  }
  ASSERT(4==0);
  syscall_exit(-1);
}

char *
check_str_pro (const char *us)
{
  char *ks;
  size_t length;

  ks = palloc_get_page (0);
  if (ks == NULL)
    thread_exit ();

  for (length = 0; length < PGSIZE; length++)
    {
      if (us >= (char *) PHYS_BASE || copy_to (ks + length, us++) == 0)
        {
          palloc_free_page (ks);
          thread_exit ();
        }

      if (ks[length] == '\0')
        return ks;
    }
  ks[PGSIZE - 1] = '\0';
  return ks;
}

int
copy_to (uint8_t *dst, const uint8_t *usrc)
{
  int eax;
  asm ("movl $1f, %%eax; movb %2, %%al; movb %%al, %0; 1:"
       : "=m" (*dst), "=&a" (eax) : "m" (*usrc));
  return eax;
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
  lock_release(&syscall_critical_section);
  /* If failed to open the file, then return -1 as error. */
  if (!new_file_open)
    return -1;
  struct thread* t = thread_current();
  t->process_files[t->fd-2] = new_file_open;
  int ret_value = thread_current()->fd;
  thread_current()->fd++;
  return ret_value;
}


/* System call to get the file size, given its fd.
   Check the validity of the argument first, then
   call the function of file_length and return. */
int 
syscall_filesize (int fd){  
  /* If the given fd is too large */
  if (fd-2 >= PROCESS_FILE_MAX)
    return -1;
  /* If givn fd of current thread is empty */
  if (!thread_current()->process_files[fd-2])
    return -1;
  lock_acquire(&syscall_critical_section);
  struct file *current_file = thread_current()->process_files[fd-2];
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
  /* If fd is invalid, i.e. negative or beyond the limit. */
  if (fd < 0 || fd-2 >= PROCESS_FILE_MAX)
    return 0;
  /* If the file is null, return 0. */
  if (!thread_current()->process_files[fd-2])
    return 0;
  /* If everything is ok, then call the function. */
  /* Use a lock to deal with the synchronization. */
  struct file *current_file = thread_current()->process_files[fd-2];
  lock_acquire(&syscall_critical_section);
  int ret_value = (int)file_read(current_file, buffer, size);
  lock_release(&syscall_critical_section);
  return ret_value;
}


/* System call to write to the file, given fd, a
   pointer to the buffer and the size. Check the 
   argument and call function in file system. */
int syscall_write (int fd, const void *buffer, unsigned size){
  /* If it is to write to console. */
	if (fd == 1){
    lock_acquire(&syscall_critical_section);
		putbuf(buffer, size);
    lock_release(&syscall_critical_section);
		return size;
	}
  /* If fd == 0, then return 0. */
	if (fd == 0)
    return 0;
  /* If the fd is invalid, i.e. negative or beyond the limit. */
  if (fd < 0 || fd-2 >= PROCESS_FILE_MAX)
    return 0;
  /* If the file is null, then return 0 as error. */
  if (!thread_current()->process_files[fd-2])
    return 0;
  /* If everything is ok, then call file_write to write. */
  /* Use a lock to deal with the synchronization. */
  struct file *current_file = thread_current()->process_files[fd-2];
  lock_acquire(&syscall_critical_section);
  int ret_value = (int)file_write(current_file, buffer, size);
  lock_release(&syscall_critical_section);
  return ret_value;
}


/* System call for changes the next byte to be read
   or written in open file fd to position, expressed
   in bytes from the beginning of the file. */
void syscall_seek (int fd, unsigned position){
  /* If the given fd is too large or too small. */
  if (fd-2 >= PROCESS_FILE_MAX || fd < 2)
    return;
  /* If the current file is null, return immediately. */
  if (!thread_current()->process_files[fd-2])
    return;
  /* If everything is ok, then call the function. */
  /* Use a lock to deal with the synchronization. */
  struct file *current_file = thread_current()->process_files[fd-2];
  lock_acquire(&syscall_critical_section);
  file_seek(current_file, position);
  lock_release(&syscall_critical_section);
  return;
}


/* Returns the position of the next byte to be 
   read or written in open file fd, expressed 
   in bytes from the beginning of the file. */
unsigned 
syscall_tell (int fd){
  /* If the given fd is invalid. */
  if (fd-2 >= PROCESS_FILE_MAX || fd < 2)
    return;
  /* If the current file is null. */
  if (!thread_current()->process_files[fd-2])
    return;
  /* If everything is ok, then call the function. */
  /* Use a lock to deal with the synchronization. */
  struct file *current_file = thread_current()->process_files[fd-2];
  lock_acquire(&syscall_critical_section);
  unsigned ret_value = (unsigned)file_tell(current_file);
  lock_release(&syscall_critical_section);
  return ret_value;
}


/* Closes file descriptor fd. Exiting or terminating a 
   process implicitly closes all its open file descriptors,
   as if by calling this function for each one. */
void 
syscall_close (int fd){
  /* If the given fd is too large or too small. */
  if (fd-2 >= PROCESS_FILE_MAX || fd < 2)
    return;
  /* If the current file is null. */
  if (!thread_current()->process_files[fd-2])
    return;
  /* If everything is ok, then call the function. */
  /* Use a lock to deal with the synchronization. */
  struct file *current_file = thread_current()->process_files[fd-2]; 
  lock_acquire(&syscall_critical_section);
  file_close(current_file);
  thread_current()->process_files[fd-2] = NULL;
  lock_release(&syscall_critical_section);
}