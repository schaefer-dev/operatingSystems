#include <hash.h>
#include <list.h>
#include <string.h>
#include "lib/kernel/list.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "filesys/file.h"
#include "vm/sup_page.h"
#include "vm/frame.h"
#include "userprog/process.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"

void vm_sup_page_free_stack(struct sup_page_entry *sup_page_entry);
void vm_sup_page_free_mmap(struct sup_page_entry *sup_page_entry);
void vm_sup_page_free_file(struct sup_page_entry *sup_page_entry);
bool vm_write_file_back_on_delete(struct sup_page_entry *sup_page_entry);

/* Hash function for supplemental page table entries */
unsigned
hash_vm_sup_page(const struct hash_elem *sup_p_, void *aux UNUSED)
{
  const struct sup_page_entry *sup_p = 
        hash_entry(sup_p_, struct sup_page_entry, h_elem);

  unsigned hash_val = hash_int((int) sup_p->vm_addr);
  return hash_val;
}


/* Hash compare function for supplemental page table entries */
bool
hash_compare_vm_sup_page(const struct hash_elem *a_, 
      const struct hash_elem *b_, void *aux UNUSED)
{
  const struct sup_page_entry *a = 
        hash_entry (a_, struct sup_page_entry, h_elem);

  const struct sup_page_entry *b = 
        hash_entry (b_, struct sup_page_entry, h_elem);

  bool result = (a->vm_addr) < (b->vm_addr);

  return result;
}



/* returns the sup_page_entry of thread, corresponding to passed vm_addr */
struct sup_page_entry*
vm_sup_page_lookup (struct thread *thread, void *vm_addr)
{
  struct sup_page_entry search_sup_page_entry;
  struct hash_elem *hash;

  search_sup_page_entry.vm_addr = vm_addr;
  hash = hash_find(&(thread->sup_page_hashmap), 
        &(search_sup_page_entry.h_elem));
  
  if (hash != NULL) {
    return hash_entry (hash, struct sup_page_entry, h_elem);
  } else {
    return NULL;
  }
}


/* initialize ressources for supplemental page */
void
vm_sup_page_init (struct thread *thread) {
  hash_init(&thread->sup_page_hashmap, 
        hash_vm_sup_page, hash_compare_vm_sup_page, NULL);
}


/* implementation of stack growth called by page fault handler */
bool
vm_grow_stack(void *fault_frame_addr)
{
  lock_acquire(&grow_stack_lock);
  if(vm_sup_page_lookup(thread_current(), fault_frame_addr) != NULL){
    lock_release(&grow_stack_lock);
    return true;
  }

  struct sup_page_entry *sup_page_entry =  
        vm_sup_page_allocate(fault_frame_addr, true);

  lock_acquire(&sup_page_entry->page_lock);
  sup_page_entry->pinned = true;

  void *page = vm_frame_allocate(sup_page_entry, (PAL_ZERO | PAL_USER), true);

  if (page == NULL){
    lock_release(&sup_page_entry->page_lock);
    lock_release(&grow_stack_lock);
    PANIC("stack growth could not allocate page!\n");
    return false;
  }

  bool success = install_page(sup_page_entry->vm_addr, 
        sup_page_entry->phys_addr, sup_page_entry->writable);
  
  if (!success) {
    PANIC("DEBUG: failure of page install in grow stack\n");
  } else {
    sup_page_entry->status = PAGE_STATUS_LOADED;
    sup_page_entry->pinned = false;
    lock_release(&sup_page_entry->page_lock);
    lock_release(&grow_stack_lock);
  }
  return true;
}



/* function to allocate a supplemental page table entry e.g. a stack*/
struct sup_page_entry*
vm_sup_page_allocate (void *vm_addr, bool writable){
  struct sup_page_entry *sup_page_entry = (struct sup_page_entry *) 
        malloc(sizeof(struct sup_page_entry));

  sup_page_entry->phys_addr = NULL;

  sup_page_entry->vm_addr = vm_addr;
  sup_page_entry->swap_addr = 0;
  sup_page_entry->thread = thread_current();
  sup_page_entry->status = PAGE_STATUS_NOT_LOADED;
  sup_page_entry->type = PAGE_TYPE_STACK;
  sup_page_entry->file = NULL;
  sup_page_entry->read_bytes = 0;
  sup_page_entry->mmap_id=-1;
  sup_page_entry->file_offset = 0;
  sup_page_entry->writable = writable;
  sup_page_entry->pinned = false;
  sup_page_entry->dirty = false;

  lock_init(&sup_page_entry->page_lock);

  hash_insert(&(thread_current()->sup_page_hashmap), &(sup_page_entry->h_elem));

  return sup_page_entry;
}


/* function to allocate a supplemental page table entry for files incuding the file 
and the offset within the file*/
struct sup_page_entry*
vm_sup_page_file_allocate (void *vm_addr, struct file* file, 
      off_t file_offset, off_t read_bytes, bool writable)
{
  struct sup_page_entry *sup_page_entry = (struct sup_page_entry *) 
        malloc(sizeof(struct sup_page_entry));

  sup_page_entry->phys_addr = NULL;
  sup_page_entry->vm_addr = vm_addr;
  sup_page_entry->swap_addr = 0;
  sup_page_entry->thread = thread_current();
  sup_page_entry->status = PAGE_STATUS_NOT_LOADED;
  sup_page_entry->type = PAGE_TYPE_FILE;
  sup_page_entry->file = file;
  sup_page_entry->file_offset = file_offset;
  sup_page_entry->read_bytes = read_bytes;
  sup_page_entry->mmap_id=-1;
  sup_page_entry->writable = writable;
  sup_page_entry->pinned = false;
  sup_page_entry->dirty = false;

  lock_init(&sup_page_entry->page_lock);

  hash_insert(&(thread_current()->sup_page_hashmap), &(sup_page_entry->h_elem));

  return sup_page_entry;
}


/* function to allocate a supplemental page table entry for mmaped 
  files incuding the file and the offset within the file */
struct sup_page_entry*
vm_sup_page_mmap_allocate (void *vm_addr, struct file* file, 
      off_t file_offset, off_t read_bytes, int mmap_id, bool writable)
{
  struct sup_page_entry *sup_page_entry = (struct sup_page_entry *) 
        malloc(sizeof(struct sup_page_entry));

  sup_page_entry->phys_addr = NULL;
  sup_page_entry->vm_addr = vm_addr;
  sup_page_entry->swap_addr = 0;
  sup_page_entry->thread = thread_current();
  sup_page_entry->status = PAGE_STATUS_NOT_LOADED;
  sup_page_entry->type = PAGE_TYPE_MMAP;
  sup_page_entry->file = file;
  sup_page_entry->file_offset = file_offset;
  sup_page_entry->read_bytes = read_bytes;
  sup_page_entry->mmap_id = mmap_id;
  sup_page_entry->writable = writable;
  sup_page_entry->pinned = false;
  sup_page_entry->dirty = false;

  lock_init(&sup_page_entry->page_lock);

  hash_insert(&(thread_current()->sup_page_hashmap), &(sup_page_entry->h_elem));

  return sup_page_entry;
}

/* load this sup_page into physical memory */
void
vm_sup_page_load (struct sup_page_entry *sup_page_entry){
  ASSERT(((int)(sup_page_entry->vm_addr) % PGSIZE) == 0);
  lock_acquire(&sup_page_entry->page_lock);
  sup_page_entry->pinned = true;

  switch (sup_page_entry->status)
    {
      case PAGE_STATUS_NOT_LOADED:
        {
          /* can only happen for FILE / MMAP */
          vm_load_file(sup_page_entry);
          break;
        }

      case PAGE_STATUS_SWAPPED:
        {
          vm_load_swap(sup_page_entry);
          break;
        }

      case PAGE_STATUS_LOADED:
        {
          lock_release(&sup_page_entry->page_lock);
          return;
        }
      default:
        {
          PANIC("Illegal page status in sup_page_load!\n");
        }
    }

  sup_page_entry->status = PAGE_STATUS_LOADED;
  lock_release(&sup_page_entry->page_lock);
  bool success = install_page(sup_page_entry->vm_addr, 
        sup_page_entry->phys_addr, sup_page_entry->writable);
  if (!success)
    PANIC("Page loading page install failed");
}


/* implementation of load from swap partition called by page fault handler */
bool 
vm_load_swap(struct sup_page_entry *sup_page_entry)
{
  ASSERT(lock_held_by_current_thread(&sup_page_entry->page_lock));
  ASSERT(sup_page_entry != NULL);

  void *page = vm_frame_allocate(sup_page_entry, (PAL_ZERO | PAL_USER) , 
        sup_page_entry->writable);

  if (page == NULL){
    PANIC("load file could not allocate page!\n");
  }

  vm_swap_back(sup_page_entry->swap_addr, page);

  /*indicate that frame is now loaded */
  sup_page_entry->phys_addr = page;
  sup_page_entry->status = PAGE_STATUS_LOADED;

  return true;
}

/* implementation of load file called by page fault handler */
bool 
vm_load_file(struct sup_page_entry *sup_page_entry){
  ASSERT(lock_held_by_current_thread(&sup_page_entry->page_lock));
  ASSERT(sup_page_entry != NULL);

  off_t file_offset = sup_page_entry->file_offset;

  off_t read_bytes = sup_page_entry->read_bytes;

  struct file *file = sup_page_entry->file;

  void *page = vm_frame_allocate(sup_page_entry, (PAL_ZERO | PAL_USER), 
        sup_page_entry->writable);

  if (page == NULL){
    PANIC("load file could not allocate page!\n");
  }

  if (read_bytes != 0){
    lock_acquire(&lock_filesystem);
    file_seek(file, file_offset);
    off_t file_read_bytes = file_read (file, page, read_bytes);
    memset (page + read_bytes, 0, PGSIZE - read_bytes);
    if (file_read_bytes != read_bytes){
      /* file not correctly read, free frame and indicate file not loaded */
      lock_release(&lock_filesystem);
      vm_frame_free (page, sup_page_entry->vm_addr);
      return false;
    }
    lock_release(&lock_filesystem);
  }

  /*indicate that frame is now loaded */
  sup_page_entry->phys_addr = page;
  return true;
}

/* load and pin this page (used only for syscall) */
void
vm_sup_page_load_and_pin (struct sup_page_entry *sup_page_entry)
{
  ASSERT(!lock_held_by_current_thread(&sup_page_entry->page_lock));
  /* vm_sup_page_load pins */ 
  vm_sup_page_load(sup_page_entry);
}

/* unpin this page (used only for syscall) */
void
vm_sup_page_unpin (struct sup_page_entry *sup_page_entry)
{
  lock_acquire(&sup_page_entry->page_lock);
  sup_page_entry->pinned = false;
  lock_release(&sup_page_entry->page_lock);
}


/* close the entire hashmap and free all ressources contained in it */
void
vm_sup_page_hashmap_close(struct thread *thread)
{
  hash_destroy(&(thread->sup_page_hashmap), vm_sup_page_free);
}


/* free the ressources of the page with the corresponding hash_elem 
   this function should only be used by hash_destroy! */
void
vm_sup_page_free(struct hash_elem *hash, void *aux UNUSED)
{
  struct sup_page_entry *lookup_sup_page_entry;
  lookup_sup_page_entry = hash_entry(hash, struct sup_page_entry, h_elem);

  lock_acquire(&lookup_sup_page_entry->page_lock);
  switch (lookup_sup_page_entry->type)
    {
      case PAGE_TYPE_MMAP:
        {
          vm_sup_page_free_mmap(lookup_sup_page_entry);
          break;
        }
      case PAGE_TYPE_FILE:
        {
          vm_sup_page_free_file(lookup_sup_page_entry);
          break;
        }
      case PAGE_TYPE_STACK:
        {
          vm_sup_page_free_stack(lookup_sup_page_entry);
          break;
        }
    default:
        {
          PANIC("Illegal PAGE_TYPE found!\n");
        }

    }
  free(lookup_sup_page_entry);
}

/* free the stack page in this sup_page_entry */
void
vm_sup_page_free_stack(struct sup_page_entry *sup_page_entry)
{
  ASSERT(lock_held_by_current_thread(&sup_page_entry->page_lock));
  switch (sup_page_entry->status)
    {
      case PAGE_STATUS_LOADED:
        {
          void *phys_addr = sup_page_entry->phys_addr;
          void *upage = sup_page_entry->vm_addr;
          lock_release(&sup_page_entry->page_lock);
          vm_frame_free(phys_addr, upage);
          break;
        }
      case PAGE_STATUS_NOT_LOADED:
        {
          lock_release(&sup_page_entry->page_lock);
          PANIC("STACK WITH ILLEGAL STATUS NOT_LOADED!\n");
          break;
        }
      case PAGE_STATUS_SWAPPED:
        {
          vm_swap_free(sup_page_entry->swap_addr);
          lock_release(&sup_page_entry->page_lock);
          break;
        }
      default:
        {
          PANIC("Illegal PAGE_STATUS found in free stack!\n");
        }
    }
}


/* free the mmap page in this sup_page_entry */
void
vm_sup_page_free_mmap(struct sup_page_entry *sup_page_entry)
{
  ASSERT(lock_held_by_current_thread(&sup_page_entry->page_lock));
  switch (sup_page_entry->status)
    {
      case PAGE_STATUS_LOADED:
        {
          vm_write_mmap_back(sup_page_entry);
          void *phys_addr = sup_page_entry->phys_addr;
          void *upage = sup_page_entry->vm_addr;
          lock_release(&sup_page_entry->page_lock);
          vm_frame_free(phys_addr, upage);
          break;
        }
      case PAGE_STATUS_SWAPPED:
        {
          lock_release(&sup_page_entry->page_lock);
          PANIC("MMAP page is not allowed to be swapped, ILLEGAL status!\n");
          break;
        }
      case PAGE_STATUS_NOT_LOADED:
        {
          /* in the case of NOT_LOADED nothing has to be written back */
          lock_release(&sup_page_entry->page_lock);
          break;
        }
      default:
        {
          PANIC("Illegal PAGE_STATUS found in free mmap!\n");
        }
    }
}

/* free the file page in this sup_page_entry */
void
vm_sup_page_free_file(struct sup_page_entry *sup_page_entry)
{
  ASSERT(lock_held_by_current_thread(&sup_page_entry->page_lock));
  switch (sup_page_entry->status)
    {
      case PAGE_STATUS_LOADED:
        {
          void *phys_addr = sup_page_entry->phys_addr;
          void *upage = sup_page_entry->vm_addr;
          lock_release(&sup_page_entry->page_lock);
          vm_frame_free(phys_addr, upage);
          break;
        }
      case PAGE_STATUS_SWAPPED:
        {
	        vm_swap_free(sup_page_entry->swap_addr);
          lock_release(&sup_page_entry->page_lock);
          break;
        }
      case PAGE_STATUS_NOT_LOADED:
        {
          /* in the case of NOT_LOADED nothing has to be written back */
          lock_release(&sup_page_entry->page_lock);
          break;
        }
      default:
        {
          PANIC("Illegal PAGE_STATUS found!\n");
        }
    }
}


/* writes mmap sup_page_entry back to file if dirty */
bool vm_write_mmap_back(struct sup_page_entry *sup_page_entry){
  ASSERT(lock_held_by_current_thread(&sup_page_entry->page_lock));
  if (sup_page_entry->type != PAGE_TYPE_MMAP){
    PANIC("try to remove mmap but entry is not of type mmap");
  }

  void* vaddr = sup_page_entry->vm_addr;
  bool dirty = pagedir_is_dirty(sup_page_entry->thread->pagedir, vaddr);

  if(!dirty)
    return true;

  struct file *file = sup_page_entry->file;
  off_t file_offset = sup_page_entry->file_offset;
  off_t read_bytes = sup_page_entry->read_bytes;

  lock_acquire(&lock_filesystem);
  file_write_at(file, vaddr, read_bytes, file_offset);
  lock_release(&lock_filesystem);

  return true;
}


/* removes mmap entry from hashtable and frees! */
bool vm_delete_mmap_entry(struct sup_page_entry *sup_page_entry){

  lock_acquire(&sup_page_entry->page_lock);
  struct thread *thread = sup_page_entry->thread;

  if (!vm_write_mmap_back(sup_page_entry)){
    PANIC("vm_write_mmap_back failed!\n");
  }
  struct hash_elem *hash_elem = hash_delete(&(thread->sup_page_hashmap), 
        &(sup_page_entry->h_elem));

  if (hash_elem == NULL){
    PANIC("element which should be deleted not found");
  }

  void *phys_addr = sup_page_entry->phys_addr;
  void *upage = sup_page_entry->vm_addr;

  if (sup_page_entry->status == PAGE_STATUS_LOADED){
    lock_release(&sup_page_entry->page_lock);
    ASSERT(phys_addr != NULL);
    vm_frame_free(phys_addr, upage);
  }
  
  free(sup_page_entry);
  return true;
}
