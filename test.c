#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "block.h"
#include "rufs.h"

//testing fucntions stored here after use
void test_rufs_mkfs() {
    // Initialize the diskfile_path global variable
    strcpy(diskfile_path, "./DISKFILE");

    // Step 1: Call rufs_mkfs
    printf("Testing rufs_mkfs...\n");
    rufs_mkfs();

    // Step 2: Open the disk and verify the superblock
    dev_open(diskfile_path);

    char buffer[BLOCK_SIZE];
    bio_read(0, buffer);  // Read block 0 (superblock)
    struct superblock *sb = (struct superblock *)buffer;

    printf("Superblock Values:\n");
    printf("  Magic Number: 0x%x\n", sb->magic_num);
    printf("  Max Inodes: %d\n", sb->max_inum);
    printf("  Max Data Blocks: %d\n", sb->max_dnum);
    printf("  Inode Bitmap Block: %d\n", sb->i_bitmap_blk);
    printf("  Data Bitmap Block: %d\n", sb->d_bitmap_blk);
    printf("  Inode Start Block: %d\n", sb->i_start_blk);
    printf("  Data Start Block: %d\n", sb->d_start_blk);

    // Step 3: Verify inode bitmap
    bio_read(sb->i_bitmap_blk, buffer);
    printf("Inode Bitmap First Byte: 0x%x\n", buffer[0]);  // Should show 0x01 (inode 0 marked as used)

    // Step 4: Verify data block bitmap
    bio_read(sb->d_bitmap_blk, buffer);
    printf("Data Bitmap First Byte: 0x%x\n", buffer[0]);  // Should show 0x00 (no data blocks used)

    // Step 5: Directly read the root inode
    int inode_block = sb->i_start_blk + (0 * sizeof(struct inode)) / BLOCK_SIZE;
    int inode_offset = (0 * sizeof(struct inode)) % BLOCK_SIZE;

    bio_read(inode_block, buffer);
    struct inode *root_inode = (struct inode *)(buffer + inode_offset);

    printf("Root Inode Values:\n");
    printf("  Inode Number: %d\n", root_inode->ino);
    printf("  Valid: %d\n", root_inode->valid);
    printf("  Type: 0x%x\n", root_inode->type);
    printf("  Link Count: %d\n", root_inode->link);

    dev_close();  // Clean up
}
//RUFS_MKFS WORKS

void test_rufs_init() {
    // Initialize the diskfile_path global variable
    strcpy(diskfile_path, "./DISKFILE");

    printf("Testing rufs_init...\n");

    // Call rufs_init to initialize the file system
    rufs_init(NULL);

    // Check if the superblock is loaded correctly
    printf("Superblock Values After Initialization:\n");
    printf("  Magic Number: 0x%x\n", sb.magic_num);
    printf("  Max Inodes: %d\n", sb.max_inum);
    printf("  Max Data Blocks: %d\n", sb.max_dnum);
    printf("  Inode Bitmap Block: %d\n", sb.i_bitmap_blk);
    printf("  Data Bitmap Block: %d\n", sb.d_bitmap_blk);
    printf("  Inode Start Block: %d\n", sb.i_start_blk);
    printf("  Data Start Block: %d\n", sb.d_start_blk);

    // Optional: Validate the root inode
    struct inode root_inode;
    if (readi(0, &root_inode) == 0) {
        printf("Root Inode Values:\n");
        printf("  Inode Number: %d\n", root_inode.ino);
        printf("  Valid: %d\n", root_inode.valid);
        printf("  Type: 0x%x\n", root_inode.type);
        printf("  Link Count: %d\n", root_inode.link);
    } else {
        fprintf(stderr, "Error: Failed to read root inode\n");
    }
}
//RUFS_INIT WORKS

void test_rufs_destroy() {

    strcpy(diskfile_path, "./DISKFILE");
    
    printf("Testing rufs_destroy...\n");

    // Call rufs_init to simulate system startup
    rufs_init(NULL);

    // Call rufs_destroy to simulate system shutdown
    rufs_destroy(NULL);

    printf("rufs_destroy test complete.\n");
}

// RUFS_DESTORY WORKS!!!



//testing functions for Vignesh
void test_get_avail_ino() {
    strcpy(diskfile_path, "./DISKFILE");

    printf("Testing get_avail_ino...\n");

    rufs_mkfs(); // Initialize the filesystem

    int ino1 = get_avail_ino();
    int ino2 = get_avail_ino();
    int ino3 = get_avail_ino();

    printf("First available inode: %d\n", ino1);
    printf("Second available inode: %d\n", ino2);
    printf("Third available inode: %d\n", ino3);

    if (ino1 != 1 || ino2 != 2 || ino3 != 3) {
        printf("Test failed: Incorrect inode numbers allocated.\n");
        return;
    }

    // Read and validate the inode bitmap
    char buf[BLOCK_SIZE];
    bio_read(sb.i_bitmap_blk, buf);
    printf("Inode Bitmap (First Byte): 0x%x\n", buf[0] & 0xFF);

    if ((buf[0] & 0xFF) != 0xF) { // Expecting first 4 bits to be set (inodes 0-3)
        printf("Test failed: Inode bitmap is incorrect.\n");
    } else {
        printf("Test passed: Inode allocation is correct.\n");
    }
}
//PASSED

void test_get_avail_ino_advanced() {
    strcpy(diskfile_path, "./DISKFILE");

    printf("Testing get_avail_ino (Advanced)...\n");

    rufs_mkfs(); // Initialize the filesystem

    // Allocate several inodes
    int inodes[5];
    for (int i = 0; i < 5; i++) {
        inodes[i] = get_avail_ino();
        printf("Allocated inode %d: %d\n", i + 1, inodes[i]);
    }

    // Validate allocated inodes
    for (int i = 0; i < 5; i++) {
        if (inodes[i] != i + 1) { // Expecting inodes 1 to 5
            printf("Test failed: Incorrect inode allocation. Expected %d, got %d\n", i + 1, inodes[i]);
            return;
        }
    }

    // Simulate deallocating inode 3 (clearing its bit in the bitmap)
    char buf[BLOCK_SIZE];
    bio_read(sb.i_bitmap_blk, buf); // Read the inode bitmap
    clear_bitmap((bitmap_t)buf, inodes[2]); // Clear inode 3 (index 2 in the array)
    bio_write(sb.i_bitmap_blk, buf); // Write updated bitmap back to disk
    printf("Deallocated inode 3\n");

    // Allocate again, expecting inode 3 to be reassigned
    int new_inode = get_avail_ino();
    printf("Reallocated inode: %d\n", new_inode);
    if (new_inode != inodes[2]) {
        printf("Test failed: Expected inode 3 to be reallocated, got %d\n", new_inode);
        return;
    }

    // Check the bitmap after all operations
    bio_read(sb.i_bitmap_blk, buf);
    printf("Inode Bitmap (First Byte): 0x%x\n", buf[0] & 0xFF);

    // Verify expected bitmap state
    if ((buf[0] & 0xFF) != 0x3F) { // Expecting first 6 bits set (inodes 0, 1, 2, 4, 5)
        printf("Test failed: Bitmap is incorrect. Expected 0x3F, got 0x%x\n", buf[0] & 0xFF);
        return;
    }

    printf("Test passed: Advanced inode allocation and bitmap handling are correct.\n");
}
//PASSED

