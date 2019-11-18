#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include <stdbool.h>

#define MAX_STACK_SIZE (1024 * 1024)


/* Function required by the hash table,
   use the user virtual address as the 
   key, obviously it is unique in table. */
static unsigned
splmt_hash_func(const struct hash_elem e, void* aux UNUSED){
	struct splmt_page_entry* spte = hash_entry(e, 
								struct splmt_page_entry, elem);
	retval = hash_bytes(spte->user_vaddr, sizeof(spte->user_vaddr));
	return retval;
}


/* Another function required by hash
   table, compare thire user virtual
   address to compare two entries. */
static bool
splmt_hash_less_func(const struct hash_elem* elem1, 
					 const struct hash_elem* elem1, void* aux UNUSED){
	struct splmt_page_entry* entry1 = hash_entry(
								elem1, struct splmt_page_entry, elem);
	struct splmt_page_entry* entry2 = hash_entry(
								elem2, struct splmt_page_entry, elem);
	bool retval = (unsigned)entry1->user_vaddr < 
				  (unsigned)entry2->user_vaddr;
	return retval;
}


/* Initialize the supplemental page table. */
void
spage_table_init(struct hash* table){
	hash_init(table, splmt_hash_func, splmt_hash_less_func, NULL);
}


/* Function used to free the hash table. */
static void
free_spte_elem(struct hash_elem* e, void* aux UNUSED){
	/* Not implement yet. */
}


/* Destory the page table and free the resourses. */
void
spage_table_destroy(struct hash* table){
	hash_destory(table, free_spte_elem);
}


/* Given an page table entry, find whether it is in the
   spage table. Return the pointer to entry if it is in
   the hash table; Return NULL if it doesn't. */
struct splmt_page_entry*
spage_table_find_entry(struct splmt_page_entry entry){
	struct hash_elem* e = hash_find(
				thread_current()->spage_table, &entry.elem);
	if (e == NULL)
		return NULL;
	return hash_entry(e, struct splmt_page_entry, elem);
}


/* Given an address, find the page table entry for it.
   Return the pointer to the entry if find one. Return
   NULL if failed to find the page table entry. */
struct splmt_page_entry*
spage_table_find_addr(void* addr){
	struct splmt_page_entry test;
	test.user_vaddr = pg_round_down(addr);
	return spage_table_find_entry(test);
}


/* Function for create and initialize a new page table
   entry. Only called by three other functions. */
static struct splmt_page_entry*
creat_entry(){
	struct splmt_page_entry* spte = (struct splmt_page_entry*) 
						malloc(sizeof(struct splmt_page_entry));
	if (spte == NULL)
		return NULL;
	spte->user_vaddr = NULL;
	spte->fte = NULL;
	return spte;
}


/* Function for add new entry to page table, with type
   CODE, return a pointer to the entry added. */
struct splmt_page_entry*
spage_table_add_code(void* user_page){
	struct splmt_page_entry* spte = creat_entry();
	if (spte == NULL)
		return NULL;
	spte->type = CODE;
	spte->user_vaddr = user_page;
	hash_insert(&(thread_current()->spage_table), spte);
}


/* Function for add new entry to page table, with type
   FILE, return true if success, return false if failed. */
bool
spage_table_add_file(struct file* file, int32_t offset, uint8_t* user_page,
				uint32_t valid_bytes, uint32_t zero_bytes, bool writable){
	ASSERT((valid_bytes + zero_bytes) % PGSIZE == 0)

	int page_num = (valid_bytes + zero_bytes) / PGSIZE;
	for (int i = 0; i < page_num; ++i){
		/* Calculate the parameters. */
		unsigned page_valid = valid_bytes < PGSIZE ? valid_bytes : PGSIZE;
		unsigned page_zero = PGSIZE - page_valid;
		/* Create and initialize the parameters. */
		struct splmt_page_entry* spte = creat_entry();
		/* If allocation failed. */
		if (spte == NULL)
			return false;
		spte->type = FILE;
		spte->writable = writable;
		spte->offset = offset;
		spte->user_vaddr = user_page;
		spte->file = file;
		spte->valid_bytes = page_valid;
		spte->zero_bytes = page_zero;
		/* Update the parameter for the loop. */
		offset+= page_valid;
		valid_bytes -= page_valid;
		zero_bytes -= page_zero;
		user_page += PGSIZE;
		/* Insert the entry to page table. */
		hash_insert(&(thread_current->spage_table), spte);
	}
	return true;
}


/* Function for add new entry to page table, with type
   MMAP, return a pointer to the first entry it added.
   Return NULL pointer if failed to allcate memory. */
struct splmt_page_entry*
spage_table_add_mmap(struct file *file, uint32_t valid_bytes, void *user_page){
	int page_num = valid_bytes/PGSIZE + 1;
	struct splmt_page_entry* retval;
	for (int i = 0; i < page_num; ++i){
		struct splmt_page_entry* test = spage_table_find_addr(user_page);
		if (test != NULL)
			return NULL;

		int last_page = (i == page_num);
		unsigned page_valid = last_page == 1 ? valid_bytes%PGSIZE : PGSIZE;
		unsigned page_zero = PGSIZE - page_valid;
		unsigned offset = i*PGSIZE;

		struct splmt_page_entry* spte = creat_entry();
		spte->valid_bytes = page_valid;
		spte->zero_bytes = page_zero;
		spte->user_vaddr = user_page;
		spte->offset = offset;
		spte->writable = true;
		spte->type = MMAP;
		spte->file = file;

		user_page += PGSIZE;

		hash_insert(&(thread_current()->spage_table), spte);
		if (i == 0)
			retval = spte;
	}
	return retval;
}


/* Install and load the page with type CODE, return whether 
   it succeed to install the page. */
static bool
install_code_page(struct splmt_page_entry* spte){
	if (spte == NULL)
		return false;
	void* frame_addr = frame_alloc(spte, PAL_USER | PAL_ZERO);
	if (frame_addr == NULL)
		return NULL;
	if (install_page(spte->user_vaddr, frame_addr, true)){
		spte->fte = frame_table_find(frame_addr);
		if(spte->in_swap)
			return false;
		return true;
	}
	frame_free_entry(frame_addr);
	return false;
}


/* The page with type MMAP have generally same
   procedure with type CODE, so just called the
   function we defined above. */
static bool
install_mmap_page(struct splmt_page_entry* spte){
	return install_code_page(spte);
}


/* Install and load a page with type FILE.
   Return whether it succeed to do so. */
static bool
install_file_page(struct splmt_page_entry* spte){
	void* frame_addr = frame_alloc(spte, PAL_USER);
	if (frame_addr == NULL)
		return false;

	/* Maybe need a lock here. */
	file_seek(spte->file, spte->offset);
	int bytes_read = file_read(spte->file, frame_addr, spte->valid_bytes)

	if (bytes_read != (int)spte->valid_bytes){
		frame_free_entry(frame_addr);
		return false;
	}
	memset(frame_addr + spte->valid_bytes, 0, spte->zero_bytes);

	if (install_page(spte->user_vaddr, frame_addr, spte->writable)){
		spte->fte = frame_table_find(frame_addr);
		return true;
	}
	frame_free_entry(frame_addr);
	return false;
}


/* Install and load page for all three type.*/
bool
spage_table_install_page(struct splmt_page_entry* spte){
	enum splmt_page_type type = spte->type;
	if (type == CODE)
		return install_code_page(spte);
	if (type == MMAP)
		return install_mmap_page(spte);
	if (type == FILE)
		return install_file_page(spte);
	return false;
}