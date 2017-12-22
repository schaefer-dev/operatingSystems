#include "threads/malloc.h"
#include <list.h>
#include "lib/kernel/list.h"
#include "threads/palloc.h"
#include "vm/sup_page.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"


/* lock to guarantee mutal exclusiveness on frame operations */
static struct lock frame_lock;

/* list of all frames */
static struct list frame_list;

void* evict_page(enum palloc_flags pflags);
unsigned hash_vm_frame(const struct hash_elem *hash, void *aux UNUSED);
bool hash_compare_vm_frame(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);


unsigned
hash_vm_frame(const struct hash_elem *hash, void *aux UNUSED)
{
  struct frame *frame;
  frame = hash_entry(hash, struct frame, h_elem);
  return hash_bytes(&frame->phys_addr, sizeof(frame->phys_addr));
}


bool
hash_compare_vm_frame(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct frame *a = hash_entry (a_, struct frame, h_elem);
  const struct frame *b = hash_entry (b_, struct frame, h_elem);

  return (a->phys_addr < b->phys_addr);
}


struct frame*
vm_frame_lookup (void* phys_addr)
{
  struct frame search_frame;
  struct hash_elem *hash;

  search_frame.phys_addr = phys_addr;
  hash = hash_find(&frame_hashmap, &search_frame.h_elem);
  return hash != NULL ? hash_entry (hash, struct frame, h_elem) : NULL; 
}


void
vm_frame_init () {
  printf("DEBUG: frame_init start \n");
  lock_init (&frame_lock);
  printf("DEBUG: frame lock init passed \n");
  list_init (&frame_list);
  printf("DEBUG: frame list init passed \n");
  hash_init(&frame_hashmap, hash_vm_frame, hash_compare_vm_frame, NULL);
  printf("DEBUG: frame_init end \n");
}

//TODO: add page to pagedir of the corresponding thread
//TODO: check if this function should simply return the reference to the frame b.c. there are different ways to load (files, mmap, swap...)
void*
vm_frame_allocate (struct sup_page_entry *sup_page_entry, enum palloc_flags pflags, bool writable)
{
  lock_acquire (&frame_lock);

  // try to get page using palloc
  void *page = palloc_get_page(PAL_USER | pflags);

  if (page == NULL) {
    // Frame allocation Failed
    // TODO evice page here to make room
    // TODO lock is hold at this place so evict_page shouldn't acquire the lock check if this is a good idea
    return NULL;
  }

  struct frame *frame = malloc(sizeof(struct frame));
  frame->phys_addr = page;
  frame->sup_page_entry = sup_page_entry;
  frame->pinned = false;

  // install page in pagedir of thread
  install_page(sup_page_entry->vm_addr, page, writable);

  list_push_back (&frame_list, &frame->l_elem);
  hash_insert (&frame_hashmap, &frame->h_elem);

  lock_release (&frame_lock);
  return page;
}

//TODO: check if we have to set status and phys address of sub_page_entry
void 
vm_frame_free (void* phys_addr, void* upage)
{
  lock_acquire (&frame_lock);

  struct frame *found_frame = vm_frame_lookup(phys_addr);

  if (found_frame == NULL) {
    // the table was not found, this should be impossible!
    printf("frame not found in Frame Table!");
    return;
  }

  hash_delete (&frame_hashmap, &found_frame->h_elem);
  list_remove (&found_frame->l_elem);

  palloc_free_page(phys_addr);

  //TODO: possibly this has to be removed and the original function to clear pages has to be used
  // possibly pagedir doesn't exist anymore??
  //TODO comment this in again!
  //uninstall_page(upage);

  free(found_frame);

  lock_release (&frame_lock);
  return;
}


void*
evict_page(enum palloc_flags pflags){
  //TODO: check if this is necessary (b.c. we have to ensure that the list doesn't change during eviction)
  lock_acquire(&frame_lock);
  if (list_empty(&frame_list))
    return NULL;

  struct list_elem *iterator = list_begin (&frame_list);

  // iterate over all frames until a frame is not accessed (could be more than iteration)
  while (iterator != list_end (&frame_list)){
      struct frame *f = list_entry (iterator, struct frame, l_elem);

      struct sup_page_entry *current_sup_page = f->sup_page_entry;
      struct thread *current_thread = current_sup_page->thread;
      // TODO: check if vm_addr has to be round_up or round_down
      // check if access bit is 0
      bool set = pagedir_is_accessed(current_thread->pagedir, current_sup_page->vm_addr);
      if (set){
        // page is accessed -> set accessed bit to 0 and don't evict this page in this iteration
         pagedir_set_accessed(current_thread->pagedir, current_sup_page->vm_addr, false);
      } else{
        // page is not accessed (can be evicted)-> check if it is dirty
        // TODO add swap stuff
        bool dirty = pagedir_is_dirty(current_thread->pagedir, current_sup_page->vm_addr);
        if (dirty){
          //TODO: treat this different if MMAP is used

        } else {
          // file is not dirty and not accessed -> simply free frame
          // TODO: add to swap table
          // TODO bad could be used by a function but this function would have to ensure that lock is hold

          // remove frame from list and hashmap
          list_remove(&f->l_elem);
          hash_delete (&frame_hashmap, &f->h_elem);
          /* free page and set phys_addr in sup_page to NULL and status to swapped b.c. the data of the frame
              are placed in the swap partition 
          */
          palloc_free_page(f->phys_addr);
          current_sup_page->status = PAGE_SWAPPED;
          current_sup_page->phys_addr = NULL;
          // remove page from pagedir b.c. it is no longer loaded -> sets present bit to 0
          pagedir_clear_page(current_thread->pagedir, current_sup_page->vm_addr);
          // delete the frame entry and allocate a new page
          free(f);
          void* phys_addr = palloc_get_page(pflags|PAL_ZERO);
          // release lock and return phys addr of frame
          lock_release(&frame_lock);
          return phys_addr;
        }
      }
      // page was accessed -> look at next frame if accessed
      iterator = list_next(iterator);
      if (iterator == list_end (&frame_list)){
        // all pages were accssed -> start again with first element to find a page which was not accessed recently
        iterator = list_begin(&frame_list);
      }  
  }
  // should never be reached
  lock_release(&frame_lock);
  return NULL;
}

// TODO use frame_list to iterate over it in clock algorithm
