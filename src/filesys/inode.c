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
    /* count the offset_th block of the pos, stored in offset. */
    int offset = pos / BLOCK_SECTOR_SIZE;
    
    /* If in direct part, */
    if (offset < DIRECT_BLOCK)
      /* directly fetch the data from the direct array and return. */
      return inode->data.direct_part[offset];
    
    /* If in direct part, */
    else if (offset < DIRECT_BLOCK + INDIRECT_BLOCK){
      /* fetch the entry of the indirect part, and read sector from it, */
      struct inode_indirect *indirect_parts = 
                              malloc(sizeof(struct inode_indirect));
      cache_read_sector(inode->data.indirect_part, 
                        indirect_parts, 0, BLOCK_SECTOR_SIZE);
      /* return sector that matches the offset position. */
      return indirect_parts->indirect_inode[offset - DIRECT_BLOCK];
    }
    
    /* If in double direct part, */
    else if (offset < DIRECT_BLOCK + INDIRECT_BLOCK + DOUBLE_INDIRECT){
      struct inode_indirect *indirect_parts = 
                            malloc(sizeof(struct inode_indirect));
      /* first find the entry of the first level, and load it from cache, */
      cache_read_sector (inode->data.double_indirect_part, 
                        indirect_parts, 0, BLOCK_SECTOR_SIZE);
      /* then find the entry of the second level, and load it from cache, */
      int indirect_offset = (offset - DIRECT_BLOCK -  INDIRECT_BLOCK) 
                            / INDIRECT_BLOCK;
      cache_read_sector (indirect_parts->indirect_inode[indirect_offset], 
                        indirect_parts, 0, BLOCK_SECTOR_SIZE);
      /* finally return sector that matches the offset position. */
      int double_offset = (offset - DIRECT_BLOCK - INDIRECT_BLOCK) 
                          % INDIRECT_BLOCK;
      return indirect_parts->indirect_inode[double_offset];
    }
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
inode_create (block_sector_t sector, off_t length, 
              block_sector_t parent, bool dir_or_file)
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
      disk_inode->dir_or_file = dir_or_file;
      disk_inode->parent = parent;
      struct inode_indirect inode_indirect;

      // proj4
      for (int i = 0; i < DIRECT_BLOCK; i++){
        if (i == sectors){
          // 1. if direct
          cache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
          return true;
        }
        if (disk_inode->direct_part[i] == 0){
          /* if the sectors is not allocated, fill it with zeros. */
          free_map_allocate(1, &disk_inode->direct_part[i]);
          cache_write(disk_inode->direct_part[i], 
                      zeros, 0, BLOCK_SECTOR_SIZE);
        }
      }
      // for indirect
      if (disk_inode->indirect_part == 0){
        /* if the sectors is not allocated, fill it with zeros. */
        free_map_allocate(1, &disk_inode->indirect_part);
        cache_write(disk_inode->indirect_part, 
                    zeros, 0, BLOCK_SECTOR_SIZE);
      }
      cache_read_sector(disk_inode->indirect_part, 
                        &inode_indirect, 0, BLOCK_SECTOR_SIZE);
      for (int i = 0; i < sectors - DIRECT_BLOCK; i++){
        if (inode_indirect.indirect_inode[i] == 0){
          /* if the sectors is not allocated, fill it with zeros. */
          free_map_allocate(1, &inode_indirect.indirect_inode[i]);
          cache_write(inode_indirect.indirect_inode[i], 
                      zeros, 0, BLOCK_SECTOR_SIZE);
        }
      }
      cache_write(disk_inode->indirect_part, 
                  &inode_indirect, 0, BLOCK_SECTOR_SIZE);
      // 2. if indirect
      if (sectors - DIRECT_BLOCK < INDIRECT_BLOCK){
        cache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
        return true;
      }
      // 3. if double indirect
      if (sectors - DIRECT_BLOCK - INDIRECT_BLOCK < DOUBLE_INDIRECT){
        if (disk_inode->double_indirect_part == 0){
          /* if the sectors is not allocated, fill it with zeros. */
          free_map_allocate(1, &disk_inode->double_indirect_part);
          cache_write(disk_inode->double_indirect_part, 
                      zeros, 0, BLOCK_SECTOR_SIZE);
        }
        cache_read_sector(disk_inode->double_indirect_part, 
                          &inode_indirect, 0, BLOCK_SECTOR_SIZE);
        int length = sectors - DIRECT_BLOCK - INDIRECT_BLOCK;
        int offset = length / INDIRECT_BLOCK + 1;
        for (int i = 0; i < offset; i++){
          struct inode_indirect temp;
          if (inode_indirect.indirect_inode[i] == 0){
            /* if the sectors is not allocated, fill it with zeros. */
            free_map_allocate(1, &inode_indirect.indirect_inode[i]);
            cache_write(inode_indirect.indirect_inode[i], 
                        zeros, 0, BLOCK_SECTOR_SIZE);
          }
          cache_read_sector(inode_indirect.indirect_inode[i], 
                            &temp, 0, BLOCK_SECTOR_SIZE);
          for (int i = 0; i < length; i++){
            if (temp.indirect_inode[i] == 0){
              /* if the sectors is not allocated, fill it with zeros. */
              free_map_allocate(1, &temp.indirect_inode[i]);
              cache_write(temp.indirect_inode[i], 
                          zeros, 0, BLOCK_SECTOR_SIZE);
            }
          }
          cache_write(inode_indirect.indirect_inode[i], 
                      &temp, 0, BLOCK_SECTOR_SIZE);
        }
        cache_write(disk_inode->double_indirect_part, 
                    &inode_indirect, 0, BLOCK_SECTOR_SIZE);
        cache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
        return true;
      }
    }
    // end proj4

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
  cache_read_sector (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
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
          struct inode_indirect inode_indirect;
          size_t sectors = bytes_to_sectors(inode->data.length);
          /* Firstly, free the direct part. */
          for (int i = 0; i < DIRECT_BLOCK; ++i){
            if (i == sectors){
              /* If direct, return immdiately after freeing sectors. */
              free(inode);
              return;
            }
            /* free the first i sectors. */
            free_map_release(inode->data.direct_part[i], 1);
          }
          
          /* Secondly, load indirect part and free the indirect part. */
          cache_read_sector(inode->data.indirect_part, 
                            &inode_indirect, 0, BLOCK_SECTOR_SIZE);
          for (int i = 0; i < INDIRECT_BLOCK; i++)
            /* free the indirect sectors. */
            free_map_release(inode_indirect.indirect_inode[i], 1);
          /* If indirect, return immdiately after freeing sectors. */
          if (sectors - DIRECT_BLOCK < INDIRECT_BLOCK){
            free(inode);
            return;
          }
          
          /* Thirdly, load double indirect part and free. */
          cache_read_sector(inode->data.double_indirect_part, 
                            &inode_indirect, 0, BLOCK_SECTOR_SIZE);
          int length = sectors - DIRECT_BLOCK - INDIRECT_BLOCK;
          int offset = length / INDIRECT_BLOCK + 1;
          /* free the whole offset parts of the first level of the 
             double indirect part. */
          for (int i = 0; i < offset; i++){
            /* find the entry of the second level, and load it from cache */
            struct inode_indirect temp_block;
            cache_read_sector(inode_indirect.indirect_inode[i], 
                              &temp_block, 0, BLOCK_SECTOR_SIZE);
            for (int i = 0; i < INDIRECT_BLOCK; i++)
              /* free each second-level entry. */
              free_map_release(temp_block.indirect_inode[i], 1);
          }
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
      cache_read_sector (sector_idx, buffer + bytes_read, 
                        sector_ofs, chunk_size);
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


  if ((int)byte_to_sector (inode, offset + size - 1) == -1){
    static char zeros[BLOCK_SECTOR_SIZE];
    size_t sectors = bytes_to_sectors(offset + size);
    /* If direct, finish immdiately after writing sectors. */
    for (int i = 0; i < DIRECT_BLOCK; i++){
      if (i == sectors){
        /* write direct part and start then */
        inode->data.length = offset + size;
        cache_write(inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
        goto start;
        break;
      }
      if (inode->data.direct_part[i] == 0){
        /* if the sectors is not allocated, fill it with zeros. */
        free_map_allocate(1, &inode->data.direct_part[i]);
        cache_write(inode->data.direct_part[i], zeros, 0, BLOCK_SECTOR_SIZE);
      }
    }
    /* If in indirect part of inode: */
    if (sectors - DIRECT_BLOCK < INDIRECT_BLOCK){
      /* Load indirect part and write indirect part and start then */
      write_indirect(&inode->data.indirect_part, sectors - DIRECT_BLOCK);
      inode->data.length = offset + size;
      cache_write (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
      goto start;
    }
    /* If in double indirect part of inode: */
    if (sectors - DIRECT_BLOCK - INDIRECT_BLOCK < DOUBLE_INDIRECT){
      /* Load double indirect part and write and start then */
      int length = sectors - DIRECT_BLOCK - INDIRECT_BLOCK;
      write_double(&inode->data.double_indirect_part, length);
      inode->data.length = offset + size;
      cache_write (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
      goto start;
    }
  }

  start:
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
      cache_write(sector_idx, bytes_written+buffer, sector_ofs, chunk_size);
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

/* Helper function for write operation for the cache, where we 
   need to write size sectors into the cache, and the sectors 
   are all in indirect parts of the inode. */
void write_indirect(block_sector_t* sectors, int size)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  if (*sectors == 0){
    /* if the sectors is not allocated, fill it with zeros. */
    free_map_allocate(1, sectors);
    cache_write(*sectors, zeros, 0, BLOCK_SECTOR_SIZE);
  }
  /* Load indirect part and justify whether it is allocated */
  struct inode_indirect inode_indirect;
  cache_read_sector(*sectors, &inode_indirect, 0, BLOCK_SECTOR_SIZE);
  for (int i = 0; i < size; i++){
    if (inode_indirect.indirect_inode[i] == 0){
      /* if the sectors is not allocated, fill it with zeros. */
      free_map_allocate(1, &inode_indirect.indirect_inode[i]);
      cache_write(inode_indirect.indirect_inode[i], 
                  zeros, 0, BLOCK_SECTOR_SIZE);
     }
  }
  /* write the changes into cache. */
  cache_write (*sectors, &inode_indirect, 0, BLOCK_SECTOR_SIZE);
}

/* Helper function for write operation for the cache, where we 
   need to write size sectors into the cache, and the sectors 
   are all in double indirect parts of the inode. */
void write_double(block_sector_t* sectors, int size)
{
  if (*sectors == 0){
    /* if the sectors is not allocated, fill it with zeros. */
    free_map_allocate (1, sectors);
    static char zeros[BLOCK_SECTOR_SIZE];
    cache_write (*sectors, zeros, 0, BLOCK_SECTOR_SIZE);
  }
  /* Load double indirect part and justify whether it is allocated */
  struct inode_indirect inode_indirect;
  cache_read_sector(*sectors, &inode_indirect, 0, BLOCK_SECTOR_SIZE);
  int offset = size / INDIRECT_BLOCK + 1;
  /* double indirect part is saperated into offset indirect parts. */
  for (int i = 0; i < offset; i++){
    /* for each first level of indirect block, do the same write 
       operation, and can just call the indirect function.*/
    write_indirect(&inode_indirect.indirect_inode[i], INDIRECT_BLOCK);
  }
  /* write the changes into cache. */
  cache_write (*sectors, &inode_indirect, 0, BLOCK_SECTOR_SIZE);
}