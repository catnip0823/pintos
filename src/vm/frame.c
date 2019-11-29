#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"

/* Initialize the frame table, which include initialize
   both the table list itself, as well as the lock used
   in the frame table operation. */
void 
frame_init_table(){
	list_init(&frame_table);
	lock_init(&clock_lock);
}


/* Find a frame to evict in the frame table, using clock
   algorithm. Return the pointer to the frame table entry
   to evict. Panic if not able to find the frame. */
struct frame_table_entry*
pick_frame_to_evict(){
	struct list_elem * e = list_begin(&frame_table);
	
	int iter = 0;
	while(true){
		struct frame_table_entry * fte = list_entry(e, struct frame_table_entry, elem);
		struct thread* t = fte->owner;
		if (pagedir_is_accessed(t->pagedir, fte->user_addr)){
			pagedir_set_accessed(t->pagedir, fte->user_addr, false);
			return fte;
		}

		e = list_next(e);
		if (e == list_end(&frame_table)){
			e = list_begin(&frame_table);
			iter++;
		}
		if (iter > 2){
			return NULL;
		}
	}
}


/* Allocate new frame in the physical memory. If failed
   in the first time, try to use frame eviction. Implement
   by encapsulate palloc_get_page() in palloc.c. */

void *frame_alloc(void* user_addr, enum palloc_flags flag){

	lock_acquire(&frame_lock);
	void* frame_addr = palloc_get_page(PAL_USER | flag);

	if (!frame_addr){
		struct frame_table_entry *entry_to_evict = pick_frame_to_evict();
		pagedir_clear_page(entry_to_evict->owner->pagedir, entry_to_evict->user_addr);
		unsigned idx = swap_write_out(entry_to_evict->frame_addr);
		struct splmt_page_entry* spte = spage_table_find_entry(entry_to_evict->owner->splmt_page_table, entry_to_evict->user_addr);
		spte->swap_idx = idx;
		spte->type = SWAP;
		
		lock_acquire(&clock_lock);
		list_remove (&entry_to_evict->elem);
		lock_release(&clock_lock);
		
		// pagedir_clear_page(entry_to_evict->owner->pagedir, entry_to_evict->user_addr);
		palloc_free_page(entry_to_evict->frame_addr);
		frame_addr = palloc_get_page (PAL_USER | flag);
		ASSERT (frame_addr != NULL);
	}
	frame_table_add(user_addr, frame_addr);
	lock_release(&frame_lock);
	return frame_addr;
}




/* Add a frame to frame table, by initialize a new
   frame table entry, and set the member varable, 
   push it into the list of frame table. */
void
frame_table_add(void* user_addr, void* frame_addr){
	struct frame_table_entry* fte = malloc(sizeof(struct frame_table_entry));
	if (!fte){
		return;
	}
	fte->frame_addr = frame_addr;
	fte->user_addr = user_addr;
	fte->owner = thread_current();
	lock_acquire(&clock_lock);
	list_push_back(&frame_table, &fte->elem);
	lock_release(&clock_lock);
	
}


/* Given a frame address, iterate through the frame table
   and find the entry with given frame address, return 
   pointer to entry if success, NULL if failed. */
struct frame_table_entry*
frame_table_find(void* frame_addr){
	if (!frame_addr)
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
	lock_acquire(&clock_lock);
	list_remove(&(entry->elem));
	lock_release(&clock_lock);
	palloc_free_page(frame_addr);

}
