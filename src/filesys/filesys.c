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

/* Static function for operation. */
static void do_format (void);

/* Initialize the file system. Also reformat
   the system if necessary. */
void
filesys_init (bool format){
  fs_device = block_get_role (BLOCK_FILESYS);
  /* If some error with filesys device. */
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");
  cache_init();
  inode_init ();
  free_map_init ();
  /* Check whether necessary to reformat. */
  if (format) 
    do_format ();
  /* Reopen the free map. */
  free_map_open ();
}


/* When everything is done, close the file system.
   Also write back any unsaved data to disk. */
void
filesys_done (void) 
{
  cache_write_back();
  free_map_close ();
}


/* Create a new file given the name and default
   size. Return whether it success to do so. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  /* If invalid path. */
  if (name[0] == '.')
    return false;
  block_sector_t inode_sector = 0;
  char copy_name[strlen(name) + 1];
  memcpy(copy_name, name, strlen(name) + 1);
  /* parse the given name. */
  char *save_ptr;
  char *curr_val = "";
  char *token = strtok_r (copy_name, "/", &save_ptr);
  /* Keep parsing the path. */
  while(token){
    curr_val = token;
    token = strtok_r (NULL, "/", &save_ptr);
  }
  /* Find the directory of name. */
  struct dir *dir = find_leaf(name);
  block_sector_t parent = inode_get_inumber(dir_get_inode(dir));
  /* Check whether everything success. */
  bool success = (dir != NULL
               && free_map_allocate (1, &inode_sector)
               && inode_create (inode_sector, initial_size, parent, false)
               && dir_add (dir, curr_val, inode_sector));
  /* If failed to do so. */
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  /* Close the directory. */
  dir_close (dir);
  return success;
}


/* Operation for open a file with name. Return a pointer
   to the file if it succeed, or return a null pointer if
   failed to do so. */
struct file *
filesys_open (const char *name)
{
  if (strcmp(name, "/") == 0){
    struct dir *dir = dir_open_root();
    struct inode *inode = dir_get_inode(dir);
    return file_open(inode);
  }
  /* Make a copy of the name. */
  char copy_name[strlen(name) + 1];
  memcpy(copy_name, name, strlen(name) + 1);
  char *save_ptr;
  char *curr_val = "";
  char *token = strtok_r(copy_name, "/", &save_ptr);
  /* Parse the name. */
  while(token){
    curr_val = token;
    token = strtok_r(NULL, "/", &save_ptr);
  }
  /* Find the leaf node of directory. */
  struct dir *dir = find_leaf(name);
  struct inode *inode = NULL;
  /* Look up the inode in the dictionary. */
  if (dir)
    dir_lookup(dir, curr_val, &inode);
  /* Close the directory. */
  dir_close(dir);
  return file_open(inode);
}


/* Operation for deleteing a file with given name.
   Return whether it succeed to do so.*/
bool
filesys_remove (const char *name) 
{
  /* Find the leaf node. */
  struct dir* dir = find_leaf(name);
  /* Try to delete the file. */
  bool success = dir != NULL && dir_remove (dir, name);
  /* Close the directory. */
  dir_close (dir); 
  return success;
}

/* Format the file system. Also print the status info. */
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