#ifndef VM_SUP_PAGE_H
#define VM_SUP_PAGE_H

#include "lib/kernel/hash.h"
#include "threads/palloc.h"
#include "devices/block.h"

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

    // TODO: How are files handeld in supplemental page?
    struct file *file;

    /* bool to indicat if file/page is writable */
    bool writable;
    
    /* palloc flags which are used when the frame is created */
    enum palloc_flags pflags;

    struct hash_elem h_elem;
  };


bool vm_sup_page_allocate (void *upage, void *kpage);
bool vm_sup_page_file_allocate (void *vm_addr, struct file* file, unsigned file_offset, bool writable);

void vm_sup_page_free (void* phys_addr);

struct sup_page_entry* vm_sup_page_lookup (const struct thread *thread, const void* vm_addr);
bool hash_compare_vm_sup_page(const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
unsigned hash_vm_sup_page(const struct hash_elem *hash, void *aux);
void vm_sup_page_hashmap_close(const struct thread *thread);


#endif /* vm/sup_page.h */
