#include "wfs.h"
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>


/**
mkfs.c
This C program initializes a file to an empty filesystem. The program receives three arguments: the disk image file, the number of inodes in the filesystem, and the number of data blocks in the system. The number of blocks should always be rounded up to the nearest multiple of 32 to prevent the data structures on disk from being misaligned. For example:

./mkfs -d disk_img -i 32 -b 200


initializes the existing file disk_img to an empty filesystem with 32 inodes and 224 data blocks. The size of the inode and data bitmaps are determined by the number of blocks specified by mkfs. If mkfs finds that the disk image file is too small to accomodate the number of blocks, it should exit with return code 1. mkfs should write the superblock and root inode to the disk image.
*/

void print_usage() {
    printf("Usage: mkfs -d disk_img -i num_inodes -b num_blocks\n");
}

int main(int argc, char *argv[]) {
    char *disk_img = NULL;
    size_t num_inodes = 0;
    size_t num_blocks = 0;

    int c;
    while ((c = getopt(argc, argv, "d:i:b:")) != -1) {
        switch (c) {
            case 'd':
                disk_img = optarg;
                break;
            case 'i':
                num_inodes = atoi(optarg);
                break;
            case 'b':
                num_blocks = atoi(optarg);
                break;
            default:
                print_usage();
                return 1;
        }
    }

    if (disk_img == NULL || num_inodes == 0 || num_blocks == 0) {
        print_usage();
        return 1;
    }

    // Round up the number of blocks to the nearest multiple of 32
    num_blocks = 32*((num_blocks + 31)/32);
    num_inodes = 32*((num_inodes + 31)/32);

    FILE *fp = fopen(disk_img, "r+");
    if (fp == NULL) {
        perror("fopen");
        return 1;
    }

    // Check if the disk image is large enough
    fseek(fp, 0, SEEK_END);
    size_t disk_size = ftell(fp);
    if (disk_size < num_blocks * BLOCK_SIZE) {
        fprintf(stderr, "Disk image is too small\n");
        return 1;
    }

    // Initialize the super block
    struct wfs_sb sb;
    sb.num_inodes = num_inodes;
    sb.num_data_blocks = num_blocks;
    sb.i_bitmap_ptr = sizeof(sb);
    sb.d_bitmap_ptr = sb.i_bitmap_ptr + (num_inodes + 7) / 8;
    sb.i_blocks_ptr = sb.d_bitmap_ptr + (num_blocks + 7) / 8;
    sb.d_blocks_ptr = sb.i_blocks_ptr + num_inodes * BLOCK_SIZE;//sizeof(struct wfs_inode);

    fseek(fp, 0, SEEK_SET);
    fwrite(&sb, sizeof(sb), 1, fp);


    // only needed if we need to zero out the bitmaps:

    // Initialize the inode bitmap
    char i_bitmap[num_inodes];
    memset(i_bitmap, 0, num_inodes);
    fseek(fp, sb.i_bitmap_ptr, SEEK_SET);
    fwrite(i_bitmap, sizeof(i_bitmap), 1, fp);

    // Initialize the data bitmap
    char d_bitmap[num_blocks];
    memset(d_bitmap, 0, num_blocks);
    fseek(fp, sb.d_bitmap_ptr, SEEK_SET);
    fwrite(d_bitmap, sizeof(d_bitmap), 1, fp);

    // cleanup
    fclose(fp);
}