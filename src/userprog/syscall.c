#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
// //proj3

#include "threads/synch.h"
#include "threads/vaddr.h"


static void syscall_handler (struct intr_frame *);
//proj3
void check_valid_pointer(void *pointer);

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
    check_valid_pointer((void *)f->esp);
	// printf ("Hex dump:\n");
 //    uint32_t dw = (uint32_t) PHYS_BASE - (uint32_t) f->esp;
 //    hex_dump ((uintptr_t) f->esp, f->esp, dw, true);

	int arg1;
	int arg2;
	int arg3;
	int arg4;

  switch(*(int *) f->esp){
  	case SYS_HALT:
  		break;
  	case SYS_EXIT:
  		check_valid_pointer((void*)((int*)f->esp + 1));
  		arg1 = *((int*)f->esp+1);
  		syscall_exit((int)arg1);
  		break;
  	case SYS_EXEC:
  		break;
  	case SYS_WAIT:
  		break;
  	case SYS_CREATE:
  		break;
  		break;
  	case SYS_OPEN:
  		break;
  	case SYS_FILESIZE:
  		break;
  	case SYS_READ:
  		break;
  	case SYS_WRITE:

	  	check_valid_pointer((void *)((int*)f->esp+1));
	  	check_valid_pointer((void *)((int*)f->esp+2));
	  	check_valid_pointer((void *)((int*)f->esp+3));
	  	arg1 = *((int*)f->esp+1);
	  	arg2 = *((int*)f->esp+2);
	  	arg3 = *((int*)f->esp+3);

  		int fd = (int)arg1;
  		const void *buffer = pagedir_get_page(thread_current()->pagedir, (void *)arg2);
  		unsigned size = (unsigned int)arg3;
  		f->eax = syscall_write(fd, (void*)*((int*)f->esp+2), size);
  		break;
  	case SYS_SEEK:
  		break;
  	case SYS_TELL:
  		break;
  	case SYS_CLOSE:
  		break;

  	syscall_exit(-1);
  	break;

  }


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

// void syscall_halt (void){}


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

	if (fd == 1){
		putbuf(buffer, size);
		return size;
	}
	else
	{
		/* do something else with other fd */
	}

}
// void syscall_seek (int fd, unsigned position){}
// unsigned syscall_tell (int fd){}
// void syscall_close (int fd){}
