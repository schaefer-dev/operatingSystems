#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/hash.h"
#include "threads/palloc.h"

void* vm_frame_allocate (struct sup_page_entry *sup_page_entry, enum palloc_flags pflags);

void vm_frame_free (void* phys_addr);

// TODO Pinning functions here? LOCK them!

#endif /* vm/frame.h */