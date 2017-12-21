#include "devices/block.h"
#include "vm/frame.h"
#include "thread/vaddr.h"
#include "lib/kernel/bitmap.h"

// TODO some of these dont need to be static?
static struct block *swap;
static unsigned swap_size;

/* Number of sectors necessary to store an entire page */
static struct SECTORS_FOR_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

static struct bitmap *swap_free_bitmap;

static struct lock swap_lock;


void 
vm_swap_init()
{
    swap = block_get_role(BLOCK_SWAP);

    if (swap == NULL){
        PANIC ("No SWAP disk declared!");
    }
    swap_size = block_size(swap);
    swap_free_bitmap = bitmap_create(swap_size);
    bitmap_set_all (swap_free_bitmap, true);
    lock_init(&swap_lock);
}


/* returns the first sector in which a page can be stored 
   calls Kernel Panic when not possible */
block_sector_t
vm_swap_get_free()
{
    lock_acquire(swap_lock);
    if (bitmap_all(swap_free_bitmap, 0, swap_size)){
        lock_release(swap_lock);
        PANIC("SWAP is completely full!");
        return;
    }

    block_sector_t free_sector = bitmap_scan_and_flip (swap_free_bitmap, 0, SECTORS_FOR_PAGE, true);

    if (free_sector == BITMAP_ERROR){
        lock_release(swap_lock);
        PANIC("No SWAP Block of sufficient size found!");
        return;
    }

    lock_release(swap_lock);
    return free_sector;
}


/* writes the page into the first sector in which a page can be stored 
   calls Kernel Panic when not possible */
block_sector_t
vm_swap_page(void *page)
{
    // TODO verify page reference? is_uservaddr?
    // TODO maybe we have to lock here, but vm_swap_get_free might be sufficient!

    block_sector_t free_sector = vm_swap_get_free();

    /* write SECTORS_FOR_PAGE amount of blocks into swap starting at block free_sector */
    block_sector_t sector_iterator = 0;
    for (int sector_iterator = 0; sector_iterator < SECTORS_FOR_PAGE; sector_iterator++){
        /* block_write Internally synchronizes accesses to block devices, so
           external per-block device locking is unneeded. */
        block_write(swap, free_sector + sector_iterator,
                    page + (BLOCK_SECTOR_SIZE * sector_iterator));
    }

    bitmap_set(swap_free_bitmap, free_sector, false);
    return free_sector;
}


/* writes the sectors starting at swap_sector into the passed page */
void
vm_swap_back(block_sector_t swap_sector, void *page)
{
    // TODO verify page reference? is_uservaddr?
    // TODO maybe we have to lock here, but vm_swap_get_free might be sufficient!

    ASSERT(swap_sector < swap_size);
    
    /* if the sector we are trying to swap back is free, something went horribly wrong! */
    if (bitmap.test(swap_free_bitmap, swap_sector) == true){
        printf("trying to swap back a free swap sector, this should never happen!");
        return;
    }

    /* write SECTORS_FOR_PAGE amount of blocks into page starting at block swap_sector */
    block_sector_t sector_iterator = 0;
    for (int sector_iterator = 0; sector_iterator < SECTORS_FOR_PAGE; sector_iterator++){
        /* block_write Internally synchronizes accesses to block devices, so
           external per-block device locking is unneeded. */
        block_read(swap, block_sector + sector_iterator,
                    page + (BLOCK_SECTOR_SIZE * sector_iterator));
    }

     bitmap_set(swap_free_bitmap, swap_sector, true);
}


/* just force frees the passed swap sector */
void 
vm_swap_free(block_sector_t swap_sector)
{
    if (bitmap_test(swap_free_bitmap, swap_sector) == true) {
        printf("trying to free a already free swap block, this should never happen!");
        return;
    }

    bitmap_set(swap_free_bitmap, swap_sector, true);
}