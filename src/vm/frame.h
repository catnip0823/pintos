#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>

#include "threads/synch.h"


struct list frame_table;  /* The frame table. */

/* Structure of the frame table emtry. */
struct frame_table_entry{
	void* frame_addr;              /* The frame addr of entry. */
	void* user_addr;               /* User addr of the entry. */
	struct thread* owner;          /* The thread which own the page.*/
	struct lock frame_entry_lock;  /* Lock to synchronize. */
	struct list_elem elem;        /* Elem to put it into the list. */
	struct splmt_page_entry* spte; /* Corosponding page table entry. */
};

/* Initialize the table. */
void frame_init_table();

/* Add a new table to the entry. */
void frame_table_add(void* user_addr, void* frame_addr);

/* Given a frame address, find the entry in frame table. */
struct frame_table_entry* frame_table_find(void* frame_addr);

/* Delete an entry, free the resourse. */
void frame_free_entry(void* frame_addr);

/* Find the frame to evict in the frame table. */
struct frame_table_entry* pick_frame_to_evict();

#endif
