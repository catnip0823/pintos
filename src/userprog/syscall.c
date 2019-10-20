#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
//proj3
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
//proj3
void check_valid_pointer(void *pointer, int size_pointer);

void syscall_halt (void);
void syscall_exit (int status);
// pid_t syscall_exec (const char *cmd_line);
// int syscall_wait (pid_t pid);
// bool syscall_create (const char *file, unsigned initial_size);
// bool syscall_remove (const char *file);
// int syscall_open (const char *file);
// int syscall_filesize (int fd);
// int syscall_read (int fd, void *buffer, unsigned size);
int syscall_write (int fd, const void *buffer, unsigned size);
// void syscall_seek (int fd, unsigned position);
// unsigned syscall_tell (int fd);
// void syscall_close (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // printf ("system call!\n");
  //p3
  int *sp = f->esp;
  check_valid_pointer(sp, sizeof(int));
  switch((uint32_t)(*sp)){
  	case SYS_HALT:
  		break;
  	case SYS_EXIT:
  		check_valid_pointer(sp+1, sizeof(int));
  		syscall_exit((uint32_t)(*(sp+1)));
  		break;
  	case SYS_EXEC:
  		break;
  	case SYS_WAIT:
  		break;
  	case SYS_CREATE:
  		break;
  	case SYS_REMOVE:
  		break;
  	case SYS_OPEN:
  		break;
  	case SYS_FILESIZE:
  		break;
  	case SYS_READ:
  		break;
  	case SYS_WRITE:
  		check_valid_pointer(sp+1, 3*sizeof(int));
  		uint32_t parameter1 = (uint32_t)(*(sp+1));
  		uint32_t parameter2 = (uint32_t)(*(sp+2));
  		uint32_t parameter3 = (uint32_t)(*(sp+3));
  		f->eax = syscall_write(parameter1, parameter2, parameter3);
  		break;
  	case SYS_SEEK:
  		break;
  	case SYS_TELL:
  		break;
  	case SYS_CLOSE:
  		break;

  }


  thread_exit ();
}


void check_valid_pointer(void *pointer, int size_pointer){
	for(int i = 0; i < size_pointer; i++){
		if (pointer == NULL){
			syscall_exit(1);
		}
		if (is_user_vaddr(pointer) == false)
			syscall_exit(1);
		//poj3 have problem
		pointer++;
	}
}

void syscall_halt (void){}


void syscall_exit(int status){
	thread_current()->process_terminate_message = status;
	thread_exit();
}


// pid_t syscall_exec (const char *cmd_line){}
// int syscall_wait (pid_t pid){}
// bool syscall_create (const char *file, unsigned initial_size){}
// bool syscall_remove (const char *file){}
// int syscall_open (const char *file){}
// int syscall_filesize (int fd){}
// int syscall_read (int fd, void *buffer, unsigned size){}
int syscall_write (int fd, const void *buffer, unsigned size){

}
// void syscall_seek (int fd, unsigned position){}
// unsigned syscall_tell (int fd){}
// void syscall_close (int fd){}