#include "threads/malloc.h"
#include <list.h>
#include "lib/kernel/list.h"
#include "threads/palloc.h"
#include "vm/sup_page.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/sup_page.h"



/* list of all frames */
static struct list frame_list;

void* vm_evict_page(enum palloc_flags pflags);

void vm_evict_file(struct sup_page_entry *sup_page_entry, struct frame *frame);
void vm_evict_stack(struct sup_page_entry *sup_page_entry, struct frame *frame);
void vm_evict_mmap(struct sup_page_entry *sup_page_entry);

// TODO implement hash map again for faster frame lookup


struct frame*
vm_frame_lookup (void *phys_addr)
{
  //printf("DEBUG: frame lookup begin\n");
  ASSERT(lock_held_by_current_thread(&frame_lock));

  struct frame *return_frame = NULL;
  struct list_elem *iterator;

  iterator = list_begin(&frame_list);

  if (list_empty(&frame_list)){
    return NULL;
  }

  while (iterator != list_end(&frame_list)){
    struct frame *search_frame;
    search_frame = list_entry(iterator, struct frame, l_elem);
    if (search_frame->phys_addr == phys_addr){
      return_frame = search_frame;
      break;
    }
    iterator = list_next(iterator);
  }

  //printf("DEBUG: frame lookup end\n");
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
  //printf("DEBUG: frame allocate begin\n");
  ASSERT(!lock_held_by_current_thread(&frame_lock));
  lock_acquire (&frame_lock);

  // try to get page using palloc
  void *page = palloc_get_page(PAL_USER | pflags);

  if (page == NULL) {
    // Frame allocation Failed
    // TODO evice page here to make room
    // TODO lock is hold at this place so evict_page shouldn't acquire the lock check if this is a good idea
    page = vm_evict_page(PAL_USER | pflags);
  }

  struct frame *frame = malloc(sizeof(struct frame));
  sup_page_entry->phys_addr = page;
  frame->phys_addr = page;
  frame->sup_page_entry = sup_page_entry;
  frame->pinned = false;

  // install page in pagedir of thread
  install_page(sup_page_entry->vm_addr, page, writable);

  list_push_back (&frame_list, &frame->l_elem);

  lock_release (&frame_lock);
  //printf("DEBUG: frame allocate end\n");
  return page;
}


/* called from vm_sup_page_free if page is currently loaded
   in frames. 
   Frees the frame structure and removes it from pagedir of
   the current thread. */
void 
vm_frame_free (void *phys_addr, void *upage)
{
  //printf("DEBUG: frame free begin\n");
  ASSERT(!lock_held_by_current_thread(&frame_lock));
  lock_acquire (&frame_lock);

  struct frame *found_frame = vm_frame_lookup(phys_addr);

  if (found_frame == NULL) {
    // the table was not found, this should be impossible!
    printf("frame at %p not found in Frame Table!\n", phys_addr);
    lock_release (&frame_lock);
    return;
  }

  list_remove (&found_frame->l_elem);
  palloc_free_page(phys_addr);
  uninstall_page(upage);
  free(found_frame);

  lock_release (&frame_lock);
  //printf("DEBUG: frame free end\n");
  return;
}


/* search for a page to evict in physical memory and returns the adress 
   of the eviced page which can now be written on.
   Evict_page can only be called when the frame lock is currently held */
void*
vm_evict_page(enum palloc_flags pflags){
  printf("DEBUG: frame evict begin\n");
  ASSERT (lock_held_by_current_thread(&frame_lock));
  ASSERT (!list_empty(&frame_list));

  struct list_elem *iterator = list_begin (&frame_list);

  /* iterate over all frames until a frame is not accessed 
     (could be more than iteration) */
  // TODO do we "restart" clock algorithm with every evict start, or do we keep the iterator where we found a free page last time
  // TODO in case of MMAP remember to set page_status to NOT_LOADED
  while (true){
      printf("DEBUG: iteration started\n");
      struct frame *iter_frame = list_entry (iterator, struct frame, l_elem);
      printf("DEBUG: list entry successfully created\n");
      if (iter_frame == NULL)
        printf("DEBUG: Iter Frame is NULL\n");
      struct sup_page_entry *iter_sup_page = iter_frame->sup_page_entry;
      printf("DEBUG: sup page entry read from iter frame successfully\n");
      if (iter_sup_page == NULL){
        // TODO this should never happen? Search for the reason
        printf("DEBUG: Iter sup page is NULL -> skipping\n");
        // page was accessed -> look at next frame if accessed
        printf("DEBUG: set iterator to next element\n");
        if (iterator == list_tail (&frame_list)){
          /* all pages were accessed -> start again with first element to find 
            a page which was not accessed recently */
          printf("DEBUG: continue with frame list begin again\n");
          iterator = list_begin(&frame_list);
        } else {
          iterator = list_next(iterator);
          printf("DEBUG: continue with next list entry\n");
        }
        continue;
      }
      struct thread *page_thread = iter_sup_page->thread;
      // TODO: check if vm_addr has to be round_up or round_down
      // check if access bit is 0
      printf("DEBUG: pagedir is accessed start\n");
      bool accessed_bit = pagedir_is_accessed(page_thread->pagedir, iter_sup_page->vm_addr);
      printf("DEBUG: pagedir is accessed finished\n");
      if (accessed_bit){
         /* page was accessed since previous iteration -> set accessed bit to 0 
            and don't evict this page in this iteration */
         pagedir_set_accessed(page_thread->pagedir, iter_sup_page->vm_addr, false);
      } else {

          switch (iter_sup_page->type)
          {
            case PAGE_TYPE_MMAP:
              {
                printf("DEBUG: evicting MMAP case\n");
                vm_evict_mmap(iter_sup_page);
                break;
              }
            case PAGE_TYPE_FILE:
              {
                printf("DEBUG: evicting FILE case\n");
                vm_evict_file(iter_sup_page, iter_frame);
                break;
              }
            case PAGE_TYPE_STACK:
              {
                printf("DEBUG: evicting STACK case\n");
                vm_evict_stack(iter_sup_page, iter_frame);
                break;
              }
          default:
            {
              printf("Illegal PAGE_TYPE found!\n");
              syscall_exit(-1);
              break;
            }

          }
          printf("DEBUG: removing from list started\n");
          // remove frame from list
          list_remove(&iter_frame->l_elem);
          printf("DEBUG: removing from list finished\n");
          /* free page and set phys_addr in sup_page to NULL and status to swapped b.c. the data of the frame
              are placed in the swap partition 
          */
          palloc_free_page(iter_frame->phys_addr);
          iter_sup_page->phys_addr = NULL;

          printf("DEBUG: removing from pagedir started\n");
          pagedir_clear_page(page_thread->pagedir, iter_sup_page->vm_addr);
          printf("DEBUG: removing from pagedir finished\n");
          // delete the frame entry and allocate a new page
          free(iter_frame);

          void* phys_addr = palloc_get_page(pflags | PAL_ZERO);
          printf("DEBUG: frame evict end\n");
          return phys_addr;
      }


      // page was accessed -> look at next frame if accessed
      printf("DEBUG: set iterator to next element\n");
      if (iterator == list_tail (&frame_list)){
        /* all pages were accessed -> start again with first element to find 
           a page which was not accessed recently */
        printf("DEBUG: continue with frame list begin again\n");
        iterator = list_begin(&frame_list);
      } else {
        iterator = list_next(iterator);
        printf("DEBUG: continue with next list entry\n");
      }
  }
  // should never be reached
  printf("DEBUG: frame evict end\n");
  return NULL;
}

/* writes file content to swap if necessary(page is dirty) and sets the status
  based on wheter it was dirty or not */
void vm_evict_file(struct sup_page_entry *sup_page_entry, struct frame *frame){
  struct thread *thread = sup_page_entry->thread;
  bool dirty = pagedir_is_dirty(thread->pagedir, sup_page_entry->vm_addr);
  if (dirty){
    /* changed content has to be written to swap partition */
    printf("DEBUG: Evicting file case dirty!\n");
    block_sector_t swap_block = vm_swap_page(frame->phys_addr);
    sup_page_entry->status = PAGE_STATUS_SWAPPED;
    sup_page_entry->swap_addr = swap_block;
  } else {
    /* nothing has changed, content can simply be loaded from file */
    printf("DEBUG: Evicting file case not dirty!\n");
    sup_page_entry->status = PAGE_STATUS_NOT_LOADED;
  }
}

/* writes page content to swap and sets the status */
void vm_evict_stack(struct sup_page_entry *sup_page_entry, struct frame *frame){
  block_sector_t swap_block = vm_swap_page(frame->phys_addr);
  sup_page_entry->status = PAGE_STATUS_SWAPPED;
  sup_page_entry->swap_addr = swap_block;
}

/* writes mmap file content back to file if necessary(page is dirty) and sets 
  the status to not loaded b.c. we always load from file */
void vm_evict_mmap(struct sup_page_entry *sup_page_entry){
  vm_write_mmap_back(sup_page_entry);
  sup_page_entry->status = PAGE_STATUS_NOT_LOADED;
}

// TODO use frame_list to iterate over it in clock algorithm
