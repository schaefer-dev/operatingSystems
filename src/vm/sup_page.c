#include <hash.h>
#include "lib/kernel/hash.h"
#include <list.h>
#include "lib/kernel/list.h"
#include "threads/palloc.h"


/* lock to guarantee mutal exclusiveness on frame operations */
static struct lock sup_page_lock;


struct sup_page_entry
  {
    enum page_status;

    /* physical address of supplemental page */
    void *phys_addr; 

    /* Virtual Memory adress this page will be loaded to in a page fault */
    void *vm_addr;

    /* swap address of supplemental page */
    void *swap_addr;

    /* thread this frame belongs to */
    struct thread *thread;

    bool dirty;
    bool accessed;

    // TODO: How are files handeld in supplemental page?
    struct file *file;
    
    /* palloc flags which are used when the frame is created */
    enum palloc_flags pflags;

    struct hash_elem h_elem;
  };


void
vm_sup_page_init () {
  lock_init (&sup_page_lock);
}


// TODO: impossible to have kpage as argument
// TODO: use a list of threads -> memory sharing
bool
vm_sup_page_allocate (hash sup_page_hashmap, void *upage, void *kpage)
{
  // TODO Frame Loading happens in page fault, we use round down (defined in vaddr.h) 
  // to get the supplemental page using the sup_page_hashmap

  struct thread *current_thread = thread_current();

  struct sup_page_entry *sup_page_entry = (struct sup_page_entry *) malloc(sizeof(struct sup_page_entry));

  //TODO: has to be set to NULL initially
  sup_page_entry->phys_addr = kpage;

  sup_page_entry->vm_addr = upage;
  sup_page_entry->swap_addr = NULL;
  sup_page_entry->thread = current_thread;
  //TODO: check if this should be a paramter
  sup_page_entry->status = ????;
  sup_page_entry->dirty = false;
  sup_page_entry->accessed = false;
  // TODO: could also be a paramter
  sup_page_entry->file = NULL;

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (&sup_page_hashmap, &sup_page_hashmap->h_elem);
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


void 
vm_sup_page_free (void* phys_addr) {
  lock_acquire (&frame_lock);

  /* create frame to perform the lookup with, without reference to "free" it after scope */
  struct frame lookup_frame;
  lookup_frame.phys_addr = phys_addr;

  /* search the frame with the correct hash */
  struct hash_elem *h_elem_lookup = hash_find (&frame_hashmap, &(lookup_frame.h_elem));
  if (h_elem_loopup == NULL) {
    // the table was not found, this should be impossible!
    printf("frame not found in Frame Table!");
    return;
  }

  struct frame *frame;
  frame = hash_entry(h_elem_loopup, struct frame, h_lem);

  hash_delete (&frame_hashmap, &frame->h_elem);
  list_remove (&frame->l_elem);

  lock_release (&frame_lock);
  return;
}

// TODO use frame_list to iterate over it in clock algorithm

// TODO implement hash function