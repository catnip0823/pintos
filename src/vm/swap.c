#include "vm/swap.h"
#include "threads/synch.h"
/* Initialize  the swap block. */
bool
swap_init(){
	// lock_init(&swap_lock);
    swap_block = block_get_role(BLOCK_SWAP);
	if (swap_block == NULL)
		return false;
	swap_map = bitmap_create(block_size(swap_block) / SEC_NUM_PER_PAGE);
	if (swap_map == NULL)
		return false;
	bitmap_set_all(swap_map, true);
	return true;
}


/* Write the content in the frame addr to the swap
   partition. First check the given pointer and 
   whether the block and map has been initialized.
   Return the */
unsigned int
swap_write_out(void* frame_addr){
	// if (frame_addr == NULL)
	// 	PANIC("Invalid addr for swap.");
	// if (swap_block == NULL)
	// 	PANIC("Swap block need to be initialized.");
	// if (swap_map == NULL)
	// 	PANIC("Bitmap need to be initialized.");

	size_t first_free = bitmap_scan_and_flip(swap_map, 0, 1, 1);
	// if (first_free == BITMAP_ERROR)

	unsigned int i;
	for (i = 0; i < SEC_NUM_PER_PAGE; i++)
		block_write(swap_block, first_free * SEC_NUM_PER_PAGE + i,
			(uint8_t*)frame_addr + i * BLOCK_SECTOR_SIZE);
	// printf("swap out %d\n", first_free);
	// bitmap_set(swap_map, first_free, false);
	return first_free;
}


/* Read the data in the swap partition back to frame
   address. Return a boolean value indicate whether 
   it succeed to do so. */
bool
swap_read_in(unsigned int idx, void* frame_addr){
	// printf("swap in %d!\n", idx);
	// if (frame_addr == NULL || swap_block == NULL || swap_map == NULL)
		// return false;
	// if (bitmap_test(swap_map, idx) == true)
		// PANIC("Swap in wrong place.");
	// bitmap_flip(swap_map, idx);
	unsigned int i;
	for (i = 0; i < SEC_NUM_PER_PAGE; i++)
		block_read(swap_block, idx*SEC_NUM_PER_PAGE+i,
						(uint8_t*)frame_addr + i*BLOCK_SECTOR_SIZE);
	bitmap_set(swap_map, idx, true);
	return true;
}