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

	struct dirent new_dirent = {0};
    new_dirent.ino = f_ino;
    new_dirent.valid = 1;
    strncpy(new_dirent.name, fname, NAME_LEN - 1);
    new_dirent.name[NAME_LEN - 1] = '\0';        
    new_dirent.len = name_len;
//undo

    for (int i = 0; i < 16; i++) {
        char buf[BLOCK_SIZE];

        if (dir_inode.direct_ptr[i] == 0) {
            if (i == 15) {
                // If the last block is reached and no space, directory is full
                fprintf(stderr, "dir_add: Maximum directory capacity reached.\n");
                return -ENOSPC;
            }

            int new_block = get_avail_blkno();
            if (new_block < 0) {
                fprintf(stderr, "dir_add: No available data blocks.\n");
                return -1;
            }

            dir_inode.direct_ptr[i] = new_block;
            dir_inode.size += BLOCK_SIZE;
            memset(buf, 0, BLOCK_SIZE);
            bio_write(new_block, buf);
        }

        if (bio_read(dir_inode.direct_ptr[i], buf) < 0) {
            fprintf(stderr, "dir_add: Failed to read block %d.\n", dir_inode.direct_ptr[i]);
            return -1;
        }

        struct dirent *entry = (struct dirent *)buf;
        for (int j = 0; j < BLOCK_SIZE / sizeof(struct dirent); j++) {
            //debug:
            fprintf(stderr, "dir_add: Checking entry[%d]: valid=%d, name='%s', ino=%d\n",
                    j, entry[j].valid, entry[j].valid ? entry[j].name : "NULL", entry[j].ino);
            //dup check
            if (entry[j].valid && strncmp(entry[j].name, fname, NAME_LEN) == 0 && entry[j].len == name_len) {
                fprintf(stderr, "dir_add: Duplicate entry '%s' found.\n", fname);
                return -1; // Duplicate entry detected
            }
            if (!entry[j].valid) { //if empty entry found, add new entry here
                memset(&entry[j], 0, sizeof(struct dirent));
                memcpy(&entry[j], &new_dirent, sizeof(struct dirent));
                bio_write(dir_inode.direct_ptr[i], buf);
                writei(dir_inode.ino, &dir_inode);
                fprintf(stderr, "dir_add: Entry '%s' added to block %d, index %d.\n",
                        fname, dir_inode.direct_ptr[i], j);
                return 0; //success
            }

        }
    }

    fprintf(stderr, "dir_add: No space left in directory.\n");
    return -ENOSPC;
}

//skip
int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk


}

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
        fprintf(stderr, "get_node_by_path: Failed to read inode %d.\n", current_ino);
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

     fprintf(stderr, "Destroying the file system and cleaning up resources...\n");
	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile

    dev_close(); 
    fprintf(stderr, "Disk file closed successfully.\n");


}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

    return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	

	return 0;
}

static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


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

void test_dir_add() {
    printf("Testing dir_add...\n");

    // Initialize the file system
    strcpy(diskfile_path, "./DISKFILE");
    dev_close(); // Close any existing filesystem
    rufs_mkfs(); // Force a fresh filesystem

    struct inode dir_inode = {0};

    // Initialize a directory inode (simulating a root directory)
    dir_inode.ino = 1;
    dir_inode.valid = 1;
    dir_inode.size = 0;
    dir_inode.type = S_IFDIR;
    memset(dir_inode.direct_ptr, 0, sizeof(dir_inode.direct_ptr));

    // Write the directory inode to disk
    if (writei(dir_inode.ino, &dir_inode) < 0) {
        fprintf(stderr, "Test failed: Unable to write initial directory inode.\n");
        return;
    }

    // Add entries to the directory
    const char *file1 = "file1";
    const char *file2 = "file2";
    const char *duplicate = "file1";

    if (dir_add(dir_inode, 2, file1, strlen(file1)) < 0) {
        fprintf(stderr, "Test failed: Unable to add 'file1'.\n");
        return;
    }
    printf("Added 'file1' to directory.\n");

    if (dir_add(dir_inode, 3, file2, strlen(file2)) < 0) {
        fprintf(stderr, "Test failed: Unable to add 'file2'.\n");
        return;
    }
    printf("Added 'file2' to directory.\n");

    // Attempt to add a duplicate entry
    if (dir_add(dir_inode, 4, duplicate, strlen(duplicate)) == 0) {
        fprintf(stderr, "Test failed: Duplicate entry 'file1' was added.\n");
        return;
    }
    printf("Correctly handled duplicate entry for 'file1'.\n");

    // Fill up the directory based on entry capacity
    int max_entries = (BLOCK_SIZE / sizeof(struct dirent)) * 16; // Adjust for multiple blocks
    int i;
    for (i = 4; i <= max_entries; i++) {
        char filename[10];
        snprintf(filename, sizeof(filename), "file%d", i);
        if (dir_add(dir_inode, i, filename, strlen(filename)) < 0) {
            printf("Directory full after %d entries.\n", i - 3); // Adjusting for the already added entries
            break;
        }
    }

    if (i > max_entries) {
        fprintf(stderr, "Test failed: Directory did not report full capacity as expected.\n");
        return;
    }

    // Verify final inode state
    if (readi(dir_inode.ino, &dir_inode) < 0) {
        fprintf(stderr, "Test failed: Unable to read directory inode after operations.\n");
        return;
    }

    printf("Final directory size: %d bytes.\n", dir_inode.size);
    printf("Test passed: dir_add behaves correctly.\n");
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
    test_dir_add();
    //test_get_node_by_path();
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