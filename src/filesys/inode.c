#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length){
    // proj4
    int offset = pos / BLOCK_SECTOR_SIZE;
    if (offset < COUNT_DIRECT_BLOCKS)
      return inode->data.direct_part[offset];
    else if (offset < COUNT_DIRECT_BLOCKS + PER_SECTOR_INDIRECT_BLOCKS){
      struct inode_indirect *indirect_parts = malloc(sizeof(struct inode_indirect));
      buffer_cache_read(inode->data.indirect_part, indirect_parts, 0, BLOCK_SECTOR_SIZE);
      return indirect_parts->indirect_inode[offset - COUNT_DIRECT_BLOCKS];
    }
    // end proj4
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir, block_sector_t parent)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  // ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      static char zeros[BLOCK_SECTOR_SIZE];
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = is_dir;
      disk_inode->parent = parent;

      // proj4
      for (int i = 0; i < COUNT_DIRECT_BLOCKS; i++){
        if (i == sectors){
          // 1. if direct
          buffer_cache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE, false);
          return true;
        }
        if (disk_inode->direct_part[i] == 0){
          free_map_allocate(1, &disk_inode->direct_part[i]);
          buffer_cache_write(disk_inode->direct_part[i], zeros, 0, BLOCK_SECTOR_SIZE, false);
        }
      }
      // 2. if indirect
      int indirect_num = PER_SECTOR_INDIRECT_BLOCKS;
      if (sectors - COUNT_DIRECT_BLOCKS < PER_SECTOR_INDIRECT_BLOCKS){
        indirect_num = sectors - COUNT_DIRECT_BLOCKS;
        if (disk_inode->indirect_part == 0){
          free_map_allocate(1, &disk_inode->indirect_part);
          buffer_cache_write(disk_inode->indirect_part, zeros, 0, BLOCK_SECTOR_SIZE, true);
        }
        struct inode_indirect indirect_block;
        buffer_cache_read(disk_inode->indirect_part, &indirect_block, 0, BLOCK_SECTOR_SIZE);

        for (int i = 0; i < indirect_num; i++){
          if (indirect_block.indirect_inode[i] == 0){
            free_map_allocate(1, &indirect_block.indirect_inode[i]);
            buffer_cache_write (indirect_block.indirect_inode[i], zeros, 0, BLOCK_SECTOR_SIZE, false);
          }
        }
        buffer_cache_write(disk_inode->indirect_part, &indirect_block, 0, BLOCK_SECTOR_SIZE, false);
        buffer_cache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE, false);
        return true;
      }
      // end proj4
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  buffer_cache_read (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          // proj4
          size_t sectors = bytes_to_sectors (inode->data.length);
          for (int i = 0; i < COUNT_DIRECT_BLOCKS; ++i){
            if (i == sectors){
              // 1. if direct
              free(inode);
              return;
            }
            free_map_release(inode->data.direct_part[i], 1);
          }
          // 2. if indirect
          if (sectors - COUNT_DIRECT_BLOCKS < PER_SECTOR_INDIRECT_BLOCKS){
            sectors = PER_SECTOR_INDIRECT_BLOCKS;
            block_sector_t indirect_part = inode->data.indirect_part;
            struct inode_indirect inode_indirect;
            buffer_cache_read(indirect_part, &inode_indirect, 0, BLOCK_SECTOR_SIZE);

            for (int i = 0; i < sectors; i++)
              free_map_release (indirect_part, 1);
          }
          
          block_sector_t indirect_part = inode->data.indirect_part;
          struct inode_indirect inode_indirect;
          buffer_cache_read(indirect_part, &inode_indirect, 0, BLOCK_SECTOR_SIZE);

          for (int i = 0; i < sectors; i++)
            free_map_release (indirect_part, 1);
          // end proj4

          // free_map_release (inode->data.start,
          //                   bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      buffer_cache_read (sector_idx, buffer + bytes_read, sector_ofs, chunk_size);

      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Read full sector directly into caller's buffer. */
      //     block_read (fs_device, sector_idx, buffer + bytes_read);
      //   }
      // else 
      //   {
      //      Read sector into bounce buffer, then partially copy
      //        into caller's buffer. 
      //     if (bounce == NULL) 
      //       {
      //         bounce = malloc (BLOCK_SECTOR_SIZE);
      //         if (bounce == NULL)
      //           break;
      //       }
      //     block_read (fs_device, sector_idx, bounce);
      //     memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
      //   }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  // proj4
  if ((int)byte_to_sector (inode, offset + size - 1) == -1){
    bool success = false;
    size_t sectors = bytes_to_sectors(offset + size);
    int length = 0;
    for (int i = 0; i < COUNT_DIRECT_BLOCKS; i++){
      if (i == sectors){
        // 1. if in direct
        success = true;
        break;
      }
      if (inode->data.direct_part[i] == 0){
        free_map_allocate (1, &inode->data.direct_part[i]);
        static char zeros[BLOCK_SECTOR_SIZE];
        buffer_cache_write (inode->data.direct_part[i], zeros, 0, BLOCK_SECTOR_SIZE, false);
      }
    }
    if (!success){
      // 2. if in indirect
      if (sectors - COUNT_DIRECT_BLOCKS < PER_SECTOR_INDIRECT_BLOCKS){
        length = sectors - COUNT_DIRECT_BLOCKS;
        success = true;
      }
    }
    if (success){
      static char zeros[BLOCK_SECTOR_SIZE];
      if (inode->data.indirect_part == 0){
        free_map_allocate(1, &inode->data.indirect_part);
        buffer_cache_write(inode->data.indirect_part, zeros, 0, BLOCK_SECTOR_SIZE, true);
      }
      struct inode_indirect indirect_block;
      buffer_cache_read(inode->data.indirect_part, &indirect_block, 0, BLOCK_SECTOR_SIZE);

      for (int i = 0; i < length; i++){
        if (indirect_block.indirect_inode[i] == 0){
          free_map_allocate(1, &indirect_block.indirect_inode[i]);
          buffer_cache_write(indirect_block.indirect_inode[i], zeros, 0, BLOCK_SECTOR_SIZE, false);
        }
      }
      buffer_cache_write(inode->data.indirect_part, &indirect_block, 0, BLOCK_SECTOR_SIZE, false);

      inode->data.length = offset + size;
      buffer_cache_write (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE, false);
    }
  }
  // end proj4

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      bool zeroed;

      zeroed = (sector_ofs <= 0 && chunk_size >= sector_left);
      buffer_cache_write (sector_idx, buffer + bytes_written, sector_ofs, chunk_size, zeroed);
      //   {
      //     /* Write full sector directly to disk. */
      //     block_write (fs_device, sector_idx, buffer + bytes_written);
      //   }
      // else 
      //   {
      //     /* We need a bounce buffer. */
      //     if (bounce == NULL) 
      //       {
      //         bounce = malloc (BLOCK_SECTOR_SIZE);
      //         if (bounce == NULL)
      //           break;
      //       }

      //      If the sector contains data before or after the chunk
      //        we're writing, then we need to read in the sector
      //        first.  Otherwise we start with a sector of all zeros. 
      //     if (sector_ofs > 0 || chunk_size < sector_left) 
      //       block_read (fs_device, sector_idx, bounce);
      //     else
      //       memset (bounce, 0, BLOCK_SECTOR_SIZE);
      //     memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      //     block_write (fs_device, sector_idx, bounce);
      //   }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
