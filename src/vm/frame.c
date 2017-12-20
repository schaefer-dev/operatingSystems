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
    // for shared memory implementation, this will change into a list of sup_page_entries
    struct sup_page_entry *sup_page_entry;

    /* boolean to track if the frame is currently pinned */
    bool pinned;

    struct list_elem l_elem;
    struct hash_elem h_elem;
  };

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


void*
vm_frame_allocate (struct sup_page_entry *sup_page_entry, enum palloc_flags pflags)
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


  list_push_back (&frame_list, &frame->l_elem);
  hash_insert (&frame_hashmap, &frame->h_elem);

  lock_release (&frame_lock);
  return page;
}


void 
vm_frame_free (void* phys_addr)
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

  free(found_frame);

  lock_release (&frame_lock);
  return;
}

// TODO use frame_list to iterate over it in clock algorithm