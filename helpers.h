#ifndef HELPERS
#define HELPERS
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

#include "ext2.h"

#define INODE_COUNTS 32
#define BLOCK_COUNTS 128

extern unsigned char * disk;
extern struct ext2_super_block *sb;
extern struct ext2_group_desc *gd;
extern struct ext2_inode *inode_table;





int init(const char * disk_img);
int* get_inode_bitmap();
int* get_block_bitmap();
int check_file_exist(char * fname, int * last_dir_inode, char* file);
int get_dir_entry(char* dir_name, int num, int flag);
void find_empty_blocks(int block_num, int * block_list);
int find_empty_inode();
void set_inode_bitmap(int bit_num, int val);
void set_block_bitmap(int bit_num, int val);
int add_dir_entry(int* inode_num, char* filename, int cur_inode);
struct ext2_inode* get_inode(int index);
int get_four(int a);
// void update_free_num();
int add_new_dir(void * addr, int i_ind, int p_inode);
void copy_file(int file_size, int block_num, int block_pointer, int empty_inode, int* block_list, unsigned char * src);
int check_inode(int inode_index);
int check_block(int block_index);
int get_name(char* path);
#endif
