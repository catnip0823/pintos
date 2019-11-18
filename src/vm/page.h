#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

#include "vm/frame.h"

/* Three types of page. */
enum splmt_page_type{
	CODE, FILE, MMAP
};


/* Structure of page table entry. */
struct splmt_page_table_entry{
	struct hash_elem elem;

	enum splmt_page_type type;
	void* user_vaddr;
	struct frame_table_entry* fte;

	uint32_t valid_bytes;
	uint32_t zero_bytes;
	uint32_t offset;

	bool loaded;
	bool dirty;
	bool accessed;
	bool writable;
	bool in_swap;
	bool attached;
} /* Subject to change. */


/* Function for operation on page table. */

/* Initialization. */
void
spage_table_init(struct hash* table);

/* Querry with entry or user virtual address. */
struct splmt_page_entry* 
spage_table_find_entry(struct splmt_page_entry entry);
struct splmt_page_entry*
spage_table_find_addr(void* addr);

/* Add new entry to page table. Three types in all. */
struct splmt_page_entry*
spage_table_add_code(void* user_page);
bool
spage_table_add_file(struct file* file, int32_t offset, uint8_t* user_page,
				uint32_t valid_bytes, uint32_t zero_bytes, bool writable);
struct splmt_page_entry*
spage_table_add_mmap(struct file *file, uint32_t valid_bytes, void *user_page);

/* Install and load page for page table entry. */
bool
spage_table_install_page(struct splmt_page_entry* spte)

/* Grow stack when necessary, return whether it succeed. */
bool
spage_table_grow_stack(void* user_vaddr);

/* Destory the page and free the resourse. */
void
spage_table_destroy(struct hash* table);

#endif