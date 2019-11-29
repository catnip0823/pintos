#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

#include "vm/frame.h"

#define MAX_STACK_SIZE (1024 * 1024)

/* Three types of page. */
enum splmt_page_type{
	FILE, SWAP, ZERO, FRAME
};

struct splmt_page_table{
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

}; /* Subject to change. */


/* Function for operation on page table. */

/* Initialization. */
struct splmt_page_table*
spage_table_init();

bool
spage_table_add_frame (struct splmt_page_table *splmt_page_table, void *upage, void *kpage);

bool
spage_table_add_file(struct splmt_page_table *splmt_page_table, struct file* file, int32_t offset, uint8_t* user_page,
				uint32_t valid_bytes, uint32_t zero_bytes, bool writable);

bool
spage_table_add_zero(struct splmt_page_table *entry, void* page);

/* Querry with entry or user virtual address. */
struct splmt_page_entry*
spage_table_find_entry(struct splmt_page_table *entry, void* page);

static bool
install_file_page(struct splmt_page_entry* spte, void *new_frame);

/* Install and load page for page table entry. */
void
spage_table_install_page(struct splmt_page_entry* spte, void *new_frame);

bool
spage_table_load(struct splmt_page_table *table, uint32_t *pagedir, void *upage);

/* Grow stack when necessary, return whether it succeed. */
bool
spage_table_grow_stack(void* user_vaddr);

#endif
