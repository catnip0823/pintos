#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

#include "vm/frame.h"

#define MAX_STACK_SIZE (1024 * 1024)

/* Three types of page. */
enum splmt_page_type{
	FILE, SWAP, ZERO, FRAME
};
struct splmt_page_table
{
	struct hash splmt_pages;
	
};

/* Structure of page table entry. */
struct splmt_page_entry{
	struct hash_elem elem;     /* Used to be put in the hash. */

	enum splmt_page_type type; /* The type of the page. */

	uint8_t* user_vaddr;       /* The user virtual address of the page. */
	uint8_t* frame_vaddr;      /* The physical address of the page. */

	struct file* file;         /* A pointer to the file of this page. */

	uint32_t valid_bytes;      /* Read bytes of the page, non-zero. */
	uint32_t zero_bytes;       /* Zero bytes of the page, always zero. */
	uint32_t offset;           /* The offset of this page. */
	unsigned swap_idx;         /* Indicate where it is stored if swapped. */
	
	bool writable;             /* Whether this page is writable. */

	bool accessed;             /* Whether is accessed recently. */
	bool loaded;               /* Whether is has been loaded. */
	bool in_swap;              /* Whether it is in the swap slot. */
	bool attached;             /* Whether it is installed by stack growth.*/
}; /* Subject to change. */


/* Function for operation on page table. */

/* Initialization. */
struct splmt_page_table*
spage_table_init();

/* Querry with entry or user virtual address. */
struct splmt_page_entry*
spage_table_find_entry(struct splmt_page_table *entry, void* page);

bool
spage_table_add_zero(struct splmt_page_table *entry, void* page);

struct splmt_page_entry*
spage_table_find_addr(void* addr);

/* Add new entry to page table. Three types in all. */
struct splmt_page_entry*
spage_table_add_code(void* user_page);

bool
spage_table_add_file(struct splmt_page_table *splmt_page_table, struct file* file, int32_t offset, uint8_t* user_page,
				uint32_t valid_bytes, uint32_t zero_bytes, bool writable);

struct splmt_page_entry*
spage_table_add_mmap(struct file *file, uint32_t valid_bytes, void *user_page);

/* Install and load page for page table entry. */
bool
spage_table_install_page(struct splmt_page_entry* spte, void *new_frame);

/* Grow stack when necessary, return whether it succeed. */
bool
spage_table_grow_stack(void* user_vaddr);

/* Destory the page and free the resourse. */
void
spage_table_destroy(struct hash* table);

#endif
