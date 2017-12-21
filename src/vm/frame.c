#include <hash.h>
#include "lib/kernel/hash.h"
#include "threads/malloc.h"
#include <list.h>
#include "lib/kernel/list.h"
#include "threads/palloc.h"
#include "vm/sup_page.h"
#include "userprog/process.h"
#include "vm/frame.h"


/* lock to guarantee mutal exclusiveness on frame operations */
static struct lock frame_lock;

/* list of all frames */
static struct list frame_list;

/* hashmap from phys_addr -> frame for efficient lookup */
static struct hash frame_hashmap;

unsigned
hash_vm_frame(const struct hash_elem *hash, void *aux aux)
{
  const struct frame *frame;
  frame = hash_entry(hash, struct frame, h_elem);
  return hash_bytes(&frame->phys_addr, sizeof(frame->phys_addr));
}

bool
hash_compare_vm_frame(const struct hash_elem *a_, const struct hash_elem *b_, void *aux aux)
{
  const struct frame *a = hash_entry (a_, struct hash, h_elem);
  const struct frame *b = hash_entry (b_, struct hash, h_elem);

  return (a->phys_addr < b->phys_addr);
}

struct frame*
vm_frame_lookup (const void* phys_addr)
{
  struct frame search_frame;
  struct hash_elem *hash;

  search_frame.phys_addr = phys_addr;
  hash = hash_find(&frame_hashmap, &search_frame.h_elem);
  return e != NULL ? hash_entry (hash, struct frame, h_elem) : NULL; 
}


void
vm_frame_init () {
  lock_init (&frame_lock);
  list_init (&frame_list);
  hash_init(&frame_hashmap, hash_vm_frame, hash_compare_vm_frame, NULL);
}

//TODO: add page to pagedir of the corresponding thread
void*
vm_frame_allocate (struct sup_page_entry *sup_page_entry, enum palloc_flags pflags, bool writable)
{
  lock_acquire (&frame_lock);

  // try to get page using palloc
  void *page = palloc_get_page(PAL_USER | pflags);

  if (page == NULL) {
    // Frame allocation Failed
    // TODO evice page here to make room
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
  uninstall_page(upage);

  free(found_frame);

  lock_release (&frame_lock);
  return;
}


void*
evict_page(enum palloc_flags pflags){
  //TODO: check if this is necessary (b.c. we have to ensure that the list doesn't change during eviction)
  lock_acquire(&frame_lock);
  if (list_empty(frame_list))
    return;

  struct list_elem *iterator = list_begin (frame_list);

  while (iterator != list_end (frame_list)){
      struct frame *f = list_entry (iterator, struct frame, l_elem);

      struct list_elem *removeElem = iterator;
      // TODO: this changes if sharing is implemented b.c. there is a list of sup_page_entries
      // check if access bit is 0
      struct sup_page_entry *current_sup_page = f->sup_page_entry;
      struct thread *current_thread = current_sup_page->thread;
      // TODO: check if vm_addr has to be round_up or round_down
      bool set = pagedir_is_accessed(current_thread->pagedir, sup_page_entry->vm_addr);
      if (set){
        // page is accessed -> set accessed bit to 0
         pagedir_set_accessed(current_thread->pagedir, sup_page_entry->vm_addr, false);
      } else{
        // page is not accessed -> check if it is dirty
        bool dirty = pagedir_is_dirty(current_thread->pagedir, sup_page_entry->vm_addr);
        if (dirty){
          //TODO: treat this different if MMAP is used

        } else {
          // file is not dirty and not accessed -> simply free frame
          // TODO: add to swap table
          // TODO bad could be used by a function but this function would have to ensure that lock is hold
          list_remove(&f->l_elem);
          hash_delete (&frame_hashmap, &found_frame->h_elem);
          pagedir_clear_page(current_thread->pagedir, sup_page_entry->vm_addr);
          palloc_free_page(f->phys_addr);
          sup_page_entry->status = PAGE_SWAPPED;
          sup_page_entry->phys_addr = NULL;
          uninstall_page(upage);
          free(f);
          lock_release(&frame_lock);
          return palloc_get_page(pflags);
        }
      }
      iterator = list_next(iterator);
      if (iterator == list_end (&frame_list)){
        iterator = list_begin(&frame_list);
      }  
  }
  // should never be reached
  lock_release(&frame_lock);
  return NULL;
}

// TODO use frame_list to iterate over it in clock algorithm
