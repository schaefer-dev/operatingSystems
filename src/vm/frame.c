#include <hash.h>
#include "lib/kernel/hash.h"
#include <list.h>
#include "lib/kernel/list.h"
#include "threads/palloc.h"


/* lock to guarantee mutal exclusiveness on frame operations */
static struct lock frame_lock;

/* list of all frames */
static struct list frame_list;

/* hashmap from phys_addr -> frame for efficient lookup */
static struct hash frame_hashmap;

struct frame
  {
    /* physical address of frame */
    void *phys_addr;               

    /* supplemental Page Table Entry in which this frame is stored. */
    struct sup_page_entry *sup_page_entry;

    /* thread this frame belongs to */
    struct thread *thread;

    /* boolean to track if the frame is currently pinned */
    bool pinned;

    struct list_elem l_elem;
    struct hash_elem h_elem;
  };


void
vm_frame_init () {
  lock_init (&frame_lock);
  list_init (&frame_list);
  // TODO init hashmap
}


void*
vm_frame_allocate (struct sup_page_entry *sup_page_entry, enum palloc_flags pflags) {
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
  frame->thread = thread_current();
  frame->pinned = false;


  list_push_back (&frame_list, &frame->l_elem);
  hash_insert (&frame_hashmap, &frame->h_elem);

  lock_release (&frame_lock);
  return page;
}


void 
vm_frame_free (void* phys_addr) {
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