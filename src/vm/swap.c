#include "devices/block.h"
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "lib/kernel/bitmap.h"
#include <bitmap.h>
#include "vm/swap.h"
#include <stdio.h>

// TODO some of these dont need to be static?
static struct block *swap;
static unsigned swap_size;

/* Number of sectors necessary to store an entire page */
static uint32_t SECTORS_FOR_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

/* value true -> free swap */
static struct bitmap *swap_free_bitmap;

static struct lock swap_lock;

block_sector_t vm_swap_get_free(void);

void 
vm_swap_init(void)
{
    swap = block_get_role(BLOCK_SWAP);

    if (swap == NULL){
        PANIC ("No SWAP disk declared!");
    }

    /* swap size = how many pages can be contained in the swap-block */
    swap_size = (block_size(swap) / SECTORS_FOR_PAGE);
    swap_free_bitmap = bitmap_create(swap_size);
    bitmap_set_all (swap_free_bitmap, true);
    lock_init(&swap_lock);
}


/* returns the first sector in which a page can be stored 
   calls Kernel Panic when not possible */
block_sector_t
vm_swap_get_free(void)
{
  ASSERT(lock_held_by_current_thread(&swap_lock));  

  /* get the first sector with SECTORS_FOR_PAGE successive free secors and sets these sectors to 
      occupied already */
  block_sector_t free_sector = bitmap_scan (swap_free_bitmap, 0, 1, true);

  if (free_sector == BITMAP_ERROR){
    PANIC("No SWAP Block of sufficient size found!");
    //TODO: check if return 0 is okay
    return 0;
  }

  bitmap_set (swap_free_bitmap, free_sector, false);

  return free_sector;
}


/* writes the page into the first sector in which a page can be stored 
   calls Kernel Panic when not possible */
block_sector_t
vm_swap_page(void *phys_addr)
{
  // TODO verify page reference? is_uservaddr?
  lock_acquire(&swap_lock);

  block_sector_t free_sector = vm_swap_get_free();

  /* write SECTORS_FOR_PAGE amount of blocks into swap starting at block free_sector */
  block_sector_t sector_iterator = 0;
  for (sector_iterator = 0; sector_iterator < SECTORS_FOR_PAGE; sector_iterator++){
      /* TODO block_write Internally synchronizes accesses to block devices, so
          external per-block device locking is unneeded. */
      block_write(swap, free_sector * SECTORS_FOR_PAGE + sector_iterator,
                  phys_addr + (BLOCK_SECTOR_SIZE * sector_iterator));
  }

  lock_release(&swap_lock);
  return free_sector;
}


/* writes the sectors starting at swap_sector into the passed page. Because a page
   contains out of more than one sector, we adress sectors not by their sector Number
   but rather with the index they are contained in the bitmap. This makes adressing
   much easier to understand. */
void
vm_swap_back(block_sector_t swap_sector, void *phys_addr)
{
  // TODO verify page reference? is_uservaddr?
  ASSERT(swap_sector < swap_size);
  lock_acquire(&swap_lock);

  /* if the sector we are trying to swap back is free, something went horribly wrong! */
  if (bitmap_test(swap_free_bitmap, swap_sector) == true){
    printf("trying to swap back a free swap sector, this should never happen!\n");
    lock_release(&swap_lock);
    return;
  }

  /* write SECTORS_FOR_PAGE amount of blocks into page starting at block swap_sector */
  block_sector_t sector_iterator = 0;
  for (sector_iterator = 0; sector_iterator < SECTORS_FOR_PAGE; sector_iterator++){
      /* block_write Internally synchronizes accesses to block devices, so
          external per-block device locking is unneeded. */
      block_read(swap, swap_sector * SECTORS_FOR_PAGE + sector_iterator,
                  phys_addr + (BLOCK_SECTOR_SIZE * sector_iterator));
  }

  bitmap_set(swap_free_bitmap, swap_sector, true);
  lock_release(&swap_lock);
}


/* just force frees the passed swap sector */
void 
vm_swap_free(block_sector_t swap_sector)
{
  lock_acquire(&swap_lock);
  if (bitmap_test(swap_free_bitmap, swap_sector) == true) {
      printf("trying to free a already free swap block, this should never happen!\n");
      return;
  }

  bitmap_set(swap_free_bitmap, swap_sector, true);
  lock_release(&swap_lock);
}
