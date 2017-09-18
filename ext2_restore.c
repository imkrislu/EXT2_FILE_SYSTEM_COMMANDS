#include "helpers.h"


int * last_dir_inode;
char * file;
int check_restore(void * addr, int cur_len, int limit, int entry_size);

int main(int argc, char const *argv[]) {
    struct ext2_inode * inode;
    int i, cur_len, last_rec_len, last_size, result;
    struct ext2_dir_entry * entry;

    if (argc != 3) {
        fprintf(stderr, "Usage: ext2_restore <image file location> <target file location>\n");
        exit(1);
    }
    if (init(argv[1]) != 0) {
        fprintf(stderr, "Disk image location does not exists!\n");
        exit(ENOENT);
    }
    if (argv[2][strlen(argv[2]) - 1] == '/') {
        fprintf(stderr, "Target path should not be an directory!\n");
        exit(EISDIR);
    }

    last_dir_inode = malloc(sizeof(int));
    file = malloc(sizeof(char) * EXT2_NAME_LEN);

    int f_ind = check_file_exist((char *) argv[2], last_dir_inode, file);
    // printf("%d\n", f_ind);
    if (f_ind == -1) {
        fprintf(stderr, "Target path does not exists!\n");
        free(file);
        free(last_dir_inode);
        exit(ENOENT);
    } else if (f_ind != -2) {
        fprintf(stderr, "Target file already exists!\n");
        free(file);
        free(last_dir_inode);
        exit(ENOENT);
    }

    inode = &inode_table[*last_dir_inode - 1];
    int num_block = inode->i_size / EXT2_BLOCK_SIZE;
    int cur_block;
    int entry_size = get_four(8 + strlen(file));
    for (i = 0; i < num_block; i++) {
        cur_block = inode->i_block[i];
        entry = (void*) disk + EXT2_BLOCK_SIZE * cur_block;
        cur_len = 0;
        //loop through entries in current block
        while (cur_len < EXT2_BLOCK_SIZE) {
            // printf("entry: %s , len: %d\n", entry->name, entry->rec_len);
            last_rec_len = entry->rec_len;
            last_size = get_four(8 + entry->name_len);
            if (entry->rec_len >= entry_size + last_size) {
                result = check_restore((void *) entry + last_size, 0, (int) entry->rec_len, entry_size);
                if (result > 0) {
                    // printf("result: %d\n", last_size);
                    entry->rec_len = result + last_size;
                    entry = (void *) entry + last_size + result;
                    // printf("entry: %s , len: %d\n", entry->name, entry->name_len);
                    entry->rec_len = last_rec_len - result;
                    return 0;
                }
            }
            cur_len += entry->rec_len;
            entry = (void *) entry + entry->rec_len;
        }

    }

    return ENOENT;
}


/* Check recursively within each oversized dir entry for deleted dir entries
 * addr: starting address, cur_len: current offset respect to addr, 
 * limit: only check up to the limit, entry_size: the size of the entry we are 
 * searching for.
 */
int check_restore(void * addr, int cur_len, int limit, int entry_size) {
    int block_ind, j, last_size;
    struct ext2_dir_entry * entry = (struct ext2_dir_entry *) addr;
    while (cur_len < limit) {
        last_size = get_four(8 + entry->name_len);
        if (strncmp(entry->name, file, strlen(file)) == 0) {
            // check if the entry found has the same name 
            struct ext2_inode * inode = &inode_table[entry->inode - 1];
            int * inodes_bm = get_inode_bitmap();
            // check inode unused
            if (inodes_bm[entry->inode - 1] != 1) {
                int* block_bm = get_block_bitmap();
                int blocks = inode->i_blocks/2;
                // printf("%d\n", blocks);
                for (j = 0; j < blocks; j++) {
                    //check block unused
                    block_ind = inode->i_block[j] - 1;
                    if (block_bm[block_ind] == 1) {
                        free(file);
                        free(last_dir_inode);
                        free(inodes_bm);
                        free(block_bm);
                        exit(ENOENT);
                    }
                }
                for (j = 0; j < blocks; j++) {
                    block_ind = inode->i_block[j] - 1;
                    set_block_bitmap(block_ind, 1);
                }
                inode->i_links_count++;
                inode->i_dtime = 0;
                set_inode_bitmap(entry->inode - 1, 1);
                return cur_len;
            } else {
                free(file);
                free(last_dir_inode);
                free(inodes_bm);
                exit(ENOENT);
            }
        } else if (entry->rec_len >= entry_size + last_size) {
            //another oversized entry
            int inside = check_restore((void *)entry + last_size, 0, entry->rec_len, entry_size);
            if (inside > 0){
                return cur_len + inside + last_size;
            }
        }
        cur_len += entry->rec_len;
        entry = (void *) entry + entry->rec_len;
    }
    return -1;
}
