#include "helpers.h"


int bitmap_checker(int counter, int actual_count, char* X, char* Y);
int i_mode_checker(int num);
int check_file(int f_inode);

int main(int argc, char **argv) {
	//check if input is valid
	if (argc != 2) {
        fprintf(stderr, "Usage: ext2_checker <image file location>\n");
        exit(1);
    }

    //check if disk image exists
    if (init(argv[1]) != 0) {
        fprintf(stderr, "Disk image location does not exists!\n");
        exit(ENOENT);
    }
    //check free block and free inode.
    int * inode_bitmap;
    int * block_bitmap;
    int free_block, free_inode, i;
    int fix = 0;
    char * superblock = malloc(sizeof(char)* 11);
    strncpy(superblock, "superblock", 10);
    superblock[10] = '\0';
    char * blockgroup = malloc(sizeof(char)* 12);
    strncpy(blockgroup, "block group", 11);
    blockgroup[11] = '\0';
    char * freeblock = malloc(sizeof(char)* 12);
    strncpy(freeblock, "free blocks", 11);
    freeblock[11] = '\0';
    char * freeinode = malloc(sizeof(char)* 12);
    strncpy(freeinode, "free inodes", 11);
    freeinode[11] = '\0';

    //get free inode and block counts via bitmap
    inode_bitmap = get_inode_bitmap();
    block_bitmap = get_block_bitmap();
    free_inode = 0;
    free_block = 0;
    for (i = 0; i < BLOCK_COUNTS; i++) {
        if (block_bitmap[i] == 0) {
            free_block += 1;
        }
    }
    free(block_bitmap);
    for (i = 0; i < INODE_COUNTS; i++) {
        if (inode_bitmap[i] == 0) {
            free_inode += 1;
        }
    }
    free(inode_bitmap);
    //check if superblock and block bitmap matchs
    fix += bitmap_checker(sb->s_free_blocks_count, free_block, superblock, freeblock);
    //check if group descriptor and block bitmap matchs
    fix += bitmap_checker(gd->bg_free_blocks_count, free_block, blockgroup, freeblock);
    //check if superblock and inode bitmap matchs
    fix += bitmap_checker(sb->s_free_inodes_count, free_inode, superblock, freeinode);
    //check if group descriptor and inode bitmap matchs
    fix += bitmap_checker(gd->bg_free_inodes_count, free_inode, blockgroup, freeinode);
    free(superblock);
    free(blockgroup);
    free(freeblock);
    free(freeinode);


    //check i_mode, i_dtime and inode, block
    fix += i_mode_checker(EXT2_ROOT_INO);
    if(fix > 0){
    	printf("%d file system inconsistencies repaired!\n", fix);
    } else {
    	printf("No file system inconsistencies detected!\n");
    }
    return 0;
}

//check if the superblock and group descriptor matches bitmap
int bitmap_checker(int counter, int actual_count, char* X, char* Y) {
	int z;
	if (counter != actual_count){
    	if (counter > actual_count){
    		z = counter - actual_count;
    	} else {
    		z = actual_count - counter;
    	}
    	printf("%s 's %s counter was off by %d compared to bitmap\n", X, Y, z);
    	return z;
    }
    return 0;
}

int i_mode_checker(int num) {
	struct ext2_inode * inode;
	struct ext2_inode * new_inode;
	struct ext2_dir_entry * dir;
	int block_number, b_count, inode_num, i;
	int fix = 0;
	inode = &inode_table[num - 1];
	b_count = inode->i_size/EXT2_BLOCK_SIZE;
	//if the inode is for root directory, check if the inode bitmap and i_dtime correct
	if (num == EXT2_ROOT_INO) {
		if (check_inode(num-1) == 0) {
    		set_inode_bitmap(num-1, 1);
    		printf("Fixed: inode[%d] not marked as in_use\n", num);
    		fix += 1;
    	}
    	if (inode->i_dtime != 0) {
    		inode->i_dtime = 0;
    		printf("Fixed: valid inode marked for deletion:[%d]\n", num);
    		fix += 1;
	    }
	}
	//check through all block under this directory
	for (i = 0; i < b_count && i < 12; i++) {
	    block_number = inode->i_block[i];
	    dir = (void *) disk + EXT2_BLOCK_SIZE * block_number;
	    int rec_len = 0;
	    int j;
	    //jump through the self and parent directory entry
	    for(j = 0; j < 2; j++){
		    rec_len += dir->rec_len;
		    dir = (void *) dir + dir->rec_len;
		}
		//check through all directory entry in this block(files in this directory)
	    while (rec_len < EXT2_BLOCK_SIZE) {
	        rec_len += dir->rec_len;
	        inode_num = dir->inode;
	        new_inode = &inode_table[inode_num - 1];
	        //if this dir entry lead to a file
	        if (new_inode->i_mode & EXT2_S_IFREG) {
	        	//check if the directory entry file type match its inode file type
	        	if(dir->file_type != EXT2_FT_REG_FILE) {
		        	dir->file_type = EXT2_FT_REG_FILE;
		        	printf("Fixed: Entry type vs inode mismatch: inode[%d]\n", inode_num);
		        	fix += 1;
	        	}
	        	//check if all block used by this file is marked in block bitmap
	        	fix += check_file(inode_num);
	        // if this dir entry lead to a symbolic link
	        } else if (new_inode->i_mode & EXT2_S_IFLNK){
	         	if(dir->file_type != EXT2_FT_SYMLINK) {
		        	dir->file_type = EXT2_FT_SYMLINK;
		        	printf("Fixed: Entry type vs inode mismatch: inode[%d]\n", inode_num);
		        	fix += 1;
		        }
		        //check if all block used by this link is marked in block bitmap
		        fix += check_file(inode_num);
		    // if this dir entry lead to a new directory
	        } else if (new_inode->i_mode & EXT2_S_IFDIR){
	        	if (dir->file_type != EXT2_FT_DIR) {
	        		dir->file_type = EXT2_FT_DIR;
	        		printf("Fixed: Entry type vs inode mismatch: inode[%d]\n", inode_num);
	        		fix += 1;
	        	}
	        	//check all the files in this new directory
	        	fix += i_mode_checker(inode_num);
	        }
	        //check if the inode of this directory entry is marked in block bitmap
	        if (check_inode(inode_num-1) == 0) {
	    		set_inode_bitmap(inode_num-1, 1);
	    		printf("Fixed: inode[%d] not marked as in_use\n", inode_num);
	    		fix += 1;
	    	}
	    	//check if the inode of this directory entry not marked for deletion
	    	if (new_inode->i_dtime != 0) {
	    		new_inode->i_dtime = 0;
	    		printf("Fixed: valid inode marked for deletion:[%d]\n", inode_num);
	    		fix += 1;
	    	}
		    dir = (void *) dir + dir->rec_len;
		}
		//check if this block used by directory is marked in block bitmap
		if (check_block(block_number - 1) == 0) {
			set_block_bitmap(block_number-1, 1);
	    	printf("Fixed: %d in-use data blocks not marked in_date bitmap for inode:[%d]\n", block_number, num);
	    	fix += 1;
		}
	}
	return fix;
}

//check if blocks used by file is marked in block bitmap
int check_file(int f_inode) {
	struct ext2_inode * inode;
	int block_number, b_count, b_pointer, i;
	int fix = 0;
	inode = &inode_table[f_inode - 1];
	b_count = inode->i_blocks/2;
	b_pointer = b_count > 12 ? 1 : 0;
	int lack[b_count+b_pointer];
	//if indirect pointer is used
	if (b_count > 12) {
		for (i = 0; i < 12; i++) {
            block_number = inode->i_block[i];
            lack[i] = block_number;
        }
        int block_pointer = inode->i_block[12];
        lack[11] = block_pointer;
        int* blocks = (void*) disk + EXT2_BLOCK_SIZE * block_pointer;
        for (i = 0; i < b_count - 12; i++) {
            lack[i+12] = blocks[i];
        }
	} else {
		for (i = 0; i < b_count; i++) {
		    lack[i] = inode->i_block[i];
		}
	}
	for (i = 0; i < b_count+b_pointer; i++) {
		block_number = lack[i];
		if (check_block(block_number - 1) == 0) {
			set_block_bitmap(block_number-1, 1);
	    	printf("Fixed: %d in-use data blocks not marked in_date bitmap for inode:[%d]\n", block_number, f_inode);
	    	fix += 1;
		}
	}
	return fix;
}
