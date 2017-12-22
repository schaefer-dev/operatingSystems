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

// TODO implement hash map again for faster frame lookup


struct frame*
vm_frame_lookup (void* phys_addr)
{
  //TODO check that frame lock is currently held when calling this function
  struct frame *return_frame = NULL;
  struct list_elem *iterator;

  iterator = list_begin(&frame_list);

  while (iterator != list_end(&frame_list)){
    struct frame *search_frame;
    search_frame = list_entry(iterator, struct frame, l_elem);
    if (search_frame->phys_addr == phys_addr){
      return_frame = search_frame;
      break;
    }
    iterator = list_next(iterator);
  }

  return return_frame;
}


void
vm_frame_init () {
  lock_init (&frame_lock);
  list_init (&frame_list);
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

  lock_release (&frame_lock);
  return page;
}

/* called from vm_sup_page_free if page is currently loaded
   in frames. 
   Frees the frame structure and removes it from pagedir of
   the current thread. */
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

  list_remove (&found_frame->l_elem);
  palloc_free_page(phys_addr);
  uninstall_page(upage);
  free(found_frame);

  lock_release (&frame_lock);
  return;
}


/* search for a page to evict in physical memory and returns the adress 
   of the eviced page which can now be written on.
   Evict_page can only be called when the frame lock is currently held */
void*
evict_page(enum palloc_flags pflags){
  ASSERT (lock_held_by_current_thread(&frame_lock));
  ASSERT (! list_empty(&frame_list));

  struct list_elem *iterator = list_begin (&frame_list);

  /* iterate over all frames until a frame is not accessed 
     (could be more than iteration) */
  // TODO do we "restart" clock algorithm with every evict start, or do we keep the iterator where we found a free page last time
  while (true){
      struct frame *iter_frame = list_entry (iterator, struct frame, l_elem);
      struct sup_page_entry *iter_sup_page = iter_frame->sup_page_entry;
      struct thread *page_thread = iter_sup_page->thread;
      // TODO: check if vm_addr has to be round_up or round_down
      // check if access bit is 0
      bool accessed_bit = pagedir_is_accessed(page_thread->pagedir, iter_sup_page->vm_addr);
      if (accessed_bit){
         /* page was accessed since previous iteration -> set accessed bit to 0 
            and don't evict this page in this iteration */
         pagedir_set_accessed(page_thread->pagedir, iter_sup_page->vm_addr, false);
      } else {
        // page is not accessed (can be evicted)-> check if it is dirty
        bool dirty = pagedir_is_dirty(page_thread->pagedir, iter_sup_page->vm_addr);
        if (dirty){
          //TODO: treat this different if MMAP is used

        } else {
          // file is not dirty and not accessed -> simply free frame
          // TODO: add to swap table

          // remove frame from list
          list_remove(&iter_frame->l_elem);
          /* free page and set phys_addr in sup_page to NULL and status to swapped b.c. the data of the frame
              are placed in the swap partition 
          */
          palloc_free_page(iter_frame->phys_addr);
          iter_sup_page->status = PAGE_STATUS_SWAPPED;
          iter_sup_page->phys_addr = NULL;

          pagedir_clear_page(page_thread->pagedir, iter_sup_page->vm_addr);
          // delete the frame entry and allocate a new page
          free(iter_frame);

          void* phys_addr = palloc_get_page(pflags | PAL_ZERO);
          return phys_addr;
        }
      }


      // page was accessed -> look at next frame if accessed
      iterator = list_next(iterator);
      if (iterator == list_end (&frame_list)){
        /* all pages were accessed -> start again with first element to find 
           a page which was not accessed recently */
        iterator = list_begin(&frame_list);
      }  
  }
  // should never be reached
  return NULL;
}

// TODO use frame_list to iterate over it in clock algorithm
