#ifndef VM_SWAP_H
#define VM_SWAP_H
/* Include the head files. */
#include <bitmap.h>
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "devices/block.h"

/* Define number of sector per page. */
#define SEC_NUM_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

/* Swap block and the map table. */
struct block* swap_block;
struct bitmap* swap_map;

/* Initialize, swap out and swap in.*/
void swap_init();
unsigned swap_write_out(void* frame_addr);
void swap_read_in(unsigned idx, void* frame_addr);
#endif