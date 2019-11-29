#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>

#include "threads/synch.h"


struct list frame_table;  /* The frame table. */


struct frame_table_entry{
	void* frame_addr;              /* The frame addr of entry. */
	void* user_addr;               /* User addr of the entry. */
	struct thread* owner;          /* The thread which own the page.*/
	struct lock frame_entry_lock;  /* Lock to synchronize. */

	struct list_elem lelem;        /* Elem to put it into the list. */
	struct splmt_page_entry* spte; /* Corosponding page table entry. */

	bool pinned;                   /* Whether it is attached to others.*/
};

void frame_init_table();

// void* frame_alloc(void* spte_page, enum palloc_flags flag);

void frame_table_add(void* user_addr, void* frame_addr);

struct frame_table_entry* frame_table_find(void* frame_addr);

void frame_free_entry(void* frame_addr);

// void* frame_evict(enum palloc_flags flag);

#endif