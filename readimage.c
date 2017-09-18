#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;

#define index(disk, i) (disk + (i * 1024))
#define ceilmult4(x) ((x + 3) & ~3)
#define isbit1(bitmap, i) (*(bitmap + (i / 8)) >> (i % 8) & 1)

void print_byte(unsigned char *byte) {
    printf(" ");
    for (int i = 0; i < 8; i++) {
        int bit = *byte & 1 << i;
        printf("%d", !!bit);
    }
}

void print_inode(struct ext2_inode *inode, unsigned int i) {
    char type = inode->i_mode & EXT2_S_IFDIR ? 'd' : 'f';
    printf("[%d] type: %c size: %d links: %d blocks: %d\n",
            i, type, inode->i_size, inode->i_links_count, inode->i_blocks);
    printf("[%d] Blocks: ", i);
    int blocks = inode->i_blocks >> 1;
    for (int j = 0; j < blocks; j++) {
        if (inode->i_block[j])
            printf(" %d", inode->i_block[j]);
    }
    printf("\n");
}

void print_dir_entry(unsigned int block, unsigned int i) {
    struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) index(disk, block);
    printf("    DIR BLOCK NUM: %d (for inode %d)\n", block, i);
    int rec_len = 0;
    while (rec_len < EXT2_BLOCK_SIZE) {
        char type = dir_entry->file_type == EXT2_FT_DIR ? 'd' : 'f';
        printf("Inode: %d rec_len: %d name_len: %d type: %c name:%.*s\n",
                dir_entry->inode, dir_entry->rec_len, dir_entry->name_len, type,dir_entry->name_len,  dir_entry->name);
        rec_len += dir_entry->rec_len;
        dir_entry = (void *) dir_entry + dir_entry->rec_len;
    }
}

int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);

    // TE7
    struct ext2_group_desc *g = (struct ext2_group_desc *)(disk + 2048);
    printf("Block group:\n");
    printf("    block bitmap: %d\n", g->bg_block_bitmap);
    printf("    inode bitmap: %d\n", g->bg_inode_bitmap);
    printf("    inode table: %d\n", g->bg_inode_table);
    printf("    free blocks: %d\n", g->bg_free_blocks_count);
    printf("    free inodes: %d\n", g->bg_free_inodes_count);
    printf("    used_dirs: %d\n", g->bg_used_dirs_count);

    printf("Block bitmap:");
    unsigned char *block_bm = index(disk, g->bg_block_bitmap);
    for (int i = 0; i < sb->s_blocks_count / 8; i++) {
        print_byte(block_bm + i);
    }
    printf("\n");
    printf("Inode bitmap:");
    unsigned char *inode_bm = index(disk, g->bg_inode_bitmap);
    for (int i = 0; i < sb->s_inodes_count / 8; i++) {
        print_byte(inode_bm + i);
    }

    // TE8
    printf("\n\n");
    printf("Inodes:\n");
    struct ext2_inode *inode_table = (struct ext2_inode *) index(disk, g->bg_inode_table);
    struct ext2_inode *root = &inode_table[1];
    print_inode(root, EXT2_ROOT_INO);
    for (int i = EXT2_GOOD_OLD_FIRST_INO; i < sb->s_inodes_count; i++) {
        if (isbit1(inode_bm, i))
            print_inode(&inode_table[i], i + 1);
    }

    // TE9
    printf("\n");
    printf("Directory Blocks:\n");
    int root_blocks = root->i_blocks >> 1;
    int root_direct_blocks = root_blocks < 12 ? root_blocks : 12;
    for (int i = 0; i < root_direct_blocks; i++) {
        print_dir_entry(root->i_block[i], EXT2_ROOT_INO);
    }
    for (int i = EXT2_GOOD_OLD_FIRST_INO; i < sb->s_inodes_count; i++) {
        if (isbit1(inode_bm, i) && inode_table[i].i_mode & EXT2_S_IFDIR) {
            int blocks = inode_table[i].i_blocks >> 1;
            int direct_blocks = blocks < 12 ? blocks : 12;
            for (int j = 0; j < direct_blocks; j++) {
                unsigned int block = inode_table[i].i_block[j];
                print_dir_entry(block, i + 1);
            }
        }
    }

    return 0;
}
