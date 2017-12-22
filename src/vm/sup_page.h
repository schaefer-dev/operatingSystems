#ifndef VM_SUP_PAGE_H
#define VM_SUP_PAGE_H

#include "lib/kernel/hash.h"
#include "threads/palloc.h"
#include "devices/block.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include <stdio.h>


// TODO rethink stack_size
static int STACK_SIZE = 8 * 1048576;

/* Different statuses the page can have */
enum page_status
  {
    PAGE_ZEROED = 1,              /* Page Zeroed */
    PAGE_NOT_LOADED = 2,          /* page not loaded */
    PAGE_LOADED = 4,              /* Page Loaded */
    PAGE_SWAPPED = 8,             /* Page Eviced and written to SWAP */

    // TODO More page status possible? Is file correct?
    PAGE_FILE = 16                 /* Page of File */
  };


struct sup_page_entry
  {
    enum page_status status;

    /* physical address of supplemental page */
    void *phys_addr; 

    /* Virtual Memory adress this page will be loaded to in a page fault */
    void *vm_addr;

    /* swap sector of swapped page */
    block_sector_t swap_addr;

    /* thread this frame belongs to */
    struct thread *thread;

    /* offset at which this page starts reading the referenced file */
    off_t file_offset;

    /* size of read_bytes in file at position file_offset*/

    /* file from which we read our data */
    struct file *file;

    /* bool to indicat if file/page is writable */
    bool writable;
    
    /* palloc flags which are used when the frame is created */
    enum palloc_flags pflags;

    struct hash_elem h_elem;
  };

void vm_grow_stack(void *fault_frame_addr);

unsigned hash_vm_sup_page(const struct hash_elem *sup_p_, void *aux UNUSED);
bool hash_compare_vm_sup_page(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

void vm_sup_page_init(struct thread *thread);
bool vm_sup_page_allocate (void *vm_addr, bool writable);
bool vm_sup_page_file_allocate (void *vm_addr, struct file* file, off_t file_offset, off_t read_bytes,bool writable);

void vm_sup_page_free(struct hash_elem *hash, void *aux UNUSED);
struct sup_page_entry* vm_sup_page_lookup (struct thread *thread, void* vm_addr);
void vm_sup_page_hashmap_close(struct thread *thread);
bool vm_load_file(void *fault_frame_addr);
bool vm_load_swap(void *fault_frame_addr);
bool vm_grow_stack(void *fault_frame_addr);


#endif /* vm/sup_page.h */
