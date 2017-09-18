#include "helpers.h"

int main(int argc, char **argv){
	int new_inode, src, link, i;
	struct ext2_inode * inode;

    //check if input is valid
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "Usage: ext2_ln <image file location> <-s> <source file location> <target path>\n");
        exit(1);
    }

    //check if symbolic input is valid
    if (argc == 5 && strcmp(argv[2],"-s") != 0) {
        fprintf(stderr, "Usage: ext2_ln <image file location> <-s> <source file location> <target path>\n");
        exit(1);
    }

    //check if disk image exists
    if (init(argv[1]) != 0) {
        fprintf(stderr, "Disk image location does not exists!\n");
        exit(ENOENT);
    }

    if(argc == 4){
    	src = 2;
    	link = 3;
    } else {
		src = 3;
    	link = 4;
    }

    int * last_dir_inode = malloc(sizeof(int));
    char * file = malloc(sizeof(char) * EXT2_NAME_LEN);

    int f_inode = check_file_exist(argv[src], last_dir_inode, file);
    //check if target file directory exist
	if (f_inode == -1 && f_inode == -3) {
    	fprintf(stderr, "Source directory does not exists!\n");
        free(last_dir_inode);
        free(file);
    	exit(ENOENT);
    //check if target file exist
	} else if (f_inode == -2) {
    	fprintf(stderr, "Source file does not exists!\n");
        free(last_dir_inode);
        free(file);
    	exit(ENOENT);
    //check if try to link a  hard link to directory
	} else if (link == 3 && f_inode == -4) {
    	fprintf(stderr, "Hard link can not refer to directory!\n");
        free(last_dir_inode);
        free(file);
    	exit(EISDIR);
	}

    int * link_last_dir =  malloc(sizeof(int));
    char * link_file  = malloc(sizeof(char) * EXT2_NAME_LEN);

    int link_inode = check_file_exist(argv[link], link_last_dir, link_file);
    //check if link directory exist
	if (link_inode == -1 && link_inode == -3) {
    	fprintf(stderr, "Link directory does not exists!\n");
        free(link_last_dir);
        free(link_file);
    	exit(ENOENT);
    //check if link name already exist
	} else if (link_inode != -2) {
    	fprintf(stderr, "Link name already exists\n");
        free(link_last_dir);
        free(link_file);
    	exit(EEXIST);
	}

    //get link name
  	for (i = strlen(argv[link]) - 1; i > 0; i--) {
    	if (argv[link][i-1] == '/') {
        	strncpy(link_file,  argv[link] + i, strlen(argv[link]) - i);
        	link_file[strlen(argv[link])- i] = '\0';
        	break;
    	}
    }

    //if hard link
    if(argc == 4){
        //creat dir entry for file inode in link directory
    	add_dir_entry(link_last_dir, link_file, f_inode-1);
        //add link count
    	inode = get_inode(f_inode);
    	inode->i_links_count ++;
    //if symbolic link
    } else {
    	new_inode = find_empty_inode();
        //creat dir entry for new inode in link directory
    	add_dir_entry(link_last_dir, link_file, new_inode);
        //save link path to data block
        inode = get_inode(new_inode);
        int block_num = strlen(file)/EXT2_BLOCK_SIZE;
        int block_pointer = 0;
        int * block_list = malloc(sizeof(int)*(block_num + block_pointer));
        if (block_num > 12) {
            block_pointer = 1;
        }
        copy_file(strlen(file), block_num, block_pointer, new_inode, block_list, (unsigned char *) file);
        free(block_list);
        //set new inode
        inode->i_mode = EXT2_S_IFLNK;
        inode->i_uid = 0;
        inode->i_gid = 0;
        inode->osd1 = 0;
        inode->i_generation = 0;
        inode->i_file_acl = 0;
        inode->i_dir_acl = 0;
        inode->i_faddr = 0;
    }
    free(last_dir_inode);
    free(link_last_dir);
    free(file);
    free(link_file);
	return 0;
}
