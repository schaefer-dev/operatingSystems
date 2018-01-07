#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include <stdbool.h>
#include <list.h>
#include "threads/synch.h"

/* lock to guarantee mutal exclusiveness on frame operations */
struct lock frame_lock;
struct lock grow_stack_lock;


struct frame
  {
    /* physical address of frame */
    void *phys_addr;               

    /* supplemental Page Table Entry in which this frame is stored. */
    // for shared memory implementation, this will change into a list of sup_page_entries
    struct sup_page_entry *sup_page_entry;

    // TODO check if this is needed
    // thread which holds this frame to set the pagedir
    //struct thread *thread;

    struct list_elem l_elem;
  };

void vm_frame_init(void);

// TODO writable necessary here?
void* vm_frame_allocate (struct sup_page_entry *sup_page_entry, enum palloc_flags pflags, bool writable);

void vm_frame_free (void* phys_addr, void* upage);
void debug_vm_frame_print(void);
struct frame* vm_frame_lookup (void* phys_addr);

// TODO Pinning functions here? LOCK them!

#endif /* vm/frame.h */
