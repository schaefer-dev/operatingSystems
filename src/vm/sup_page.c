#include <hash.h>
#include <list.h>
#include "lib/kernel/list.h"
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

/* Hash function for supplemental page table entries */
unsigned
hash_vm_sup_page(const struct hash_elem *sup_p_, void *aux UNUSED)
{
  const struct sup_page_entry *sup_p = hash_entry(sup_p_, struct sup_page_entry, h_elem);
  unsigned hash_val = hash_bytes(&sup_p->vm_addr, sizeof(sup_p->vm_addr));
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
  struct sup_page_entry search_sup_page_entry;
  struct hash_elem *hash;


  search_sup_page_entry.vm_addr = vm_addr;
  hash = hash_find(&(thread->sup_page_hashmap), &(search_sup_page_entry.h_elem));
  
  // TODO refactor this line!
  return hash != NULL ? hash_entry (hash, struct sup_page_entry, h_elem) : NULL; 
}


/* close the entire hashmap and free all ressources contained in it */
void
vm_sup_page_hashmap_close(struct thread *thread)
{
  hash_destroy(&(thread->sup_page_hashmap), vm_sup_page_free);
}


/* free the ressources of the page with the corresponding hash_elem 
   this function should only be used by hash_destroy! */
// TODO might have to be static to be found by hash_destroy!
void
vm_sup_page_free(struct hash_elem *hash, void *aux UNUSED)
{
  struct sup_page_entry *lookup_sup_page_entry;
  lookup_sup_page_entry = hash_entry(hash, struct sup_page_entry, h_elem);

  if (lookup_sup_page_entry->status == PAGE_STATUS_SWAPPED){
    /* Case of the Page content swapped */ 
    // TODO 

  } else if (lookup_sup_page_entry->status == PAGE_STATUS_LOADED){
    /* Case of the Page content in physical Memory */
    // TODO 

  } else if (lookup_sup_page_entry->status == PAGE_STATUS_NOT_LOADED){
    /* Case of the Page content not loaded */
    // TODO

  } else {
    // TODO remove this later
    printf("illegal page status for free, should never happen!\n");
  }
  
  free(lookup_sup_page_entry);
}


/* initialize ressources for supplemental page */
void
vm_sup_page_init (struct thread *thread) {
  bool success = hash_init(&thread->sup_page_hashmap, hash_vm_sup_page, hash_compare_vm_sup_page, NULL);
}


/* function to allocate a supplemental page table entry e.g. a stack*/
bool
vm_sup_page_allocate (void *vm_addr, bool writable)
{
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

  /* check if there is already the same hash contained in the hashmap, in which case we abort! */
  struct hash_elem *prev_elem;
  prev_elem = hash_insert (&(current_thread->sup_page_hashmap), &(sup_page_entry->h_elem));
  if (prev_elem == NULL) {
    return true;
  }
  else {
    /* creation of supplemental page failed, because there was already an entry 
       with the same hashvalue */
    free (sup_page_entry);
    return false;
  }
}

/* function to allocate a supplemental page table entry for files incuding the file 
and the offset within the file*/
bool
vm_sup_page_file_allocate (void *vm_addr, struct file* file, off_t file_offset, off_t read_bytes, bool writable)
{
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

  /* check if there is already the same hash contained in the hashmap, in which case we abort! */
  struct hash_elem *prev_elem;
  prev_elem = hash_insert (&(current_thread->sup_page_hashmap), &(sup_page_entry->h_elem));
  if (prev_elem == NULL) {
    return true;
  }
  else {
    /* creation of supplemental page failed, because there was already an entry 
       with the same hashvalue */
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

  /* check if there is already the same hash contained in the hashmap, in which case we abort! */
  struct hash_elem *prev_elem;
  prev_elem = hash_insert (&(current_thread->sup_page_hashmap), &(sup_page_entry->h_elem));
  if (prev_elem == NULL) {
    return true;
  }
  else {
    /* creation of supplemental page failed, because there was already an entry 
       with the same hashvalue */
    free (sup_page_entry);
    return false;
  }
}


// TODO check this
/* implementation of stack growth called by page fault handler */
bool
vm_grow_stack(void *fault_frame_addr){
  
  struct thread *thread = thread_current();

  if (vm_sup_page_allocate(fault_frame_addr, true)){

    struct sup_page_entry *sup_page_entry = vm_sup_page_lookup(thread, fault_frame_addr);

    void *page = vm_frame_allocate(sup_page_entry, (PAL_ZERO | PAL_USER) , true);

    if (page == NULL){
      printf("stack growth could not allocate page!\n");
      return false;
    }

    sup_page_entry->status = PAGE_STATUS_LOADED;

    return true;

  } else {
    return false;
  }
}

//TODO: implement this function
/* implementation of load from swap partition called by page fault handler */
bool 
vm_load_swap(void *fault_frame_addr){
  struct thread *thread = thread_current();

  struct sup_page_entry *sup_page = vm_sup_page_lookup(thread, fault_frame_addr);

  block_sector_t swap_addr = sup_page -> swap_addr;

  bool writable = sup_page->writable;

  void *page = vm_frame_allocate(vm_sup_page_lookup(thread, fault_frame_addr), (PAL_ZERO | PAL_USER) , writable);

  if (page == NULL){
    printf("load file could not allocate page!\n");
    return false;
  }

  vm_swap_back(swap_addr, page);

  /*indicate that frame is now loaded */
  sup_page->status = PAGE_STATUS_LOADED;

  return true;

}

/* implementation of load file called by page fault handler */
bool 
vm_load_file(void *fault_frame_addr){
  struct thread *thread = thread_current();

  struct sup_page_entry *sup_page = vm_sup_page_lookup(thread, fault_frame_addr);

  struct file *file = sup_page->file;

  off_t file_offset = sup_page->file_offset;

  off_t read_bytes = sup_page->read_bytes;

  bool writable = sup_page->writable;

  void *page = vm_frame_allocate(vm_sup_page_lookup(thread, fault_frame_addr), (PAL_ZERO | PAL_USER) , writable);

  if (page == NULL){
    printf("load file could not allocate page!\n");
    return false;
  }

  if (read_bytes != 0){
    lock_acquire(&lock_filesystem);
    off_t file_read_bytes = file_read_at (file, page, read_bytes, file_offset);
    lock_release(&lock_filesystem);
    if (file_read_bytes!= read_bytes){
      /* file not correctly read, free frame and indicate file not loaded */
      vm_frame_free (page, fault_frame_addr);
      //printf("DEBUG: file not correctly read in load_file!\n");
      return false;
    }
  }

  /*indicate that frame is now loaded */
  sup_page->status = PAGE_STATUS_LOADED;

  //printf("DEBUG: load_file finished cleanly!\n");
  return true;
}


// TODO implement hash function
