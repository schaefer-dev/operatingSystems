#include "userprog/syscall.h"
#include <stdio.h>
#include "lib/string.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include <kernel/console.h>
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

static unsigned max_file_name = 14;

void syscall_init (void);
void validate_pointer(const void* pointer);
void* read_argument_at_index(struct intr_frame *f, int arg_offset);
void syscall_exit(const int exit_type);
void syscall_halt(void);
tid_t syscall_exec(const char *cmd_line);
int syscall_write(int fd, const void *buffer, unsigned size);
bool syscall_create(const char *file_name, unsigned initial_size);
bool syscall_remove(const char *file_name);
bool check_file_name(const char *file_name);
int syscall_open(const char *file_name);
int syscall_filesize(int fd);
struct file* get_file(int fd);
void syscall_seek(int fd, unsigned position);
unsigned syscall_tell(int fd);
void syscall_close(int fd);
struct list_elem* get_list_elem(int fd);

struct lock lock_filesystem;

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
  lock_init (&lock_filesystem);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void *esp = (int*) f->esp;

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
        char *cmd_line = *((char**) read_argument_at_index(f, 0));
        validate_pointer(cmd_line);
        f->eax = syscall_exec(cmd_line);
        break;
      }

    case SYS_WAIT:
      break;

    case SYS_CREATE:
      {
	//printf("start system create\n");
        char *file_name= *((char**) read_argument_at_index(f,0));
				validate_pointer(file_name);
				if (!check_file_name(file_name)){
					f->eax = false;
				} else {
		      unsigned initial_size = *((unsigned*) read_argument_at_index(f,sizeof(char*)));
		      f->eax = syscall_create(file_name, initial_size);
				}
        break;
      }

    case SYS_REMOVE:
      {
				char *file_name= *((char**) read_argument_at_index(f,0));
				validate_pointer(file_name);
        if (!check_file_name(file_name)){
          f->eax = false;
        } else {
          f->eax = syscall_remove(file_name);
        }
        break;
      }

    case SYS_OPEN:
      {
        char *file_name= *((char**) read_argument_at_index(f,0));
        validate_pointer(file_name);
        if (!check_file_name(file_name)){
          f->eax = false;
        } else {
          f->eax = syscall_open(file_name);
        }
        break;
      }

    case SYS_FILESIZE:
      {
        int fd = *((int*)read_argument_at_index(f,0)); 
        f->eax = syscall_filesize(fd);
        break;
      }

    case SYS_READ:
      break;

    case SYS_WRITE:
      {
        lock_acquire(&lock_filesystem);
        int fd = *((int*)read_argument_at_index(f,0)); 
        void *buffer = *((void**)read_argument_at_index(f,sizeof(int))); 
        unsigned size = *((unsigned*)read_argument_at_index(f,2*sizeof(int))); 
        int returnvalue = syscall_write(fd, buffer, size);
        f->eax = returnvalue;
        lock_release(&lock_filesystem);
        break;
      }

    case SYS_SEEK:
      {
        int fd = *((int*)read_argument_at_index(f,0)); 
        unsigned position = *((unsigned*)read_argument_at_index(f,sizeof(int)));
        syscall_seek(fd, position);
        break;
      }

    case SYS_TELL:
      {
        int fd = *((int*)read_argument_at_index(f,0)); 
        f->eax = syscall_tell(fd);
        break;
      }

    case SYS_CLOSE:
      {
        printf("enter sys_close syscall\n");
        int fd = *((int*)read_argument_at_index(f,0)); 
        syscall_close(fd);
        break;
      }

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
  //printf ("DEBUG: Forced thread_exit after FIRST syscall!\n");
  //thread_exit ();
}

void
validate_pointer(const void* pointer){
  // Validation of pointer
  uint32_t *pagedir = thread_current()->pagedir;
  if (pointer == NULL || !is_user_vaddr(pointer) || pagedir_get_page(pagedir, pointer)==NULL){
    // Exit if pointer is not valid
    syscall_exit(-1);
  }
}
  

// Read argument of frame f at location shift.
void *
read_argument_at_index(struct intr_frame *f, int arg_offset){

  void *esp = (void*) f->esp;
  validate_pointer(esp);

  void *argument = esp + sizeof(int) + arg_offset;
  validate_pointer(argument);

  return argument;
}

void
syscall_exit(const int exit_type){
  // TODO check for held locks
  // TODO notify parent and save exit value?
  printf("%s: exit(%d)\n", thread_current()->name, exit_type);
  thread_exit();
}

int
syscall_write(int fd, const void *buffer, unsigned size){
  struct file_desc *fd_struct;
  int returnvalue = 0;

  //printf("DEBUG: write started with fd |%i|!\n", fd);

  validate_pointer(buffer);

  // use temporary buffer to make sure we don't overflow?
  if (fd == STDOUT_FILENO){
    //printf("DEBUG: putbuff called with size |%i| and pointer |%p| with content |%s|!\n", size, buffer, buffer);
    putbuf(buffer,size);
    returnvalue = size;
  }
  else{
    // TODO read fd_struct and get file handler
    
    // check if file NULL
    if (fd_struct != NULL){
      returnvalue = -1;
    }

    // TODO I/O Operations, make sure its locked here

    returnvalue = size;
  }

  return returnvalue;

}

void
syscall_halt(){
  shutdown_power_off();
}

tid_t
syscall_exec(const char *cmd_line){
  // TODO make sure to not change program which is running during runtime (see project description)
  // TODO must return pid -1 (=TID_ERROR), if the program cannot load or run for any reason
  // TODO process_execute returns the thread id of the new process
  tid_t tid = process_execute(cmd_line); 
  return tid;  // return to process (tid)
}

bool
syscall_create(const char *file_name, unsigned initial_size){
  lock_acquire(&lock_filesystem);
  //printf("CREATE with file_name = |%s| and size %u\n", file, initial_size);
  bool success = filesys_create(file_name, initial_size);
  lock_release(&lock_filesystem);
  return success;
}

bool
syscall_remove(const char *file_name){
  lock_acquire(&lock_filesystem);
  bool success = filesys_remove(file_name);
  lock_release(&lock_filesystem);
  return success;
}

bool check_file_name (const char *file_name){
	if (file_name == NULL){
		syscall_exit(-1);
	} else if (strlen(file_name)>max_file_name){
		return false;
	}
	return true;
}

int syscall_open(const char *file_name){
  lock_acquire(&lock_filesystem);
  struct file *new_file = filesys_open(file_name);
  if (new_file == NULL){
    lock_release(&lock_filesystem);
    return -1;
  }
  struct thread *t = thread_current();
  int current_fd = t->current_fd;
  t->current_fd += 1;
  struct file_entry *current_entry = malloc(sizeof(struct file_entry));
  current_entry->fd = current_fd;
  current_entry->file = new_file;
  list_push_back (&t->file_list, &current_entry->elem);
  lock_release(&lock_filesystem);
  return current_fd;
}

int syscall_filesize(int fd){
  struct file* file = get_file(fd);
  if (file == NULL){
    return -1;
  }
  lock_acquire(&lock_filesystem);
  unsigned size = file_length(file);
  lock_release(&lock_filesystem);
  return size;
}

/* searchs the file in current thread */
struct file*
get_file(int fd){
  struct thread *t = thread_current();
  struct list files= t->file_list;
  struct list_elem *e;

  for (e = list_begin (&files); e != list_end (&files);
       e = list_next (e)) {
      struct file_entry *f = list_entry (e, struct file_entry, elem);
      if (f->fd == fd){
        return f->file;
      }
  }
  return NULL;
}

void syscall_seek(int fd, unsigned position){
  struct file *file = get_file(fd);
  if (file == NULL){
    syscall_exit(-1);
  }
  lock_acquire(&lock_filesystem);
  file_seek(file, position);
  lock_release(&lock_filesystem);
}

unsigned syscall_tell(int fd){
  struct file *file = get_file(fd);
  if (file == NULL){
    syscall_exit(-1);
  }
  lock_acquire(&lock_filesystem);
  unsigned pos = file_tell(file);
  lock_release(&lock_filesystem);
  return pos;
}

void syscall_close(int fd){
  lock_acquire(&lock_filesystem);
  printf("DEBUG: entering syscall close\n");
  struct list_elem *e = get_list_elem(fd);
  if (e == NULL){
    syscall_exit(-1);
  }
  struct file_entry *f = list_entry (e, struct file_entry, elem);
  if (f->file == NULL){
    syscall_exit(-1);
  }

  file_close(f->file);
  list_remove (&f->elem);
  lock_release(&lock_filesystem);
}

struct list_elem*
get_list_elem(int fd){
  printf("DEBUG: entering get elem\n");
  struct thread *t = thread_current();
  struct list files= t->file_list;
  struct list_elem *e;

  printf("DEBUG: List_size: |%i|\n", list_size(&files));
  for (e = list_begin (&files); e != list_end (&files);
       e = list_next (e)) {
    printf("DEBUG: entered_loop \n");
  }

  for (e = list_begin (&files); e != list_end (&files);
       e = list_next (e)) {
      struct file_entry *f = list_entry (e, struct file_entry, elem);
      if (f->fd == fd){
        return e;
      }
  }
  return NULL;
}
