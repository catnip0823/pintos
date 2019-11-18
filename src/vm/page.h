#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

#include "vm/frame.h"


enum splmt_page_type{
	CODE, FILE, MMAP
};


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
} /* Subject to change. */

#endif