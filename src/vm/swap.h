#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>

#include "threads/vaddr.h"
#include "threads/synch.h"
#include "devices/block.h"

#define SEC_NUM_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

enum swap_status
{
	FREE, IN_USE
};


struct block* swap_block;
struct bitmap* swap_map;
struct lock swap_lock;

// void swap_init();
unsigned swap_write_out(void* frame_addr);
//void swap_read_in(unsigned idx, void* frame_addr);

#endif