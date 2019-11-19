#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>

#include "threads/synch.h"
#include "vm/page.h"


struct list frame_table;

struct lock frame_table_lock;

struct frame_table_entry{
	void* frame_addr;
	struct thread* owner;
	struct list_elem elem;
	struct splmt_page_table_entry* spte;
};

void frame_init_table(void);

// void* frame_alloc(struct splmt_page_table_entry* spte, enum palloc_flags flag);

void frame_table_add(struct splmt_page_table_entry* spte, void* frame_addr);

struct frame_table_entry* frame_table_find(void* frame_addr);

void frame_free_entry(void* frame_addr);

// void* frame_evict(enum palloc_flags flag);

#endif