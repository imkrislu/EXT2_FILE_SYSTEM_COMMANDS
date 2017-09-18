#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "ext2.h"
#include "helpers.h"

unsigned char * disk;
struct ext2_super_block *sb;
struct ext2_group_desc *gd;
struct ext2_inode *inode_table;
#define INODE_COUNTS 32
#define BLOCK_COUNTS 128

//init disk, superblock, group descriptor
int init(const char * disk_img) {
    int fd = open(disk_img, O_RDWR);
    if (fd == -1) {
        return -1;
    }
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    gd = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE*2);
    inode_table = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE*gd->bg_inode_table);
    return 0;
}


/*
 * Get inode or block bitmap
 */
int* get_bitmap(void * addr, int counts) {
    int * bitmap = malloc(sizeof(int) * counts);
    if (bitmap == NULL) {
        perror("Malloc bitmap failed");
        exit(-1);
    }
    int i, byte, bit;
    for (i = 0; i < counts; i++) {
        byte = i / 8;
        bit = i % 8;
        char * byte_off = addr + byte;
        bitmap[i] = (*byte_off >> bit) & 1;
    }
    return bitmap;
}

//get inode bitmap
int* get_inode_bitmap() {
    return get_bitmap((void *)disk + gd->bg_inode_bitmap*EXT2_BLOCK_SIZE, INODE_COUNTS);
}

//get block bitmap
int* get_block_bitmap() {
    return get_bitmap((void *)disk + gd->bg_block_bitmap*EXT2_BLOCK_SIZE, BLOCK_COUNTS);
}

//get inode of file given parent directory
int get_dir_entry(char* dir_name, int num, int flag) {
    int i;
    struct ext2_inode * inode;
    struct ext2_dir_entry * dir;
    int block_number, b_count, rec_len;
    inode = &inode_table[num - 1];

    b_count = inode->i_size/EXT2_BLOCK_SIZE;
    for (i = 0; i < b_count && i < 12; i++) {
        block_number = inode->i_block[i];
        dir = (void *) disk + EXT2_BLOCK_SIZE * block_number;
        rec_len = 0;
        while (rec_len < EXT2_BLOCK_SIZE) {
            rec_len += dir->rec_len;
            if (strncmp(dir->name, dir_name, dir->name_len) == 0) {
                //if find directory with same name wanted
                if (flag == 1 && (dir->file_type == EXT2_FT_DIR)) {
                    return dir->inode;
                //if find file or link with same name wanted
                } else if (flag == 0 && ((dir->file_type == EXT2_FT_REG_FILE) || (dir->file_type == EXT2_FT_SYMLINK))) {
                    return dir->inode;
                }
            }
            dir = (void *) dir + dir->rec_len;
        }
    }
    return -1;
}

// check if given path file or directory exists
// flag == 1: directory flag == 0: file
int check_file_exist(char * fname, int * last_dir_inode, char* file) {
    int i, ind, i_ind;
    char dir_name[EXT2_NAME_LEN];
    if (strlen(fname) <= 0 || strncmp(fname, "/", 1) != 0) {
        return -1;
    }
    i_ind = 2;
    ind = 1;

    for (i = 1; i < strlen(fname) + 1; i++) {
        if (strncmp(fname + ind, "/", 1) == 0) {
            ind++;
        }
        //the path indicates it is a file
        if (i == strlen(fname)) {
            //get file name
            strncpy(dir_name, fname + ind, i - ind);
            dir_name[i - ind] = '\0';
            strcpy(file, dir_name);
            *last_dir_inode = i_ind;
            i_ind = get_dir_entry(file, i_ind, 0);
            //found the file
            if (i_ind != -1) {
                return i_ind;
            //didnt find the file
            } else {
                return -2;
            }
        //found directory
        } else if (strncmp(fname + i, "/", 1) == 0) {
            strncpy(dir_name, fname + ind, i - ind );
            dir_name[i - ind] = '\0';
            strcpy(file, dir_name);
            ind = i;
            *last_dir_inode = i_ind;
            i_ind = get_dir_entry(file, i_ind, 1);
            if (i_ind == -1 && i == strlen(fname) - 1) {
                // didn't find the last directory
                return -3;
            } else if (i_ind == -1) {
                // didn't find the directory
                return -1;
            } else if (i == strlen(fname) - 1) {
                //find the last directry
                //update the parent directory
                *last_dir_inode = i_ind;
                file[0] = '\0';
                return -4;
            }
        }
    }
    return 0;
}

//find enough empty blocks
void find_empty_blocks(int block_num, int * block_list) {
    int * block_bitmap;
    int num, i;

    if (sb->s_free_blocks_count < block_num) {
        perror("Not enough block space");
        exit(ENOSPC);
    }

    block_bitmap = get_block_bitmap();
    num = 0;
    for (i = 0; i < BLOCK_COUNTS; i++) {
        if (num == block_num) {
            break;
        }
        if (block_bitmap[i] == 0) {
            block_list[num] = i;
            num += 1;
        }
    }
    free(block_bitmap);
}

//find one empty inode
int find_empty_inode() {
    int * inode_bitmap;
    int p;
    inode_bitmap = get_inode_bitmap();
    if (sb->s_free_inodes_count < 1) {
        perror("Not enough inode space");
        exit(ENOSPC);
    }

    for (p = 0; p < INODE_COUNTS; p++) {
        if (inode_bitmap[p] == 0) {
            free(inode_bitmap);
            return p;
        }
    }
    perror("Not enough inode space");
    exit(ENOSPC);
}

//set bitmap
void set_bitmap(void * addr, int bit_num, int val) {
    int byte = bit_num / 8;
    int bit = bit_num % 8;
    char* byte_off = addr + byte;
    *byte_off = (*byte_off & ~(1 << bit)) | (val << bit);
}

//set block bitmap to val, and update superblock and group descriptor
void set_inode_bitmap(int bit_num, int val) {
    if (val == 1 && (check_inode(bit_num) == 0)) {
        sb->s_free_inodes_count -= 1;
        gd->bg_free_inodes_count -= 1;
    } else if (val == 0 && (check_inode(bit_num) == 1)) {
        sb->s_free_inodes_count += 1;
        gd->bg_free_inodes_count += 1;
    }
    set_bitmap(disk + gd->bg_inode_bitmap * EXT2_BLOCK_SIZE, bit_num, val);
}

//set inode bitmap to val, and update superblock and group descriptor
void set_block_bitmap(int bit_num, int val) {
    if (val == 1 && (check_block(bit_num) == 0)) {
        sb->s_free_blocks_count -= 1;
        gd->bg_free_blocks_count -= 1;
    } else if (val == 0 && (check_block(bit_num) == 1)) {
        sb->s_free_blocks_count += 1;
        gd->bg_free_blocks_count += 1;
    }
    set_bitmap(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE, bit_num, val);
}


int get_four(int a) {
    return ((a-1)/4+1)*4;
}

//add dir entry under certain directory
int add_dir_entry(int* inode_num, char* filename, int cur_inode) {
    int i, block_number;
    struct ext2_inode *inode =  &inode_table[*inode_num - 1];
    struct ext2_dir_entry *entry;
    int entry_size = get_four(8 + strlen(filename));
    int flag = 0;
    int contained_blocks, last_size;

    contained_blocks = inode->i_size/EXT2_BLOCK_SIZE;
    for (i = 0; i < contained_blocks && i < 12; i++) {
        block_number = inode->i_block[i];
        entry = (void*) disk + EXT2_BLOCK_SIZE * block_number;
        // printf("blcok_Num %d\n", block_number);
        // entry = (struct ext2_dir_entry *) disk + EXT2_BLOCK_SIZE * block_number;
        int rec_len = 0;

        while (rec_len < EXT2_BLOCK_SIZE) {
            rec_len += entry->rec_len;
            last_size = get_four(8 + entry->name_len);

            if (entry->rec_len >= entry_size + last_size) {
                int last_rec_len = entry->rec_len;
                entry->rec_len = last_size;
                entry = (void *)entry + last_size;
                entry->inode = cur_inode + 1;
                entry->rec_len = last_rec_len - last_size;
                entry->name_len = get_four(strlen(filename));
                if (inode_table[cur_inode].i_mode & EXT2_S_IFREG) {
                    entry->file_type = EXT2_FT_REG_FILE;
                } else if (inode_table[cur_inode].i_mode & EXT2_S_IFDIR) {
                    entry->file_type = EXT2_FT_DIR;
                }
                strncpy((void *) entry + 8, filename, entry->name_len);
                flag = 1;
                break;
            }
            entry = (void *) entry + entry->rec_len;
        }

        if (flag) {
            break;
        }

    }
    if (!flag) {
        int * new_block = malloc(sizeof(int));
        if (new_block == NULL) {
            perror("malloc");
            exit(-1);
        }
        find_empty_blocks(1, new_block);
        entry = (void *) disk + EXT2_BLOCK_SIZE * (*new_block + 1);
        entry->inode = cur_inode + 1;
        entry->rec_len = EXT2_BLOCK_SIZE;
        entry->name_len = get_four(strlen(filename));
        if (inode_table[cur_inode].i_mode & EXT2_S_IFREG) {
            entry->file_type = EXT2_FT_REG_FILE;
        } else if (inode_table[cur_inode].i_mode & EXT2_S_IFDIR) {
            entry->file_type = EXT2_FT_DIR;
        }
        strncpy((void *) entry + 8, filename, entry->name_len);
        set_block_bitmap(new_block[0], 1);
        inode->i_size += EXT2_BLOCK_SIZE;
        inode->i_blocks += 2;
        inode->i_block[contained_blocks] = new_block[0] + 1;
        free(new_block);
    }
    return 0;
}

struct ext2_inode* get_inode(int index) {
    return &inode_table[index-1];
}

int add_new_dir(void * addr, int i_ind, int p_inode) {
    struct ext2_dir_entry * entry =  (void *) addr;
    entry->inode = i_ind + 1;
    entry->rec_len = 12;
    entry->file_type = EXT2_FT_DIR;
    entry->name_len = strlen(".");
    strcpy(entry->name, ".");
    entry = (void*) entry + 12;
    entry->inode = p_inode;
    entry->rec_len = EXT2_BLOCK_SIZE - 12;
    entry->file_type = EXT2_FT_DIR;
    entry->name_len = strlen("..");
    strcpy(entry->name, "..");
    struct ext2_inode* inode = get_inode(p_inode);
    inode->i_links_count += 1;
    gd->bg_used_dirs_count++;
    return 0;
}

//copy file to blocks
void copy_file(int file_size, int block_num, int block_pointer, int empty_inode, int* block_list, unsigned char * src){
    int copied = 0;
    int remaining = file_size;
    int i;
    int block_index;
    for (i = 0; i < block_num; i++) {
        block_index = block_list[i];
        if (remaining < EXT2_BLOCK_SIZE) {
            memcpy(disk + EXT2_BLOCK_SIZE * (block_index + 1), src + copied, remaining);
            remaining = 0;
            set_block_bitmap(block_index, 1);
            break;
        }
        else {
            memcpy(disk + EXT2_BLOCK_SIZE * (block_index + 1), src + copied, EXT2_BLOCK_SIZE);
            remaining -= EXT2_BLOCK_SIZE;
            copied += EXT2_BLOCK_SIZE;
            set_block_bitmap(block_index, 1);
        }
    }


    //build block pointer
    int pointer_index;
    if (block_pointer > 0) {
        pointer_index =  block_list[block_num];
        int* pointer_addr = (void*) disk + EXT2_BLOCK_SIZE * (pointer_index + 1);
        for (i = 12; i < block_num; i++) {
            *pointer_addr = block_list[i] + 1;
            pointer_addr++;
        }
        set_block_bitmap(pointer_index, 1);
    }

    //set inode
    struct ext2_inode * inode = &inode_table[empty_inode];
    inode->i_mode = EXT2_S_IFREG;
    inode->i_size = file_size;
    inode->i_links_count = 1;
    inode->i_blocks = block_num * 2;
    inode->i_uid = 0;
    inode->i_gid = 0;
    inode->osd1 = 0;
    inode->i_generation = 0;
    inode->i_file_acl = 0;
    inode->i_dir_acl = 0;
    inode->i_faddr = 0;
    for (i = 0; i < block_num && i < 12; i++) {
        inode->i_block[i] = block_list[i] + 1;
    }
    if (block_pointer > 0) {
        inode->i_block[12] = pointer_index + 1;

    }
    set_inode_bitmap(empty_inode, 1);
}


//file name from a path
int get_name(char* path) {
    int i;
    char name[255];
    for (i = strlen(path) - 2; i > 0; i--) {
        if (i == 0) {
            strcpy(name, path);
            strcpy(path, name);
            return 0;
        } else if (path[i-1] == '/') {
            strncpy(name, path + i, strlen(path) - 1 - i);
            name[strlen(path) - 1 - i] = '\0';
            strcpy(path, name);
            return 0;
        }
    }
    return -1;
}

int check_inode(int inode_index){
    int * bitmap;
    bitmap = get_inode_bitmap();
    if(bitmap[inode_index] == 1){
        free(bitmap);
        return 1;
    } else {
        free(bitmap);
        return 0;
    }
}

int check_block(int block_index){
    int * bitmap;
    bitmap = get_block_bitmap();
    if(bitmap[block_index] == 1){
        free(bitmap);
        return 1;
    } else {
        free(bitmap);
        return 0;
    }
}
