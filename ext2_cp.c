
#include "helpers.h"

int main(int argc, char **argv) {
    int s_fd;
    int * block_list;
    int file_size;
    int block_num, f_inode;
    
    if (argc != 4) {
        fprintf(stderr, "Usage: ext2_cp <image file location> <source file location> <target path>\n");
        exit(1);
    }
    if (init(argv[1]) != 0) {
        fprintf(stderr, "Disk image location does not exists!\n");
        exit(ENOENT);
    }

    int * last_dir_inode = malloc(sizeof(int));
    char * file = malloc(sizeof(char) * EXT2_NAME_LEN);
    if (argv[3][strlen(argv[3]) - 1] == '/') {
        char dirname[EXT2_NAME_LEN];
        strcpy(file, argv[2]);
        file[strlen(file)] = '/';
        file[strlen(file) + 1] = '\0';
        get_name(file);
        strcpy(dirname, argv[3]);
        strcpy(dirname + strlen(dirname), file);
        strcpy(file, dirname);
        f_inode = check_file_exist(file, last_dir_inode, file);
    } else {
        f_inode = check_file_exist(argv[3], last_dir_inode, file);
    }
    if (f_inode == -1){
        fprintf(stderr, "Target path does not exists!\n");
        free(last_dir_inode);
        free(file);
        exit(ENOENT);
    }
    if (f_inode != -2 && f_inode != -4){
        fprintf(stderr, "Directory or file already exists!\n");
        free(last_dir_inode);
        free(file);
        exit(EEXIST);
    }

//last check


    s_fd = open(argv[2], O_RDONLY);
    if (s_fd == -1) {
        fprintf(stderr, "File does not exists\n");
        free(last_dir_inode);
        free(file);
        exit(ENOENT);
    }
    file_size = lseek(s_fd, 0, SEEK_END);
    block_num = (file_size - 1)/EXT2_BLOCK_SIZE + 1;
    // bool indirect = false;
    int block_pointer = 0;

    if (block_num > 12){
        block_pointer += 1;
        // if (block_num > 268) //double indirect?
    }

    block_list = malloc(sizeof(int) * block_num+block_pointer);
    find_empty_blocks(block_num+block_pointer, block_list);
    int empty_inode = find_empty_inode();

    //copy file to block
    unsigned char* src = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, s_fd, 0);
    if (src == MAP_FAILED) {
        perror("mmap for src");
        free(last_dir_inode);
        free(file);
        exit(1);
    }

    copy_file(file_size, block_num, block_pointer, empty_inode, block_list, src);


    if (file[0] == '\0') {
        strcpy(file, argv[2]);
        file[strlen(file)] = '/';
        file[strlen(file) + 1] = '\0';
        get_name(file);
    }

    add_dir_entry(last_dir_inode, file, empty_inode);

    // update_free_num();
    free(last_dir_inode);
    free(file);
    free(block_list);
    return 0;
}
