#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/fat.h" /* eleshock */

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
 * Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk {
/* eleshock */
#ifdef EFILESYS
	cluster_t start; // for project4
	enum file_type type; // Jack
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */
	uint32_t unused[124];               /* Not used. */
#else
	disk_sector_t start;                /* First data sector. */
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */
	uint32_t unused[125];               /* Not used. */
#endif
};

/* Returns the number of sectors to allocate for an inode SIZE
 * bytes long. */
static inline size_t
bytes_to_sectors (off_t size) {
	return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode {
	struct list_elem elem;              /* Element in inode list. */
	int open_cnt;                       /* Number of openers. */
	bool removed;                       /* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
/* prj4 filesys - yeopto */
#ifdef EFILESYS
	cluster_t cluster;
#else	
	disk_sector_t sector;               /* Sector number of disk location. */
#endif
	struct inode_disk data;             /* Inode content. */
};

/* Returns the disk sector that contains byte offset POS within
 * INODE.
 * Returns -1 if INODE does not contain data for a byte at offset
 * POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) {
	ASSERT (inode != NULL);
#ifdef EFILESYS
	/* eleshock */
	if (pos < inode->data.length)
	{
		cluster_t clst = inode->data.start; // first cluster idx
		int count = pos / DISK_SECTOR_SIZE;
		for (int i = 0; i < count; i++) {
			clst = fat_get(clst);
		}
		return cluster_to_sector(clst);
	}
#else
	if (pos < inode->data.length)
		return inode->data.start + pos / DISK_SECTOR_SIZE;
#endif
	else
		return -1;
}

/* Jack */
/* Extend file */
static bool
check_and_extend_file (struct inode *inode, off_t pos, off_t size) {
	ASSERT (inode != NULL);

	if (pos + size <= inode->data.length)
		return true;
	
	cluster_t need, last, curr;
	for (curr = inode->data.start; curr != EOChain; curr = fat_get(curr))
		last = curr;

	if ((need = bytes_to_sectors(pos + size) - \
		 bytes_to_sectors(inode->data.length > 0? inode->data.length: 1)) > 0) {
		cluster_t new;

		if (fat_create_multi_chain(last, need, &new)) {
			size_t i;
			cluster_t curr_cluster = new;
			
			static char zeros_extend[DISK_SECTOR_SIZE];
			for (i = 0; i < need; i++)
			{
				ASSERT (curr_cluster != EOChain);
				disk_write (filesys_disk, cluster_to_sector(curr_cluster), zeros_extend); 
				curr_cluster = fat_get(curr_cluster);
			}
		} else {
			return false;
		}
	}

	/* Jack */
	/* 현 EOF가 있는 섹터에서 EOF 이후의 공간을 0으로 채우는 부분
	 * 그러나 inode create에서 파일을 생성할 때 애초에 모든 섹터를 0으로 채워놨기 때문에
	 * 현재 작업은 중복인 듯 하여 주석처리함 */
	// uint8_t *bounce = calloc (1, DISK_SECTOR_SIZE);
	// if (bounce == NULL)
	// 	return false;
	// uint32_t eof_ofs = inode->data.length % DISK_SECTOR_SIZE;
	// uint32_t eof_left = DISK_SECTOR_SIZE - eof_ofs;
	// disk_read (filesys_disk, cluster_to_sector(last), bounce);
	// memset (bounce + eof_ofs, 0, eof_left);
	// disk_write (filesys_disk, cluster_to_sector(last), bounce); 
	// free(bounce);	

	inode->data.length = pos + size;
	disk_write (filesys_disk, cluster_to_sector(inode->cluster), &inode->data);

	return true;
}

/* List of open inodes, so that opening a single inode twice
 * returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) {
	list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
 * writes the new inode to sector SECTOR on the file system
 * disk.
 * Returns true if successful.
 * Returns false if memory or disk allocation fails. */
#ifdef EFILESYS
bool
inode_create (disk_sector_t sector, off_t length, enum file_type type) {
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	 * one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);
	if (disk_inode != NULL) {
		/* Jack */
		cluster_t clusters = length > 0? bytes_to_sectors (length): 1;
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		disk_inode->type = type;
		if (fat_create_multi_chain(0, clusters, &disk_inode->start)) {
			disk_write (filesys_disk, sector, disk_inode);
			
			static char zeros[DISK_SECTOR_SIZE];
			size_t i;
			cluster_t curr_cluster = disk_inode->start;
			
			for (i = 0; i < clusters; i++)
			{
				ASSERT (curr_cluster != EOChain);
				disk_write (filesys_disk, cluster_to_sector(curr_cluster), zeros); 
				curr_cluster = fat_get(curr_cluster);
			}
			success = true; 
		}
		free (disk_inode);		
	}
	return success;
}
#else
bool
inode_create (disk_sector_t sector, off_t length) {
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	 * one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);
	if (disk_inode != NULL) {
		size_t sectors = bytes_to_sectors (length);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		if (free_map_allocate (sectors, &disk_inode->start)) {
			disk_write (filesys_disk, sector, disk_inode);
			if (sectors > 0) {
				static char zeros[DISK_SECTOR_SIZE];
				size_t i;

				for (i = 0; i < sectors; i++) 
					disk_write (filesys_disk, disk_inode->start + i, zeros); 
			}
			success = true; 
		}
		free (disk_inode);
	}
	return success;
}
#endif 

/* Reads an inode from SECTOR
 * and returns a `struct inode' that contains it.
 * Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) {
	struct list_elem *e;
	struct inode *inode;

	/* Check whether this inode is already open. */
	for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
			e = list_next (e)) {
		inode = list_entry (e, struct inode, elem);
#ifndef EFILESYS
		if (inode->sector == sector) {
			inode_reopen (inode);
			return inode; 
		}
#else
		if (inode->cluster == sector_to_cluster(sector)) {
			inode_reopen (inode);
			return inode; 
		}
#endif
	}

	/* Allocate memory. */
	inode = malloc (sizeof *inode);
	if (inode == NULL)
		return NULL;

	/* Initialize. */
	list_push_front (&open_inodes, &inode->elem);

	/* Jack */
#ifndef EFILESYS	
	inode->sector = sector;
#else
	inode->cluster = sector_to_cluster(sector);
#endif
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	// lock_init(&inode->inode_lock);
#ifndef EFILESYS
	disk_read (filesys_disk, inode->sector, &inode->data);
#else
	disk_read (filesys_disk, cluster_to_sector(inode->cluster), &inode->data);
#endif
	return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode) {
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode) {
	/* Jack */
#ifndef EFILESYS
	return inode->sector;
#else
	return cluster_to_sector(inode->cluster);
#endif
}

/* Closes INODE and writes it to disk.
 * If this was the last reference to INODE, frees its memory.
 * If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) {
	/* Ignore null pointer. */
	if (inode == NULL)
		return;

	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0) {
		/* Remove from inode list and release lock. */
		list_remove (&inode->elem);

#ifdef EFILESYS
		if (inode->removed) {
			fat_remove_chain (inode->cluster, 0);
			fat_remove_chain (inode->data.start, 0);
		}
#else
		/* Deallocate blocks if removed. */
		if (inode->removed) {
			free_map_release (inode->sector, 1);
			free_map_release (inode->data.start,
					bytes_to_sectors (inode->data.length)); 
		}
#endif
		free (inode); 
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
 * has it open. */
void
inode_remove (struct inode *inode) {
	ASSERT (inode != NULL);
	inode->removed = true;
}

bool
inode_get_removed (struct inode *inode) {
	ASSERT (inode != NULL);
	return inode->removed;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
 * Returns the number of bytes actually read, which may be less
 * than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) {
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;
	while (size > 0) {
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer. */
			disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
		} else {
			/* Read sector into bounce buffer, then partially copy
			 * into caller's buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			disk_read (filesys_disk, sector_idx, bounce);
			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}
	free (bounce);

	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
 * Returns the number of bytes actually written, which may be
 * less than SIZE if end of file is reached or an error occurs.
 * (Normally a write at end of file would extend the inode, but
 * growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
		off_t offset) {
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;

	if (inode->deny_write_cnt || !check_and_extend_file(inode, offset, size)) // Jack
		return 0;

	while (size > 0) {
		/* Sector to write, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Write full sector directly to disk. */
			disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
		} else {
			/* We need a bounce buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}

			/* If the sector contains data before or after the chunk
			   we're writing, then we need to read in the sector
			   first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || chunk_size < sector_left) 
				disk_read (filesys_disk, sector_idx, bounce);
			else
				memset (bounce, 0, DISK_SECTOR_SIZE);
			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
			disk_write (filesys_disk, sector_idx, bounce); 
		}

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
 * Must be called once by each inode opener who has called
 * inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) {
	ASSERT (inode->deny_write_cnt > 0);
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode) {
	return inode->data.length;
}

/* Jack */
/* Return file type from inode */
enum file_type
inode_get_type (const struct inode *inode) {
	return inode->data.type;
}

// /*** Jack ***/
// /* Lock acquire for inode */
// void inode_acquire(struct inode *i)
// {
// 	lock_acquire(&i->inode_lock);
// }

// /* Lock release for inode */
// void inode_release(struct inode *i)
// {
// 	lock_release(&i->inode_lock);
// }
