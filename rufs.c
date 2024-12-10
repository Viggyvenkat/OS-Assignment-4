/*
 *  Copyright (C) 2023 CS416 Rutgers CS
 *	Tiny File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26
#define NAME_LEN 255

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here
struct superblock sb;

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

    //Inode 0 is reserved, so bitmap should reflect it as used 

	// Step 1: Read inode bitmap from disk
	
	// Step 2: Traverse inode bitmap to find an available slot

	// Step 3: Update inode bitmap and write to disk 

	char buf[BLOCK_SIZE];
    if (bio_read(sb.i_bitmap_blk, buf) < 0) {
        fprintf(stderr, "Error: Failed to read inode bitmap from block %d\n", sb.i_bitmap_blk);
        return -1;
    }

    for (int i = 0; i < MAX_INUM; i++) {
        if (!get_bitmap((bitmap_t)buf, i)) { 
            set_bitmap((bitmap_t)buf, i);

            if (bio_write(sb.i_bitmap_blk, buf) < 0) {
                fprintf(stderr, "Error: Failed to write updated inode bitmap to block %d\n", sb.i_bitmap_blk);
                return -1;
            }

            fprintf(stderr, "get_avail_ino: Found available inode %d\n", i);
            return i; 
        }
    }

    return -1; 
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	
	// Step 2: Traverse data block bitmap to find an available slot

	// Step 3: Update data block bitmap and write to disk 

	char buf[BLOCK_SIZE];
    if (bio_read(sb.d_bitmap_blk, buf) < 0) {
        fprintf(stderr, "Error: Failed to read data block bitmap from block %d\n", sb.d_bitmap_blk);
        return -1; 
    }

    for (int i = 0; i < MAX_DNUM; i++) {
        if (!get_bitmap((bitmap_t)buf, i)) {
            set_bitmap((bitmap_t)buf, i);

            if (bio_write(sb.d_bitmap_blk, buf) < 0) {
                fprintf(stderr, "Error: Failed to write updated data block bitmap to block %d\n", sb.d_bitmap_blk);
                return -1;
            }

            fprintf(stderr, "get_avail_blkno: Found available block %d\n", i);
            return i;
        }
    }
    fprintf(stderr, "get_avail_blkno: No available data blocks\n");
    return -1;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {
    if (ino >= sb.max_inum) {
        fprintf(stderr, "Error: Inode number %d is out of bounds (max: %d)\n", ino, sb.max_inum);
        return -1;
    }

    uint32_t blk_num = sb.i_start_blk + (ino * sizeof(struct inode)) / BLOCK_SIZE;
    uint32_t offset = (ino * sizeof(struct inode)) % BLOCK_SIZE;

    char buf[BLOCK_SIZE];
    if (bio_read(blk_num, buf) < 0) {
        fprintf(stderr, "Error: Failed to read block %d for inode %d\n", blk_num, ino);
        return -1;
    }

    memcpy(inode, buf + offset, sizeof(struct inode));

    // Debugging output
    fprintf(stderr, "readi: Reading inode %d from block %d, offset %d\n", ino, blk_num, offset);
    fprintf(stderr, "readi: Inode Values: Valid=%d, Type=0x%x, Link=%d\n", inode->valid, inode->type, inode->link);

    return 0;
}

int writei(uint16_t ino, struct inode *inode) {
    if (ino >= sb.max_inum) {
        fprintf(stderr, "Error: Inode number %d is out of bounds (max: %d)\n", ino, sb.max_inum);
        return -1;
    }

    uint32_t blk_num = sb.i_start_blk + (ino * sizeof(struct inode)) / BLOCK_SIZE;
    uint32_t offset = (ino * sizeof(struct inode)) % BLOCK_SIZE;

    char buf[BLOCK_SIZE];
    if (bio_read(blk_num, buf) < 0) {
        fprintf(stderr, "Error: Failed to read block %d for inode %d\n", blk_num, ino);
        return -1; 
    }

    memcpy(buf + offset, inode, sizeof(struct inode));

    // Debugging output
    fprintf(stderr, "writei: Writing inode %d to block %d, offset %d\n", ino, blk_num, offset);
    fprintf(stderr, "writei: Inode Values: Valid=%d, Type=0x%x, Link=%d\n", inode->valid, inode->type, inode->link);

    if (bio_write(blk_num, buf) < 0) {
        fprintf(stderr, "Error: Failed to write block %d for inode %d\n", blk_num, ino);
        return -1; 
    }

    return 0; 
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)

  // Step 2: Get data block of current directory from inode

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure
    
	struct inode dir_inode;
    
    if (readi(ino, &dir_inode) < 0) {
        return -1; 
    }

    for (int i = 0; i < 16 && dir_inode.direct_ptr[i] != 0; i++) {
        char buf[BLOCK_SIZE];
        if (bio_read(dir_inode.direct_ptr[i], buf) < 0) {
            return -1; 
        }

        struct dirent *entry = (struct dirent *)buf;
        for (int j = 0; j < BLOCK_SIZE / sizeof(struct dirent); j++) {
            if (entry[j].valid && strncmp(entry[j].name, fname, name_len) == 0) {
                memcpy(dirent, &entry[j], sizeof(struct dirent));
                return 0; 
            }
        }
    }

    return -1;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

    //Changed test code to re-read the inode from disk after each dir_add() call in test code. 
    //This ensures that dir_inode in the test is always in sync with what’s on disk.

    // First, do a duplicate check with dir_find().
    // checks all existing entries across all allocated blocks.
    struct dirent existing_entry;
    if (dir_find(dir_inode.ino, fname, name_len, &existing_entry) == 0) {
        // The entry already exists in the directory
        fprintf(stderr, "dir_add: Duplicate entry '%s' found before adding.\n", fname);
        return -1;
    }

	struct dirent new_dirent = {0};
    new_dirent.ino = f_ino;
    new_dirent.valid = 1;
    strncpy(new_dirent.name, fname, NAME_LEN - 1);
    new_dirent.name[NAME_LEN - 1] = '\0';        
    new_dirent.len = (uint16_t)name_len;

    char buf[BLOCK_SIZE];
    int entries_per_block = BLOCK_SIZE / sizeof(struct dirent);

    //Try to find a free slot in existing blocks
    for (int i = 0; i < 16; i++) {
        if (dir_inode.direct_ptr[i] == 0) {
            // No more allocated blocks, break and allocate new block if needed
            break;
        }

        if (bio_read(dir_inode.direct_ptr[i], buf) < 0) {
            fprintf(stderr, "dir_add: Failed to read block %d.\n", dir_inode.direct_ptr[i]);
            return -1;
        }

        struct dirent *entry = (struct dirent *)buf;
        for (int j = 0; j < entries_per_block; j++) {
            if (!entry[j].valid) { 
                // Found a free slot in an existing block
                memset(&entry[j], 0, sizeof(struct dirent));
                memcpy(&entry[j], &new_dirent, sizeof(struct dirent));
                if (bio_write(dir_inode.direct_ptr[i], buf) < 0) {
                    fprintf(stderr, "dir_add: Failed to write updated block.\n");
                    return -1;
                }
                if (writei(dir_inode.ino, &dir_inode) < 0) {
                    fprintf(stderr, "dir_add: Failed to write updated inode.\n");
                    return -1;
                }
                fprintf(stderr, "dir_add: Entry '%s' added to existing block %d, index %d.\n",
                        fname, dir_inode.direct_ptr[i], j);
                return 0;
            }
        }
    }

    // No free slot in existing blocks, allocate a new block
    for (int i = 0; i < 16; i++) {
        if (dir_inode.direct_ptr[i] == 0) {
            if (i == 15) {
                // No more space for new blocks
                fprintf(stderr, "dir_add: Maximum directory capacity reached.\n");
                return -ENOSPC;
            }

            int new_block = get_avail_blkno();
            if (new_block < 0) {
                fprintf(stderr, "dir_add: No available data blocks.\n");
                return -1;
            }

            int abs_block = sb.d_start_blk + new_block;
            dir_inode.direct_ptr[i] = abs_block;
            dir_inode.size += BLOCK_SIZE;
            memset(buf, 0, BLOCK_SIZE);
            if (bio_write(abs_block, buf) < 0) {
                fprintf(stderr, "dir_add: Failed to write new block.\n");
                return -1;
            }

            if (writei(dir_inode.ino, &dir_inode) < 0) {
                fprintf(stderr, "dir_add: Failed to write updated inode after new block allocation.\n");
                return -1;
            }

            // Now add the entry in the newly allocated block
            if (bio_read(abs_block, buf) < 0) {
                fprintf(stderr, "dir_add: Failed to read newly allocated block %d.\n", abs_block);
                return -1;
            }

            struct dirent *entry = (struct dirent *)buf;
            // The new block is empty, so entry[0] is guaranteed free
            memset(&entry[0], 0, sizeof(struct dirent));
            memcpy(&entry[0], &new_dirent, sizeof(struct dirent));
            if (bio_write(abs_block, buf) < 0) {
                fprintf(stderr, "dir_add: Failed to write new entry to new block.\n");
                return -1;
            }

            if (writei(dir_inode.ino, &dir_inode) < 0) {
                fprintf(stderr, "dir_add: Failed to write updated inode after adding entry.\n");
                return -1;
            }

            fprintf(stderr, "dir_add: Entry '%s' added to new block %d, index 0.\n", fname, abs_block);
            return 0;
        }
    }

    fprintf(stderr, "dir_add: No space left in directory.\n");
    return -ENOSPC;
}

//skip
int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {return 0;}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

    //debug:
    printf("Resolving path: %s\n", path);
    
	if (strcmp(path, "/") == 0) {
        return readi(ino, inode);
    }

    char path_copy[PATH_MAX];
    strncpy(path_copy, path, PATH_MAX);

    char *token = strtok(path_copy, "/");
    struct inode current_inode;
    if (readi(ino, &current_inode) < 0) {
       
        return -1;
    }

    while (token != NULL) {
        struct dirent dir_entry;
        if (dir_find(current_inode.ino, token, strlen(token), &dir_entry) < 0) {
            printf("Error: '%s' not found.\n", token);
            return -1;
        }

        if (readi(dir_entry.ino, &current_inode) < 0) {
            printf("Error: Failed to read inode %d for '%s'.\n", dir_entry.ino, token);
            return -1; 
        }

        token = strtok(NULL, "/");
    }

    memcpy(inode, &current_inode, sizeof(struct inode));
    return 0;
}

/* 
 * Make file system
 */
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
    dev_init(diskfile_path);

	// write superblock information
    sb.magic_num = MAGIC_NUM;
    sb.max_inum = MAX_INUM;
    sb.max_dnum = MAX_DNUM;
    sb.i_bitmap_blk = 1;
    sb.d_bitmap_blk = 2;
    sb.i_start_blk = 3;
    sb.d_start_blk = sb.i_start_blk + (MAX_INUM * sizeof(struct inode)) / BLOCK_SIZE;

    char buffer[BLOCK_SIZE] = {0};
    memcpy(buffer, &sb, sizeof(sb));
    bio_write(0, buffer); // Write superblock to block 0

	// initialize inode bitmap
    // initialize data block bitmap
    bitmap_t inode_bitmap = calloc(1, BLOCK_SIZE);
    bitmap_t data_bitmap = calloc(1, BLOCK_SIZE);
    bio_write(sb.i_bitmap_blk, inode_bitmap); // Write empty inode bitmap
    //debug
    fprintf(stderr, "Data Block Bitmap Initialized: %02x\n", data_bitmap[0]);
    bio_write(sb.d_bitmap_blk, data_bitmap); // Write empty data block bitmap

    //initializing root directory inode
    struct inode root_inode;
    root_inode.ino = 0;
    root_inode.valid = 1;
    root_inode.size = 0;
    root_inode.type = S_IFDIR;
    root_inode.link = 2; // "." and ".."
    memset(root_inode.direct_ptr, 0, sizeof(root_inode.direct_ptr));
    memset(root_inode.indirect_ptr, 0, sizeof(root_inode.indirect_ptr));

    //mark inode 0
    set_bitmap(inode_bitmap, 0);
    bio_write(sb.i_bitmap_blk, inode_bitmap); // Update inode bitmap on disk
    writei(0, &root_inode); // Write root inode to disk

    free(inode_bitmap);
    free(data_bitmap);


	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs

  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk

    // Attempt to open the disk file
    if (dev_open(diskfile_path) < 0) {
        fprintf(stderr, "Disk file not found. Creating a new file system...\n");
        if (rufs_mkfs() < 0) {
            fprintf(stderr, "Error: Failed to create a new file system\n");
            return NULL;
        }
    } else {
        fprintf(stderr, "Disk file found. Initializing file system...\n");

        // Read the superblock from disk
        char buffer[BLOCK_SIZE];
        if (bio_read(0, buffer) < 0) {
            fprintf(stderr, "Error: Failed to read superblock\n");
            return NULL;
        }
        memcpy(&sb, buffer, sizeof(struct superblock));

        // Validate the superblock magic number
        if (sb.magic_num != MAGIC_NUM) {
            fprintf(stderr, "Error: Invalid magic number in superblock\n");
            return NULL;
        }

        // Debugging output
        fprintf(stderr, "rufs_init: Superblock loaded:\n");

        // Validate and debug data block bitmap
        char bitmap_buf[BLOCK_SIZE];
        if (bio_read(sb.d_bitmap_blk, bitmap_buf) < 0) {
            fprintf(stderr, "Error: Failed to read data block bitmap from disk.\n");
            return NULL;
        }
        fprintf(stderr, "Data Block Bitmap Loaded: %02x\n", bitmap_buf[0]);

    }

    return NULL;
}

static void rufs_destroy(void *userdata) {
    char buffer[BLOCK_SIZE];
    struct inode inode_buffer;

    for (int i = 0; i < sb.max_inum; i++) {
        uint32_t blk_num = sb.i_start_blk + (i * sizeof(struct inode)) / BLOCK_SIZE;
        uint32_t offset = (i * sizeof(struct inode)) % BLOCK_SIZE;

        if (bio_read(blk_num, buffer) < 0) {
            fprintf(stderr, "rufs_destroy: Failed to read inode block %d\n", blk_num);
            continue;
        }

        memcpy(&inode_buffer, buffer + offset, sizeof(struct inode));
        if (inode_buffer.valid) {
            if (inode_buffer.indirect_ptr[0] != 0) {
                char indirect_block_buf[BLOCK_SIZE];
                if (bio_read(inode_buffer.indirect_ptr[0], indirect_block_buf) < 0) {
                    fprintf(stderr, "rufs_destroy: Failed to read indirect block %d\n", inode_buffer.indirect_ptr[0]);
                } else {
                    memset(indirect_block_buf, 0, BLOCK_SIZE);
                    bio_write(inode_buffer.indirect_ptr[0], indirect_block_buf); 
                }
            }
        }
    }

    if (bio_read(sb.d_bitmap_blk, buffer) < 0) {
        fprintf(stderr, "rufs_destroy: Failed to read data block bitmap\n");
    } else {
        memset(buffer, 0, BLOCK_SIZE);
        bio_write(sb.d_bitmap_blk, buffer);
    }

    if (bio_read(sb.i_bitmap_blk, buffer) < 0) {
        fprintf(stderr, "rufs_destroy: Failed to read inode bitmap\n");
    } else {
        memset(buffer, 0, BLOCK_SIZE);
        bio_write(sb.i_bitmap_blk, buffer);
    }

    dev_close();
    fprintf(stderr, "rufs_destroy: All resources de-allocated and disk file closed.\n");
}


static int rufs_getattr(const char *path, struct stat *stbuf) {
    struct inode inode;

    // Step 1: Call get_node_by_path() to get inode from path
    if (get_node_by_path(path, 0, &inode) < 0) {
        fprintf(stderr, "rufs_getattr: Failed to find inode for path %s\n", path);
        return -1;
    }

    // Step 2: Fill attributes of file into stbuf from inode
    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_ino = inode.ino;
    stbuf->st_mode = inode.type;
    stbuf->st_nlink = inode.link;
    stbuf->st_size = inode.size;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_blksize = BLOCK_SIZE;
    stbuf->st_blocks = (inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    stbuf->st_mtime = time(NULL);

    fprintf(stderr, "rufs_getattr: Path %s attributes filled. Inode: %d, Type: %d, Size: %ld\n",
            path, inode.ino, inode.type, inode.size);

    return 0;
}


static int rufs_opendir(const char *path, struct fuse_file_info *fi) {
    struct inode inode;

    // Step 1: Call get_node_by_path() to get inode from path
    if (get_node_by_path(path, 0, &inode) < 0) {
        fprintf(stderr, "rufs_opendir: Failed to find inode for path %s\n", path);
        return -1;
    }

    if ((inode.type & S_IFDIR) == 0) {
        fprintf(stderr, "rufs_opendir: Path %s is not a directory\n", path);
        return -1;
    }

    return 0;
}


static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    struct inode dir_inode;

    printf("Resolving path: %s\n", path);

    // Step 1: Call get_node_by_path() to get inode from path
    if (get_node_by_path(path, 0, &dir_inode) < 0) {
        fprintf(stderr, "Error: Path not found\n");
        return -ENOENT;
    }

    if ((dir_inode.type & S_IFDIR) == 0) {
        fprintf(stderr, "Error: Not a directory\n");
        return -ENOTDIR;
    }

    // Step 2: Read directory entries from its data blocks
    for (int i = 0; i < 16 && dir_inode.direct_ptr[i] != 0; i++) {
        char block[BLOCK_SIZE];
        memset(block, 0, BLOCK_SIZE);

        printf("Reading block %d\n", dir_inode.direct_ptr[i]);

        if (bio_read(dir_inode.direct_ptr[i], block) < 0) {
            fprintf(stderr, "Error reading block %d\n", dir_inode.direct_ptr[i]);
            return -EIO;
        }

        struct dirent *entry = (struct dirent *)block;
        int entries_per_block = BLOCK_SIZE / sizeof(struct dirent);

        for (int j = 0; j < entries_per_block; j++) {
            if (entry[j].valid) {
                printf("Found valid entry: %s\n", entry[j].name);

                // Use the filler function to add the entry to the buffer
                if (filler(buffer, entry[j].name, NULL, 0) != 0) {
                    fprintf(stderr, "Error: Buffer overflow\n");
                    return -ENOMEM;
                }
            }
        }
    }

    printf("readdir completed successfully\n");
    return 0;
}



static int rufs_mkdir(const char *path, mode_t mode) {
    char parent_path[PATH_MAX];
    char dir_name[NAME_LEN];
    struct inode parent_inode;

    // Step 1: Use dirname() and basename() to separate parent directory path and target directory name
    strncpy(parent_path, path, PATH_MAX);
    strncpy(dir_name, path, NAME_LEN);
    char *parent_dir = dirname(parent_path);
    char *base_name = basename(dir_name);

    // Step 2: Call get_node_by_path() to get inode of parent directory
    if (get_node_by_path(parent_dir, 0, &parent_inode) < 0) {
        return -1;
    }

    // Ensure the parent inode is a directory
    if ((parent_inode.type & S_IFDIR) == 0) {
        return -1;
    }

    // Step 3: Call get_avail_ino() to get an available inode number
    int new_ino = get_avail_ino();
    if (new_ino < 0) {
        return -1; 
    }

    // Step 4: Call dir_add() to add directory entry of target directory to parent directory
    if (dir_add(parent_inode, new_ino, base_name, strlen(base_name)) < 0) {
        return -1;
    }

    // Step 5: Update inode for target directory
    struct inode new_dir_inode;
    memset(&new_dir_inode, 0, sizeof(struct inode));
    new_dir_inode.ino = new_ino;
    new_dir_inode.valid = 1;
    new_dir_inode.size = 0;
    new_dir_inode.type = S_IFDIR | mode;
    new_dir_inode.link = 2;
    memset(new_dir_inode.direct_ptr, 0, sizeof(new_dir_inode.direct_ptr));
    memset(new_dir_inode.indirect_ptr, 0, sizeof(new_dir_inode.indirect_ptr));

    // Step 6: Call writei() to write inode to disk
    if (writei(new_ino, &new_dir_inode) < 0) {
        return -1;
    }

    return 0;
}


//skip
static int rufs_rmdir(const char *path) {return 0;}
static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {return 0;}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char parent_path[PATH_MAX];
    char file_name[NAME_LEN];
    struct inode parent_inode;

    // Step 1: Use dirname() and basename() to separate parent directory path and target file name
    strncpy(parent_path, path, PATH_MAX);
    strncpy(file_name, path, NAME_LEN);
    char *parent_dir = dirname(parent_path);
    char *base_name = basename(file_name);

    // Step 2: Call get_node_by_path() to get inode of parent directory
    if (get_node_by_path(parent_dir, 0, &parent_inode) < 0) {
        fprintf(stderr, "rufs_create: Parent directory %s not found\n", parent_dir);
        return -1;
    }

    // Ensure the parent inode is a directory
    if ((parent_inode.type & S_IFDIR) == 0) {
        fprintf(stderr, "rufs_create: Parent path %s is not a directory\n", parent_dir);
        return -1;
    }

    // Step 3: Call get_avail_ino() to get an available inode number
    int new_ino = get_avail_ino();
    if (new_ino < 0) {
        return -1;
    }

    // Step 4: Call dir_add() to add directory entry of target file to parent directory
    if (dir_add(parent_inode, new_ino, base_name, strlen(base_name)) < 0) {
        return -1;
    }

    // Step 5: Update inode for target file
    struct inode new_file_inode;
    memset(&new_file_inode, 0, sizeof(struct inode));
    new_file_inode.ino = new_ino;
    new_file_inode.valid = 1;
    new_file_inode.size = 0;
    new_file_inode.type = S_IFREG | mode;
    new_file_inode.link = 1;
    memset(new_file_inode.direct_ptr, 0, sizeof(new_file_inode.direct_ptr));
    memset(new_file_inode.indirect_ptr, 0, sizeof(new_file_inode.indirect_ptr));

    // Step 6: Call writei() to write inode to disk
    if (writei(new_ino, &new_file_inode) < 0) {
        return -1;
    }

    return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {
    struct inode file_inode;

    // Step 1: Call get_node_by_path() to get inode from path
    if (get_node_by_path(path, 0, &file_inode) < 0) {
        return -1;
    }

    if ((file_inode.type & S_IFREG) == 0) {
        return -1; 
    }

    return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct inode file_inode;

    // Step 1: Call get_node_by_path() to get inode from path
    if (get_node_by_path(path, 0, &file_inode) < 0) {
        fprintf(stderr, "Error: Invalid path %s\n", path);
        return -1;
    }

    fprintf(stderr, "[DEBUG] rufs_read: Inode info - ino: %d, size: %lu, type: %x\n",
            file_inode.ino, file_inode.size, file_inode.type);

    // Ensure the inode represents a regular file
    if ((file_inode.type & S_IFREG) == 0) {
        fprintf(stderr, "Error: Path %s is not a regular file\n", path);
        return -1;
    }

    // Validate offset
    if (offset >= file_inode.size) {
        fprintf(stderr, "[DEBUG] rufs_read: Offset %lu beyond file size %lu\n", offset, file_inode.size);
        return 0; // No data to read
    }

    // Step 2: Based on size and offset, read its data blocks from disk
    size_t bytes_read = 0;
    size_t bytes_to_read = size;
    off_t current_offset = offset;
    char block_buf[BLOCK_SIZE];

    for (int i = 0; i < 16 && bytes_to_read > 0 && current_offset < file_inode.size; i++) {
        if (file_inode.direct_ptr[i] == 0) {
            fprintf(stderr, "[DEBUG] rufs_read: Direct pointer %d is 0, stopping read.\n", i);
            break;
        }

        int block_no = file_inode.direct_ptr[i];
        fprintf(stderr, "[DEBUG] rufs_read: Reading block %d for direct pointer %d\n", block_no, i);

        if (bio_read(block_no, block_buf) < 0) {
            fprintf(stderr, "Error: Failed to read block %d\n", block_no);
            return -1;
        }

        size_t block_offset = current_offset % BLOCK_SIZE;
        size_t available_bytes = BLOCK_SIZE - block_offset;
        size_t bytes_to_copy = (bytes_to_read < available_bytes) ? bytes_to_read : available_bytes;

        memcpy(buffer + bytes_read, block_buf + block_offset, bytes_to_copy);

        fprintf(stderr, "[DEBUG] rufs_read: Copied %lu bytes from block %d at offset %lu\n",
                bytes_to_copy, block_no, block_offset);

        bytes_read += bytes_to_copy;
        bytes_to_read -= bytes_to_copy;
        current_offset += bytes_to_copy;
    }

    // Step 3: Return the number of bytes read
    fprintf(stderr, "[DEBUG] rufs_read: Total bytes read %lu\n", bytes_read);
    return bytes_read;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct inode file_inode;

    if (get_node_by_path(path, 0, &file_inode) < 0) {
        fprintf(stderr, "Error: Invalid path %s\n", path);
        return -1;
    }

    fprintf(stderr, "[DEBUG] rufs_write: Inode info - ino: %d, size: %lu, type: %x\n",
            file_inode.ino, file_inode.size, file_inode.type);

    if ((file_inode.type & S_IFREG) == 0) {
        fprintf(stderr, "Error: Path %s is not a regular file\n", path);
        return -1;
    }

    size_t bytes_written = 0;
    size_t bytes_to_write = size;
    off_t current_offset = offset;
    char block_buf[BLOCK_SIZE];

    for (int i = 0; i < 16 && bytes_to_write > 0; i++) {
        int block_no;

        if (current_offset >= BLOCK_SIZE) {
            current_offset -= BLOCK_SIZE;
            continue;
        }

        if (file_inode.direct_ptr[i] == 0) {
            block_no = get_avail_blkno();
            if (block_no < 0) {
                fprintf(stderr, "Error: No available blocks for writing.\n");
                return -1;
            }
            file_inode.direct_ptr[i] = block_no;
            fprintf(stderr, "[DEBUG] rufs_write: Allocated new block %d for direct pointer %d\n", block_no, i);
        } else {
            block_no = file_inode.direct_ptr[i];
        }

        fprintf(stderr, "[DEBUG] rufs_write: Writing to block %d for direct pointer %d\n", block_no, i);

        if (bio_read(block_no, block_buf) < 0) {
            fprintf(stderr, "Error: Failed to read block %d before writing\n", block_no);
            return -1;
        }

        size_t block_offset = current_offset % BLOCK_SIZE;
        size_t available_space = BLOCK_SIZE - block_offset;
        size_t bytes_to_copy = (bytes_to_write < available_space) ? bytes_to_write : available_space;

        memcpy(block_buf + block_offset, buffer + bytes_written, bytes_to_copy);

        fprintf(stderr, "[DEBUG] rufs_write: Copied %lu bytes to block %d at offset %lu\n",
                bytes_to_copy, block_no, block_offset);

        if (bio_write(block_no, block_buf) < 0) {
            fprintf(stderr, "Error: Failed to write block %d\n", block_no);
            return -1;
        }

        bytes_written += bytes_to_copy;
        bytes_to_write -= bytes_to_copy;
        current_offset += bytes_to_copy;
    }

    file_inode.size = (offset + bytes_written > file_inode.size) ? offset + bytes_written : file_inode.size;

    fprintf(stderr, "[DEBUG] rufs_write: Updated inode size to %lu\n", file_inode.size);

    if (writei(file_inode.ino, &file_inode) < 0) {
        fprintf(stderr, "Error: Failed to write inode %d\n", file_inode.ino);
        return -1;
    }

    fprintf(stderr, "[DEBUG] rufs_write: Total bytes written %lu\n", bytes_written);
    return bytes_written;
}


//skip
static int rufs_unlink(const char *path) {return 0;}
static int rufs_truncate(const char *path, off_t size) {return 0;}
static int rufs_release(const char *path, struct fuse_file_info *fi) {return 0;}
static int rufs_flush(const char * path, struct fuse_file_info * fi) {return 0;}
static int rufs_utimens(const char *path, const struct timespec tv[2]) {return 0;}

static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};

//testing
//keep this in cause some tests use it
void clear_bitmap(bitmap_t bitmap, int index) {
    bitmap[index / 8] &= ~(1 << (index % 8));
}

//testing helper
void debug_bitmap(const char *msg, bitmap_t bitmap, int size) {
    printf("%s: ", msg);
    for (int i = 0; i < size; i++) {
        printf("%02x ", bitmap[i]);
    }
    printf("\n");
}

//helper for tests. used in each test to initialize file sys
void initialize_test_fs() {
    strcpy(diskfile_path, "./DISKFILE");
    dev_close(); // Close any existing filesystem
    rufs_mkfs(); // Create a fresh filesystem
}

void test_rufs_readdir() {
    printf("Testing rufs_readdir...\n");

    initialize_test_fs();

    // Create a directory
    if (rufs_mkdir("/testdir", 0755) < 0) {
        fprintf(stderr, "Test failed: Unable to create /testdir.\n");
        return;
    }

    // Read the root directory
    char buf[1024];
    if (rufs_readdir("/", buf, NULL, 0, NULL) < 0) {
        fprintf(stderr, "Test failed: Unable to read root directory.\n");
        return;
    }

    printf("Test passed: rufs_readdir read root directory successfully.\n");
}

void test_rufs_read_write() {
    printf("Testing rufs_read and rufs_write...\n");

    initialize_test_fs();

    // Create a file
    if (rufs_create("/testfile", 0644, NULL) < 0) {
        fprintf(stderr, "Test failed: Unable to create /testfile.\n");
        return;
    }

    // Write to the file
    const char *data = "Hello, World!";
    if (rufs_write("/testfile", data, strlen(data), 0, NULL) < 0) {
        fprintf(stderr, "Test failed: Unable to write to /testfile.\n");
        return;
    }

    // Read back the file
    char buffer[64];
    memset(buffer, 0, sizeof(buffer));
    if (rufs_read("/testfile", buffer, strlen(data), 0, NULL) < 0) {
        fprintf(stderr, "Test failed: Unable to read from /testfile.\n");
        return;
    }

    if (strcmp(buffer, data) != 0) {
        fprintf(stderr, "Test failed: Data mismatch. Expected '%s', got '%s'.\n", data, buffer);
        return;
    }

    printf("Test passed: rufs_read and rufs_write worked successfully.\n");
}

// Conditional Main Function
// when testing, run:
// make CFLAGS="-D_FILE_OFFSET_BITS=64 -DTEST_MODE"
//./rufs
#ifdef TEST_MODE
int main() {
    //test_rufs_mkfs();
    //test_rufs_init();
    //test_rufs_destroy();
    //test_get_avail_ino();
    //test_get_avail_ino_advanced();
    //test_get_avail_blkno();
    //test_get_avail_blkno_advanced();
    //test_readi();
    //test_writei();
    //test_dir_find();
    //test_dir_find_advanced();
    //test_dir_add();
    //test_get_node_by_path();
    //test_dir_find_only();
    //test_dir_add_basic();
    //test_dir_add_multiple();
    //test_dir_add_duplicate();
    //test_rufs_destroy();
    //test_rufs_destroy();
    //test_rufs_getattr();
    //test_rufs_opendir();
    //test_rufs_readdir();
    //test_rufs_mkdir();
    //test_rufs_open();
    test_rufs_read_write(); 

    return 0;
}
#else
//when running benchmarks, just do make 
int main(int argc, char *argv[]) {
    int fuse_stat;

    getcwd(diskfile_path, PATH_MAX);
    strcat(diskfile_path, "/DISKFILE");

    fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

    return fuse_stat;
}
#endif