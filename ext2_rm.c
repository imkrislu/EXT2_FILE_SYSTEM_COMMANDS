#include "helpers.h"


int main(int argc, char **argv) {
    struct ext2_dir_entry * dir;
    struct ext2_dir_entry * last_dir;
    struct ext2_dir_entry * next_dir;
    struct ext2_inode * inode;
    struct ext2_inode * dir_inode;
    int remove = 0;
    int i, j, block_number, block_index, b_count;

    //check if input is valid
    if (argc != 3) {
        fprintf(stderr, "Usage: ext2_rm <image file location> <target file location>\n");
        exit(1);
    }
    //check if disk image exists
    if (init(argv[1]) != 0) {
        fprintf(stderr, "Disk image location does not exists!\n");
        exit(ENOENT);
    }

    int * last_dir_inode = malloc(sizeof(int));
    char * file = malloc(sizeof(char) * EXT2_NAME_LEN);

    int f_inode = check_file_exist(argv[2], last_dir_inode, file);
    //check if target directory exists
    if (f_inode == -1 && f_inode == -3) {
        fprintf(stderr, "Target path does not exists!\n");
        free(last_dir_inode);
        free(file);
        exit(ENOENT);
    //check if target file exists
    } else if (f_inode == -2) {
        fprintf(stderr, "Target file does not exists!\n");
        free(last_dir_inode);
        free(file);
        exit(ENOENT);
    //check if target is a directory
    } else if (f_inode == -4) {
        fprintf(stderr, "Can not remove directory!\n");
        free(last_dir_inode);
        free(file);
        exit(EISDIR);
    }



    //remove dir_entry in file's directory
    dir_inode = get_inode(last_dir_inode[0]);
    b_count = dir_inode->i_size/EXT2_BLOCK_SIZE;

    for (i = 0; i < b_count && i < 12; i++) {
        block_number = dir_inode->i_block[i];
        dir = (void *) disk + EXT2_BLOCK_SIZE * block_number;
        last_dir = (void *) disk + EXT2_BLOCK_SIZE * block_number;
        int rec_len = 0;
        while (rec_len < EXT2_BLOCK_SIZE) {
            rec_len += dir->rec_len;
            if (dir->inode == f_inode && dir->file_type == EXT2_FT_REG_FILE) {
                if(dir->rec_len == EXT2_BLOCK_SIZE){
                    dir_inode->i_blocks -= 2;
                    dir_inode->i_size -= EXT2_BLOCK_SIZE;
                    set_block_bitmap(block_number-1, 0);
                    for (j = i; j < b_count-1; j++){
                        dir_inode->i_block[j] = dir_inode->i_block[j+1];
                    }
                } else if(dir->rec_len == rec_len){
                    next_dir = (void*) dir + dir->rec_len;
                    dir->rec_len += next_dir->rec_len;
                } else {
                    last_dir->rec_len += dir->rec_len;
                }
                remove = 1;
                break;
            }
            last_dir = dir;
            dir = (void *) dir + dir->rec_len;
        }
        if(remove){
            break;
        }
    }
    free(last_dir_inode);
    free(file);

    //remove blockc the file uses
    inode = get_inode(f_inode);
    //reduce the links count
    inode->i_links_count--;
    //if the file's link is 0 or the file is a symbolic link
    if (inode->i_links_count == 0 || inode->i_mode & EXT2_S_IFLNK) {
        int block_num = inode->i_blocks/2;
        //if uses indirect pointer
        if (block_num > 12) {
            //free blocks pointed by direct pointer
            for (i = 0; i < 12; i++) {
                block_index = inode->i_block[i];
                printf("%d\n", block_index);
                set_block_bitmap(block_index -1 , 0);
            }
            //free indirect pointer
            int block_pointer = inode->i_block[12];
            set_block_bitmap(block_pointer - 1, 0);
            int* blocks = (void*) disk + EXT2_BLOCK_SIZE * block_pointer;
            //free blocks pointered by indirect pointer
            for (i = 0; i < block_num - 12; i++) {
                set_block_bitmap(*blocks - 1, 0);
                blocks++;
            }
        } else {
            for (i = 0; i < block_num; i++) {
                block_index = inode->i_block[i];
                set_block_bitmap(block_index -1 , 0);
            }
        }
        //set file inode free
        set_inode_bitmap(f_inode-1, 0);
        //set i_dtime
        time_t ltime; /* calendar time */
        ltime = time(NULL);
        inode->i_dtime = (unsigned int)ltime;
    }
    return 0;
}
