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

static unsigned splmt_hash_func(const struct hash_elem *e, void* aux UNUSED);
static bool splmt_hash_less_func(const struct hash_elem* elem1, 
					 const struct hash_elem* elem2, void* aux UNUSED);
void
spage_munmap(struct thread * thread, struct file *f, void *page,  off_t offset, size_t bytes);


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
	return retval;
}


/* Initialize the supplemental page table. */
struct splmt_page_table*
spage_table_init(){
	struct splmt_page_table *supt = (struct splmt_page_table*)malloc(sizeof(struct splmt_page_table));
	hash_init(&supt->splmt_pages, splmt_hash_func, splmt_hash_less_func, NULL);
	return supt;
}

/* Function for create and initialize a new page table
   entry. Only called by three other functions. */
static struct splmt_page_entry*
creat_entry(struct file* file, enum splmt_page_type type, off_t offset, uint32_t valid_bytes, uint32_t zero_bytes,
	void* user_vaddr, void* frame_vaddr, bool writable){
	struct splmt_page_entry* spte = (struct splmt_page_entry*) 
						malloc(sizeof(struct splmt_page_entry));
	if (spte == NULL)
		return NULL;
	spte->file = file;
	spte->type = type;
	spte->offset = offset;
	spte->valid_bytes = valid_bytes;
	spte->zero_bytes = zero_bytes;
	spte->user_vaddr = user_vaddr;
	spte->frame_vaddr = frame_vaddr;
	spte->writable = writable;
	return spte;
}


bool
spage_table_add_frame (struct splmt_page_table *splmt_page_table, void *upage, void *kpage)
{
	struct splmt_page_entry *entry;
	entry = (struct splmt_page_entry *) malloc(sizeof(struct splmt_page_entry));
	entry->user_vaddr = upage;
	entry->frame_vaddr = kpage;
	entry->type = FRAME;
	entry->writable = true;
	
	if (hash_insert (&splmt_page_table->splmt_pages, &entry->elem) == NULL) {
		return true;
	}
	else {
		free (entry);
		return false;
	}
}


/* Function for add new entry to page table, with type
   FILE, return true if success, return false if failed. */
bool
spage_table_add_file(struct splmt_page_table *splmt_page_table, struct file* file, 
				off_t offset, uint8_t* user_page,
				uint32_t valid_bytes, uint32_t zero_bytes, bool writable){
	struct splmt_page_entry* entry = creat_entry(file, FILE, offset, valid_bytes, zero_bytes, user_page, NULL, writable);
	if (hash_insert(&splmt_page_table->splmt_pages, &entry->elem) == NULL)
		return true;
	return false;
}


bool
spage_table_add_zero(struct splmt_page_table *splmt_page_table, void* page){
	struct splmt_page_entry* entry = creat_entry(NULL, ZERO, NULL, NULL, NULL, page, NULL, NULL);
	if (hash_insert(&splmt_page_table->splmt_pages, &entry->elem) == NULL)
		return true;
	return false;
}


/* Given an page table entry, find whether it is in the
   spage table. Return the pointer to entry if it is in
   the hash table; Return NULL if it doesn't. */
struct splmt_page_entry*
spage_table_find_entry(struct splmt_page_table *entry, void* page){
	struct splmt_page_entry s;
	s.user_vaddr = page;
	struct hash_elem* e = hash_find(
				&entry->splmt_pages, &s.elem);
	if (e == NULL)
		return NULL;
	return hash_entry(e, struct splmt_page_entry, elem);
}


/* Install and load a page with type FILE.
   Return whether it succeed to do so. */
static bool
install_file_page(struct splmt_page_entry* spte, void *new_frame){
	file_seek(spte->file, spte->offset);
	int bytes_read = file_read(spte->file, new_frame, spte->valid_bytes);
	if (bytes_read != (int)spte->valid_bytes){
		return false;
	}
	memset(new_frame + bytes_read, 0, spte->zero_bytes);
	return true;
}


/* Install and load page for all three type.*/
void
spage_table_install_page(struct splmt_page_entry* spte, void *new_frame){
	enum splmt_page_type type = spte->type;
	if (type == FILE){
		install_file_page(spte, new_frame);
		return;
	}
	if (type == SWAP){
		swap_read_in(spte->swap_idx, new_frame);
		return;
	}
	if (type == ZERO){
		memset(new_frame, 0, PGSIZE);
		return;
	}
}

bool
spage_table_load(struct splmt_page_table *table, uint32_t *pagedir, void *upage){
	struct splmt_page_entry* entry = spage_table_find_entry(table, upage);
	if (!entry)
		return false;
	if (entry->type == FRAME)
		return true;

	void *new_frame_item = frame_alloc(upage, PAL_USER);
	if (!new_frame_item)
		return false;


	spage_table_install_page(entry, new_frame_item);


	if (entry->type == FILE){
		if (!pagedir_set_page(pagedir, upage, new_frame_item, entry->writable)){
			return false;
		}
	}
	else{
		if (!pagedir_set_page(pagedir, upage, new_frame_item, true)){
			return false;
		}
	}
	entry->frame_vaddr = new_frame_item;
	entry->type = FRAME;
	pagedir_set_dirty (pagedir, new_frame_item, false);
	return true;
}


void
spage_munmap(
    struct thread * thread, struct file *f, void *page, off_t offset, size_t bytes)
{
	struct splmt_page_entry *entry = spage_table_find_entry(thread->splmt_page_table, page);
	if (entry->type == FRAME){
		if (pagedir_is_dirty(thread->pagedir, entry->user_vaddr))
			file_write_at (f, entry->user_vaddr, bytes, offset);
		pagedir_clear_page (thread->pagedir, entry->user_vaddr);
	}
	if (entry->type == SWAP){
		void *new_page = palloc_get_page(0);
		swap_read_in(entry->swap_idx, new_page);
		file_write_at (f, new_page, PGSIZE, offset);
		palloc_free_page(new_page);
	}
	hash_delete(&thread->splmt_page_table->splmt_pages, &entry->elem);
}


void check_and_setup_stack(bool is_user, struct intr_frame *f, void *fault_addr){
	void *temp_sp = f->esp;
	void *fault_page = (void*)pg_round_down(fault_addr);
	if (!is_user)
		temp_sp = thread_current()->esp;
	if (fault_addr > PHYS_BASE - MAX_STACK_SIZE && fault_addr < PHYS_BASE){
		if (temp_sp <= fault_addr || fault_addr + 4 == temp_sp || fault_addr + 32 == temp_sp){
			if (!spage_table_find_entry(thread_current()->splmt_page_table, fault_page))
				spage_table_add_zero (thread_current()->splmt_page_table, fault_page);
		}
	}
}











