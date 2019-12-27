#include "filesys/cache.h"
#include "devices/timer.h"
#include "threads/thread.h"
#include "filesys.h"

/* Define some macros. */
#define CACHE_ENTRY_NUM 64        		/* Number of entry in cache. */
#define WRITE_BACK_FREQ 10      		/* Frequency of write back. */

/* Tools for synchronization. */
static struct lock cache_lock;          /* Lock for whole cache. */
static struct list read_ahead_queue;    /* Queue for read ahead. */
static struct lock ahead_lock;  	    /* Lock for read ahead. */
static struct condition ahead_cond;     /* Condition for read_ahead. */

/* Cache body. */
static struct cache_entry cache[CACHE_ENTRY_NUM];   /* Array of the cache. */

/* Static function for operation. */
struct cache_entry * cache_find_sector(block_sector_t sector);
struct cache_entry * cache_evict(void);
void                 cache_write_back_func(void *aux UNUSED);
void                 cache_read_ahead(void *aux UNUSED);
void                 thread_entry_write_back (void *);


/* Initialize the buffer cache. Including the
   lock, list and conditional variable. */
void
cache_init(void){
    int i = 0;
    /* Initialize lock for each entry. */
    while (i < CACHE_ENTRY_NUM){
        lock_init(&cache[i].entry_lock);
        cache[i].valid = false;
		i++;
    }
    /* Initialize other tools. */
    cond_init(&ahead_cond);
    lock_init(&cache_lock);
    lock_init(&ahead_lock);
    list_init(&read_ahead_queue);
    /* Create thread for write back and read ahead. */
    thread_create("cache_write_back", PRI_DEFAULT,
    				cache_write_back_func, NULL);
    thread_create("cache_read_ahead", PRI_DEFAULT, 
    					cache_read_ahead, NULL);
}


/* Find the given sector in the cache. Return the pointer
   to the entry if found. Return a NULL pointer if it does
   not in the cache yet. */
struct cache_entry *
cache_find_sector(block_sector_t sector) {
    size_t i = 0;
    while (i < CACHE_ENTRY_NUM) {
		struct cache_entry* entry = cache +i;
		lock_acquire(&entry->entry_lock);
		/* If it is not valid, continue. */
		if (!entry->valid){
			lock_release(&entry->entry_lock);
			i++;
			continue;
		}
		/* If found one.*/
		if (entry->sector == sector)
			return entry;
		lock_release(&entry->entry_lock);
		i++;
    }
    return NULL;
}


/* Write operation for the cache. If cache hit, directly
   read the data; if miss, first bring in the sector. */
void
cache_write(block_sector_t sector, void *buffer,
							int offset, int size){
    lock_acquire(&cache_lock);
    /* Try to find the sector. */
    struct cache_entry *entry = cache_find_sector(sector);
    if (entry)
        lock_release(&cache_lock);
    else{
        /* If cache miss. */
        entry = cache_evict();
        entry->dirty = false;
        entry->sector = sector;
        block_read(fs_device, sector, entry->data);
        lock_release(&cache_lock);
    }
    /* Write the data. */
    memcpy(entry->data + offset, buffer, size);
    entry->dirty = true;
    entry->accessed = true;
    lock_release(&entry->entry_lock);
}


/* Read operation for the cache. If cache hit, directly
   read the data; if miss, first bring in the sector. */
void
cache_read_sector(block_sector_t sector, void *buffer,
									int offset, int size){
	lock_acquire(&cache_lock);
	/* Try to find the sector. */
    struct cache_entry *entry = cache_find_sector(sector);
    if (entry)
        lock_release(&cache_lock);
    else{
        /* If cache miss. */
        entry = cache_evict();
        entry->dirty = false;
        entry->sector = sector;
        block_read(fs_device, sector, entry->data);
        lock_release(&cache_lock);
    }
    /* copy the data out. */
    memcpy(buffer, entry->data + offset, (size_t) size);
    /* Mark as accessed. */
    entry->accessed = true;
    lock_release(&entry->entry_lock);
}


/* Evict an entry in the cache. Using the clock
   algorithm. Return the pointer to the entry. */
struct cache_entry *
cache_evict(void) {
    int idx = 0;
    struct cache_entry *entry;
    while (1) {
        entry = idx + cache;
        /* If fail, move to the next one. */
        if (!lock_try_acquire(&entry->entry_lock)) {
            idx %= CACHE_ENTRY_NUM;
            continue;
        }
        /* If find the new entry. */
        if (!entry->valid) {
            entry->valid = true;
            return entry;
        }
        /* Second chance. */
        if (entry->accessed)
            entry->accessed = false;
        else if (!entry->accessed){
        	/* Move out the item. */
            if (entry->dirty) {
            	/* Write back the data. */
                block_write(fs_device, entry->sector, entry->data);
            }
            entry->dirty = false;
            return entry;
        }
        idx %= CACHE_ENTRY_NUM;
        lock_release(&entry->entry_lock);
    }
}


/* periodically writes all the dirty sectors
   back to disk and then goes to sleep */
void 
cache_write_back_func(void *aux UNUSED) {
    while (true) {
        timer_sleep(WRITE_BACK_FREQ * TIMER_FREQ);
        cache_write_back();
    }
}

/* function for writing back dirty sectors to disk. Also
   first to make sure that it is valid. */
void
cache_write_back(void){
    size_t i = 0;
    while (i < CACHE_ENTRY_NUM) {
        struct cache_entry *entry = cache + i;
        lock_acquire(&entry->entry_lock);
		/* Make sure whether valid or not. */
        /* If not modified, unnecessary to write. */
		if (!entry->valid || !entry->dirty){
			lock_release(&entry->entry_lock);
			i++;
			continue;
		}
        block_write(fs_device, entry->sector, entry->data);
        entry->dirty = false;
        lock_release(&entry->entry_lock);
		i++;
    }
}


/* Function for read ahead. Do some operation
   in the waiting list. */
void 
cache_read_ahead(void *aux UNUSED){
    while (true) {
        lock_acquire(&ahead_lock);
        while (list_empty(&read_ahead_queue))
            cond_wait(&ahead_cond, &ahead_lock);
        struct entry_read *ahead_entry = list_entry(
			list_pop_front(&read_ahead_queue), struct entry_read, elem);
        // free(ahead_entry);
		cache_read_sector(ahead_entry->sector, NULL, BLOCK_SECTOR_SIZE, 0);
        lock_release(&ahead_lock);
    }
}
