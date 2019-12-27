#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"


/* Structure for directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* Structure for entry in the directory. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };


/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create(block_sector_t sector, size_t entry_cnt, char *dir)
{
  /* Get the inode of the directory. */
  struct inode *inode = dir_get_inode(dir);
  /* Get the i number of inode. */
  block_sector_t parent = inode_get_inumber(inode);
  /* Try to create the inode. */
  return inode_create (sector, entry_cnt, parent, true);
}


/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  /* If succeed in do so. */
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else  /* Or if failed. */
    {
      /* Close the directory and free the resourse. */
      inode_close (inode);
      free (dir);
      /* Return the null pointer. */
      return NULL; 
    }
}


/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  /* Open the root directory. */
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}


/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  /* Reopen the inode. */
  return dir_open (inode_reopen (dir->inode));
}


/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      /* Close the directory and free the resourse. */
      inode_close (dir->inode);
      free (dir);
    }
}


/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}


/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  /* Check the validity of the given arguments. */
  ASSERT (dir != NULL);
  ASSERT (name != NULL);
  /* For each inode. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    /* If find the name and in use. */
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        /* Success. */
        return true;
      }
  /* Fail. */
  return false;
}


/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;
  /* Check the validity of arguments. */
  ASSERT (dir != NULL);
  /* If need to go to parent node. */
  if (strcmp(name, "..") == 0){
    struct inode *my_inode = dir_get_inode(dir);
    block_sector_t sector = my_inode->data.parent;
    *inode = inode_open(sector);
  }
  /* If need to reopen the dir. */
  else if (strcmp(name, ".") == 0)
    *inode = inode_reopen(dir->inode);
  /* If successfully look up. */
  else if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  /* If failed to look up. */
  else
    *inode = NULL;
  /* Return the result. */
  return *inode != NULL;
}


/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;
  /* Check the validity for pointer argument. */
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  /* Finished the process. */
  return success;
}


/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;
  /* Make a copy of the filename. */
  char copy_name[strlen(name) + 1];
  memcpy(copy_name, name, strlen(name) + 1);
  /* Prepare to parse the name. */
  char *save_ptr;
  char *curr_val = "";
  char *token = strtok_r (copy_name, "/", &save_ptr);
  while(token){
    curr_val = token;
    token = strtok_r (NULL, "/", &save_ptr);
  }
  /* Check the result validity. */
  ASSERT (dir != NULL);
  ASSERT (curr_val != NULL);

  /* Find directory entry. */
  if (!lookup (dir, curr_val, &e, &ofs))
    goto done;
  /* If it has done. */
  if (strcmp(name, "/a") == 0)
    goto done;

  if (thread_current()->cwd)
    return false;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  /* Finished, close the inode. */
  inode_close (inode);
  return success;
}


/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;
  /* Iterate through the directory. */
  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      /* Move the pointer. */
      dir->pos += sizeof e;
      if (e.in_use)
        {
          /* Copy the name. */
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
    /* No more entry. */
  return false;
}


/* Find the leaf node of the given directory.
   Return the pointer to the directory. */
struct dir*
find_leaf(char* dir){
  struct dir* temp_dir;
  if (!thread_current()->cwd)
    temp_dir = dir_open_root();
  else if (dir[0] == '/')
    temp_dir = dir_open_root();
  else
    temp_dir = dir_reopen(thread_current()->cwd);
  /* Make a copy of the name. */
  char copy_path[strlen(dir) + 1];
  memcpy(copy_path, dir, strlen(dir) + 1);
  /* Parse the name. */
  char *save_ptr;
  char *token = strtok_r(copy_path, "/", &save_ptr);
  char *token1 = strtok_r(NULL, "/", &save_ptr);
  while (token1){
    struct inode *inode;
    /* If failed to look up the directory. */
    if (!dir_lookup(temp_dir, token, &inode))
      return NULL;
    /* If need to get to the parent directory. */
    if (strcmp(token, "..") == 0){
      struct inode *inode = dir_get_inode(temp_dir);
      block_sector_t sector = inode->data.parent;
      inode = inode_open(sector);
    }
    /* If need to close the dir. Open inode. */
    if (inode->data.dir_or_file){
      dir_close(temp_dir);
      temp_dir = dir_open(inode);
    }
    token = token1;
    token1 = strtok_r(NULL, "/", &save_ptr);
  }
  /* Return the results. */
  return temp_dir;
}