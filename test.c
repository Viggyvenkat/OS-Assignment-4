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

void test_get_avail_blkno() {
    // Set up disk file path
    strcpy(diskfile_path, "./DISKFILE");

    printf("Testing get_avail_blkno...\n");

    // Create and initialize the file system
    rufs_mkfs();

    // Allocate blocks
    int blkno1 = get_avail_blkno();
    printf("Allocated block 1: %d\n", blkno1);

    int blkno2 = get_avail_blkno();
    printf("Allocated block 2: %d\n", blkno2);

    int blkno3 = get_avail_blkno();
    printf("Allocated block 3: %d\n", blkno3);

    // Validate bitmap after allocations
    char buf[BLOCK_SIZE];
    bio_read(sb.d_bitmap_blk, buf);
    printf("Bitmap After Allocations: %02x\n", buf[0]);
    if (buf[0] != 0x07) {
        printf("Test failed: Incorrect bitmap after allocations. Expected 0x07, got 0x%02x.\n", buf[0]);
        return;
    }

    // Deallocate one block
    clear_bitmap((bitmap_t)buf, blkno2); // Clear the second block
    bio_write(sb.d_bitmap_blk, buf);
    printf("Deallocated block: %d\n", blkno2);

    // Validate bitmap after deallocation
    bio_read(sb.d_bitmap_blk, buf);
    printf("Bitmap After Deallocation: %02x\n", buf[0]);
    if (buf[0] != 0x05) {
        printf("Test failed: Incorrect bitmap after deallocation. Expected 0x05, got 0x%02x.\n", buf[0]);
        return;
    }

    // Reallocate the cleared block
    int blkno4 = get_avail_blkno();
    printf("Reallocated block: %d\n", blkno4);

    // Validate bitmap after reallocation
    bio_read(sb.d_bitmap_blk, buf);
    printf("Bitmap After Reallocation: %02x\n", buf[0]);
    if (buf[0] == 0x07 && blkno4 == blkno2) {
        printf("Test passed: Data block allocation and bitmap handling are correct.\n");
    } else {
        printf("Test failed: Incorrect bitmap or reallocation. Expected 0x07 and block %d, got 0x%02x and block %d.\n",
               blkno2, buf[0], blkno4);
    }
}

//passed

void test_get_avail_blkno_advanced() {
    printf("Testing get_avail_blkno (Advanced)...\n");

    // Initialize the diskfile_path global variable
    strcpy(diskfile_path, "./DISKFILE");

    // Create or load the filesystem
    rufs_mkfs();

    // Allocate a buffer to read and verify the bitmap
    char buf[BLOCK_SIZE];

    // Track allocated blocks
    int allocated_blocks[10];

    // Step 1: Bulk allocation of 10 blocks
    printf("Bulk Allocating 10 Blocks...\n");
    for (int i = 0; i < 10; i++) {
        int blkno = get_avail_blkno();
        if (blkno == -1) {
            fprintf(stderr, "Error: Failed to allocate block %d\n", i);
            return;
        }
        allocated_blocks[i] = blkno;
        printf("Allocated block %d: %d\n", i + 1, blkno);
    }

    // Read bitmap and validate it after allocations
    bio_read(sb.d_bitmap_blk, buf);
    printf("Bitmap After Allocations: %02x\n", buf[0]);

    // Step 2: Random deallocation
    printf("Deallocating 2 Blocks...\n");
    clear_bitmap((bitmap_t)buf, allocated_blocks[3]);
    clear_bitmap((bitmap_t)buf, allocated_blocks[7]);
    bio_write(sb.d_bitmap_blk, buf);

    printf("Bitmap After Deallocation: %02x\n", buf[0]);

    // Step 3: Reallocate deallocated blocks
    printf("Reallocating Deallocated Blocks...\n");
    int reallocated_block1 = get_avail_blkno();
    int reallocated_block2 = get_avail_blkno();

    printf("Reallocated block 1: %d\n", reallocated_block1);
    printf("Reallocated block 2: %d\n", reallocated_block2);

    // Validate that reallocated blocks match deallocated blocks
    if ((reallocated_block1 == allocated_blocks[3] && reallocated_block2 == allocated_blocks[7]) ||
        (reallocated_block1 == allocated_blocks[7] && reallocated_block2 == allocated_blocks[3])) {
        printf("Test passed: Reallocated blocks match deallocated blocks.\n");
    } else {
        fprintf(stderr, "Test failed: Reallocated blocks do not match deallocated blocks.\n");
        return;
    }

    // Read bitmap and validate it after reallocation
    bio_read(sb.d_bitmap_blk, buf);
    printf("Bitmap After Reallocation: %02x\n", buf[0]);

    // Step 4: Exhaust remaining blocks
    printf("Exhausting Remaining Blocks...\n");
    int blkno;
    while ((blkno = get_avail_blkno()) != -1) {
        printf("Exhausted block: %d\n", blkno);
    }

    // Final validation: Ensure no more blocks are available
    blkno = get_avail_blkno();
    if (blkno == -1) {
        printf("Test passed: No more blocks available.\n");
    } else {
        fprintf(stderr, "Test failed: Unexpected block available: %d\n", blkno);
    }
}

//PASSED

void test_readi() {
    printf("Testing readi...\n");

    // Ensure diskfile_path is properly set
    strcpy(diskfile_path, "./DISKFILE");

    rufs_init(NULL);
   

    struct inode inode_data;

    // Step 1: Read the root inode (inode 0)
    if (readi(0, &inode_data) < 0) {
        fprintf(stderr, "Test failed: Unable to read root inode\n");
        return;
    }

    // Step 2: Print the inode values
    printf("Inode 0 Values:\n");
    printf("  Inode Number: %d\n", inode_data.ino);
    printf("  Valid: %d\n", inode_data.valid);
    printf("  Type: 0x%x\n", inode_data.type);
    printf("  Link Count: %d\n", inode_data.link);

    // Step 3: Validate values (assuming root inode was initialized in rufs_mkfs)
    if (inode_data.ino == 0 && inode_data.valid == 1 &&
        inode_data.type == S_IFDIR && inode_data.link == 2) {
        printf("Test passed: Inode values are correct.\n");
    } else {
        printf("Test failed: Inode values are incorrect.\n");
    }
}
//PASSED

void test_writei() {
    printf("Testing writei...\n");

    // Ensure diskfile_path is properly set
    strcpy(diskfile_path, "./DISKFILE");

    rufs_init(NULL);

    struct inode test_inode = {
        .ino = 1,
        .valid = 1,
        .size = 1024,
        .type = S_IFREG,
        .link = 1,
        .direct_ptr = {0},
        .indirect_ptr = {0}
    };

    // Step 1: Write the test inode
    if (writei(1, &test_inode) < 0) {
        fprintf(stderr, "Test failed: Unable to write test inode\n");
        return;
    }

    // Step 2: Read back the inode to verify it was written correctly
    struct inode read_inode;
    if (readi(1, &read_inode) < 0) {
        fprintf(stderr, "Test failed: Unable to read back test inode\n");
        return;
    }

    // Step 3: Validate values
    printf("Written Inode 1 Values:\n");
    printf("  Inode Number: %d\n", read_inode.ino);
    printf("  Valid: %d\n", read_inode.valid);
    printf("  Size: %d\n", read_inode.size);
    printf("  Type: 0x%x\n", read_inode.type);
    printf("  Link Count: %d\n", read_inode.link);

    if (read_inode.ino == 1 && read_inode.valid == 1 &&
        read_inode.size == 1024 && read_inode.type == S_IFREG &&
        read_inode.link == 1) {
        printf("Test passed: Inode values are correct after writei.\n");
    } else {
        printf("Test failed: Inode values are incorrect after writei.\n");
    }
}
//passed

void test_dir_find() {
    printf("Testing dir_find...\n");

    // Set the diskfile path
    strcpy(diskfile_path, "./DISKFILE");
    rufs_init(NULL);

    // Step 1: Create a directory inode
    struct inode dir_inode = {0};
    dir_inode.ino = 1;
    dir_inode.valid = 1;
    dir_inode.size = BLOCK_SIZE;
    dir_inode.type = S_IFDIR;
    dir_inode.link = 2;

    // Initialize a block with directory entries
    char block[BLOCK_SIZE] = {0};
    struct dirent *entries = (struct dirent *)block;

    entries[0].valid = 1;
    entries[0].ino = 2;
    strncpy(entries[0].name, "file1", sizeof(entries[0].name));

    entries[1].valid = 1;
    entries[1].ino = 3;
    strncpy(entries[1].name, "file2", sizeof(entries[1].name));

    // Assign block to directory inode
    dir_inode.direct_ptr[0] = sb.d_start_blk;
    writei(1, &dir_inode);

    // Write the block to disk
    bio_write(sb.d_start_blk, block);

    // Step 2: Test finding an existing entry
    struct dirent found_entry;
    if (dir_find(1, "file1", strlen("file1"), &found_entry) == 0) {
        printf("Test passed: Found 'file1'. Inode: %d\n", found_entry.ino);
    } else {
        printf("Test failed: 'file1' not found.\n");
    }

    // Step 3: Test finding a missing entry
    if (dir_find(1, "missing", strlen("missing"), &found_entry) < 0) {
        printf("Test passed: 'missing' not found as expected.\n");
    } else {
        printf("Test failed: Unexpectedly found 'missing'.\n");
    }
}
//PASSED


void test_dir_find_advanced() {
    printf("Testing dir_find (Advanced)...\n");

    // Ensure diskfile_path is properly set
    strcpy(diskfile_path, "./DISKFILE");
    rufs_init(NULL);

    struct inode dir_inode;
    struct dirent entries[3];
    struct dirent result;

    // Step 1: Prepare a directory inode
    memset(&dir_inode, 0, sizeof(struct inode));
    dir_inode.ino = 1;
    dir_inode.valid = 1;
    dir_inode.size = BLOCK_SIZE;
    dir_inode.type = S_IFDIR;
    dir_inode.link = 2;

    // Allocate a block for the directory
    int block_num = get_avail_blkno();
    dir_inode.direct_ptr[0] = sb.d_start_blk + block_num;

    // Initialize directory entries
    strcpy(entries[0].name, "file1");
    entries[0].valid = 1;
    entries[0].ino = 2;

    strcpy(entries[1].name, "file2");
    entries[1].valid = 1;
    entries[1].ino = 3;

    strcpy(entries[2].name, "file3");
    entries[2].valid = 1;
    entries[2].ino = 4;

    // Write the entries to the allocated block
    if (bio_write(dir_inode.direct_ptr[0], entries) < 0) {
        fprintf(stderr, "Test failed: Unable to write directory entries.\n");
        return;
    }

    // Write the directory inode to disk
    if (writei(1, &dir_inode) < 0) {
        fprintf(stderr, "Test failed: Unable to write directory inode.\n");
        return;
    }

    // Step 2: Test finding an existing entry ("file1")
    if (dir_find(1, "file1", strlen("file1"), &result) == 0) {
        printf("Test passed: Found 'file1'. Inode: %d\n", result.ino);
    } else {
        printf("Test failed: Unable to find 'file1'.\n");
    }

    // Step 3: Test finding another existing entry ("file2")
    if (dir_find(1, "file2", strlen("file2"), &result) == 0) {
        printf("Test passed: Found 'file2'. Inode: %d\n", result.ino);
    } else {
        printf("Test failed: Unable to find 'file2'.\n");
    }

    // Step 4: Test finding a non-existing entry ("missing")
    if (dir_find(1, "missing", strlen("missing"), &result) < 0) {
        printf("Test passed: 'missing' not found as expected.\n");
    } else {
        printf("Test failed: Found 'missing' unexpectedly.\n");
    }

    // Step 5: Test finding an entry with a name that exceeds the maximum length
    char long_name[NAME_LEN + 10];
    memset(long_name, 'a', NAME_LEN + 9);
    long_name[NAME_LEN + 9] = '\0';

    if (dir_find(1, long_name, strlen(long_name), &result) < 0) {
        printf("Test passed: Long name not found as expected.\n");
    } else {
        printf("Test failed: Found long name unexpectedly.\n");
    }

    // Step 6: Test finding a valid entry but with extra trailing characters
    if (dir_find(1, "file1_extra", strlen("file1_extra"), &result) < 0) {
        printf("Test passed: 'file1_extra' not found as expected.\n");
    } else {
        printf("Test failed: Found 'file1_extra' unexpectedly.\n");
    }
}

//PASSED

//DOESN'T PASS. KEEPS ADDING DUPLICATE ENTRIES. ISSUE WITH CHECKING LOGIC
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

//doesn't pass all tests, issues with some, i believe related to dir_add
void test_get_node_by_path() {
    printf("Testing get_node_by_path...\n");

    // Initialize the file system
    strcpy(diskfile_path, "./DISKFILE");
    dev_close(); // Close any existing filesystem
    rufs_mkfs(); // Force a fresh filesystem

    // Initialize the root directory inode
    struct inode root_inode = {0};
    root_inode.ino = 0;
    root_inode.valid = 1;
    root_inode.type = S_IFDIR;
    memset(root_inode.direct_ptr, 0, sizeof(root_inode.direct_ptr));
    writei(0, &root_inode);

    // Add entries to the root directory
    dir_add(root_inode, 1, "dir1", strlen("dir1"));
    dir_add(root_inode, 2, "file1", strlen("file1"));

    // Initialize an inode for "dir1"
    struct inode dir1_inode = {0};
    dir1_inode.ino = 1;
    dir1_inode.valid = 1;
    dir1_inode.type = S_IFDIR;
    memset(dir1_inode.direct_ptr, 0, sizeof(dir1_inode.direct_ptr));
    writei(1, &dir1_inode);

    // Add entries to "dir1"
    dir_add(dir1_inode, 3, "file2", strlen("file2"));

    // Test cases
    struct inode result;
    if (get_node_by_path("/", 0, &result) == 0) {
        printf("Test passed: Found '/'.\n");
    } else {
        printf("Test failed: Could not find '/'.\n");
    }

    if (get_node_by_path("/dir1", 0, &result) == 0) {
        printf("Test passed: Found '/dir1'.\n");
    } else {
        printf("Test failed: Could not find '/dir1'.\n");
    }

    if (get_node_by_path("/dir1/file2", 0, &result) == 0) {
        printf("Test passed: Found '/dir1/file2'.\n");
    } else {
        printf("Test failed: Could not find '/dir1/file2'.\n");
    }

    if (get_node_by_path("/file1", 0, &result) == 0) {
        printf("Test passed: Found '/file1'.\n");
    } else {
        printf("Test failed: Could not find '/file1'.\n");
    }

    if (get_node_by_path("/nonexistent", 0, &result) < 0) {
        printf("Test passed: '/nonexistent' not found as expected.\n");
    } else {
        printf("Test failed: Unexpectedly found '/nonexistent'.\n");
    }
}

void test_dir_add_basic() {
    printf("Testing dir_add basic scenario...\n");

    // Initialize filesystem
    strcpy(diskfile_path, "./DISKFILE");
    dev_close(); // Close any existing filesystem
    if (rufs_mkfs() < 0) {
        fprintf(stderr, "Failed to create filesystem.\n");
        return;
    }

    // Create a directory inode (e.g., inode #1)
    struct inode dir_inode;
    memset(&dir_inode, 0, sizeof(struct inode));
    dir_inode.ino = 1;          // Not the root, just a test directory
    dir_inode.valid = 1;
    dir_inode.type = S_IFDIR; 
    dir_inode.size = 0;         // No entries yet
    dir_inode.link = 2;         // '.' and '..' counts, if applicable
    memset(dir_inode.direct_ptr, 0, sizeof(dir_inode.direct_ptr));

    // Write this directory inode to disk
    if (writei(dir_inode.ino, &dir_inode) < 0) {
        fprintf(stderr, "Test failed: Unable to write initial directory inode.\n");
        return;
    }

    // Add a single file entry to this directory
    const char *filename = "testfile";
    uint16_t new_file_ino = 2; // Assume we got an inode for the file earlier, or just pick one
    if (dir_add(dir_inode, new_file_ino, filename, strlen(filename)) < 0) {
        fprintf(stderr, "Test failed: dir_add() could not add '%s'.\n", filename);
        return;
    }
    printf("Successfully added '%s' to directory with inode=%d.\n", filename, dir_inode.ino);

    // Verify that the file was added using dir_find
    struct dirent found_entry;
    if (dir_find(dir_inode.ino, filename, strlen(filename), &found_entry) == 0) {
        // Check if found_entry matches what we expect
        if (found_entry.ino == new_file_ino) {
            printf("Test passed: '%s' found with correct inode=%d.\n", filename, found_entry.ino);
        } else {
            fprintf(stderr, "Test failed: Found '%s' but inode=%d does not match expected=%d.\n",
                    filename, found_entry.ino, new_file_ino);
        }
    } else {
        fprintf(stderr, "Test failed: Could not find '%s' after dir_add().\n", filename);
    }
}
//PASSED

void test_dir_add_multiple() {
    printf("Testing dir_add with multiple entries...\n");
    strcpy(diskfile_path, "./DISKFILE");
    dev_close();
    if (rufs_mkfs() < 0) {
        fprintf(stderr, "Failed to create filesystem.\n");
        return;
    }

    // Set up a directory inode
    struct inode dir_inode;
    memset(&dir_inode, 0, sizeof(struct inode));
    dir_inode.ino = 1;
    dir_inode.valid = 1;
    dir_inode.type = S_IFDIR;
    dir_inode.size = 0;
    dir_inode.link = 2;
    memset(dir_inode.direct_ptr, 0, sizeof(dir_inode.direct_ptr));

    if (writei(dir_inode.ino, &dir_inode) < 0) {
        fprintf(stderr, "Failed to write directory inode.\n");
        return;
    }

    // Add multiple files
    const char *files[] = { "file1", "file2", "file3", "file4" };
    int num_files = sizeof(files)/sizeof(files[0]);

    // Assign arbitrary inode numbers for testing
    uint16_t inodes[] = { 2, 3, 4, 5 };

    // Add each file and verify
    for (int i = 0; i < num_files; i++) {
        if (dir_add(dir_inode, inodes[i], files[i], strlen(files[i])) < 0) {
            fprintf(stderr, "Test failed: Could not add '%s' (ino=%d) to directory.\n", files[i], inodes[i]);
            return;
        }

        // Verify that the file was actually added
        struct dirent found_entry;
        if (dir_find(dir_inode.ino, files[i], strlen(files[i]), &found_entry) < 0) {
            fprintf(stderr, "Test failed: Could not find '%s' after adding.\n", files[i]);
            return;
        } else {
            if (found_entry.ino == inodes[i]) {
                printf("Successfully found '%s' with ino=%d.\n", files[i], found_entry.ino);
            } else {
                fprintf(stderr, "Test failed: Expected ino=%d for '%s', got %d.\n", inodes[i], files[i], found_entry.ino);
                return;
            }
        }
    }

    printf("All multiple additions test passed: files added and found successfully.\n");
}

//PASSED

void test_dir_add_duplicate() {
    printf("Testing dir_add with duplicate entries...\n");
    
    strcpy(diskfile_path, "./DISKFILE");
    dev_close();
    if (rufs_mkfs() < 0) {
        fprintf(stderr, "Failed to create filesystem.\n");
        return;
    }

    // Set up a directory inode
    struct inode dir_inode;
    memset(&dir_inode, 0, sizeof(struct inode));
    dir_inode.ino = 1;
    dir_inode.valid = 1;
    dir_inode.type = S_IFDIR;
    dir_inode.size = 0;
    dir_inode.link = 2;
    memset(dir_inode.direct_ptr, 0, sizeof(dir_inode.direct_ptr));

    if (writei(dir_inode.ino, &dir_inode) < 0) {
        fprintf(stderr, "Failed to write directory inode.\n");
        return;
    }

    const char *filename = "duplicate_test";
    uint16_t file_ino = 2; 

    // Add the file the first time
    if (dir_add(dir_inode, file_ino, filename, strlen(filename)) < 0) {
        fprintf(stderr, "Test failed: dir_add() could not add '%s' initially.\n", filename);
        return;
    }
    printf("Successfully added '%s' to directory.\n", filename);

    // Verify the file was added
    struct dirent found_entry;
    if (dir_find(dir_inode.ino, filename, strlen(filename), &found_entry) < 0) {
        fprintf(stderr, "Test failed: Could not find '%s' after adding.\n", filename);
        return;
    } else {
        printf("Verified '%s' was found with ino=%d.\n", filename, found_entry.ino);
    }

    // Attempt to add the same file again (duplicate)
    int ret = dir_add(dir_inode, file_ino + 1, filename, strlen(filename));
    if (ret == 0) {
        fprintf(stderr, "Test failed: dir_add() allowed a duplicate '%s'.\n", filename);
        return;
    } else {
        printf("Test passed: dir_add() correctly rejected duplicate '%s'.\n", filename);
    }
}

//PASSED

//TESTS FOR VIGNESH RUFS METHODS:

void test_rufs_destroy() {
    printf("Testing rufs_destroy...\n");

    // Initialize the file system
    initialize_test_fs();

    // Simulate operations
    struct inode test_inode;
    if (get_avail_ino() < 0) {
        fprintf(stderr, "Test failed: Unable to allocate inode.\n");
        return;
    }

    // Call rufs_destroy
    rufs_destroy(NULL);

    // Check if bitmaps are cleared
    char buf[BLOCK_SIZE];
    bio_read(sb.i_bitmap_blk, buf);
    for (int i = 0; i < MAX_INUM / 8; i++) {
        if (buf[i] != 0) {
            fprintf(stderr, "Test failed: Inode bitmap not cleared.\n");
            return;
        }
    }
    bio_read(sb.d_bitmap_blk, buf);
    for (int i = 0; i < MAX_DNUM / 8; i++) {
        if (buf[i] != 0) {
            fprintf(stderr, "Test failed: Data block bitmap not cleared.\n");
            return;
        }
    }

    printf("Test passed: rufs_destroy cleared resources successfully.\n");
}

void test_rufs_getattr() {
    printf("Testing rufs_getattr...\n");

    initialize_test_fs();

    struct stat stbuf;
    if (rufs_getattr("/", &stbuf) < 0) {
        fprintf(stderr, "Test failed: Unable to get attributes for root.\n");
        return;
    }

    printf("Root attributes: ino=%ld, mode=%o, size=%ld\n", stbuf.st_ino, stbuf.st_mode, stbuf.st_size);

    printf("Test passed: rufs_getattr successfully fetched attributes.\n");
}

void test_rufs_opendir() {
    printf("Testing rufs_opendir...\n");

    initialize_test_fs();

    if (rufs_opendir("/", NULL) < 0) {
        fprintf(stderr, "Test failed: Unable to open root directory.\n");
        return;
    }

    printf("Test passed: rufs_opendir opened root directory successfully.\n");
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

void test_rufs_mkdir() {
    printf("Testing rufs_mkdir...\n");

    initialize_test_fs();

    if (rufs_mkdir("/testdir", 0755) < 0) {
        fprintf(stderr, "Test failed: Unable to create /testdir.\n");
        return;
    }

    struct inode dir_inode;
    if (get_node_by_path("/testdir", 0, &dir_inode) < 0) {
        fprintf(stderr, "Test failed: /testdir not found after creation.\n");
        return;
    }

    printf("Test passed: rufs_mkdir created directory successfully.\n");
}

void test_rufs_open() {
    printf("Testing rufs_open...\n");

    initialize_test_fs();

    // Create a file
    if (rufs_create("/testfile", 0644, NULL) < 0) {
        fprintf(stderr, "Test failed: Unable to create /testfile.\n");
        return;
    }

    if (rufs_open("/testfile", NULL) < 0) {
        fprintf(stderr, "Test failed: Unable to open /testfile.\n");
        return;
    }

    printf("Test passed: rufs_open opened file successfully.\n");
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

