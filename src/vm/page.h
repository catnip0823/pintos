#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

#include "vm/frame.h"

/* define max stack size */
#define MAX_STACK_SIZE (1024 * 1024)

/* Four types of page. */
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

/* Add an entry into the page table with type FRAME. */
bool
spage_table_add_frame (struct splmt_page_table *splmt_page_table,
					   void *upage, void *kpage);

/* Add an entry into the page table with type FILE. */
bool
spage_table_add_file(struct splmt_page_table *splmt_page_table, 
					struct file* file, int32_t offset, uint8_t* user_page,
					uint32_t valid_bytes, uint32_t zero_bytes, bool writable);

/* Add an entry into the page table with type ZERO. */
bool
spage_table_add_zero(struct splmt_page_table *entry, void* page);

/* Find the entry of the page table with user virtual address. */
struct splmt_page_entry*
spage_table_find_entry(struct splmt_page_table *table, void* page);

/* Install and load a page with type FILE.
   Return whether it succeed to do so. */
static void
install_file_page(struct splmt_page_entry* spte, void *new_frame);

/* When a mapping is unmapped, all pages written to
   by the process are written back to the file.*/
void
spage_munmap(struct thread * thread, struct file *f,
			 void *page,  off_t offset, size_t bytes);

/* Install and load page for page table entry, 
   function called at spage_table_load(). */
void
spage_table_install_page(struct splmt_page_entry* spte, void *new_frame);

/* Load the page from FILE, FRAME, ZERO, or SWAP. */
bool
spage_table_load(struct splmt_page_table *table, uint32_t *pagedir, void *upage);

/* Grow stack when necessary, return whether it succeed. */
bool
spage_table_grow_stack(void* user_vaddr);

#endif
