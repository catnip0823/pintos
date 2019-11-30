#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "vm/page.h"
#include "vm/frame.h"

/* Function required by the hash table, defined below. */
static unsigned splmt_hash_func(const struct hash_elem *e, void* aux UNUSED);
static bool splmt_hash_less_func(const struct hash_elem* elem1, 
					 const struct hash_elem* elem2, void* aux UNUSED);



/* Function required by the hash table,
   use the user virtual address as the 
   key, obviously it is unique in table. */
static unsigned
splmt_hash_func(const struct hash_elem * e, void* aux UNUSED){
	struct splmt_page_entry* spte = hash_entry(e, 
								struct splmt_page_entry, elem);
	return hash_int((int)spte->user_vaddr);
}


/* Another function required by hash
   table, compare thire user virtual
   address to compare two entries. */
static bool
splmt_hash_less_func(const struct hash_elem* elem1, 
					 const struct hash_elem* elem2, void* aux UNUSED){
	struct splmt_page_entry* entry1 = hash_entry(
								elem1, struct splmt_page_entry, elem);
	struct splmt_page_entry* entry2 = hash_entry(
								elem2, struct splmt_page_entry, elem);
	bool retval = (unsigned)entry1->user_vaddr < 
				  (unsigned)entry2->user_vaddr;
	/* return whether the user addr of elem1 is less than elem2. */
	return retval;
}


/* Initialize the supplemental page table. */
struct splmt_page_table*
spage_table_init(){
	struct splmt_page_table *supt = (struct splmt_page_table*)
								malloc(sizeof(struct splmt_page_table));
	hash_init(&supt->splmt_pages,splmt_hash_func,splmt_hash_less_func,NULL);
	/* Return the new initialized supplemental page table. */
	return supt;
}

/* Function for create and initialize a new page table
   entry. Only called by three other functions. */
static struct splmt_page_entry*
creat_entry(struct file* file, enum splmt_page_type type, 
			off_t offset, uint32_t valid_bytes, uint32_t zero_bytes,
			void* user_vaddr, void* frame_vaddr, bool writable){
	/* Allocate a new space for the new entry. */
	struct splmt_page_entry* spte = (struct splmt_page_entry*) 
						malloc(sizeof(struct splmt_page_entry));
	/* return null if the new entry of the page table allocate failed. */
	if (spte == NULL)
		return NULL;
	/* set the necessary parameters of the new entry. */
	spte->file = file;
	spte->type = type;
	spte->offset = offset;
	spte->valid_bytes = valid_bytes;
	spte->zero_bytes = zero_bytes;
	spte->user_vaddr = user_vaddr;
	spte->frame_vaddr = frame_vaddr;
	spte->writable = writable;
	/* Return the new supplemental page table entry. */
	return spte;
}

/* Function for add new entry to page table, with type
   FRAME, return true if success, return false if failed. */
bool
spage_table_add_frame(struct splmt_page_table *splmt_page_table,
					  void *upage, void *kpage)
{
	/* Allocate a new space for the new entry. */
	struct splmt_page_entry *entry;
	entry = (struct splmt_page_entry *)
			malloc(sizeof(struct splmt_page_entry));
	entry->user_vaddr = upage;
	entry->frame_vaddr = kpage;
	entry->type = FRAME;
	entry->writable = true;
	/* Insert the new entry into the splmt page table. */
	if (!hash_insert (&splmt_page_table->splmt_pages, &entry->elem)){
		return true;
	}
	else {
		free(entry);
		return false;
	}
	/* return whether successfully add an entry into the page table. */
}


/* Function for add new entry to page table, with type
   FILE, return true if success, return false if failed. */
bool
spage_table_add_file(struct splmt_page_table *splmt_page_table, 
					 struct file* file, off_t offset, 
					 uint8_t* user_page, uint32_t valid_bytes, 
					 uint32_t zero_bytes, bool writable){
	/* Allocate and set a new space for the new entry. */
	struct splmt_page_entry* entry = creat_entry(file, FILE, offset, 
									 valid_bytes, zero_bytes, user_page,
									 NULL, writable);
	if (!hash_insert(&splmt_page_table->splmt_pages, &entry->elem))
		return true;
	return false;
	/* return whether successfully add an entry into the page table. */
}

/* Function for add new entry to page table, with type
   ZERO, return true if success, return false if failed. */
bool
spage_table_add_zero(struct splmt_page_table *splmt_page_table, void* page){
	/* Allocate and set a new space for the new entry. */
	struct splmt_page_entry* entry = creat_entry(NULL, ZERO, NULL,
									 NULL, NULL, page, NULL, NULL);
	if (hash_insert(&splmt_page_table->splmt_pages, &entry->elem) == NULL)
		return true;
	return false;
	/* return whether successfully add an entry into the page table. */
}


/* Given a page, find whether it is in the spage
   table. Return the pointer to entry if it is in
   the hash table; Return NULL if it doesn't. */
struct splmt_page_entry*
spage_table_find_entry(struct splmt_page_table *table, void* page){
	struct splmt_page_entry s;
	s.user_vaddr = page;
	struct hash_elem* e = hash_find(&table->splmt_pages, &s.elem);
	/* Return NULL if it is in the hash table. */
	if (e == NULL)
		return NULL;
	/* Return the pointer to entry if it is in the hash table. */
	return hash_entry(e, struct splmt_page_entry, elem);
}


/* Install and load a page with type FILE.
   Return whether it succeed to do so. */
static void
install_file_page(struct splmt_page_entry* spte, void *new_frame){
	file_seek(spte->file, spte->offset);
	int bytes_read = file_read(spte->file, new_frame, spte->valid_bytes);
	if (bytes_read != spte->valid_bytes){
		return;
	}
	/* Set the zero_bytes bytes to 0. */
	memset(new_frame + bytes_read, 0, spte->zero_bytes);
}


/* Install and load page for all three type.*/
void
spage_table_install_page(struct splmt_page_entry* spte, void *new_frame){
	enum splmt_page_type type = spte->type;
	/* Install and load a page with type FILE. */
	if (type == FILE){
		install_file_page(spte, new_frame);
		return;
	}
	/* swap in a page from swap with type SWAP. */
	if (type == SWAP){
		swap_read_in(spte->swap_idx, new_frame);
		return;
	}
	/* set a zero page with type ZERO. */
	if (type == ZERO){
		memset(new_frame, 0, PGSIZE);
		return;
	}
}

/* Load the page from FILE, FRAME, ZERO, or SWAP. */
bool
spage_table_load(struct splmt_page_table *table, 
				 uint32_t *pagedir, void *upage){
	/* Locate the page that faulted in the supplemental page table. */
	struct splmt_page_entry* entry = spage_table_find_entry(table,upage);
	/* If the user page is not in the splmt page table, return error. */
	if (!entry)
		return false;
	/* If the page is already in frame, no action 
	   is necessary and return. */
	if (entry->type == FRAME)
		return true;
	/* Allocate a new space in frame table to store the new frame */
	void *new_frame_item = frame_alloc(upage, PAL_USER);
	/* If cannot allocate a new space, return error. */
	if (!new_frame_item)
		return false;

	/* Install and load page for all three type.*/
	spage_table_install_page(entry, new_frame_item);

	/* We also need to care about whether the file is writable.
	   If the access is an attempt to write to a read-only page,
	   then the access is invalid */
	if (entry->type == FILE){
		/* if the type is FILE, set the page 
		   the same as entry's writable. */
		if (!pagedir_set_page(pagedir, upage, new_frame_item,
								entry->writable)){
			/* false if memory allocation failed. */
			return false;
		}
	}
	else{
		/* if the type is FILE, set the page writable. */
		if (!pagedir_set_page(pagedir, upage, new_frame_item, true)){
			/* false if memory allocation failed. */
			return false;
		}
	}
	entry->frame_vaddr = new_frame_item;
	entry->type = FRAME;
	pagedir_set_dirty (pagedir, new_frame_item, false);
	/* Return true if load the page into physical successful. */
	return true;
}


/* When a mapping is unmapped, all pages written to
   by the process are written back to the file.*/
void
spage_munmap(struct thread *thread, struct file *f,
			 void *page, off_t offset, size_t bytes){
	struct splmt_page_entry *entry = spage_table_find_entry(
									 thread->splmt_page_table, page);
	if (entry->type == FRAME){
		/* Only need to write back into the FILE when is dirty */
		if (pagedir_is_dirty(thread->pagedir, entry->user_vaddr)){
			/* write size bytes back into the FILE startting offset. */
			file_write_at(f, entry->user_vaddr, bytes, offset);
		}
		pagedir_clear_page(thread->pagedir, entry->user_vaddr);
	}
	if (entry->type == SWAP){
		/* allocate a new page to write into the FILE. */
		void *new_page = palloc_get_page(0);
		swap_read_in(entry->swap_idx, new_page);
		/* write the page into the FILE startting offset. */
		file_write_at(f, new_page, PGSIZE, offset);
		palloc_free_page(new_page);
	}
	/* Removed the pages from the process's list of virtual pages.*/
	hash_delete(&thread->splmt_page_table->splmt_pages, &entry->elem);
}

/* Grow stack when necessary, return whether it succeed. */
void check_and_setup_stack(bool is_user, struct intr_frame *f,
						   void *fault_addr){
	void *temp_sp = f->esp;
	void *fault_page = (void*)pg_round_down(fault_addr);
	if (!is_user)
		temp_sp = thread_current()->esp;
	/* if the fault address is between the physical base - max stack size 
	   and the physcal base, we can handle the page fauly by growing the 
	   stack. */
	if (fault_addr > PHYS_BASE - MAX_STACK_SIZE && fault_addr < PHYS_BASE){
		/* slove the page fault stack below the stack pointer, and
		   the page fault 4 and 32 bytes below the stack pointer. */
		if (temp_sp <= fault_addr || 
			fault_addr + 4 == temp_sp || 
			fault_addr + 32 == temp_sp){
			/* If cannot find the entry of the page table with user virtual
			   address, add new entry to page table with type ZERO. */
			struct thread *curr = thread_current();
			if (!spage_table_find_entry(curr->splmt_page_table, fault_page))
				spage_table_add_zero (curr->splmt_page_table, fault_page);
		}
	}
}
