#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
// //proj3

#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"


static void syscall_handler (struct intr_frame *);
//proj3
void check_valid_pointer(void *pointer);
const char *check_physical_pointer(void *pointer);
struct lock syscall_critical_section;

int get_value(uint8_t * ptr);
bool check_str(void* ptr);
void check_pointer(void* pointer, size_t size);

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

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&syscall_critical_section);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // ASSERT(1==0);
    check_pointer((void *)f->esp, 4);
	// printf ("Hex dump:\n");
 //    uint32_t dw = (uint32_t) PHYS_BASE - (uint32_t) f->esp;
 //    hex_dump ((uintptr_t) f->esp, f->esp, dw, true);

	int arg1;
	int arg2;
	int arg3;
  char *file;
  // printf("%d\n", *(int *) f->esp);

  switch(*(int *) f->esp){
  	case SYS_HALT:
      syscall_halt();
  		break;
  	case SYS_EXIT:
  		check_valid_pointer((void*)((int*)f->esp + 1));
  		arg1 = *((int*)f->esp+1);
  		syscall_exit((int)arg1);
  		break;
  	case SYS_EXEC:
      check_pointer((void*)((int*)f->esp + 1), 4);
      arg1 = *((int*)f->esp+1);
      //if (!check_str(*(char**)(int*)f->esp+1))
	      //syscall_exit(-1);
      arg1 = check_physical_pointer((void*)arg1);
  		f->eax = syscall_exec((char *)arg1);
      break;
  	case SYS_WAIT:
      check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      //char* t = *(char**)((int*)f->esp+1);
      //if (!check_str(t))
       // syscall_exit(-1);
      f->eax = syscall_wait((int)arg1);
  		break;
  	case SYS_CREATE:
      check_valid_pointer((void*)((int*)f->esp + 1));
      check_valid_pointer((void*)((int*)f->esp + 2));
      arg1 = *((int*)f->esp+1);
      arg2 = *((int*)f->esp+2);
      file = (char*)arg1;
      file = check_physical_pointer((void*)file);
      f->eax = syscall_create(file, (unsigned int)arg2);
  		break;
    case SYS_REMOVE:
      check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      file = (char*)arg1;
      file = check_physical_pointer((void*)file);
      f->eax = syscall_remove(file);
      break;
  	case SYS_OPEN:
  		check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      file = (char*)arg1;
      file = check_physical_pointer((void*)file);
      f->eax = syscall_open(file);
      break;
  	case SYS_FILESIZE:
      check_valid_pointer((void*)((int*)f->esp + 1));
      arg1 = *((int*)f->esp+1);
      f->eax = syscall_filesize((int)arg1);
  		break;
  	case SYS_READ:
  		check_valid_pointer((void *)((int*)f->esp+1));
      check_valid_pointer((void *)((int*)f->esp+2));
      check_valid_pointer((void *)((int*)f->esp+3));
      arg1 = *((int*)f->esp+1);
      arg2 = *((int*)f->esp+2);
      arg3 = *((int*)f->esp+3);
      // check_valid_pointer((void*)arg2);
      arg2 = check_physical_pointer((void*)arg2);
      f->eax = syscall_read((int)arg1, (void*)arg2, (unsigned int)arg3);
      break;
  	case SYS_WRITE:
	  	check_valid_pointer((void *)((int*)f->esp+1));
	  	check_valid_pointer((void *)((int*)f->esp+2));
	  	check_valid_pointer((void *)((int*)f->esp+3));
	  	arg1 = *((int*)f->esp+1);
	  	arg2 = *((int*)f->esp+2);
	  	arg3 = *((int*)f->esp+3);
      // check_valid_pointer((void*)arg2);
      arg2 = check_physical_pointer((void*)arg2);
  		f->eax = syscall_write((int)arg1, (void*)arg2, (unsigned int)arg3);
  		break;
  	case SYS_SEEK:
  		check_valid_pointer((void *)((int*)f->esp+1));
      check_valid_pointer((void *)((int*)f->esp+2));
      arg1 = *((int*)f->esp+1);
      arg2 = *((int*)f->esp+2);
      syscall_seek((int)arg1, (unsigned int)arg2);
      break;
  	case SYS_TELL:
      check_valid_pointer((void *)((int*)f->esp+1));
      arg1 = *((int*)f->esp+1);
      f->eax = syscall_tell((int)arg1);
  		break;
  	case SYS_CLOSE:
  		check_valid_pointer((void *)((int*)f->esp+1));
      arg1 = *((int*)f->esp+1);
      syscall_close((int)arg1);
      break;

  	syscall_exit(-1);
  	break;

  }


}

int get_value(uint8_t * ptr){
  if (!is_user_vaddr(ptr))
    return -1;
  int retval;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (retval) : "m" (*ptr));
  return retval;
}

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
}

void check_pointer(void* pointer, size_t size){
  check_valid_pointer((uint8_t*)pointer + size - 1);
}

void check_valid_pointer(void *pointer){
	if (pointer == NULL)
		syscall_exit(-1);
	if (is_user_vaddr(pointer) == false)
		syscall_exit(-1);
	if (is_kernel_vaddr(pointer))
		syscall_exit(-1);
	if (!pagedir_get_page(thread_current()->pagedir, pointer))
		syscall_exit(-1);
}

const char *check_physical_pointer(void *pointer){
	if (pointer == NULL)
		syscall_exit(-1);
	if (is_user_vaddr(pointer) == false)
		syscall_exit(-1);
	if (is_kernel_vaddr(pointer))
		syscall_exit(-1);
	void * ret_value = (void *)pagedir_get_page(thread_current()->pagedir, pointer);
	if (!ret_value)
		syscall_exit(-1);
	return (const char *)ret_value;
}

void syscall_halt (void){
  // lock_acquire(&syscall_critical_section);
  shutdown_power_off();
  // lock_release(&syscall_critical_section);
}

void syscall_exit(int status){
  // lock_acquire(&syscall_critical_section);
	thread_current()->process_terminate_message = status;
	thread_exit();
  // lock_release(&syscall_critical_section);
}

int syscall_exec (const char *cmd_line){
  lock_acquire(&syscall_critical_section);
  int ret_value = process_execute(cmd_line);
  lock_release(&syscall_critical_section);
  return ret_value;
}

int syscall_wait (int pid){
  // lock_acquire(&syscall_critical_section);
  int ret_value = process_wait(pid);
  // lock_release(&syscall_critical_section);
  return ret_value;
}

bool syscall_create (const char *file, unsigned initial_size){
  lock_acquire(&syscall_critical_section);
  bool ret_value = filesys_create(file, initial_size);
  lock_release(&syscall_critical_section);
  return ret_value;
}

bool syscall_remove (const char *file){
  lock_acquire(&syscall_critical_section);
  bool ret_value = filesys_remove(file);
  lock_release(&syscall_critical_section);
  return ret_value;
}

int syscall_open (const char *file){
  lock_acquire(&syscall_critical_section);
  struct file *new_file_open = filesys_open(file);
  if (!new_file_open){
    lock_release(&syscall_critical_section);
    return -1;
  }
  thread_current()->process_files[thread_current()->fd-2] = new_file_open;
  int ret_value = thread_current()->fd;
  thread_current()->fd++;
  lock_release(&syscall_critical_section);
  return ret_value;
}

int syscall_filesize (int fd){
  lock_acquire(&syscall_critical_section);
  if (fd-2 >= PROCESS_FILE_MAX){
    lock_release(&syscall_critical_section);
    return -1;
  }
  if (!thread_current()->process_files[fd-2]){
    lock_release(&syscall_critical_section);
    return -1;
  }
  struct file *current_file = thread_current()->process_files[fd-2];
  int ret_value = (int)file_length(current_file);
  lock_release(&syscall_critical_section);
  return ret_value;
}

int syscall_read (int fd, void *buffer, unsigned size){
  lock_acquire(&syscall_critical_section);
  if (fd == 0){
    lock_release(&syscall_critical_section);
    return (int)input_getc();
  }
  if (fd == 1){
    lock_release(&syscall_critical_section);
    return 0;
  }
  if (fd <= 1 || fd-2 >= PROCESS_FILE_MAX){
    lock_release(&syscall_critical_section);
    return 0;
  }
  if (!thread_current()->process_files[fd-2]){
    lock_release(&syscall_critical_section);
    return 0;
  }
  struct file *current_file = thread_current()->process_files[fd-2];
  int ret_value = (int)file_read(current_file, buffer, size);
  lock_release(&syscall_critical_section);
  return ret_value;
}

int syscall_write (int fd, const void *buffer, unsigned size){
  lock_acquire(&syscall_critical_section);
	if (fd == 1){
		putbuf(buffer, size);
    lock_release(&syscall_critical_section);
		return size;
	}
	if (fd == 0){
    lock_release(&syscall_critical_section);
    return 0;
  }
  if (fd <= 1 || fd-2 >= PROCESS_FILE_MAX){
    lock_release(&syscall_critical_section);
    return 0;
  }
  if (!thread_current()->process_files[fd-2]){
    lock_release(&syscall_critical_section);
    return 0;
  }
  struct file *current_file = thread_current()->process_files[fd-2];
  int ret_value = (int)file_write(current_file, buffer, size);
  lock_release(&syscall_critical_section);
  return ret_value;
}

void syscall_seek (int fd, unsigned position){
  lock_acquire(&syscall_critical_section);
  if (fd-2 >= PROCESS_FILE_MAX || fd < 2){
    lock_release(&syscall_critical_section);
    return;
  }
  if (!thread_current()->process_files[fd-2]){
    lock_release(&syscall_critical_section);
    return;
  }
  struct file *current_file = thread_current()->process_files[fd-2];
  file_seek(current_file, position);
  lock_release(&syscall_critical_section);
  return;
}
unsigned syscall_tell (int fd){
  lock_acquire(&syscall_critical_section);
  if (fd-2 >= PROCESS_FILE_MAX || fd < 2){
    lock_release(&syscall_critical_section);
    return;
  }
  if (!thread_current()->process_files[fd-2]){
    lock_release(&syscall_critical_section);
    return;
  }
  struct file *current_file = thread_current()->process_files[fd-2];
  unsigned ret_value = (unsigned)file_tell(current_file);
  lock_release(&syscall_critical_section);
  return ret_value;
}
void syscall_close (int fd){
  lock_acquire(&syscall_critical_section);
  if (fd-2 >= PROCESS_FILE_MAX || fd < 2){
    lock_release(&syscall_critical_section);
    return;
  }
  if (!thread_current()->process_files[fd-2]){
    lock_release(&syscall_critical_section);
    return;
  }
  struct file *current_file = thread_current()->process_files[fd-2];
  file_close(current_file);
  thread_current()->process_files[fd-2] = NULL;
  lock_release(&syscall_critical_section);
}
