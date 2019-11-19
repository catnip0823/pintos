#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"



/* Initialize the frame table, which include initialize
   both the table list itself, as well as the lock used
   in the frame table operation. */
void 
frame_init_table(void){
	lock_init(&frame_table_lock);
	list_init(&frame_table);
}



void*
frame_evict(enum palloc_flags flag){
	PANIC("Not implement yet.");
}



/* Allocate new frame in the physical memory. If failed
   in the first time, try to use frame eviction. Implement
   by encapsulate palloc_get_page() in palloc.c. */
void * frame_alloc(struct splmt_page_table_entry* spte, enum palloc_flags flag){
	if ((flag >> 2) == 0 )  /* If given flag not user. */
		return NULL;
	void* frame_addr = palloc_get_page(flag);

	if (frame_addr){        /* If succeed. */
		frame_table_add(spte, frame_addr);
		return frame_addr;
	}

	/* Maybe should use while loop? */
	frame_addr = frame_evict(flag);

	if (frame_addr){        /* If succeed. */
		frame_table_add(spte, frame_addr);
		return frame_addr;
	} else 
		PANIC("Still unable to allocate frame.");
	return NULL;
}


/* Add a frame to frame table, by initialize a new
   frame table entry, and set the member varable, 
   push it into the list of frame table. */
void
frame_table_add(struct splmt_page_table_entry* spte, void* frame_addr){
	struct frame_table_entry* fte = (struct 
			frame_table_entry*)malloc(sizeof(struct frame_table_entry));
	fte->frame_addr = frame_addr;
	fte->spte = spte;
	fte->owner = thread_current();
	lock_acquire(&frame_table_lock);
	list_push_back(&frame_table, &fte->elem);
	lock_release(&frame_table_lock);
}


/* Given a frame address, iterate through the frame table
   and find the entry with given frame address, return 
   pointer to entry if success, NULL if failed. */
struct frame_table_entry*
frame_table_find(void* frame_addr){
	if (frame_addr == NULL)
		return NULL;
	/* Iterate through the list of table. */
	struct list_elem* e;
	for (e = list_begin(&frame_table); e != list_end(&frame_table);
		 e = list_next(e)){
		struct frame_table_entry* entry = list_entry(
			                    e, struct frame_table_entry, elem);
		if (entry->frame_addr != frame_addr)
			continue;
		return entry;
	}
	return NULL;
}


/* Free the frame table entry give the
   addr, remove it from the list of table
   and free the memory of entry and page. */
void
frame_free_entry(void* frame_addr){
	struct frame_table_entry* entry = frame_table_find(frame_addr);
	if (entry == NULL)
		PANIC("Invalid frame_addr to free.");
	lock_acquire(&frame_table_lock);
	list_remove(&(entry->elem));
	palloc_free_page(frame_addr);
	free(entry);
	lock_release(&frame_table_lock);
}
