#include "helpers.h"



int main(int argc, char **argv) {
    char* dirname = malloc(255 * sizeof(char));
    int * last_dir_inode = malloc(sizeof(int));
    char * file = malloc(sizeof(char) * EXT2_NAME_LEN);


    if (argc != 3) {
        fprintf(stderr, "Usage: ext2_cp <disk image file location> <target path>\n");
        exit(1);
    }
    if (init(argv[1]) != 0) {
        fprintf(stderr, "Disk image location does not exists!\n");
        exit(ENOENT);
    }
    strcpy(dirname, argv[2]);
    if (dirname[strlen(dirname) - 1] != '/') {
        dirname[strlen(dirname)] = '/';
        dirname[strlen(dirname) + 1] = '\0';
    }
    int f_inode = check_file_exist(dirname, last_dir_inode, file);

    if (f_inode == -1){
        fprintf(stderr, "Target path does not exists!\n");
        exit(ENOENT);
    }else if (f_inode == -4){
        fprintf(stderr, "Target directory already exists!\n");
        exit(EEXIST);
    }

    int* free_blocks = malloc(sizeof(int));
    find_empty_blocks(1, free_blocks);

    int empty_inode = find_empty_inode();
    add_new_dir((void *) disk + EXT2_BLOCK_SIZE * (1 + free_blocks[0]), empty_inode, *last_dir_inode);

    struct ext2_inode * inode = get_inode(empty_inode+1);
    inode->i_block[0] = free_blocks[0] + 1;
    inode->i_mode = EXT2_S_IFDIR;
    inode->i_size = EXT2_BLOCK_SIZE;
    inode->i_links_count = 2;
    inode->i_blocks = 2;
    inode->i_uid = 0;
    inode->i_gid = 0;
    inode->osd1 = 0;
    inode->i_generation = 0;
    inode->i_file_acl = 0;
    inode->i_dir_acl = 0;
    inode->i_faddr = 0;
    set_inode_bitmap(empty_inode, 1);
    set_block_bitmap(free_blocks[0], 1);
    free(free_blocks);


    // find dir name

    if (get_name(dirname) != 0) {
        perror("getname failed");
    }
    add_dir_entry(last_dir_inode, dirname, empty_inode);

    free(dirname);
    free(file);
    free(last_dir_inode);
    return 0;

}
