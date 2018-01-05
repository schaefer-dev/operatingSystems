#include <hash.h>
#include <list.h>
#include "lib/kernel/list.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "vm/sup_page.h"
#include "vm/frame.h"
#include "userprog/process.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"

void vm_sup_page_free_stack(struct sup_page_entry *sup_page_entry);
void vm_sup_page_free_mmap(struct sup_page_entry *sup_page_entry);
void vm_sup_page_free_file(struct sup_page_entry *sup_page_entry);
bool vm_write_file_back_on_delete(struct sup_page_entry *sup_page_entry);

/* Hash function for supplemental page table entries */
unsigned
hash_vm_sup_page(const struct hash_elem *sup_p_, void *aux UNUSED)
{
  const struct sup_page_entry *sup_p = hash_entry(sup_p_, struct sup_page_entry, h_elem);
  unsigned hash_val = hash_int((int) sup_p->vm_addr);
  return hash_val;
}


/* Hash compare function for supplemental page table entries */
bool
hash_compare_vm_sup_page(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct sup_page_entry *a = hash_entry (a_, struct sup_page_entry, h_elem);
  const struct sup_page_entry *b = hash_entry (b_, struct sup_page_entry, h_elem);

  bool result = (a->vm_addr) < (b->vm_addr);

  return result;
}


/* returns the sup_page_entry of thread, corresponding to passed vm_addr */
struct sup_page_entry*
vm_sup_page_lookup (struct thread *thread, void *vm_addr)
{
  lock_acquire(&thread->sup_page_lock);
  struct sup_page_entry search_sup_page_entry;
  struct hash_elem *hash;

  search_sup_page_entry.vm_addr = vm_addr;
  hash = hash_find(&(thread->sup_page_hashmap), &(search_sup_page_entry.h_elem));
  
  // TODO refactor this line!
  lock_release(&thread->sup_page_lock);
  return hash != NULL ? hash_entry (hash, struct sup_page_entry, h_elem) : NULL; 
}


/* close the entire hashmap and free all ressources contained in it */
void
vm_sup_page_hashmap_close(struct thread *thread)
{
  lock_acquire(&thread->sup_page_lock);
  hash_destroy(&(thread->sup_page_hashmap), vm_sup_page_free);
  lock_release(&thread->sup_page_lock);
}


void
vm_sup_page_load_and_pin (struct sup_page_entry *sup_page_entry)
{
  lock_acquire(&sup_page_entry->page_lock);
  ASSERT(sup_page_entry != NULL);
  lock_acquire(&(sup_page_entry->pin_lock));
  sup_page_entry->pinned = true;
  lock_release(&(sup_page_entry->pin_lock));
  vm_sup_page_load(sup_page_entry);
  lock_release(&sup_page_entry->page_lock);
}


void
vm_sup_page_unpin (struct sup_page_entry *sup_page_entry)
{
  ASSERT(sup_page_entry != NULL);
  lock_acquire(&(sup_page_entry->pin_lock));
  sup_page_entry->pinned = false;
  lock_release(&(sup_page_entry->pin_lock));
}


// TODO it would probably be a good idea to lock all loading of stuff (to not evict in the meantime)
// try to lock sup_page_lock during sup_page_load
void
vm_sup_page_load (struct sup_page_entry *sup_page_entry){
  struct thread *current_thread = thread_current();
  ASSERT(lock_held_by_current_thread(&sup_page_entry->page_lock));
  lock_acquire(&(sup_page_entry->pin_lock));
  sup_page_entry->pinned = true;
  lock_release(&(sup_page_entry->pin_lock));

  switch (sup_page_entry->status)
    {
      case PAGE_STATUS_NOT_LOADED:
        {
          /* can only happen for FILE / MMAP */
          vm_load_file(sup_page_entry);
          break;
        }

      case PAGE_STATUS_SWAPPED:
        {
          vm_load_swap(sup_page_entry);
          break;
        }

      case PAGE_STATUS_LOADED:
        {
          /* page already loaded! */
          if (pagedir_get_page (current_thread->pagedir, sup_page_entry->vm_addr) == false){
            sup_page_entry->pinned = false;
            syscall_exit(-1);
          }
          return;
        }
      default:
        {
          printf("Illegal page status in sup_page_load!\n");
        }
    }
  lock_acquire(&(sup_page_entry->pin_lock));
  sup_page_entry->pinned = false;
  lock_release(&(sup_page_entry->pin_lock));
}


/* free the ressources of the page with the corresponding hash_elem 
   this function should only be used by hash_destroy! */
// TODO might have to be static to be found by hash_destroy!
void
vm_sup_page_free(struct hash_elem *hash, void *aux UNUSED)
{
  struct thread *current_thread = thread_current();
  ASSERT(lock_held_by_current_thread(&current_thread->sup_page_lock));
  lock_acquire(&frame_lock);

  //printf("DEBUG: vm_sup_page_free started!\n");
  struct sup_page_entry *lookup_sup_page_entry;
  lookup_sup_page_entry = hash_entry(hash, struct sup_page_entry, h_elem);

  lock_acquire(&lookup_sup_page_entry->page_lock);

  ASSERT(lookup_sup_page_entry != NULL);

  switch (lookup_sup_page_entry->type)
    {
      case PAGE_TYPE_MMAP:
        {
          //printf("DEBUG: free sup page of mmap\n");
          vm_sup_page_free_mmap(lookup_sup_page_entry);
          break;
        }
      case PAGE_TYPE_FILE:
        {
          //printf("DEBUG: free sup page of file\n");
          vm_sup_page_free_file(lookup_sup_page_entry);
          break;
        }
      case PAGE_TYPE_STACK:
        {
          //printf("DEBUG: free sup page of stack\n");
          vm_sup_page_free_stack(lookup_sup_page_entry);
          break;
        }
    default:
      {
        printf("Illegal PAGE_TYPE found!\n");
        lock_release(&lookup_sup_page_entry->page_lock);
        lock_release(&frame_lock);
        syscall_exit(-1);
        break;
      }

    }

  lock_release(&lookup_sup_page_entry->page_lock);
  lock_release(&frame_lock);
  free(lookup_sup_page_entry);
}


void
vm_sup_page_free_stack(struct sup_page_entry *sup_page_entry)
{
  struct thread *current_thread = thread_current();
  ASSERT(lock_held_by_current_thread(&current_thread->sup_page_lock));
  switch (sup_page_entry->status)
    {
      case PAGE_STATUS_LOADED:
        {
          void *phys_addr = sup_page_entry->phys_addr;
          void *upage = sup_page_entry->vm_addr;
          vm_frame_free(phys_addr, upage);
          break;
        }
      case PAGE_STATUS_NOT_LOADED:
        {
          printf("STACK WITH ILLEGAL STATUS NOT_LOADED!\n");
          break;
        }
      case PAGE_STATUS_SWAPPED:
        {
          vm_swap_free(sup_page_entry->swap_addr);
          break;
        }
      default:
        {
          printf("Illegal PAGE_STATUS found!\n");
          syscall_exit(-1);
          break;
        }
    }
}


void
vm_sup_page_free_mmap(struct sup_page_entry *sup_page_entry)
{
  struct thread *current_thread = thread_current();
  ASSERT(lock_held_by_current_thread(&current_thread->sup_page_lock));
  switch (sup_page_entry->status)
    {
      case PAGE_STATUS_LOADED:
        {
          vm_write_mmap_back(sup_page_entry);
          void *phys_addr = sup_page_entry->phys_addr;
          void *upage = sup_page_entry->vm_addr;
          vm_frame_free(phys_addr, upage);
          break;
        }
      case PAGE_STATUS_SWAPPED:
        {
          printf("MMAP page is not allowed to be swapped!\n");
          syscall_exit(-1);
          break;
        }
      case PAGE_STATUS_NOT_LOADED:
        {
          /* in the case of NOT_LOADED nothing has to be written back */
          break;
        }
      default:
        {
          printf("Illegal PAGE_STATUS found!\n");
          syscall_exit(-1);
          break;
        }

    }

}

void
vm_sup_page_free_file(struct sup_page_entry *sup_page_entry)
{
  struct thread *current_thread = thread_current();
  ASSERT(lock_held_by_current_thread(&current_thread->sup_page_lock));
    switch (sup_page_entry->status)
    {
      case PAGE_STATUS_LOADED:
        {
          void *phys_addr = sup_page_entry->phys_addr;
          void *upage = sup_page_entry->vm_addr;
          //printf("DEBUG: freeing file with vaddr: %p\n", upage);
          vm_frame_free(phys_addr, upage);
          //printf("DEBUG: reaching after frame_free in sup_page_free_file\n");
          break;
        }
      case PAGE_STATUS_SWAPPED:
        {
	  vm_swap_free(sup_page_entry->swap_addr);
          break;
        }
      case PAGE_STATUS_NOT_LOADED:
        {
          /* in the case of NOT_LOADED nothing has to be written back */
          break;
        }
      default:
        {
          printf("Illegal PAGE_STATUS found!\n");
          syscall_exit(-1);
          break;
        }

    }

}


/* initialize ressources for supplemental page */
void
vm_sup_page_init (struct thread *thread) {
  struct thread *current_thread = thread_current();
  lock_acquire(&current_thread->sup_page_lock);
  bool success = hash_init(&thread->sup_page_hashmap, hash_vm_sup_page, hash_compare_vm_sup_page, NULL);
  lock_release(&current_thread->sup_page_lock);
}


/* function to allocate a supplemental page table entry e.g. a stack*/
bool
vm_sup_page_allocate (void *vm_addr, bool writable){

  // TODO Frame Loading happens in page fault, we use round down (defined in vaddr.h) 
  // to get the supplemental page using the sup_page_hashmap

  struct thread *current_thread = thread_current();

  struct sup_page_entry *sup_page_entry = (struct sup_page_entry *) malloc(sizeof(struct sup_page_entry));

  sup_page_entry->phys_addr = NULL;

  sup_page_entry->vm_addr = vm_addr;
  //TODO: check if 0 is okay
  sup_page_entry->swap_addr = 0;
  sup_page_entry->thread = current_thread;
  sup_page_entry->status = PAGE_STATUS_NOT_LOADED;
  sup_page_entry->type = PAGE_TYPE_STACK;
  sup_page_entry->file = NULL;
  sup_page_entry->read_bytes = 0;
  sup_page_entry->mmap_id=-1;
  sup_page_entry->file_offset = 0;
  sup_page_entry->writable = writable;
  sup_page_entry->pinned = false;
  lock_init(&(sup_page_entry->pin_lock));
  lock_init(&(sup_page_entry->page_lock));

  /* check if there is already the same hash contained in the hashmap, in which case we abort! */
  struct hash_elem *prev_elem;
  struct sup_page_entry *previous_sup_page_entry = vm_sup_page_lookup(current_thread, vm_addr);
  if (previous_sup_page_entry != NULL)
    return true;
  lock_acquire(&current_thread->sup_page_lock);
  prev_elem = hash_insert (&(current_thread->sup_page_hashmap), &(sup_page_entry->h_elem));
  lock_release(&current_thread->sup_page_lock);
  if (prev_elem == NULL) {
    return true;
  }
  else {
    /* creation of supplemental page failed, because there was already an entry 
       with the same hashvalue */
    // TODO lots of hash collisions here!
    printf("hash collision in sup page allocate (stack)! With sup page of type: %i and status %i\n", previous_sup_page_entry->type, previous_sup_page_entry->status);
    free (sup_page_entry);
    return false;
  }
}


/* function to allocate a supplemental page table entry for files incuding the file 
and the offset within the file*/
bool
vm_sup_page_file_allocate (void *vm_addr, struct file* file, off_t file_offset, off_t read_bytes, bool writable)
{
  //ASSERT(file_offset % PGSIZE == 0);
  // TODO implement writable parameter
  // TODO Frame Loading happens in page fault, we use round down (defined in vaddr.h) 
  // to get the supplemental page using the sup_page_hashmap

  struct thread *current_thread = thread_current();

  struct sup_page_entry *sup_page_entry = (struct sup_page_entry *) malloc(sizeof(struct sup_page_entry));

  sup_page_entry->phys_addr = NULL;

  sup_page_entry->vm_addr = vm_addr;
  sup_page_entry->swap_addr = 0;
  sup_page_entry->thread = current_thread;
  sup_page_entry->status = PAGE_STATUS_NOT_LOADED;
  sup_page_entry->type = PAGE_TYPE_FILE;
  sup_page_entry->file = file;
  sup_page_entry->file_offset = file_offset;
  sup_page_entry->read_bytes = read_bytes;
  sup_page_entry->mmap_id=-1;
  sup_page_entry->writable = writable;
  sup_page_entry->pinned = false;
  lock_init(&(sup_page_entry->pin_lock));
  lock_init(&(sup_page_entry->page_lock));

  /* check if there is already the same hash contained in the hashmap, in which case we abort! */
  struct hash_elem *prev_elem;
  lock_acquire(&current_thread->sup_page_lock);
  prev_elem = hash_insert (&(current_thread->sup_page_hashmap), &(sup_page_entry->h_elem));
  lock_release(&current_thread->sup_page_lock);
  if (prev_elem == NULL) {
    return true;
  }
  else {
    /* creation of supplemental page failed, because there was already an entry 
       with the same hashvalue */
    printf("hash collision in sup page allocate for file!!!!\n");
    free (sup_page_entry);
    return false;
  }
}

/* function to allocate a supplemental page table entry for mmaped files incuding the file 
and the offset within the file*/
bool
vm_sup_page_mmap_allocate (void *vm_addr, struct file* file, off_t file_offset, 
  off_t read_bytes, int mmap_id, bool writable)
{
  ASSERT(file_offset % PGSIZE == 0);
  // TODO implement writable parameter
  // TODO Frame Loading happens in page fault, we use round down (defined in vaddr.h) 
  // to get the supplemental page using the sup_page_hashmap

  struct thread *current_thread = thread_current();

  struct sup_page_entry *sup_page_entry = (struct sup_page_entry *) malloc(sizeof(struct sup_page_entry));

  sup_page_entry->phys_addr = NULL;

  sup_page_entry->vm_addr = vm_addr;
  sup_page_entry->swap_addr = 0;
  sup_page_entry->thread = current_thread;
  sup_page_entry->status = PAGE_STATUS_NOT_LOADED;
  sup_page_entry->type = PAGE_TYPE_MMAP;
  sup_page_entry->file = file;
  sup_page_entry->file_offset = file_offset;
  sup_page_entry->read_bytes = read_bytes;
  sup_page_entry->mmap_id = mmap_id;
  sup_page_entry->writable = writable;
  sup_page_entry->pinned = false;
  lock_init(&(sup_page_entry->pin_lock));
  lock_init(&(sup_page_entry->page_lock));

  /* check if there is already the same hash contained in the hashmap, in which case we abort! */
  struct hash_elem *prev_elem;
  lock_acquire(&current_thread->sup_page_lock);
  prev_elem = hash_insert (&(current_thread->sup_page_hashmap), &(sup_page_entry->h_elem));
  lock_release(&current_thread->sup_page_lock);
  if (prev_elem == NULL) {
    return true;
  }
  else {
    /* creation of supplemental page failed, because there was already an entry 
       with the same hashvalue */
    printf("hash collision in sup page allocate (MMAP)!!!!\n");
    free (sup_page_entry);
    return false;
  }
}


// TODO check this
/* implementation of stack growth called by page fault handler */
bool
vm_grow_stack(void *fault_frame_addr)
{
  struct thread *thread = thread_current();
  struct sup_page_entry *before_sup_page_entry = vm_sup_page_lookup(thread, fault_frame_addr);

  if (before_sup_page_entry != NULL)
    return true;

  if (vm_sup_page_allocate(fault_frame_addr, true)){

    struct sup_page_entry *sup_page_entry = vm_sup_page_lookup(thread, fault_frame_addr);
    ASSERT(sup_page_entry != NULL);

    lock_acquire(&sup_page_entry->page_lock);

    void *page = vm_frame_allocate(sup_page_entry, (PAL_ZERO | PAL_USER) , true);

    if (page == NULL){
      printf("stack growth could not allocate page!\n");
      return false;
    }

    sup_page_entry->status = PAGE_STATUS_LOADED;
    lock_release(&sup_page_entry->page_lock);

    return true;

  } else {
    return false;
  }
}

//TODO: implement this function
/* implementation of load from swap partition called by page fault handler */
bool 
vm_load_swap(struct sup_page_entry *sup_page_entry)
{
  ASSERT(lock_held_by_current_thread(&sup_page_entry->page_lock));

  ASSERT(sup_page_entry != NULL);

  block_sector_t swap_addr = sup_page_entry -> swap_addr;

  bool writable = sup_page_entry->writable;

  void *page = vm_frame_allocate(sup_page_entry, (PAL_ZERO | PAL_USER) , writable);

  if (page == NULL){
    printf("load file could not allocate page!\n");
    return false;
  }

  vm_swap_back(swap_addr, page);

  /*indicate that frame is now loaded */
  sup_page_entry->status = PAGE_STATUS_LOADED;

  return true;

}

/* implementation of load file called by page fault handler */
bool 
vm_load_file(struct sup_page_entry *sup_page_entry){
  ASSERT(lock_held_by_current_thread(&sup_page_entry->page_lock));

  ASSERT(sup_page_entry != NULL);

  struct file *file = sup_page_entry->file;
  ASSERT(file != NULL);

  off_t file_offset = sup_page_entry->file_offset;

  off_t read_bytes = sup_page_entry->read_bytes;

  bool writable = sup_page_entry->writable;

  void *page = vm_frame_allocate(sup_page_entry, (PAL_ZERO | PAL_USER) , writable);

  if (page == NULL){
    printf("load file could not allocate page!\n");
    return false;
  }

  if (read_bytes != 0){
    file_seek(file, file_offset);
    off_t file_read_bytes = file_read (file, page, read_bytes);
    memset (page + read_bytes, 0, PGSIZE - read_bytes);
    if (file_read_bytes != read_bytes){
      /* file not correctly read, free frame and indicate file not loaded */
      printf("DEBUG: file not correctly read in vm_load_file!\n");
      vm_frame_free (page, fault_frame_addr);
      return false;
    }
  }

  /*indicate that frame is now loaded */
  sup_page_entry->status = PAGE_STATUS_LOADED;

  //printf("DEBUG: load_file finished cleanly!\n");
  return true;
}

/* writes the changes back to file if dirty */
bool vm_write_mmap_back(struct sup_page_entry *sup_page_entry){
  ASSERT(lock_held_by_current_thread(&sup_page_entry->page_lock));

  ASSERT(sup_page_entry != NULL);

  if (sup_page_entry->type != PAGE_TYPE_MMAP){
    printf("try to remove mmap but entry is not of type mmap");
    return false;
  }
  void* vaddr = sup_page_entry->vm_addr;
  void* phys_addr = sup_page_entry->phys_addr;
  struct thread *current_thread = sup_page_entry->thread;
  bool dirty = pagedir_is_dirty(current_thread->pagedir, vaddr);
  if(!dirty)
    return true;
  struct file *file = sup_page_entry->file;
  off_t file_offset = sup_page_entry->file_offset;
  off_t read_bytes = sup_page_entry->read_bytes;
  lock_acquire(&lock_filesystem);

  off_t written_bytes = file_write_at(file, vaddr, read_bytes, file_offset);

  lock_release(&lock_filesystem);
  return true;
}


/* removes mmap entry from hashtable and frees! */
bool vm_delete_mmap_entry(struct sup_page_entry *sup_page_entry){
  
  ASSERT(sup_page_entry != NULL);
  struct thread *thread = sup_page_entry->thread;
  ASSERT(!lock_held_by_current_thread(&lock_filesystem));
  ASSERT(!lock_held_by_current_thread(&frame_lock));
  ASSERT(!lock_held_by_current_thread(&sup_page_entry->page_lock));
  ASSERT(!lock_held_by_current_thread(&thread->sup_page_lock));

  // added page_lock again to test fixed page_parallel

  lock_acquire(&frame_lock);
  lock_acquire(&thread->sup_page_lock);
  lock_acquire(&sup_page_entry->page_lock);
  if (!vm_write_mmap_back(sup_page_entry)){
    printf("vm_write_mmap_back failed! This should never happen!\n");
    lock_release(&sup_page_entry->page_lock);
    lock_release(&frame_lock);
    lock_release(&thread->sup_page_lock);
    return false;
  }
  struct hash_elem *hash_elem = hash_delete(&(thread->sup_page_hashmap), &(sup_page_entry->h_elem));

  if (hash_elem == NULL){
    printf("element which should be deleted not found");
    lock_release(&sup_page_entry->page_lock);
    lock_release(&frame_lock);
    lock_release(&thread->sup_page_lock);
    return false;
  }

  void *phys_addr = sup_page_entry->phys_addr;
  void *upage = sup_page_entry->vm_addr;

  if (sup_page_entry->status == PAGE_STATUS_LOADED){
    ASSERT(phys_addr != NULL);
    vm_frame_free(phys_addr, upage);
  }
  
  lock_release(&sup_page_entry->page_lock);
  lock_release(&thread->sup_page_lock);
  free(sup_page_entry);
  lock_release(&frame_lock);

  return true;
}
