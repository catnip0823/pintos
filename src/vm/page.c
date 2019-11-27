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
		swap_read_in(spte->swap_idx, new_frame);
		spte->in_swap = false;
		spte->loaded = true;
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
	// printf("%d\n",1 );

	struct splmt_page_entry* entry = spage_table_find_entry(table, upage);
	// printf("%d\n",0 );
	// printf("%d\n",entry->type  );
	// printf("%d\n",entry==NULL );
	if (!entry)
		return false;
	if (entry->type == FRAME)
		return true;

	void *new_frame_item = frame_alloc(PAL_USER, upage);
	if (!new_frame_item)
		return false;

	if (spage_table_install_page(entry, new_frame_item) == false)
		return false;

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

	entry->frame_vaddr = new_frame_item;
	entry->type = FRAME;
	pagedir_set_dirty (pagedir, new_frame_item, false);
	return true;
}


bool
vm_supt_mm_unmap(
    struct splmt_page_table *supt, uint32_t *pagedir,
    void *page, struct file *f, off_t offset, size_t bytes)
{
	struct splmt_page_entry *spte = spage_table_find_entry(supt, page);
  // if(spte == NULL) {
  //   PANIC ("munmap - some page is missing; can't happen!");
  // }

  // Pin the associated frame if loaded
  // otherwise, a page fault could occur while swapping in (reading the swap disk)
  // if (spte->status == ON_FRAME) {
  //   ASSERT (spte->kpage != NULL);
  //   vm_frame_pin (spte->kpage);
  // }

  // see also, vm_load_page()
  switch (spte->type){
  case FRAME:{
    // ASSERT (spte->kpage != NULL);

    if (pagedir_is_dirty(pagedir, spte->user_vaddr))
    	file_write_at (f, spte->user_vaddr, bytes, offset);

    // clear the page mapping, and release the frame
    // vm_frame_free (spte->kpage);
    pagedir_clear_page (pagedir, spte->user_vaddr);
    break;
}
  case SWAP:
    {
    	// if (pagedir_is_dirty(pagedir, spte->user_vaddr))
      // {
        // load from swap, and write back to file
        void *tmp_page = palloc_get_page(0); // in the kernel
        // vm_swap_in (spte->swap_index, tmp_page);
        swap_read_in(spte->swap_idx, tmp_page);
        file_write_at (f, tmp_page, PGSIZE, offset);
        palloc_free_page(tmp_page);
      // }
      // else {
      //   // just throw away the swap.
      //   vm_swap_free (spte->swap_index);
      // }
    }
    break;

  case FILE:
    // do nothing.
    break;
}
hash_delete(&supt->splmt_pages, &spte->elem);
return true;
}








// /* Grow stack when necessary, return whether it succeed. */
// bool
// spage_table_grow_stack(void* user_vaddr){
// 	if ((size_t)(PHYS_BASE - pg_round_down(user_vaddr)) > MAX_STACK_SIZE)
// 	 	return false;
// 	struct splmt_page_entry* spte = (struct splmt_page_entry*) malloc(
// 										sizeof(struct splmt_page_entry));
// 	if (spte == NULL)
// 		return false;
// 	spte->attached = true;
// 	spte->writable = true;
// 	spte->loaded = true;
// 	spte->user_vaddr = pg_round_down(user_vaddr);
// 	spte->type = CODE;

// 	uint8_t* frame_addr = frame_alloc(spte, PAL_USER);
// 	if (frame_addr == NULL){
// 		free(spte);
// 		return false;
// 	}
// 	struct frame_table_entry* fte = frame_table_find(frame_addr);
// 	fte->page_addr = spte;
// 	spte->fte = fte;
// 	if (!install_page(spte->user_vaddr, frame_addr, true)){
// 		frame_free_entry(frame_addr);
// 		free(spte);
// 		return false;
// 	}
// 	hash_insert(&(thread_current()->spage_table), spte);
// 	return true;
// }

// /* Function for add new entry to page table, with type
//    MMAP, return a pointer to the first entry it added.
//    Return NULL pointer if failed to allcate memory. */
// struct splmt_page_entry*
// spage_table_add_mmap(struct file *file, uint32_t valid_bytes, void *user_page){
// 	int page_num = valid_bytes/PGSIZE + 1;
// 	struct splmt_page_entry* retval;
// 	for (int i = 0; i < page_num; ++i){
// 		struct splmt_page_entry* test = spage_table_find_addr(user_page);
// 		if (test != NULL)
// 			return NULL;

// 		int last_page = (i == page_num);
// 		unsigned page_valid = last_page == 1 ? valid_bytes%PGSIZE : PGSIZE;
// 		unsigned page_zero = PGSIZE - page_valid;
// 		unsigned offset = i*PGSIZE;

// 		struct splmt_page_entry* spte = creat_entry();
// 		spte->valid_bytes = page_valid;
// 		spte->zero_bytes = page_zero;
// 		spte->user_vaddr = user_page;
// 		spte->offset = offset;
// 		spte->writable = true;
// 		spte->type = MMAP;
// 		spte->file = file;

// 		user_page += PGSIZE;

// 		hash_insert(&(thread_current()->spage_table), spte);
// 		if (i == 0)
// 			retval = spte;
// 	}
// 	return retval;
// }


// /* Install and load the page with type CODE, return whether 
//    it succeed to install the page. */
// static bool
// install_code_page(struct splmt_page_entry* spte){
// 	if (spte == NULL)
// 		return false;
// 	void* frame_addr = frame_alloc(spte, PAL_USER | PAL_ZERO);
// 	if (frame_addr == NULL)
// 		return NULL;
// 	if (install_page(spte->user_vaddr, frame_addr, true)){
// 		spte->fte = frame_table_find(frame_addr);
// 		if(spte->in_swap)
// 			return false;
// 		return true;
// 	}
// 	frame_free_entry(frame_addr);
// 	return false;
// }


// /* The page with type MMAP have generally same
//    procedure with type CODE, so just called the
//    function we defined above. */
// static bool
// install_mmap_page(struct splmt_page_entry* spte){
// 	return install_code_page(spte);
// }


