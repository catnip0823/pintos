#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>

#include "threads/synch.h"


struct list frame_table;

struct lock frame_table_lock;

struct frame_table_entry{
	void* frame_addr;
	void* user_addr;
	struct thread* owner;

	struct list_elem lelem;
	struct splmt_page_entry* spte;

	bool pinned;
};

void frame_init_table();

// void* frame_alloc(void* spte_page, enum palloc_flags flag);

void frame_table_add(void* user_addr, void* frame_addr);

struct frame_table_entry* frame_table_find(void* frame_addr);

void frame_free_entry(void* frame_addr);

// void* frame_evict(enum palloc_flags flag);

#endif