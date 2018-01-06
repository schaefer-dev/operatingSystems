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

static struct list frame_list;
struct list_elem *clock_iterator;

void* vm_evict_page(enum palloc_flags pflags);
void vm_evict_page_next_iterator(void);

void vm_evict_file(struct sup_page_entry *sup_page_entry, struct frame *frame);
void vm_evict_stack(struct sup_page_entry *sup_page_entry, struct frame *frame);
void vm_evict_mmap(struct sup_page_entry *sup_page_entry);

// TODO implement hash map again for faster frame lookup

struct frame*
vm_frame_lookup (void *phys_addr)
{
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

  return return_frame;
}


void
vm_frame_init () {
  lock_init (&frame_lock);
  list_init (&frame_list);
  clock_iterator = NULL;
}


void*
vm_frame_allocate (struct sup_page_entry *sup_page_entry, enum palloc_flags pflags, bool writable)
{
  lock_acquire (&frame_lock);

  void *page = palloc_get_page(PAL_USER | pflags);

  if (page == NULL) {
    page = vm_evict_page(PAL_USER | pflags);
  }

  struct frame *frame = malloc(sizeof(struct frame));
  sup_page_entry->phys_addr = page;
  frame->phys_addr = page;
  frame->sup_page_entry = sup_page_entry;
  frame->sup_page_entry->status = PAGE_STATUS_LOADED;

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
vm_frame_free (void *phys_addr, void *upage)
{
  ASSERT(upage != NULL);
  ASSERT(phys_addr != NULL);

  struct frame *found_frame = vm_frame_lookup(phys_addr);

  if (found_frame == NULL) {
    // the table was not found, this should be impossible!
    printf("frame at %p not found in Frame Table!\n", phys_addr);
    return;
  }

  /* if removed frame is current itertion move iterator one step */
  if (clock_iterator != NULL){
    struct frame *iter_frame = list_entry (clock_iterator, struct frame, l_elem);
    if (found_frame->phys_addr == iter_frame->phys_addr){
      vm_evict_page_next_iterator();
    }
  }

  list_remove (&found_frame->l_elem);
  palloc_free_page(phys_addr);
  // TODO is uninstall_page correct here? Maybe better put elsewhere
  uninstall_page(upage);
  free(found_frame);

  return;
}


void
vm_evict_page_next_iterator(void){
  if (list_empty(&frame_list)){
    clock_iterator = NULL;
  }
  if (clock_iterator == list_tail (&frame_list)){
    /* all pages were accessed -> start again with first element to find 
      a page which was not accessed recently */
    clock_iterator = list_begin(&frame_list);
  } else {
    clock_iterator = list_next(clock_iterator);
  }
}


/* search for a page to evict in physical memory and returns the adress 
   of the eviced page which can now be written on.
   Evict_page can only be called when the frame lock is currently held */
void*
vm_evict_page(enum palloc_flags pflags){
  ASSERT (lock_held_by_current_thread(&frame_lock));
  ASSERT (!list_empty(&frame_list));

  if (clock_iterator == NULL)
    clock_iterator = list_begin (&frame_list);

  /* iterate over all frames until a frame is not accessed 
     (could be more than iteration) */
  while (true){
      struct frame *iter_frame = list_entry (clock_iterator, struct frame, l_elem);
       if (iter_frame == NULL)
         printf("DEBUG: Iter Frame is NULL, this should never happen\n");
      struct sup_page_entry *iter_sup_page = iter_frame->sup_page_entry;

      if (iter_sup_page == NULL){
        // TODO this should never happen? Search for the reason
        //printf("DEBUG: Iter sup page of frame is NULL at phys_addr: %p -> skipping in eviction\n", iter_frame->phys_addr);
        // page was accessed -> look at next frame if accessed
        vm_evict_page_next_iterator();
        continue;
      }

      /* if sup page is pinned look at next page */
      /* TODO check why i cant acquire page lock here as it should be different sup page 
         than the one i am currently allocating for */
      if (iter_sup_page->pinned == true){
        vm_evict_page_next_iterator();
        continue;
      }

      struct thread *page_thread = iter_sup_page->thread;

      bool accessed_bit = pagedir_is_accessed(page_thread->pagedir, iter_sup_page->vm_addr);
      if (accessed_bit){
         /* page was accessed since previous iteration -> set accessed bit to 0 
            and don't evict this page in this iteration */
         pagedir_set_accessed(page_thread->pagedir, iter_sup_page->vm_addr, false);
      } else {

        switch (iter_sup_page->type)
        {
          case PAGE_TYPE_MMAP:
            {
              vm_evict_mmap(iter_sup_page);
              break;
            }
          case PAGE_TYPE_FILE:
            {
              vm_evict_file(iter_sup_page, iter_frame);
              break;
            }
          case PAGE_TYPE_STACK:
            {
              vm_evict_stack(iter_sup_page, iter_frame);
              break;
            }
        default:
          {
            syscall_exit(-1);
            break;
          }

        }
        /* frame is current itertion move iterator one step */
        vm_evict_page_next_iterator();

        list_remove(&(iter_frame->l_elem));

        /* free page and set phys_addr in sup_page to NULL and status to swapped b.c. the data of the frame
            are placed in the swap partition 
        */
        palloc_free_page(iter_frame->phys_addr);

        pagedir_clear_page(page_thread->pagedir, iter_sup_page->vm_addr);
        free(iter_frame);

        void* phys_addr = palloc_get_page(pflags | PAL_ZERO);

        return phys_addr;
      }

      // page was accessed -> look at next frame if accessed
      vm_evict_page_next_iterator();
  }
  return NULL;
}

/* writes file content to swap if necessary(page is dirty) and sets the status
  based on wheter it was dirty or not */
void vm_evict_file(struct sup_page_entry *sup_page_entry, struct frame *frame){
  ASSERT(sup_page_entry != NULL);
  
  struct thread *thread = sup_page_entry->thread;
  bool dirty = pagedir_is_dirty(thread->pagedir, sup_page_entry->vm_addr);
  if (dirty){
    /* changed content has to be written to swap partition */
    block_sector_t swap_block = vm_swap_page(frame->phys_addr);
    sup_page_entry->status = PAGE_STATUS_SWAPPED;
    sup_page_entry->swap_addr = swap_block;
  } else {
    /* nothing has changed, content can simply be loaded from file */
    sup_page_entry->status = PAGE_STATUS_NOT_LOADED;
  }
}

/* writes page content to swap and sets the status */
void vm_evict_stack(struct sup_page_entry *sup_page_entry, struct frame *frame){
  ASSERT(sup_page_entry != NULL);
  ASSERT (lock_held_by_current_thread(&frame_lock));

  block_sector_t swap_block = vm_swap_page(frame->phys_addr);
  sup_page_entry->status = PAGE_STATUS_SWAPPED;
  sup_page_entry->swap_addr = swap_block;
}

/* writes mmap file content back to file if necessary(page is dirty) and sets 
  the status to not loaded b.c. we always load from file */
void vm_evict_mmap(struct sup_page_entry *sup_page_entry){
  ASSERT(sup_page_entry != NULL);
  ASSERT (lock_held_by_current_thread(&frame_lock));

  vm_write_mmap_back(sup_page_entry);
  sup_page_entry->status = PAGE_STATUS_NOT_LOADED;
}
