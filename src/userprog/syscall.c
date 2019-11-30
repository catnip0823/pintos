#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/kernel/list.h"

#include "userprog/exception.h"

/* The max length of a command line. */
#define MAX_CMD_LEN 50

/* The lock to be used in system call. */
struct lock syscall_critical_section;

/* Structure of the memory owned by a process. */
struct process_mmap{
  struct list_elem elem;  /* Elem to put into the list. */
  struct file* file;      /* A pointer to the mmap file. */
  void *addr;             /* the virtual address the file map to. */
  int id;                 /* the size of the mapped file. */
};


/* Function as the system call handler. */
static void syscall_handler (struct intr_frame *);

/* Function to check the validity of the pointer. */
void check_valid_pointer(void *pointer);
void check_vaid(void *pointer);
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
int syscall_mmap(int fd, void *addr);
void syscall_munmap(int mapping);
static int get_user (const uint8_t *uaddr);
void check_buffer(void *pointer);

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
  thread_current()->esp = f->esp;

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
      // syscall_halt();

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
      check_valid_pointer((void*)file);
      f->eax = syscall_create(file, (unsigned int)arg2);
  		break;
    case SYS_REMOVE:
      /* Check validity of arguments. */
      check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      file = (char*)arg1;
      check_valid_pointer((void*)file);
      f->eax = syscall_remove(file);
      break;
  	case SYS_OPEN:
      /* Check validity of arguments. */
  		check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      file = (char*)arg1;
      check_valid_pointer((void*)file);
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
      check_vaid((void*)arg2);
      check_buffer((void*)arg2);
      check_buffer((void*)arg2 + arg3 - 1);
      f->eax = syscall_read((int)arg1, (void*)arg2, (unsigned int)arg3);
      break;
  	case SYS_WRITE:
	  	arg1 = *((int*)f->esp+1);
	  	arg2 = *((int*)f->esp+2);
	  	arg3 = *((int*)f->esp+3);
      // check_physical_pointer((void*)arg2);
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

    /* for vm syscall mmap and munmap */
    #ifdef VM
    case SYS_MMAP:
      /* Check validity of arguments. */
      check_valid_pointer((void *)((int*)f->esp+1));
      check_valid_pointer((void *)((int*)f->esp+2));
      arg1 = *((int*)f->esp+1);
      arg2 = *((int*)f->esp+2);      
      f->eax = syscall_mmap((int)arg1, (void*)arg2);
      break;
    case SYS_MUNMAP:
      /* Check validity of arguments. */
      check_valid_pointer((void *)((int*)f->esp+1));
      arg1 = *((int*)f->esp+1);
      syscall_munmap((int)arg1);
      break;

    #endif


  }
}


/* Check the validity of the given pointer, including
   whether it is null, or beyond PHY_BASE, or is kernel
   addr, or invalid page */
void 
check_valid_pointer(void *pointer){
  /* if the given pointer is null, exit immediately */
	if (pointer == NULL)
		syscall_exit(-1);
  /* if the given pointer is not user addr, exit immediately */
	if (is_user_vaddr(pointer) == false)
		syscall_exit(-1);
  /* if the given pointer is kernel addr, exit immediately */
	if (is_kernel_vaddr(pointer))
		syscall_exit(-1);
  /* if cannot get physical address corresponds to 
  the current thread's pagedir, exit immediately */
	if (!pagedir_get_page(thread_current()->pagedir, pointer))
		syscall_exit(-1);
}


/* Check the validity of physical pointers, just
   like the function above, and return the char 
   pointer of the page directory */
void
check_vaid(void *pointer){
  /* if the given pointer is null, exit immediately */
  if (pointer == NULL)
    syscall_exit(-1);
  /* if the given pointer is not user addr, exit immediately */
  if (is_user_vaddr(pointer) == false)
    syscall_exit(-1);
  /* if the given pointer is kernel addr, exit immediately */
  if (is_kernel_vaddr(pointer))
    syscall_exit(-1);
}


/* Check the pointer with the given size,
   intend to deal with the boundary case.
   Implement by calling previous function. */
void 
check_pointer(void* pointer, size_t size){
  check_valid_pointer((uint8_t*)pointer + size - 1);
}


/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* For read syscall, check whether can read byte ret_value
   at user virtual address pointer, return immediately if
   a segfault occurred.  */
void check_buffer(void *pointer){
  if (get_user(pointer) == -1)
    syscall_exit(-1);
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
		// lock_acquire(&syscall_critical_section);
		putbuf(buffer, size);
		// lock_release(&syscall_critical_section);
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

/* Maps the file open as fd into the process's virtual
   address space. The entire file is mapped into 
   consecutive virtual pages starting at addr. */
int syscall_mmap(int fd, void *addr){
  /* fail if addr is 0. */
  /* fail if addr is not page-aligned. */
  if (addr == NULL || pg_ofs(addr) != 0)
    return -1;

  /* fd 0 and 1(console input and output) are not mappable. */
  if (fd == 0 || fd == 1)
    return -1;

  /* find the file with fd. */
  lock_acquire(&syscall_critical_section);
  struct file *current_file;
  if (thread_current()->process_files[fd-2])
    current_file = file_reopen(thread_current()->process_files[fd-2]);

  /* mmap fail if current_file has a length of zero bytes */
  if ((!current_file) || file_length(current_file) == 0){
    lock_release(&syscall_critical_section);
    return -1;
  }

  /* Find whether the range of pages mapped overlaps any
     existing set of mapped pages, if true return -1. */
  struct splmt_page_table *pt = thread_current()->splmt_page_table;
  for (size_t i = 0; i < file_length(current_file); i += PGSIZE){
    if (spage_table_find_entry(pt, addr + i)){
      /* If the virtual address is in the page table, 
         need not map and return -1 immediately. */
      lock_release(&syscall_critical_section);
      return -1;
    }
  }

  /* If all the conditions are satisfied, start mapping. */
  for (size_t i = 0; i < file_length(current_file); i += PGSIZE){
    uint32_t valid_bytes = file_length(current_file) - i;
    if (PGSIZE < valid_bytes)
      valid_bytes = PGSIZE;
    /* Add new entry to page table, with type FILE. */
    spage_table_add_file(pt, current_file, i, addr + i, valid_bytes, 
                         PGSIZE - valid_bytes, true);
  }

  /* allocate a new space to store the map of the process. */
  struct process_mmap *map = (struct process_mmap*)
                             malloc(sizeof(struct process_mmap));
  /* find mapping ID that identifies the mapping within the process. */
  int return_val;
  if (list_empty(&thread_current()->list_mmap))
    return_val = 1;
  else{
    /* find the last elem's mmap id of the mapped list. */
    struct list_elem * last_elem = list_back(&thread_current()->list_mmap);
    return_val = list_entry(last_elem, struct process_mmap, elem)->id + 1;
  }
  /* store the important imformation of the mapp. */
  map->id = return_val; 
  map->file = current_file;
  map->addr = addr;

  /* add the new map into the process's map list. */
  list_push_back(&thread_current()->list_mmap, &map->elem);
  lock_release(&syscall_critical_section);
  /* If finally success, returns a "mapping ID" uniquely
     identifies the mapping within the process */
  return return_val;
}


/* Unmaps the mapping designated by mapping, which must 
   be a mapping ID returned by a previous call to mmap 
   by the same process that has not yet been unmapped. */
void syscall_munmap(int mapping){
  struct process_mmap *map;
  /* check whether the mapping ID is in process's map list. */
  bool if_not_in_list = true;
  for (struct list_elem *item = list_begin(&thread_current()->list_mmap);
                         item != list_end(&thread_current()->list_mmap); 
                         item = list_next(item)){
    map = list_entry(item, struct process_mmap, elem);
    if (mapping == map->id){
      /* If mapping ID is in process's map list, break and ummap. */
      if_not_in_list = false;
      break;
    }
  }
  /* return immediately if mapping ID is in process's map list. */
  if (if_not_in_list)
    return;

  lock_acquire(&syscall_critical_section);
  /* Start ummapping. */
  for (size_t i = 0; i < file_length(map->file); i += PGSIZE){
    void *addr = map->addr + i;
    uint32_t valid_bytes = file_length(map->file) - i;
    if (PGSIZE < valid_bytes)
      valid_bytes = PGSIZE;
    /* All pages written to by the process 
       are written back to the file.*/
    spage_munmap(thread_current(), map->file, addr, i, valid_bytes);
  }
  /* Remove the map from the process's map list. */
  list_remove(&map->elem);
  lock_release(&syscall_critical_section);
}

/* Function called at process exit, free the 
   current process's resources and exit. */
void free_all(){
  struct list *mmlist = &thread_current()->list_mmap;
  struct list_elem *e = list_begin (mmlist);
  struct process_mmap *desc = list_entry(e, struct process_mmap, elem);
  /* ummap all the mapped memory owned by the process. */
  syscall_munmap (desc->id);
}
