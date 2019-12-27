#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

/* Include the header file we need. */
#include "devices/block.h"
#include "threads/synch.h"


/* Entry in the cache array. */
struct cache_entry {
    char data[BLOCK_SECTOR_SIZE];   /* Data buffer. */
    struct lock entry_lock;         /* Lock for this entry. */
    block_sector_t sector;          /* Sector number of the data. */
    bool valid;                     /* Whether initialized. */
    bool dirty;                     /* Modified or not. */
    bool accessed;                  /* Been read or not. */
};


/* Entry for read ahead. */
struct entry_read {
    struct list_elem elem;          /* Elem for put in list. */
    block_sector_t sector;          /* Sector number. */
};


/* Function for cache operation. */
void cache_init(void);
void cache_write(block_sector_t sector, void *buffer,
				int offset, int size);
void cache_read_sector(block_sector_t sector, void *buffer,
						int offset, int size);
void cache_write_back(void);

#endif /* filesys/cache.h */