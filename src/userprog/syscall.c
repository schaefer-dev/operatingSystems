#include "userprog/syscall.h"
#include <stdio.h>
#include "lib/string.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);

void syscall_init (void);
void validate_pointer(void* pointer);
void* read_argument_at_index(struct intr_frame *f, int arg_index);
void syscall_exit(const int exit_type);
void syscall_halt(void);
void syscall_exec(const char *cmd_line, struct intr_frame *f);

// TODO:
// save list of file descriptors in thread
// save list of child processes in thread
// save partent process in thread
// struct child-thread which contains *thread, parent,

// parse arguments in start_process and pass **arguments and num_arguments
// to load and to setup_stack where the real work will be done
// setup_stack setups stack like described in description

// lock file system operations with lock in syscall.c
// make sure to mark executables as "non writable" as explained in description
// nicht nach stin und stout lesen
// file descriptor table in thread as list 


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void *esp = (int32_t*) f->esp;

  validate_pointer(esp);

  // syscall type int is stored at adress esp
  int32_t syscall_type = *((int*)esp);

  switch (syscall_type)
    {
    case SYS_HALT:
      {
        syscall_halt();
        break;
      }

    case SYS_EXIT:
      {
        int exit_type = *((int*) read_argument_at_index(f, 0));
        syscall_exit(exit_type);
        break;
      }

    case SYS_EXEC:
      {
        char *cmd_line = (char*) read_argument_at_index(f, 0);
        syscall_exec(cmd_line, f);
        break;
      }

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
      break;

    case SYS_SEEK:
      break;

    case SYS_TELL:
      break;

    case SYS_CLOSE:
      break;

    case SYS_MMAP:
      // TODO project 3
      break;

    case SYS_MUNMAP:
      // TODO project 3
      break;

    case SYS_CHDIR:
      // TODO project 4
      break;

    case SYS_MKDIR:
      // TODO project 4
      break;

    case SYS_READDIR:
      // TODO project 4
      break;

    case SYS_ISDIR:
      // TODO project 4
      break;

    case SYS_INUMBER:
      // TODO project 4
      break;

    default:
      printf("this should not happen! DEFAULT SYSCALL CASE");
      break;
    }
  
  // TODO remove this later
  printf ("system call!\n");
  thread_exit ();
}

void
validate_pointer(void* pointer){
  // Validation of pointer
  if (pointer == NULL || !is_user_vaddr(pointer)){
    // Exit if pointer is not valid
    syscall_exit(-1);
  }
}
  

// Read argument of frame f at location shift.
void *
read_argument_at_index(struct intr_frame *f, int arg_index){

  void *esp = (void*) f->esp;
  validate_pointer(esp);

  void *argument = esp + 1 + arg_index;
  validate_pointer(argument);

  return argument;
}

void
syscall_exit(const int exit_type){
  // check for held locks
  char terminating_thread_name[16];
  strlcpy(terminating_thread_name, thread_name(), 16);
  printf("%s: exit(%d)\n", terminating_thread_name, exit_type);

}

void
syscall_halt(){
  shutdown_power_off();
}

void
syscall_exec(const char *cmd_line, struct intr_frame *f){
  // TODO make sure to not change program which is running during runtime (see project description)
  // TODO must return pid -1 (=TID_ERROR), if the program cannot load or run for any reason
  // TODO process_execute returns the thread id of the new process
  tid_t tid = process_execute(cmd_line); 
  f->eax = tid;  // return to process (tid)
}
