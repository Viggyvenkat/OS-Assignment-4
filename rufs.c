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
        return -1;
    }

    for (int i = 0; i < MAX_INUM; i++) {
        if (!get_bitmap((bitmap_t)buf, i)) { 
            set_bitmap((bitmap_t)buf, i);

            if (bio_write(sb.i_bitmap_blk, buf) < 0) {
                return -1;
            }

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
        return -1; 
    }

    for (int i = 0; i < MAX_DNUM; i++) {
        if (!get_bitmap((bitmap_t)buf, i)) {
            set_bitmap((bitmap_t)buf, i);

            if (bio_write(sb.d_bitmap_blk, buf) < 0) {
                return -1;
            }

            return i;
        }
    }
    return -1;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {
    if (ino >= sb.max_inum) {
        return -1;
    }

    uint32_t blk_num = sb.i_start_blk + (ino * sizeof(struct inode)) / BLOCK_SIZE;
    uint32_t offset = (ino * sizeof(struct inode)) % BLOCK_SIZE;

    char buf[BLOCK_SIZE];
    if (bio_read(blk_num, buf) < 0) {
        return -1;
    }

    memcpy(inode, buf + offset, sizeof(struct inode));
    return 0;
}

int writei(uint16_t ino, struct inode *inode) {
    if (ino >= sb.max_inum) {
        return -1;
    }

    uint32_t blk_num = sb.i_start_blk + (ino * sizeof(struct inode)) / BLOCK_SIZE;
    uint32_t offset = (ino * sizeof(struct inode)) % BLOCK_SIZE;

    char buf[BLOCK_SIZE];
    if (bio_read(blk_num, buf) < 0) {
        return -1; 
    }

    memcpy(buf + offset, inode, sizeof(struct inode));

    if (bio_write(blk_num, buf) < 0) {
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

    struct dirent existing_entry;
    if (dir_find(dir_inode.ino, fname, name_len, &existing_entry) == 0) {
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

    for (int i = 0; i < 16; i++) {
        if (dir_inode.direct_ptr[i] == 0) {
            break;
        }

        if (bio_read(dir_inode.direct_ptr[i], buf) < 0) {
            return -1;
        }

        struct dirent *entry = (struct dirent *)buf;
        for (int j = 0; j < entries_per_block; j++) {
            if (!entry[j].valid) { 
                memset(&entry[j], 0, sizeof(struct dirent));
                memcpy(&entry[j], &new_dirent, sizeof(struct dirent));
                if (bio_write(dir_inode.direct_ptr[i], buf) < 0) {
                    return -1;
                }
                if (writei(dir_inode.ino, &dir_inode) < 0) {
                    return -1;
                }
                return 0;
            }
        }
    }

    for (int i = 0; i < 16; i++) {
        if (dir_inode.direct_ptr[i] == 0) {
            if (i == 15) {
                return -1;
            }

            int new_block = get_avail_blkno();
            if (new_block < 0) {
                return -1;
            }

            int abs_block = sb.d_start_blk + new_block;
            dir_inode.direct_ptr[i] = abs_block;
            dir_inode.size += BLOCK_SIZE;
            memset(buf, 0, BLOCK_SIZE);
            if (bio_write(abs_block, buf) < 0) {
                return -1;
            }

            if (writei(dir_inode.ino, &dir_inode) < 0) {
                return -1;
            }

            if (bio_read(abs_block, buf) < 0) {
                return -1;
            }

            struct dirent *entry = (struct dirent *)buf;
            memset(&entry[0], 0, sizeof(struct dirent));
            memcpy(&entry[0], &new_dirent, sizeof(struct dirent));
            if (bio_write(abs_block, buf) < 0) {
                return -1;
            }
            if (writei(dir_inode.ino, &dir_inode) < 0) {
                return -1;
            }
            return 0;
        }
    }

    return -1;
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
    bio_write(0, buffer);

    bitmap_t inode_bitmap = calloc(1, BLOCK_SIZE);
    bitmap_t data_bitmap = calloc(1, BLOCK_SIZE);
    bio_write(sb.i_bitmap_blk, inode_bitmap);
    bio_write(sb.d_bitmap_blk, data_bitmap);

    //initializing root directory inode
    struct inode root_inode;
    root_inode.ino = 0;
    root_inode.valid = 1;
    root_inode.size = 0;
    root_inode.type = S_IFDIR;
    root_inode.link = 2;
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
        if (rufs_mkfs() < 0) {
            return NULL;
        }
    } else {
        char buffer[BLOCK_SIZE];
        if (bio_read(0, buffer) < 0) {
            return NULL;
        }
        memcpy(&sb, buffer, sizeof(struct superblock));

        if (sb.magic_num != MAGIC_NUM) {
            return NULL;
        }

        char bitmap_buf[BLOCK_SIZE];
        if (bio_read(sb.d_bitmap_blk, bitmap_buf) < 0) {
            return NULL;
        }
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
            continue;
        }

        memcpy(&inode_buffer, buffer + offset, sizeof(struct inode));
        if (inode_buffer.valid) {
            if (inode_buffer.indirect_ptr[0] != 0) {
                char indirect_block_buf[BLOCK_SIZE];
                if (bio_read(inode_buffer.indirect_ptr[0], indirect_block_buf) < 0) {
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
}

static int rufs_getattr(const char *path, struct stat *stbuf) {
    struct inode inode;

    // Step 1: Call get_node_by_path() to get inode from path
    if (get_node_by_path(path, 0, &inode) < 0) {
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

    return 0;
}


static int rufs_opendir(const char *path, struct fuse_file_info *fi) {
    struct inode inode;

    if (get_node_by_path(path, 0, &inode) < 0) {
        return -1;
    }

    if ((inode.type & S_IFDIR) == 0) {
        return -1;
    }

    return 0;
}


static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    struct inode dir_inode;

    // Step 1: Call get_node_by_path() to get inode from path
    if (get_node_by_path(path, 0, &dir_inode) < 0) {
        return -1;
    }

    if ((dir_inode.type & S_IFDIR) == 0) {
        return -ENOTDIR;
    }

    filler(buffer, ".", NULL, 0);
    filler(buffer, "..", NULL, 0);
    for (int i = 0; i < 16 && dir_inode.direct_ptr[i] != 0; i++) {
        char block_buf[BLOCK_SIZE];
        if (bio_read(dir_inode.direct_ptr[i], block_buf) < 0) {
            return -1; 
        }

        struct dirent *entry = (struct dirent *)block_buf;
        int entries_per_block = BLOCK_SIZE / sizeof(struct dirent);

        // Iterate through the directory entries in this block
        for (int j = 0; j < entries_per_block; j++) {
            if (entry[j].valid == 1) {
                filler(buffer, entry[j].name, NULL, 0);
            }
        }
    }

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

    if (get_node_by_path(parent_dir, 0, &parent_inode) < 0) {
        return -1;
    }

    if ((parent_inode.type & S_IFDIR) == 0) {
        return -1;
    }

    int new_ino = get_avail_ino();
    if (new_ino < 0) {
        return -1; 
    }

    if (dir_add(parent_inode, new_ino, base_name, strlen(base_name)) < 0) {
        return -1;
    }

    struct inode new_dir_inode;
    memset(&new_dir_inode, 0, sizeof(struct inode));
    new_dir_inode.ino = new_ino;
    new_dir_inode.valid = 1;
    new_dir_inode.size = 0;
    new_dir_inode.type = S_IFDIR | mode;
    new_dir_inode.link = 2;
    memset(new_dir_inode.direct_ptr, 0, sizeof(new_dir_inode.direct_ptr));
    memset(new_dir_inode.indirect_ptr, 0, sizeof(new_dir_inode.indirect_ptr));

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

    strncpy(parent_path, path, PATH_MAX);
    strncpy(file_name, path, NAME_LEN);
    char *parent_dir = dirname(parent_path);
    char *base_name = basename(file_name);

    if (get_node_by_path(parent_dir, 0, &parent_inode) < 0) {
        return -1;
    }

    if ((parent_inode.type & S_IFDIR) == 0) {
        return -1;
    }

    int new_ino = get_avail_ino();
    if (new_ino < 0) {
        return -1;
    }

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

    fprintf(stderr, "[DEBUG] rufs_read: Inode info - ino: %d, size: %u, type: %x\n",
            file_inode.ino, file_inode.size, file_inode.type);

    // Ensure the inode represents a regular file
    if ((file_inode.type & S_IFREG) == 0) {
        fprintf(stderr, "Error: Path %s is not a regular file\n", path);
        return -1;
    }

    // Validate offset
    if (offset >= file_inode.size) {
        fprintf(stderr, "[DEBUG] rufs_read: Offset %lu beyond file size %u\n", offset, file_inode.size);
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

    fprintf(stderr, "[DEBUG] rufs_write: Inode info - ino: %d, size: %u, type: %x\n",
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

    fprintf(stderr, "[DEBUG] rufs_write: Updated inode size to %u\n", file_inode.size);

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
// A simple mock filler that stores directory names in a global array
#define MAX_TEST_ENTRIES 128
static const char *dir_entries[MAX_TEST_ENTRIES];
static int entry_count = 0;

int test_filler(void *buf, const char *name, const struct stat *st, off_t off) {
    if (entry_count < MAX_TEST_ENTRIES) {
        dir_entries[entry_count++] = strdup(name); // copy entry name
    }
    return 0;
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
    //test_rufs_read_write(); 
    test_rufs_readdir_multiple_entries();

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