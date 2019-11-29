#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/vaddr.h"

struct frame_table_entry*
find_entry_to_evict();

/* Initialize the frame table, which include initialize
   both the table list itself, as well as the lock used
   in the frame table operation. */
void 
frame_init_table(){
	list_init(&frame_table);
}

// struct frame_table_entry* pick_frame_to_evict()
// {
// 	struct list_elem * e = list_begin(&frame_table);
// 	struct frame_table_entry * fte = list_entry(e, struct frame_table_entry, lelem);
// 	return fte;
// }

/*
void*
frame_evict(enum palloc_flags flags){
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
}*/


/* Find a frame to evict in the frame table, using clock
   algorithm. Return the pointer to the frame table entry
   to evict. Panic if not able to find the frame. */
struct frame_table_entry*
pick_frame_to_evict(){
	struct list_elem * e = list_begin(&frame_table);
	
	int iter = 0;
	while(1){
		struct frame_table_entry * fte = list_entry(e, struct frame_table_entry, lelem);
		struct thread* t = fte->owner;
		// if (fte->pinned == false){
			if (pagedir_is_accessed(t->pagedir, fte->user_addr)){
				// printf("jinqulehhhhhhhhh\n");
				pagedir_set_accessed(t->pagedir, fte->user_addr, false);
				// e = list_next(e);
				// continue;
			// } else{
				return fte;
			}

			
		// }
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


void* frame_evict (enum palloc_flags flags)
{

  struct list_elem *e = list_begin(&frame_table);
  
  while (true)
    {
      struct frame_table_entry *fte = list_entry(e, struct frame_table_entry, lelem);
      if (/*!fte->pinned &&*/ fte->spte->type != SWAP)
	{
	  struct thread *t = fte->owner;
	  if (pagedir_is_accessed(t->pagedir, fte->spte->user_vaddr))
	    {
	      pagedir_set_accessed(t->pagedir, fte->spte->user_vaddr, false);
	    }
	  else
	    {
	      if (pagedir_is_dirty(t->pagedir, fte->spte->user_vaddr))
		{
		      fte->spte->type = SWAP;
		      fte->spte->swap_idx = swap_write_out(fte->frame_addr);
		}
	      fte->spte->loaded = false;
	      list_remove(&fte->lelem);
	      pagedir_clear_page(t->pagedir, fte->spte->user_vaddr);
	      palloc_free_page(fte->frame_addr);
	      free(fte);
	      return palloc_get_page(flags);
	    }
	}
      e = list_next(e);
      if (e == list_end(&frame_table))
	{
	  e = list_begin(&frame_table);
	}
    }
}




/* Allocate new frame in the physical memory. If failed
   in the first time, try to use frame eviction. Implement
   by encapsulate palloc_get_page() in palloc.c. */

void * frame_alloc(void* user_addr, enum palloc_flags flag){

	lock_acquire(&frame_lock);
	void* frame_addr = palloc_get_page(PAL_USER | flag);

	if (!frame_addr){

	// printf("evict!\n");

	struct frame_table_entry *f_evicted = pick_frame_to_evict();
	// printf("hhhh!\n");
	ASSERT(pg_ofs (f_evicted->user_addr) == 0);
    pagedir_clear_page(f_evicted->owner->pagedir, f_evicted->user_addr);
    // printf("start swap out!\n");
    // if (!lock_try_acquire (&f_evicted->frame_entry_lock))
    // 	continue;
    unsigned idx = swap_write_out( f_evicted->frame_addr );
    // printf("end swap out!\n");
    struct splmt_page_entry* spte = spage_table_find_entry(f_evicted->owner->splmt_page_table, f_evicted->user_addr);
    spte->swap_idx = idx;
    spte->in_swap = true;
    spte->type = SWAP;
    ASSERT(spte != NULL);
    //printf("666\n");
    // list_remove(&f_evicted->lelem);
	ASSERT (pg_ofs (f_evicted->frame_addr) == 0);
	// palloc_free_page(f_evicted->frame_addr);

	// palloc_free_page(f_evicted->frame_addr);
	// printf("st\n");
	list_remove (&f_evicted->lelem);
	// printf("zheli \n");
	ASSERT (pg_ofs (f_evicted->frame_addr) == 0);
	pagedir_clear_page(f_evicted->owner->pagedir, f_evicted->user_addr);
	palloc_free_page(f_evicted->frame_addr);
	// lock_release(&f->frame_entry_lock);
    // frame_addr = palloc_get_page (PAL_USER | flag);
    // printf("end evict\n");

    frame_addr = palloc_get_page (PAL_USER | flag);
    ASSERT (frame_addr != NULL);


		// frame_addr = frame_evict(flag);
	}
	// if (!lock_try_acquire (&f->frame_entry_lock))
	// 	continue;
	frame_table_add(user_addr, frame_addr);
	lock_release(&frame_lock);
	return frame_addr;
	
	
}




// void* frame_alloc (enum palloc_flags flags, struct sup_page_entry *spte)
// {
// 	//printf("Allocate frame.\n");
// 	printf("%d\n", flags);
// 	printf("%d\n", PAL_USER);
// 	printf("%d\n",(flags & PAL_USER) == 0 );
//   if ( (flags & PAL_USER) == 0 )
//     {
//       return NULL;
//     }
//   void *frame = palloc_get_page(flags);
//   if (frame)
//     {
//       frame_table_add(spte, frame);
//     }
//   else
//     {
//       while (!frame)
// 	{	
// 	  PANIC("Need to evict\n");
// 	  frame = frame_evict(flags);
// 	}
//       if (!frame)
// 	{
// 	  PANIC ("Frame could not be evicted because swap is full!");
// 	}
//       frame_table_add(spte, frame);
//     }
//   return frame;
// }


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
	// lock_init (&fte->frame_entry_lock);
	if (user_addr == NULL)
		PANIC("null");
	fte->user_addr = user_addr;
	fte->owner = thread_current();
	// fte->spte = spte;
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
	struct frame_table_entry* entry = frame_table_find(frame_addr);
	if (entry == NULL)
		PANIC("Invalid frame_addr to free.");
	
	list_remove(&(entry->lelem));
	palloc_free_page(frame_addr);

}
