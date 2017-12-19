#ifndef VM_SUP_PAGE_H
#define VM_SUP_PAGE_H

#include "lib/kernel/hash.h"
#include "threads/palloc.h"

bool vm_sup_page_allocate (hash sup_page_hashmap, void *upage, void *kpage);

void vm_sup_page_free (void* phys_addr);


/* Different statuses the page can have */
enum page_status
  {
    PAGE_ZEROED = 1,              /* Page Zeroed */
    PAGE_LOADED = 2,              /* Page Loaded */
    PAGE_SWAPPED = 4,             /* Page Eviced and written to SWAP */

    // TODO More page status possible? Is file correct?
    PAGE_FILE = 8                 /* Page of File */
  };

#endif /* vm/sup_page.h */