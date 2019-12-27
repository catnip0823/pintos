#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");
  cache_init();
  inode_init ();
  free_map_init ();
  
  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  cache_write_back();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  if (name[0] == '.')
    return false;
  block_sector_t inode_sector = 0;
  char copy_name[strlen(name) + 1];
  memcpy(copy_name, name, strlen(name) + 1);
  
  char *save_ptr;
  char *curr_val = "";
  char *token = strtok_r (copy_name, "/", &save_ptr);
  while(token){
    curr_val = token;
    token = strtok_r (NULL, "/", &save_ptr);
  }

  struct dir *dir = find_leaf(name);
  block_sector_t parent = inode_get_inumber(dir_get_inode(dir));
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, parent, false)
                  && dir_add (dir, curr_val, inode_sector));

  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  if (strcmp(name, "/") == 0){
    struct dir *dir = dir_open_root();
    struct inode *inode = dir_get_inode(dir);
    return file_open(inode);
  }

  char copy_name[strlen(name) + 1];
  memcpy(copy_name, name, strlen(name) + 1);
  char *save_ptr;
  char *curr_val = "";
  char *token = strtok_r(copy_name, "/", &save_ptr);
  while(token){
    curr_val = token;
    token = strtok_r(NULL, "/", &save_ptr);
  }

  struct dir *dir = find_leaf(name);
  struct inode *inode = NULL;
  if (dir)
    dir_lookup(dir, curr_val, &inode);
  dir_close(dir);
  return file_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir* dir = find_leaf(name);
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!inode_create (ROOT_DIR_SECTOR, 16, ROOT_DIR_SECTOR, true))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
