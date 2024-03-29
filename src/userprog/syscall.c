#include "userprog/syscall.h"
#include <stdio.h>
#include "lib/string.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "kernel/console.h"
#include "devices/input.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "vm/sup_page.h"
#include "vm/frame.h"

static void syscall_handler (struct intr_frame *);

static int max_file_name = 14;

void syscall_init (void);
void validate_pointer(const void* pointer, void* esp);
void validate_buffer(const void* buffer, unsigned size, void* esp);
int validate_string(const char* buffer, void* esp);
void* read_argument_at_index(struct intr_frame *f, int arg_offset);
void syscall_exit(const int exit_type);
void syscall_halt(void);
tid_t syscall_exec(const char *cmd_line, void *esp);
int syscall_write(int fd, const void *buffer, unsigned size, void *esp);
int syscall_read(int fd, void *buffer, unsigned size, void *esp);
bool syscall_create(const char *file_name, unsigned initial_size, void *esp);
bool syscall_remove(const char *file_name, void *esp);
bool check_file_name(const char *file_name);
int syscall_open(const char *file_name, void *esp);
int syscall_filesize(int fd);
struct file* get_file(int fd);
void syscall_seek(int fd, unsigned position);
unsigned syscall_tell(int fd);
void syscall_close(int fd);
struct list_elem* get_list_elem(int fd);
bool validate_mmap(int fd, void *vaddr, void *esp);
bool validate_mmap_address(void *vaddr, void *esp);
bool check_mmap_overlap(void *vaddr, unsigned size, void *esp);
int syscall_mmap(int fd, void *vaddr, void *esp);
void syscall_munmap (mapid_t mapping);
void * read_mmap_argument_at_index(struct intr_frame *f, int arg_offset);

void load_and_pin_string(const void *buffer, void *esp);
void unpin_string(const void *buffer);
void load_and_pin_buffer(const void *buffer, unsigned size, void *esp);
void unpin_buffer(const void *buffer, unsigned size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&lock_filesystem);
}

/* takes care of reading arguments and passing them to the correct syscall
   function */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  if(f == NULL)
    syscall_exit(-1);
  void *esp = (int*) f->esp;

  validate_pointer(esp, esp);

  thread_current()->syscall_esp = esp;


  /* syscall type int is stored at adress esp */
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
        validate_pointer(cmd_line, esp);
        f->eax = syscall_exec(cmd_line, esp);
        break;
      }

    case SYS_WAIT:
      {
        pid_t pid = *((pid_t*) read_argument_at_index(f, 0));
        /* process_wait takes care of the work! */
        f->eax = process_wait(pid);
        break;
      }

    case SYS_CREATE:
      {
        char *file_name= *((char**) read_argument_at_index(f,0));
        validate_pointer(file_name, esp);
        unsigned initial_size = *((unsigned*) read_argument_at_index(f,sizeof(char*)));
        f->eax = syscall_create(file_name, initial_size, esp);
        break;
      }

    case SYS_REMOVE:
      {
        char *file_name= *((char**) read_argument_at_index(f,0));
        validate_pointer(file_name, esp);
        f->eax = syscall_remove(file_name, esp);
        break;
      }

    case SYS_OPEN:
      {
        char *file_name= *((char**) read_argument_at_index(f,0));
        validate_pointer(file_name, esp);
        f->eax = syscall_open(file_name, esp);
        break;
      }

    case SYS_FILESIZE:
      {
        int fd = *((int*)read_argument_at_index(f,0)); 
        f->eax = syscall_filesize(fd);
        break;
      }

    case SYS_READ:
      {
        int fd = *((int*)read_argument_at_index(f,0)); 
        void *buffer = *((void**)read_argument_at_index(f,sizeof(int))); 
        unsigned size = *((unsigned*)read_argument_at_index(f,2*sizeof(int))); 
        int returnvalue = syscall_read(fd, buffer, size, esp);
        f->eax = returnvalue;
      }
      break;

    case SYS_WRITE:
      {
        int fd = *((int*)read_argument_at_index(f,0)); 
        void *buffer = *((void**)read_argument_at_index(f,sizeof(int))); 
        unsigned size = *((unsigned*)read_argument_at_index(f,2*sizeof(int))); 
        int returnvalue = syscall_write(fd, buffer, size, esp);
        f->eax = returnvalue;
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
        int fd = *((int*)read_argument_at_index(f,0)); 
        syscall_close(fd);
        break;
      }

    case SYS_MMAP:
      {
        int fd = *((int*)read_argument_at_index(f,0));
        void *addr = *((void**)read_mmap_argument_at_index(f,sizeof(int)));
        f->eax = syscall_mmap(fd, addr, f->esp);
        break;
      }

    case SYS_MUNMAP:
      {
        int mapping = *((int*)read_argument_at_index(f,0)); 
        syscall_munmap(mapping);
        break;
      }

    case SYS_CHDIR:
      // project 4
      break;

    case SYS_MKDIR:
      // project 4
      break;

    case SYS_READDIR:
      // project 4
      break;

    case SYS_ISDIR:
      // project 4
      break;

    case SYS_INUMBER:
      // project 4
      break;

    default:
      {
        syscall_exit(-1);
        break;
      }
    }
  thread_current()->syscall_esp = NULL;
}


/* calls syscall_exit(-1) if the passed pointer is not valid in the current 
   context */
void
validate_pointer(const void *pointer, void *esp){
  struct thread *thread = thread_current();
  void *frame_pointer = pg_round_down(pointer);
  if (pointer == NULL || !is_user_vaddr(pointer)){
    syscall_exit(-1);
  }
  if (vm_sup_page_lookup(thread, frame_pointer) == NULL)
    if ((pointer) < esp - 32) 
      syscall_exit(-1);
}


/* calls syscall_exit(-1) if the passed buffer is not valid in the current 
   context */
void
validate_buffer(const void *buffer, unsigned size, void *esp){
  unsigned i = 0;
  const char* buffer_iter = buffer;
  while (i < (size)){
    validate_pointer(buffer_iter + i, esp);
    i += 1;
  }
}


/* calls syscall_exit(-1) if the passed "string" is not valid in the current 
   context, otherwise returns length of string */
int
validate_string(const char *buffer, void *esp){
  int length = 0;
  const char* buffer_iter = buffer;
  validate_pointer(buffer_iter, esp);
  while (true){
    if (*buffer_iter == '\0')
      break; 
      
    buffer_iter += 1;
    length += 1;
    validate_pointer(buffer_iter, esp);
  }

  return length;
}

/* load and pin the sup_pages of this buffer */
void
load_and_pin_buffer(const void *buffer, unsigned size, void *esp){
  void *last_vm_addr = NULL;

  unsigned i = 0;
  const char* buffer_iter = buffer;
  while (i < (size)){
    void *vm_addr = pg_round_down(buffer_iter);
    if (vm_addr == last_vm_addr){
      i += 1;
      continue;
    }
    last_vm_addr = vm_addr;
    if (((int32_t)buffer_iter + 32 >= (int32_t)esp) &&
          ((int32_t)buffer_iter < (int32_t)PHYS_BASE) &&
          ((int32_t)PHYS_BASE - (int32_t)STACK_SIZE <=
                                  (int32_t)vm_addr)){
      vm_grow_stack(vm_addr);
    }
    vm_sup_page_load_and_pin(vm_sup_page_lookup(thread_current(), vm_addr));
    i += 1;
  }
}

/* unpin the sup_pages of this buffer */
void
unpin_buffer(const void *buffer, unsigned size){
  void *last_vm_addr = NULL;

  unsigned i = 0;
  const char* buffer_iter = buffer;

  while (i < (size)){
    void *vm_addr = pg_round_down(buffer_iter);
    if (vm_addr == last_vm_addr){
      i += 1;
      continue;
    }
    last_vm_addr = vm_addr;
    vm_sup_page_unpin(vm_sup_page_lookup(thread_current(), vm_addr));
    i += 1;
  }
}

/* load and pin the sup_pages of this string */
void
load_and_pin_string(const void *buffer, void *esp){
  void *last_vm_addr = NULL;

  const char* buffer_iter = buffer;
  while (true){
    if (*buffer_iter == '\0')
      break; 
      
    void *vm_addr = pg_round_down(buffer_iter);
    if (vm_addr == last_vm_addr){
      buffer_iter += 1;
      continue;
    }
    last_vm_addr = vm_addr;
    if (((int32_t)buffer_iter + 32 >= (int32_t)esp) && 
          ((int32_t)buffer_iter < (int32_t)PHYS_BASE) && 
          ((int32_t)PHYS_BASE - (int32_t)STACK_SIZE <= 
                  (int32_t)vm_addr)){
      vm_grow_stack(vm_addr);
    }
    vm_sup_page_load_and_pin(vm_sup_page_lookup(thread_current(), vm_addr));
    buffer_iter += 1;
  }

}

/* unpin the sup_pages of this string */
void
unpin_string(const void *buffer){
  void *last_vm_addr = NULL;

  const char* buffer_iter = buffer;
  while (true){
    if (*buffer_iter == '\0')
      break; 
      
    void *vm_addr = pg_round_down(buffer_iter);
    if (vm_addr == last_vm_addr){
      buffer_iter += 1;
      continue;
    }
    last_vm_addr = vm_addr;
    vm_sup_page_unpin(vm_sup_page_lookup(thread_current(), vm_addr));
    buffer_iter += 1;
  }
}


/* return argument of frame f at location shift */
void *
read_argument_at_index(struct intr_frame *f, int arg_offset){

  void *esp = (void*) f->esp;
  validate_pointer(esp, esp);

  void *argument = esp + sizeof(int) + arg_offset;
  validate_pointer(argument, esp);

  return argument;
}

void *
read_mmap_argument_at_index(struct intr_frame *f, int arg_offset){
  void *esp = (void*) f->esp;
  void *argument = esp + sizeof(int) + arg_offset;
  return argument;
}

/* execute the exit syscall with the passed exit_type 
   Terminates the current user program, returning status
   to the kernel  */
void
syscall_exit(const int exit_type){

  struct thread* terminating_thread = thread_current();
  struct child_process* terminating_child = terminating_thread->child_process;

  if (terminating_child != NULL){
    ASSERT(!lock_held_by_current_thread(&terminating_child->child_process_lock));
    lock_acquire(&terminating_child->child_process_lock);
    if (terminating_child->parent != -1){
      /* parent is still running -> has to store information */
      terminating_child->terminated = true;
      terminating_child->exit_status = exit_type;
      cond_signal(&terminating_child->terminated_cond, &terminating_child->child_process_lock);
      lock_release(&terminating_child->child_process_lock);
    } else {
      /* parent is already terminated -> free ressources */
      lock_release(&terminating_child->child_process_lock);
      free(terminating_child);
      terminating_child = NULL;
    }
  }

  if (lock_held_by_current_thread(&lock_filesystem))
    lock_release(&lock_filesystem);
  if (lock_held_by_current_thread(&frame_lock))
    lock_release(&frame_lock);

  /* close all files in this thread and free ressources */
  clear_files();

  printf("%s: exit(%d)\n", thread_current()->name, exit_type);

  thread_exit();
}


/* Writes size bytes from the file with the filedescriptor fd into
   the passed buffer. Returns the number of bytes actually written, 
   which could be less than size if some bytes could not be read. */
int
syscall_write(int fd, const void *buffer, unsigned size, void *esp){

  int returnvalue = 0;

  /* check if the entire buffer is valid */
  validate_buffer(buffer,size, esp);

  load_and_pin_buffer(buffer, size, esp);

  if (fd == STDOUT_FILENO){
    putbuf(buffer,size);
    returnvalue = size;
  }
  else if (fd == STDIN_FILENO){
    returnvalue = -1;
  }
  else{
    struct file *file_ = get_file(fd);
    
    // check if file NULL
    if (file_ == NULL){
      returnvalue = 0;
    }else{
      lock_acquire(&lock_filesystem);
      returnvalue = file_write(file_, buffer, size);
      lock_release(&lock_filesystem);
    }
  }
  unpin_buffer(buffer, size);
  return returnvalue;
}


/* reads size amount of bytes from the file with the file descryptor fd.
   Returns the number of bytes actually read or -1 if the file could not
   be read */
int
syscall_read(int fd, void *buffer, unsigned size, void *esp){
  int returnvalue = 0;

  /* check if the entire buffer is valid */
  validate_buffer(buffer, size, esp);

  load_and_pin_buffer(buffer, size, esp);

  if (fd == STDOUT_FILENO){
    returnvalue = -1;
  }
  else if (fd == STDIN_FILENO){
    unsigned size_left = size;
    uint8_t *buffer_copy = (uint8_t *)buffer;
    uint8_t input_char = 0;
      
    while (size_left > 1){
      input_char = input_getc();
      if (input_char == 0)
        break;
      else{
        size_left -= 1;
        *buffer_copy = input_char;
        buffer_copy += 1;
      }
    }

    returnvalue = size - size_left;
  }
  else{
    struct file *file_ = get_file(fd);
    
    if (file_ == NULL){
      returnvalue = 0;
    }else{
      lock_acquire(&lock_filesystem);
      returnvalue = file_read(file_, buffer, size);
      lock_release(&lock_filesystem);
    }
  }

  unpin_buffer(buffer, size);
  return returnvalue;
}

/* Terminates pintos */
void
syscall_halt(){
  shutdown_power_off();
}


/* Runs the passed executable and returns the new process's
   program id. Returns -1 if the program cannot load or run
   for any reason. */
tid_t
syscall_exec(const char *cmd_line, void *esp){

  if (cmd_line == NULL){
    return -1;
  }

  int length = validate_string(cmd_line, esp);
  if (length == 0){
    return -1;
  }

  load_and_pin_string(cmd_line, esp);
  pid_t pid = process_execute(cmd_line);
  
  struct child_process *current_child = get_child(pid);
  if (current_child == NULL){
    unpin_string(cmd_line);
    return -1;
  }

  ASSERT(!lock_held_by_current_thread(&current_child->child_process_lock));
  lock_acquire(&current_child->child_process_lock);

  while(current_child->successfully_loaded == NOT_LOADED){
    cond_wait(&current_child->loaded, &current_child->child_process_lock);
  }

  if (current_child->successfully_loaded == LOAD_FAILURE){
    lock_release(&current_child->child_process_lock);
    unpin_string(cmd_line);
    return -1;
  }

  lock_release(&current_child->child_process_lock);
  unpin_string(cmd_line);
  return pid;  // return to process pid
}


/* Creates a new file with the name file_name with size initial_size.
   Returns true if successful, false otherwise. 
   NOTE: it does not open the file! */
bool
syscall_create(const char *file_name, unsigned initial_size, void *esp){
  int length = validate_string(file_name, esp);
  if (length > max_file_name)
    return false;
  load_and_pin_string(file_name, esp);
  ASSERT(!lock_held_by_current_thread(&lock_filesystem));
  lock_acquire(&lock_filesystem);
  bool success = filesys_create(file_name, initial_size);
  lock_release(&lock_filesystem);
  unpin_string(file_name);
  return success;
}


/* deletes the file file_name and returns true if successful, false otherwise */
bool
syscall_remove(const char *file_name, void *esp){
  int length = validate_string(file_name, esp);
  if (length > max_file_name)
    return false;
  load_and_pin_string(file_name, esp);
  ASSERT(!lock_held_by_current_thread(&lock_filesystem));
  lock_acquire(&lock_filesystem);
  bool success = filesys_remove(file_name);
  lock_release(&lock_filesystem);
  unpin_string(file_name);
  return success;
}

/* opens the file file_name, returns non-negative file descriptor for
   the opened file if succesful, -1 otherwise */
int syscall_open(const char *file_name, void *esp){
  int length = validate_string(file_name, esp);
  if (length > max_file_name)
    return -1;
  load_and_pin_string(file_name, esp);
  ASSERT(!lock_held_by_current_thread(&lock_filesystem));
  lock_acquire(&lock_filesystem);
  struct file *new_file = filesys_open(file_name);
  if (new_file == NULL){
    lock_release(&lock_filesystem);
    unpin_string(file_name);
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
  unpin_string(file_name);
  return current_fd;
}


/* returns the size of the file with the file descriptor fd in bytes */
int syscall_filesize(int fd){
  struct file* file = get_file(fd);
  if (file == NULL){
    return -1;
  }
  ASSERT(!lock_held_by_current_thread(&lock_filesystem));
  lock_acquire(&lock_filesystem);
  unsigned size = file_length(file);
  lock_release(&lock_filesystem);
  return size;
}


/* searchs the file in current thread */
struct file*
get_file(int fd){
  struct thread *t = thread_current();
  struct list *open_files = &(t->file_list);

  if (list_empty(open_files))
      return NULL;

  struct list_elem *iterator = list_begin (open_files);

  while (iterator != list_end (open_files)){
      struct file_entry *f = list_entry (iterator, struct file_entry, elem);
      if (f->fd == fd)
        return f->file;
      iterator = list_next(iterator);
  }
  return NULL;
}

/* clears all open files in current thread */
void
clear_files(){
  struct thread *t = thread_current();
  struct list *open_files = &(t->file_list);

  if (list_empty(open_files))
      return;

  struct list_elem *iterator = list_begin (open_files);

  while (iterator != list_end (open_files)){
      struct file_entry *f = list_entry (iterator, struct file_entry, elem);
      if (f->file != NULL){
        file_close(f->file);
      }
      struct list_elem *removeElem = iterator; 
      iterator = list_next(iterator);
      list_remove (removeElem);
      free(f);
  }
}


/* searchs the file in current thread and returns list_elem */
struct list_elem*
get_list_elem(int fd){
  struct thread *t = thread_current();
  struct list *open_files= &(t->file_list);
  
  if (list_empty(open_files))
      return NULL;

  struct list_elem *iterator = list_begin (open_files);


  while (iterator != list_end(open_files)){
      struct file_entry *f = list_entry (iterator, struct file_entry, elem);
      if (f->fd == fd)
        return iterator;
      iterator = list_next(iterator);
  }
  return NULL;
}


/* Changes the next byte to be read or written in the file with filedescriptor
   fd to the position position. */
void syscall_seek(int fd, unsigned position){
  struct file *file = get_file(fd);
  if (file == NULL){
    syscall_exit(-1);
  }
  ASSERT(!lock_held_by_current_thread(&lock_filesystem));
  lock_acquire(&lock_filesystem);
  file_seek(file, position);
  lock_release(&lock_filesystem);
}


/* Returns the position of the next byte to be read or written in the file with
   file-descriptor fd */
unsigned syscall_tell(int fd){
  struct file *file = get_file(fd);
  if (file == NULL){
    syscall_exit(-1);
  }
  ASSERT(!lock_held_by_current_thread(&lock_filesystem));
  lock_acquire(&lock_filesystem);
  unsigned pos = file_tell(file);
  lock_release(&lock_filesystem);
  return pos;
}


/* Closes the file with filedescriptor fd */
void syscall_close(int fd){
  ASSERT(!lock_held_by_current_thread(&lock_filesystem));
  lock_acquire(&lock_filesystem);
  struct list_elem *element = get_list_elem(fd);

  if (element == NULL){
    lock_release(&lock_filesystem);
    return;
  }
  struct file_entry *f = list_entry (element, struct file_entry, elem);

  if (f->file == NULL){
    lock_release(&lock_filesystem);
    return;
  }

  file_close(f->file);
  list_remove (element);
  free(f);
  lock_release(&lock_filesystem);
}

/* checks if vaddr is page aligned and fd is coorect */
bool validate_mmap(int fd, void *vaddr, void *esp){
  /* check if fd is 0 or 1 because this is invalid */
  if( fd == 0 || fd == 1){
    printf("fd is not correct\n");
    return false;
  }

  /* check if vaddr is 0 or not page aligned
    because this is invalid */
  if ((uint32_t)vaddr == 0 || ((uint32_t)vaddr % PGSIZE) != 0 ||
	!(is_user_vaddr(vaddr))){
    return false;
  }

  /* check if mapping overlaps previous mappings */
  if (vm_sup_page_lookup(thread_current(), vaddr)){
    return false; 
  }

  /* check if it doesnt grow over stack */
  if (vaddr + PGSIZE >= esp)
    return false;

  return true;
}

/* checks if vaddr is already mapped -> overlap
 returns false if maped address overlaps */
bool validate_mmap_address(void *vaddr, void *esp){
  if (vm_sup_page_lookup(thread_current(), vaddr) || !(is_user_vaddr(vaddr)))
    return false; 

  /* check if it doesnt grow over stack */
  if (vaddr + PGSIZE >= esp)
    return false;

  return true;
}

/* checks if mmap overlaps */
bool check_mmap_overlap(void *vaddr, unsigned size, void *esp){
  while (size > 0) {
    /* Calculate how to fill this page. */
    off_t page_read_bytes = size;
    if (size > PGSIZE){
      page_read_bytes = PGSIZE;
    }

    if (!validate_mmap_address(vaddr, esp)){
      return false;
    }
    size -= page_read_bytes;
    vaddr += PGSIZE;
  }
  return true;
}

/* syscall to create mmap files */
mapid_t syscall_mmap(int fd, void *vaddr, void *esp){
  if (!validate_mmap(fd, vaddr, esp))
    return -1;

  lock_acquire(&lock_filesystem);
  struct file *open_file = get_file(fd);
  if (open_file == NULL){
    lock_release(&lock_filesystem);
    return -1;
  }

  /* reopen file as mentioned in description */
  struct file* file = file_reopen(open_file);
  if (file == NULL){
    lock_release(&lock_filesystem);
    return -1;
  }

  unsigned size = file_length(file);
  if (size == 0){
    lock_release(&lock_filesystem);
    return -1;
  }

  struct thread *thread = thread_current();
  mapid_t current_mmapid = thread->current_mmapid;
  thread->current_mmapid += 1;
  lock_release(&lock_filesystem);

  /* check if mmap overlaps */
  if (!check_mmap_overlap(vaddr, size, esp))
    return -1;

  /* offset in file is initially 0 */
  off_t ofs = 0;
  void *start_vaddr = vaddr;
  unsigned needed_pages = 0;

  /* save file in sup_page */
  while (size > 0) {
    /* Calculate how to fill this page. */
    off_t page_read_bytes = size;
    if (size > PGSIZE){
      page_read_bytes = PGSIZE;
    }

    /* Add page to supplemental page table */
    if (!vm_sup_page_mmap_allocate (vaddr, file, ofs, page_read_bytes, 
          current_mmapid, true)){
      return -1;
    }
    needed_pages += 1;
    ofs += page_read_bytes;
    size -= page_read_bytes;
    vaddr += PGSIZE;
  }

  /* add mmap_entry to mmap hashmap */
  struct mmap_entry *mmap_entry = (struct mmap_entry *) 
        malloc(sizeof(struct mmap_entry));
  mmap_entry->mmap_id = current_mmapid;
  mmap_entry->start_vaddr = start_vaddr;
  mmap_entry->needed_pages = needed_pages;

  struct hash_elem *insert_elem;
  insert_elem = hash_insert (&(thread->mmap_hashmap), &(mmap_entry->h_elem));
  if (insert_elem != NULL) {
    PANIC("mmap could not be inserted in hash map\n");
  }

  return current_mmapid; 
}

/* syscall to remove and write back mmap files */
void syscall_munmap (mapid_t mapping){
  struct thread *thread = thread_current();
  /* get mmap entry from hash map and check if one is found */
  struct mmap_entry *mmap_entry = mmap_entry_lookup (thread, mapping);
  if (mmap_entry == NULL){
    PANIC("MMAP: mmap-entry null in munmap\n");
  }
  
  unsigned needed_pages = mmap_entry->needed_pages;
  void *vaddr = mmap_entry->start_vaddr;
  ASSERT(needed_pages > 0);
  
  struct hash_elem *hash_elem = hash_delete (&(thread->mmap_hashmap), 
        &(mmap_entry->h_elem));
  if (hash_elem == NULL){
    PANIC("MMAP: element which should be deleted not found\n");
  }
  free(mmap_entry);

  while(needed_pages > 0){
    struct sup_page_entry *sup_page_entry = vm_sup_page_lookup(thread, vaddr);
    if (sup_page_entry == NULL){
      PANIC("MMAP: addr to delete does not exist\n");
    }
    if (!vm_delete_mmap_entry(sup_page_entry)){
      PANIC("MMAP: delete not possible\n");
    }
    needed_pages -= 1;
    vaddr += PGSIZE;
  }
}