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
// #include <stdbool.h>




static unsigned splmt_hash_func(const struct hash_elem *e, void* aux UNUSED);
static bool splmt_hash_less_func(const struct hash_elem* elem1, 
					 const struct hash_elem* elem2, void* aux UNUSED);
static void hash_destroy_func(struct hash_elem* e, void* aux UNUSED);
static struct splmt_page_entry*
creat_entry(struct file* file, off_t offset, uint32_t valid_bytes, uint32_t zero_bytes,
	uint8_t* user_vaddr, uint8_t* frame_vaddr, bool writable);
static bool
install_code_page(struct splmt_page_entry* spte);
static bool
install_mmap_page(struct splmt_page_entry* spte);
static bool
install_file_page(struct splmt_page_entry* spte, void *new_frame);


/* Function required by the hash table,
   use the user virtual address as the 
   key, obviously it is unique in table. */
static unsigned
splmt_hash_func(const struct hash_elem * e, void* aux UNUSED){
	struct splmt_page_entry* spte = hash_entry(e, 
								struct splmt_page_entry, elem);
	// unsigned retval = hash_bytes(spte->user_vaddr, sizeof(spte->user_vaddr));
	// return retval;
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

	// hash_init(table, splmt_hash_func, splmt_hash_less_func, NULL);
}



/* Function used to free the hash table. */
// static void
// hash_destroy_func(const hash_elem *e, void *aux UNUSED){

// 	struct splmt_page_entry* spte = hash_entry(e,
// 		                 struct splmt_page_entry, elem);
// 	if (spte->user_vaddr){
// 		// pagedir_clear_page(thread_current()->pagedir, spte->user_vaddr);
// 		frame_free_entry(spte->fte->frame_addr);
// 	}
// 	free(spte);
// }


// /* Destory the page table and free the resourses. */
// void
// spage_table_destroy(struct splmt_page_table *supt){
// 	hash_destroy(&supt->splmt_pages, hash_destroy_func);
// 	free(supt);
// }




bool
spage_table_add_frame (struct splmt_page_table *splmt_page_table, uint8_t *upage, uint8_t *kpage)
{
  struct splmt_page_entry *entry;
  entry = (struct splmt_page_entry *) malloc(sizeof(struct splmt_page_entry));

  entry->user_vaddr = upage;
  entry->frame_vaddr = kpage;
  entry->type = FRAME;
  entry->writable = true;
  // frame_table_add(entry, kpage);
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
	struct splmt_page_entry* entry = creat_entry(file, offset, valid_bytes, zero_bytes, user_page, NULL, writable);
	entry->type = FILE;
	if (hash_insert(&splmt_page_table->splmt_pages, &entry->elem) == NULL)
		return true;
	return false;
}


bool
spage_table_add_zero(struct splmt_page_table *table, void* page){
	struct splmt_page_entry* entry = creat_entry(NULL, NULL, NULL, NULL, page, NULL, NULL);
	entry->type = ZERO;

	if (hash_insert(&table->splmt_pages, &entry->elem) == NULL)
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
	// printf("%d\n", hash_size(&entry->splmt_pages));

	if (e == NULL)
		return NULL;
	return hash_entry(e, struct splmt_page_entry, elem);
}

/* Function for create and initialize a new page table
   entry. Only called by three other functions. */
static struct splmt_page_entry*
creat_entry(struct file* file, off_t offset, uint32_t valid_bytes, uint32_t zero_bytes,
	uint8_t* user_vaddr, uint8_t* frame_vaddr, bool writable){
	struct splmt_page_entry* spte = (struct splmt_page_entry*) 
						malloc(sizeof(struct splmt_page_entry));
	if (spte == NULL)
		return NULL;
	
	spte->file = file;
	spte->offset = offset;
	spte->valid_bytes = valid_bytes;
	spte->zero_bytes = zero_bytes;
	spte->user_vaddr = user_vaddr;
	spte->frame_vaddr = frame_vaddr;
	spte->writable = writable;
	return spte;
}





/* Install and load a page with type FILE.
   Return whether it succeed to do so. */
static bool
install_file_page(struct splmt_page_entry* spte, void *new_frame){
	// void* frame_addr = frame_alloc(spte, PAL_USER);
	// if (frame_addr == NULL)
	// 	return false;

	file_seek(spte->file, spte->offset);
	int bytes_read = file_read(spte->file, new_frame, spte->valid_bytes);

	if (bytes_read != (int)spte->valid_bytes){
		// frame_free_entry(frame_addr);
		return false;
	}

	memset(new_frame + bytes_read, 0, spte->zero_bytes);

	// if (install_page(spte->user_vaddr, frame_addr, spte->writable)){
	// 	spte->fte = frame_table_find(frame_addr);
	// 	return true;
	// }
	// frame_free_entry(frame_addr);
	return true;
}


/* Install and load page for all three type.*/
bool
spage_table_install_page(struct splmt_page_entry* spte, void *new_frame){
	enum splmt_page_type type = spte->type;
	if (type == FILE){
		return install_file_page(spte, new_frame);
	}
	if (type == SWAP){
		// printf("aaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
		swap_read_in(spte->swap_idx, new_frame);
		// printf("bbbbbbbbbbbbbbbbbbbbbbbbbbb\n");
		// spte->in_swap = false;
		// spte->loaded = true;
		return true;
	}
	if (type == ZERO){
		memset(new_frame, 0, PGSIZE);
		return true;
	}
	// if (type == FRAME)
	// 	return true;
	return false;
}

bool
spage_table_load(struct splmt_page_table *table, uint32_t *pagedir, void *upage){
	// printf("start pt load\n" );

	struct splmt_page_entry* entry = spage_table_find_entry(table, upage);
	// printf("end find entry\n",0 );
	// printf("entry type %d\n",entry->type  );
	// printf("%d\n",entry==NULL );
	if (!entry)
		return false;
	if (entry->type == FRAME)
		return true;

	void *new_frame_item = frame_alloc(upage, PAL_USER);
	if (!new_frame_item)
		return false;

	if (spage_table_install_page(entry, new_frame_item) == false)
		return false;
// printf("seco\n");
	if (entry->type == FILE){
		if (pagedir_set_page(pagedir, upage, new_frame_item, entry->writable) == false){
			return false;
		}
	}
	else{
		if (pagedir_set_page(pagedir, upage, new_frame_item, true) == false){
			return false;
		}
	}
// printf("las time\n");
	entry->frame_vaddr = new_frame_item;
	entry->type = FRAME;
	pagedir_set_dirty (pagedir, new_frame_item, false);
	// printf("finish %d\n",entry->type  );
	// printf("las\n");
	return true;
}


bool
vm_supt_mm_unmap(
    struct splmt_page_table *supt, uint32_t *pagedir,
    void *page, struct file *f, off_t offset, size_t bytes)
{
	struct splmt_page_entry *spte = spage_table_find_entry(supt, page);

  // see also, vm_load_page()
  switch (spte->type){
  case FRAME:
    // ASSERT (spte->kpage != NULL);

    if (pagedir_is_dirty(pagedir, spte->user_vaddr))
    	file_write_at (f, spte->user_vaddr, bytes, offset);

    // clear the page mapping, and release the frame
    // vm_frame_free (spte->kpage);
    pagedir_clear_page (pagedir, spte->user_vaddr);
    break;

  case SWAP:
    {
        void *tmp_page = palloc_get_page(0);
        swap_read_in(spte->swap_idx, tmp_page);
        file_write_at (f, tmp_page, PGSIZE, offset);
        palloc_free_page(tmp_page);

    }
    break;

  case FILE:
    break;
}
hash_delete(&supt->splmt_pages, &spte->elem);
return true;
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