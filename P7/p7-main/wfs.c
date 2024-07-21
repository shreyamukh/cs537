#include "wfs.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <sys/mman.h>

#ifndef S_IFDIR
#define S_IFDIR 0040000 /* Directory.  */
#endif

// Macro definitions for common operations
#define BITS_TO_BYTES(x) ((x + 7) / 8)
#define SET_BIT(bm, pos, val) do { \
    char* bm_ = (char*)bm; \
    int byte_ = pos / 8; \
    int bit_ = pos % 8; \
    if (val) \
        bm_[byte_] |= (1 << bit_); \
    else \
        bm_[byte_] &= ~(1 << bit_); \
} while (0)
#define GET_BIT(bm, pos) (((char*)bm)[pos / 8] >> (pos % 8) & 1)
#define IS_EMPTY(bm, size) (checkEmpty(bm, size))

// Disk pointer
char *disk;
// WFS data structures
struct wfs_sb *superblock;
struct wfs_inode *inodes;
struct wfs_inode *rootInode;
void *dBitmap;
void *iBitmap;
char *dBlocks;

// Indirect block structure
struct indirect_block {
    off_t blocks[BLOCK_SIZE / sizeof(off_t)];
};

// Print usage information
void printOptions() {
    printf("Usage: ./wfs disk_path [FUSE options] mount_point\n");
}

// Check if a bitmap is empty
int checkEmpty(void *bitmap, size_t size) {
    size = BITS_TO_BYTES(size);
    char *bitmap_ptr = bitmap;
    for (int i = 0; i < size; i++) {
        if (bitmap_ptr[i] != 0) {
            return 0;
        }
    }
    return 1;
}

// Get the data block at the specified index
char *getBlock(int index) {
    return disk + index;
}

// Get the inode at the specified index
struct wfs_inode *getInode(int index) {
    return (struct wfs_inode *)((char *)inodes + (index * BLOCK_SIZE));
}

// Walk the path and return the corresponding inode
struct wfs_inode *walk(const char *path) {
    struct wfs_inode *current = rootInode;
    char *path_n = strdup(path);

    if (path_n[0] == '/') {
        path_n++;
    }
    if (path_n[strlen(path_n) - 1] == '/') {
        path_n[strlen(path_n) - 1] = '\0';
    }

    // return the current inode if not empty
    if (strlen(path_n) == 0) {
        return current;
    }

    // Tokenize the path and traverse the directory tree
    char *token = strtok(path_n, "/");
    int found = 0;
    while (token != NULL) {
        if ((current->mode & S_IFDIR) != S_IFDIR) {
            break;
        }
        found = 0;

        // Search for the directory entry in the current inode's data blocks
        for (int i = 0; i < current->size / BLOCK_SIZE; i++) {
            if (found) {
                break;
            }

            char *dataBlock = getBlock(current->blocks[i]);
            struct wfs_dentry *dentry = (struct wfs_dentry *)dataBlock;

            for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
                if (dentry[j].num == 0) {
                    continue;
                }
                if (strcmp(dentry[j].name, token) == 0) {
                    current = getInode(dentry[j].num);
                    found = 1;
                    break;
                }
            }
        }

        if (!found) {
            return NULL;
        }
        token = strtok(NULL, "/");
    }

    // Return the found inode if the path is fully traversed
    if (found && token == NULL) {
        return current;
    } else {
        return NULL;
    }
}

// Get the parent_dir inode of the specified path
struct wfs_inode *parent(const char *path) {
    char *path_n = strdup(path);
    int length = strlen(path_n);
    int lastSlash = length - 1;

    // Find the last slash in the path
    while (path_n[lastSlash] != '/') {
        lastSlash--;
    }

    // Truncate the path to get the parent_dir directory path
    if (lastSlash != 0) {
        path_n[lastSlash] = '\0';
    } else {
        path_n[1] = '\0';
    }

    // Walk the parent_dir directory path and return the parent_dir inode
    struct wfs_inode *parent_dir = walk(path_n);
    return parent_dir;
}

// FUSE operations

// Get file attributes
static int wfs_getattr(const char *path, struct stat *stbuf) {
    printf("getattr: %s\n", path);
    struct wfs_inode *inode = walk(path);
    if (inode == NULL) {
        return -ENOENT;
    }
    stbuf->st_uid = inode->uid;
    stbuf->st_gid = inode->gid;
    stbuf->st_atime = inode->atim;
    stbuf->st_mtime = inode->mtim;
    stbuf->st_mode = inode->mode;
    stbuf->st_size = inode->size;
    return 0;
}

// Create a file node
static int wfs_mknod(const char *path, mode_t mode, dev_t dev) {
    printf("mknod: %s\n", path);
    struct wfs_inode *inode = walk(path);
    if (inode != NULL) {
        return -EEXIST;
    }
    // Get the parent_dir directory inode
    struct wfs_inode *parent_dir = parent(path);

    // Find a free directory entry in the parent_dir directory
    struct wfs_dentry *dentry = NULL;
    int found = 0;
    for (int i = 0; i < parent_dir->size / BLOCK_SIZE; i++) {
        char *dataBlock = getBlock(parent_dir->blocks[i]);
        dentry = (struct wfs_dentry *)dataBlock;
        for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
            if (dentry[j].num == 0) {
                found = 1;
                dentry = &dentry[j];
                break;
            }
        }
        if (found) {
            break;
        }
    }

    // If no free directory entry found, allocate a new data block
    if (!found) {
        for (int i = 0; i < superblock->num_data_blocks; i++) {
            if (GET_BIT(dBitmap, i) == 0) {
                SET_BIT(dBitmap, i, 1);
                off_t block_addr = (off_t)(dBlocks - disk + i * BLOCK_SIZE);
                parent_dir->blocks[parent_dir->size / BLOCK_SIZE] = block_addr;
                parent_dir->size += BLOCK_SIZE;
                dentry = (struct wfs_dentry *)getBlock(block_addr);
                found = 1;
                break;
            }
        }
    }

    // If no space left, return an error
    if (!found) {
        return -ENOSPC;
    }

    // Allocate a new inode for the file
    int new_inode_num = -1;
    for (int i = 0; i < superblock->num_inodes; i++) {
        if (GET_BIT(iBitmap, i) == 0) {
            SET_BIT(iBitmap, i, 1);
            new_inode_num = i;
            break;
        }
    }
    if (new_inode_num == -1) {
        return -ENOSPC;
    }

    // Initialize the new inode
    struct wfs_inode *new_inode = getInode(new_inode_num);
    new_inode->num = new_inode_num;
    new_inode->mode = mode;
    new_inode->uid = getuid();
    new_inode->gid = getgid();
    new_inode->size = 0;
    new_inode->nlinks = 1;
    new_inode->atim = time(NULL);
    new_inode->mtim = time(NULL);
    new_inode->ctim = time(NULL);

    // Add the new file to the parent_dir directory
    dentry->num = new_inode_num;
    int lastSlash = strlen(path) - 1;
    while (path[lastSlash] != '/') {
        lastSlash--;
    }
    strcpy(dentry->name, path + lastSlash + 1);

    // Increment the parent_dir directory's nlinks
    parent_dir->nlinks++;
    return 0;
}

// Create a directory
static int wfs_mkdir(const char *path, mode_t mode) {
    printf("mkdir: %s\n", path);
    struct wfs_inode *inode = walk(path);
    if (inode != NULL) {
        return -EEXIST;
    }

    // Get the parent_dir directory inode
    struct wfs_inode *parent_dir = parent(path);

    // Check if the parent_dir is a directory
    if ((parent_dir->mode & S_IFDIR) != S_IFDIR) {
        return -ENOTDIR;
    }

    // Find a free directory entry in the parent_dir directory
    struct wfs_dentry *dentry = NULL;
    int found = 0;
    for (int i = 0; i < parent_dir->size / BLOCK_SIZE; i++) {
        char *dataBlock = getBlock(parent_dir->blocks[i]);
        dentry = (struct wfs_dentry *)dataBlock;
        for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
            if (dentry[j].num == 0) {
                found = 1;
                dentry = &dentry[j];
                break;
            }
        }
    }

    // If no free directory entry found, allocate a new data block
    if (!found) {
        for (int i = 0; i < superblock->num_data_blocks; i++) {
            if (GET_BIT(dBitmap, i) == 0) {
                SET_BIT(dBitmap, i, 1);
                off_t block_addr = (off_t)(dBlocks - disk + i * BLOCK_SIZE);
                parent_dir->blocks[parent_dir->size / BLOCK_SIZE] = block_addr;
                parent_dir->size += BLOCK_SIZE;
                dentry = (struct wfs_dentry *)getBlock(block_addr);
                found = 1;
                break;
            }
        }
    }

    // If no space left, return an error
    if (!found) {
        return -ENOSPC;
    }

    // Allocate a new inode for the directory
    int new_inode_num = -1;
    for (int i = 0; i < superblock->num_inodes; i++) {
        if (GET_BIT(iBitmap, i) == 0) {
            SET_BIT(iBitmap, i, 1);
            new_inode_num = i;
            break;
        }
    }
    if (new_inode_num == -1) {
        return -ENOSPC;
    }

    // Initialize the new directory inode
    struct wfs_inode *new_inode = getInode(new_inode_num);
    new_inode->num = new_inode_num;
    new_inode->mode = S_IFDIR;
    new_inode->uid = getuid();
    new_inode->gid = getgid();
    new_inode->size = 0;
    new_inode->nlinks = 1;
    new_inode->atim = time(NULL);
    new_inode->mtim = time(NULL);
    new_inode->ctim = time(NULL);

    // Add the new directory to the parent_dir directory
    dentry->num = new_inode_num;
    int lastSlash = strlen(path) - 1;
    while (path[lastSlash] != '/') {
        lastSlash--;
    }
    strcpy(dentry->name, path + lastSlash + 1);

    // Increment the parent_dir directory's nlinks
    parent_dir->nlinks++;

    return 0;
}

// Unlink a file
static int wfs_unlink(const char *path) {
    printf("unlink: %s\n", path);

    struct wfs_inode *inode = walk(path);
    if (inode == NULL) {
        return -ENOENT;
    }

    // Decrement the inode's nlinks
    inode->nlinks--;

    // If nlinks becomes 0, free the inode and data blocks
    if (inode->nlinks == 0) {
        // Free the inode
        SET_BIT(iBitmap, inode->num, 0);

        // Free the data blocks
        for (int i = 0; i < inode->size / BLOCK_SIZE; i++) {
            int d_index = (inode->blocks[i] - superblock->d_blocks_ptr) / BLOCK_SIZE;
            SET_BIT(dBitmap, d_index, 0);
            memset(getBlock(inode->blocks[i]), 0, BLOCK_SIZE);
        }
        if (inode->size % BLOCK_SIZE != 0) {
            int d_index = (inode->blocks[inode->size / BLOCK_SIZE] - superblock->d_blocks_ptr) / BLOCK_SIZE;
            SET_BIT(dBitmap, d_index, 0);
            memset(getBlock(inode->blocks[inode->size / BLOCK_SIZE]), 0, BLOCK_SIZE);
        }

        // Get the parent_dir directory inode
        struct wfs_inode *parent_dir = parent(path);
        parent_dir->nlinks--;

        // Remove the directory entry from the parent_dir directory
        for (int i = 0; i < parent_dir->size / BLOCK_SIZE; i++) {
            char *dataBlock = getBlock(parent_dir->blocks[i]);
            struct wfs_dentry *dentry = (struct wfs_dentry *)dataBlock;
            for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
                if (dentry[j].num == inode->num) {
                    dentry[j].num = 0;
                    dentry[j].name[0] = '\0';
                    break;
                }
            }
        }
    }

    return 0;
}

// Remove a directory
static int wfs_rmdir(const char *path) {
    printf("rmdir: %s\n", path);

    struct wfs_inode *inode = walk(path);
    if (inode == NULL) {
        return -ENOENT;
    }

    // Check if the inode is a directory
    if ((inode->mode & S_IFDIR) != S_IFDIR) {
        return -ENOTDIR;
    }

    // Get the parent_dir directory inode
    struct wfs_inode *parent_dir = parent(path);

    // Remove the directory entry from the parent_dir directory
    for (int i = 0; i < parent_dir->size / BLOCK_SIZE; i++) {
        char *dataBlock = getBlock(parent_dir->blocks[i]);
        struct wfs_dentry *dentry = (struct wfs_dentry *)dataBlock;
        for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
            if (dentry[j].num == inode->num) {
                dentry[j].num = 0;
                break;
            }
        }
    }

    // Decrement the parent_dir directory's nlinks
    parent_dir->nlinks--;

    // Free the inode
    SET_BIT(iBitmap, inode->num, 0);

    // Free the data blocks
    for (int i = 0; i < inode->size / BLOCK_SIZE; i++) {
        int d_index = (inode->blocks[i] - superblock->d_blocks_ptr) / BLOCK_SIZE;
        SET_BIT(dBitmap, d_index, 0);
        memset(getBlock(inode->blocks[i]), 0, BLOCK_SIZE);
    }

    return 0;
}

// Read data from a file
static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("read: %s\n", path);

    struct wfs_inode *inode = walk(path);
    if (inode == NULL) {
        return -ENOENT;
    }

    // Check if the inode is a directory
    if ((inode->mode & S_IFDIR) == S_IFDIR) {
        return -EISDIR;
    }

    // Calculate the block number and offset within the block
    int blockNum = offset / BLOCK_SIZE;
    int block_offset = offset % BLOCK_SIZE;

    // Check if the offset is greater than the file size
    if (offset >= inode->size) {
        return 0;
    }

    // Read the data from the blocks
    int num_full_blocks = (size + offset) / BLOCK_SIZE;
    int last_block_size = (size + offset) % BLOCK_SIZE;

    int indirect_blocks_start_index = blockNum - IND_BLOCK;
    struct indirect_block *indBlock = (struct indirect_block *)getBlock(inode->blocks[IND_BLOCK]);

    if (indirect_blocks_start_index < 0) {
        indirect_blocks_start_index = 0;
    }

    int directBlock_num = num_full_blocks;
    if (directBlock_num > IND_BLOCK - blockNum) {
        directBlock_num = IND_BLOCK - blockNum;
    }
    if (directBlock_num < 0) {
        directBlock_num = 0;
    }
    int indBlock_num = num_full_blocks - directBlock_num;

    // Read the direct blocks
    for (int i = 0; i < directBlock_num; i++) {
        char *dataBlock = getBlock(inode->blocks[blockNum + i]);
        memcpy(buf + i * BLOCK_SIZE, dataBlock + block_offset, BLOCK_SIZE - block_offset);
        block_offset = 0;
    }

    // Read the indirect blocks
    if (indBlock_num > 0) {
        for (int i = 0; i < indBlock_num; i++) {
            char *dataBlock = getBlock(indBlock->blocks[i + indirect_blocks_start_index]);
            memcpy(buf + directBlock_num * BLOCK_SIZE + i * BLOCK_SIZE, dataBlock + block_offset, BLOCK_SIZE);
            block_offset = 0;
        }
    }

    // Read the last block
    if (last_block_size > 0) {
        char *dataBlock;
        if (indBlock_num > 0) {
            dataBlock = getBlock(indBlock->blocks[indBlock_num + indirect_blocks_start_index]);
        } else if (blockNum + directBlock_num == IND_BLOCK) {
            dataBlock = getBlock(indBlock->blocks[0]);
        } else {
            dataBlock = getBlock(inode->blocks[blockNum + directBlock_num]);
        }
        memcpy(buf + directBlock_num * BLOCK_SIZE + indBlock_num * BLOCK_SIZE, dataBlock, last_block_size);
    }

    return size;
}

// Write data to a file
static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("write: %s\n", path);

    struct wfs_inode *inode = walk(path);
    if (inode == NULL) {
        return -ENOENT;
    }

    // Check if the inode is a directory
    if ((inode->mode & S_IFDIR) == S_IFDIR) {
        return -EISDIR;
    }

    // Calculate the block number and offset within the block
    int blockNum = offset / BLOCK_SIZE;
    int block_offset = offset % BLOCK_SIZE;

    // Check if the offset is greater than the file size
    if (offset > inode->size) {
        return 0;
    }

    // Calculate the number of new blocks needed
    int num_new_blocks = (size + offset - inode->size) / BLOCK_SIZE;
    if (num_new_blocks < 0) {
        num_new_blocks = 0;
    }
    int last_block_size = (size + offset) % BLOCK_SIZE;
    if (last_block_size > 0 && size + offset > inode->size) {
        num_new_blocks++;
    }

    int existing_blocks = inode->size / BLOCK_SIZE;
    int existing_direct_blocks = existing_blocks;
    if (existing_direct_blocks > IND_BLOCK) {
        existing_direct_blocks = IND_BLOCK;
    }
    int existing_indirect_blocks = existing_blocks - existing_direct_blocks;

    int new_direct_blocks = num_new_blocks;
    if (new_direct_blocks > IND_BLOCK - existing_blocks) {
        new_direct_blocks = IND_BLOCK - existing_blocks;
    }
    if (new_direct_blocks < 0) {
        new_direct_blocks = 0;
    }

    int new_indirect_blocks = num_new_blocks - new_direct_blocks;

    int found = 0;

    // Allocate new direct blocks
    for (int i = 0; i < new_direct_blocks; i++) {
        found = 0;
        for (int j = 0; j < superblock->num_data_blocks; j++) {
            if (GET_BIT(dBitmap, j) == 0) {
                SET_BIT(dBitmap, j, 1);
                inode->blocks[existing_blocks + i] = (off_t)(dBlocks - disk + j * BLOCK_SIZE);
                inode->size += BLOCK_SIZE;
                found = 1;
                break;
            }
        }
    }

    // Allocate new indirect blocks
    // Check if we need to allocate the indirect block
    if (inode->blocks[IND_BLOCK] == 0 && (new_indirect_blocks > 0 || inode->blocks[D_BLOCK] != 0)) {
        found = 0;
        for (int j = 0; j < superblock->num_data_blocks; j++) {
            if (GET_BIT(dBitmap, j) == 0) {
                SET_BIT(dBitmap, j, 1);
                inode->blocks[IND_BLOCK] = (off_t)(dBlocks - disk + j * BLOCK_SIZE);
                found = 1;
                break;
            }
        }
        if (!found) {
            return -ENOSPC;
        }
    }
    struct indirect_block *indBlock = (struct indirect_block *)getBlock(inode->blocks[IND_BLOCK]);

    for (int i = 0; i < new_indirect_blocks; i++) {
        found = 0;
        for (int j = 0; j < superblock->num_data_blocks; j++) {
            if (GET_BIT(dBitmap, j) == 0) {
                SET_BIT(dBitmap, j, 1);
                indBlock->blocks[existing_indirect_blocks + i] = (off_t)(dBlocks - disk + j * BLOCK_SIZE);
                inode->size += BLOCK_SIZE;
                found = 1;
                break;
            }
        }
    }

    if (!found && (new_indirect_blocks > 0 || new_direct_blocks > 0)) {
        return -ENOSPC;
    }

    int num_full_blocks = (size + offset) / BLOCK_SIZE;
    int indirect_blocks_start_index = blockNum - IND_BLOCK;

    if (indirect_blocks_start_index < 0) {
        indirect_blocks_start_index = 0;
    }

    int directBlock_num = num_full_blocks - blockNum;
    if (directBlock_num > IND_BLOCK - blockNum) {
        directBlock_num = IND_BLOCK - blockNum;
    }
    if (directBlock_num < 0) {
        directBlock_num = 0;
    }
    int indBlock_num = num_full_blocks - blockNum - directBlock_num;

    // Write to the direct blocks
    for (int i = 0; i < directBlock_num; i++) {
        char *dataBlock = getBlock(inode->blocks[blockNum + i]);
        memcpy(dataBlock + block_offset, buf + i * BLOCK_SIZE, BLOCK_SIZE - block_offset);
        block_offset = 0;
    }

    // Write to the indirect blocks
    if (indBlock_num > 0) {
        for (int i = 0; i < indBlock_num; i++) {
            char *dataBlock = getBlock(indBlock->blocks[i + indirect_blocks_start_index]);
            memcpy(dataBlock + block_offset, buf + directBlock_num * BLOCK_SIZE + i * BLOCK_SIZE, BLOCK_SIZE);
            block_offset = 0;
        }
    }

    // Write to the last block
    if (last_block_size > 0) {
        char *dataBlock;
        if (indBlock_num > 0) {
            dataBlock = getBlock(indBlock->blocks[indBlock_num + indirect_blocks_start_index]);
        } else if (blockNum + directBlock_num >= IND_BLOCK) {
            dataBlock = getBlock(indBlock->blocks[blockNum - IND_BLOCK]);
        } else {
            dataBlock = getBlock(inode->blocks[blockNum + directBlock_num]);
        }
        memcpy(dataBlock + block_offset, buf + directBlock_num * BLOCK_SIZE + indBlock_num * BLOCK_SIZE - offset % BLOCK_SIZE, last_block_size);
        if (num_new_blocks > 0) {
            inode->size -= BLOCK_SIZE - last_block_size;
        }
    }

    if (inode->size < size + offset) {
        inode->size = size + offset;
    }

    return size;
}

// Read directory entries
static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    printf("readdir: %s\n", path);

    struct wfs_inode *inode = walk(path);
    if (inode == NULL) {
        return -ENOENT;
    }

    if ((inode->mode & S_IFDIR) != S_IFDIR) {
        return -ENOTDIR;
    }

    // Read the directory entries from the inode's data blocks
    for (int i = 0; i < inode->size / BLOCK_SIZE; i++) {
        char *dataBlock = getBlock(inode->blocks[i]);
        struct wfs_dentry *dentry = (struct wfs_dentry *)dataBlock;
        for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++) {
            if (dentry[j].num != 0) {
                filler(buf, dentry[j].name, NULL, 0);
            }
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printOptions();
        return 1;
    }

    static struct fuse_operations ops = {
        .getattr = wfs_getattr,
        .mknod = wfs_mknod,
        .mkdir = wfs_mkdir,
        .unlink = wfs_unlink,
        .rmdir = wfs_rmdir,
        .read = wfs_read,
        .write = wfs_write,
        .readdir = wfs_readdir,
    };

    // Open the disk image
    FILE *fp = fopen(argv[1], "r+");
    if (fp == NULL) {
        perror("fopen");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    size_t disk_size = ftell(fp);

    // Map the disk image into memory
    disk = mmap(NULL, disk_size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(fp), 0);
    if (disk == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Initialize disk structures
    superblock = (struct wfs_sb *)disk;
    inodes = (struct wfs_inode *)(disk + superblock->i_blocks_ptr / sizeof(char));
    iBitmap = disk + superblock->i_bitmap_ptr / sizeof(char);
    dBitmap = disk + superblock->d_bitmap_ptr / sizeof(char);
    dBlocks = disk + superblock->d_blocks_ptr;
    rootInode = inodes;

    // Check if the file system is empty
    if (IS_EMPTY(iBitmap, superblock->num_inodes)) {
        // Create the root inode
        rootInode->num = 0;
        rootInode->mode = S_IFDIR;
        rootInode->uid = getuid();
        rootInode->gid = getgid();
        rootInode->size = 0;
        rootInode->nlinks = 1;
        rootInode->atim = time(NULL);
        rootInode->mtim = time(NULL);
        rootInode->ctim = time(NULL);

        // Set the first block to 0
        rootInode->blocks[0] = 0;

        // Set the rest of the blocks to -1
        for (int i = 1; i < N_BLOCKS; i++) {
            rootInode->blocks[i] = 0;
        }

        // Set the first bit in inode bitmap to 1
        SET_BIT(iBitmap, 0, 1);
    }

    argv[1] = argv[0];
    return fuse_main(argc - 1, argv + 1, &ops, NULL);
} 