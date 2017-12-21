#include <hash.h>
#include "lib/kernel/hash.h"
#include <list.h>
#include "lib/kernel/list.h"
#include "threads/palloc.h"

void vm_sup_page_free(struct hash_elem *hash, void *aux aux);

/* Hash function for supplemental page table entries */
unsigned
hash_vm_sup_page(const struct hash_elem *hash, void *aux aux)
{
  const struct sup_page_entry *sup_page_entry;
  frame = hash_entry(hash, struct sup_page_entry, h_elem);
  return hash_bytes(&sup_page_entry->vm_addr, sizeof(sup_page_entry->vm_addr));
}

/* Hash compare function for supplemental page table entries */
bool
hash_compare_vm_sup_page(const struct hash_elem *a_, const struct hash_elem *b_, void *aux)
{
  const struct sup_page_entry *a = hash_entry (a_, struct hash, h_elem);
  const struct sup_page_entry *b = hash_entry (b_, struct hash, h_elem);

  return (a->vm_addr < b->vm_addr);
}

/* returns the sup_page_entry of thread, corresponding to passed vm_addr */
struct sup_page_entry*
vm_sup_page_lookup (const struct thread *thread, const void* vm_addr)
{
  struct sup_page_entry search_sup_page_entry;
  struct hash_elem *hash;

  search_sup_page_entry.vm_addr = vm_addr;
  hash = hash_find(&(thread->sup_frame_hashmap), &search_frame.h_elem);
  return e != NULL ? hash_entry (hash, struct sup_page_entry, h_elem) : NULL; 
}

/* close the entire hashmap and free all ressources contained in it */
void
vm_sup_page_hashmap_close(const struct thread *thread)
{
  hash_destroy(thread->sup_page_hashmap, vm_sup_page_free);
}

/* free the ressources of the page with the corresponding hash_elem 
   this function should only be used by hash_destroy! */
// TODO might have to be static to be found by hash_destroy!
void
vm_sup_page_free(struct hash_elem *hash, void *aux)
{
  struct sup_page_entry *lookup_sup_page_entry;
  lookup_sup_page_entry = hash_entry(hash, struct sup_page_entry, h_elem);

  if (lookup_sup_page_entry->status == page_status.PAGE_SWAPPED){
    /* Case of the Page content swapped */ 
    // TODO 

  } else if (lookup_sup_page_entry->page_status == page_status.PAGE_LOADED){
    /* Case of the Page content in physical Memory */
    // TODO 

  } else if (lookup_sup_page_entry->page_status == page_status.PAGE_NOT_LOADED){
    /* Case of the Page content not loaded */
    // TODO

  } else {
    // TODO remove this later
    printf("illegal page status for free, should never happen!\n")
  }
  
  free(lookup_sup_page_entry);
}


/* initialize ressources for supplemental page */
void
vm_sup_page_init () {
  
}

/* function to allocate a supplemental page table entry e.g. a stack*/
bool
vm_sup_page_allocate (void *vm_addr)
{
  // TODO Frame Loading happens in page fault, we use round down (defined in vaddr.h) 
  // to get the supplemental page using the sup_page_hashmap

  struct thread *current_thread = thread_current();

  struct sup_page_entry *sup_page_entry = (struct sup_page_entry *) malloc(sizeof(struct sup_page_entry));

  sup_page_entry->phys_addr = NULL;

  sup_page_entry->vm_addr = vm_addr;
  sup_page_entry->swap_addr = NULL;
  sup_page_entry->thread = current_thread;
  sup_page_entry->status = page_stats.PAGE_NOT_LOADED;
  sup_page_entry->file = NULL;
  sup_page_entry->file_offset = 0;

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
vm_sup_page_file_allocate (void *vm_addr, struct file* file, unsigned file_offset)
{
  // TODO Frame Loading happens in page fault, we use round down (defined in vaddr.h) 
  // to get the supplemental page using the sup_page_hashmap

  struct thread *current_thread = thread_current();

  struct sup_page_entry *sup_page_entry = (struct sup_page_entry *) malloc(sizeof(struct sup_page_entry));

  sup_page_entry->phys_addr = NULL;

  sup_page_entry->vm_addr = vm_addr;
  sup_page_entry->swap_addr = NULL;
  sup_page_entry->thread = current_thread;
  sup_page_entry->status = page_stats.PAGE_NOT_LOADED;
  sup_page_entry->file = file;
  sup_page_entry->file_offset = file_offset;

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


// TODO implement hash function
