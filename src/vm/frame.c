#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"

struct frame_table_entry*
find_entry_to_evict();

/* Initialize the frame table, which include initialize
   both the table list itself, as well as the lock used
   in the frame table operation. */
void 
frame_init_table(){
	lock_init(&frame_table_lock);
	list_init(&frame_table);
}



void*
frame_evict(enum palloc_flags flags){
	lock_acquire(&frame_table_lock);
	struct list_elem * e = list_begin(&frame_table);
	struct frame_table_entry * fte = find_entry_to_evict();
	if (!fte)
		PANIC("Failed to evict.");
	// ASSERT()
	fte->spte->swap_idx = swap_write_out(fte->frame_addr);
	fte->spte->type = SWAP;
	fte->spte->loaded = true;
	fte->spte->in_swap = true;
	list_remove(&fte->lelem);
	pagedir_clear_page(thread_current()->pagedir, fte->spte->user_vaddr);
	palloc_free_page(fte->frame_addr);
	free(fte);
	return palloc_get_page(flags);
}


/* Find a frame to evict in the frame table, using clock
   algorithm. Return the pointer to the frame table entry
   to evict. Panic if not able to find the frame. */
struct frame_table_entry*
find_entry_to_evict(){
	lock_acquire(&frame_table_lock);
	struct list_elem * e = list_begin(&frame_table);
	struct thread* t = thread_current();
	int iter = 0;
	while(1){
		struct frame_table_entry * fte = list_entry(e, struct frame_table_entry, lelem);
		if (fte->pinned == false){
			if (pagedir_is_accessed(t->pagedir, fte->spte->user_vaddr)){
				pagedir_set_accessed(t->pagedir, fte->spte->user_vaddr, false);
			} else {
				return fte;
			}
		}
		e = list_next(e);
		if (e == list_end(&frame_table)){
			e = list_begin(&frame_table);
			iter++;
		}
		if (iter > 5){
			return NULL;
			PANIC("Can not find frame to evict.");
		}
	}
}



/* Allocate new frame in the physical memory. If failed
   in the first time, try to use frame eviction. Implement
   by encapsulate palloc_get_page() in palloc.c. */
void * frame_alloc(void* spte_page, enum palloc_flags flag){	
	lock_acquire(&frame_table_lock);


	void* frame_addr = palloc_get_page(PAL_USER | flag);

	if (!frame_addr)
		frame_addr = frame_evict(flag);



	/* Maybe should use while loop? */
	
	// if (!frame_addr){
	// 	return;
	// 	struct frame_table_entry* fte = frame_evict(thread_current()->pagedir);
	// 	pagedir_clear_page(fte->owner->pagedir, fte->user_addr);
	// 	bool is_dirty = pagedir_is_dirty(fte->owner->pagedir, fte->user_addr)
	// 				 || pagedir_is_dirty(fte->owner->pagedir, fte->frame_addr);
	// }

	frame_table_add(spte_page, frame_addr);
	lock_release(&frame_table_lock);
	return frame_addr;
	
	
}


/* Add a frame to frame table, by initialize a new
   frame table entry, and set the member varable, 
   push it into the list of frame table. */
void
frame_table_add(void* spte, void* frame_addr){
	struct frame_table_entry* fte = malloc(sizeof(struct frame_table_entry));
	if (!fte)
		return;
	fte->frame_addr = frame_addr;
	fte->user_addr = spte;
	fte->owner = thread_current();
	fte->spte = spte;
	list_push_back(&frame_table, &fte->lelem);
	
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
			                    e, struct frame_table_entry, lelem);
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
	lock_acquire(&frame_table_lock);
	struct frame_table_entry* entry = frame_table_find(frame_addr);
	if (entry == NULL)
		PANIC("Invalid frame_addr to free.");
	
	list_remove(&(entry->lelem));
	palloc_free_page(frame_addr);
	// free(entry);

	lock_release(&frame_table_lock);
}
