/**
 * Author: Razi Murshed
 * McGill ID: 260516333
 * Description: Contains the API written for a Simple File System
 * Date - 17th November, 2015
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "disk_emu.h"
#include "sfs_api.h"

/*Defining useful values and functions as Macros*/
#define ADJUST(x)((x/BLOCKSIZE + 1) * BLOCKSIZE) //Adjust x bits to the block size
#define MAGIC_NUMBER 256
#define BLOCKSIZE 512
#define NUM_BLOCKS 531
#define MAX_FILES 99
#define SUPERBLOCK 0
#define SUPERBLOCK_SIZE 1
#define FREELIST 1
#define FREELIST_SIZE 1
#define DIRECTORY_LOCATION 2
#define DIRECTORY_SIZE 4
#define INODE_TABLE 6
#define INODE_TABLE_SIZE 13
#define START 19
#define FILENAME "razi_murshed.sfs"
#define END_OF_FILE 3000

/*Directory Data Structure*/
typedef struct directoryEntry {
	char filename[MAXFILENAME + 1];
	unsigned int inode;
} dir_entry;

/*Struct to describe inode*/
typedef struct inodeEntry {
	unsigned int mode;
	unsigned int link_cnt;
	unsigned int size;
	unsigned int direct_ptr[12];
	unsigned int indirect_ptr;
} inode_t;

/*Data structure for file descriptors*/
typedef struct fileDescriptorEntry {
	unsigned int inode;
	unsigned int rw_ptr;
} fd_table_t;

/*Initializing global variables*/
int location_in_directory = 0;
int number_of_files;
int unsigned_size = sizeof(unsigned int);
dir_entry *root_dir;
inode_t *inode_table;
fd_table_t **fd_table;

/*Creates the super block*/
int create_super_block() {
	unsigned int *buff = malloc(BLOCKSIZE);

	if (buff == NULL) {
		return -1;
	}
	//set up all important superblock data
	buff[0] = MAGIC_NUMBER;
	buff[1] = BLOCKSIZE;
	buff[2] = NUM_BLOCKS;
	buff[3] = FREELIST;
	buff[4] = DIRECTORY_LOCATION;
	buff[5] = DIRECTORY_SIZE;
	buff[6] = INODE_TABLE;
	buff[7] = INODE_TABLE_SIZE;
	buff[8] = START;

	write_blocks(SUPERBLOCK, SUPERBLOCK_SIZE, buff);
	free(buff);
	return 0;
}

/*Creates and adds a root directory*/
int createRootDir() {
	dir_entry *buff = malloc(ADJUST(MAX_FILES*sizeof(dir_entry)));

	if (buff == NULL) {
		return -1;
	}
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		//set up directory values
		buff[i] = (dir_entry) {.filename = "\0",.inode = END_OF_FILE};
			}
	write_blocks(DIRECTORY_LOCATION, DIRECTORY_SIZE, buff);
	free(buff);
	return 0;
}

/*Function to create the free list*/
int set_free_list() {
	unsigned int *buff = malloc(BLOCKSIZE);
	if (buff == NULL) {
		return -1;
	}
	int i;
	for (i = 0; i < (BLOCKSIZE) / unsigned_size; i++) {
		buff[i] = ~0;
	}
	write_blocks(FREELIST, FREELIST_SIZE, buff);
	free(buff);
	return 0;
}
/*Assigns bit to free*/
void set_free(unsigned int index) {
	if (index > NUM_BLOCKS) {
		printf("Trying to allocate excess blocks at once \n");
		return;
	}
	int byte = index / (8 * unsigned_size); //find exact location
	int bit = index % (8 * unsigned_size);
	unsigned int *buff = malloc(BLOCKSIZE);

	if (buff == NULL) {
		printf("Cannot Assign free bit \n");
		return;
	}
	read_blocks(FREELIST, FREELIST_SIZE, buff);
	buff[byte] |= 1 << bit; //sets bit to 1(free)
	write_blocks(FREELIST, FREELIST_SIZE, buff);
	free(buff);
}

/*Set the index in Free List when allocated*/
void allocate(unsigned int index) {
	int byte = index / (8 * unsigned_size);
	int bit = index % (8 * unsigned_size);
	unsigned int *buff = malloc(BLOCKSIZE);

	if (buff == NULL) {
		printf("Cannot assign allocated bit\n");
		return;
	}
	if (index >= BLOCKSIZE) {
		printf("Trying to allocate excess blocks at once \n");
		return;
	}
	read_blocks(FREELIST, FREELIST_SIZE, buff);
	buff[byte] &= ~(1 << bit);
	write_blocks(FREELIST, FREELIST_SIZE, buff);
	free(buff);
}

/*Creates an inode table*/
int make_inode_table() {
	inode_t *buff = malloc(ADJUST((MAX_FILES+1)*sizeof(inode_t)));

	if (buff == NULL) {
		return -1;
	}

	int i;
	for (i = 0; i < MAX_FILES + 1; i++) {
		buff[i].mode = 0;
		buff[i].link_cnt = 0;
		buff[i].size = 0;
		buff[i].indirect_ptr = END_OF_FILE;
		int j;
		for (j = 0; j < 12; j++) {
			buff[i].direct_ptr[j] = END_OF_FILE;
		}
	}
	write_blocks(INODE_TABLE, INODE_TABLE_SIZE, buff);
	free(buff);
	return 0;
}

/*Search and locate next free bit in map*/
int find_free_bit() {
	unsigned int *buff = malloc(BLOCKSIZE);

	if (buff == NULL) {
		printf("Cannot find free bit \n");
		return -1;
	}

	read_blocks(FREELIST, FREELIST_SIZE, buff);
	int i;
	for (i = 0; i < (BLOCKSIZE) / unsigned_size; i++) {
		int find = ffs(buff[i]);
		if (find) {

			if (find + i * 8 * unsigned_size - 1 < BLOCKSIZE)
				return find + i * 8 * unsigned_size - 1;
		}
	}
	return -1;
}

int mksfs(int fresh) {

	/*Create file from scratch*/
	if (fresh == 1) {
		//Check if this sfs exits. If so delete.
		if (access(FILENAME, F_OK) != -1) {
			unlink(FILENAME);
		}
		//Initialize Disk
		if (init_fresh_disk(FILENAME, BLOCKSIZE, NUM_BLOCKS) != 0) {
			printf("Could not create SFS\n");
			return -1;
		}
		//Creates super block
		if (create_super_block() != 0) {
			printf("Cannot create superblock \n");
			return -1;
		}
		//Try to create free list
		if (set_free_list() != 0) {
			printf("Cannot create free list\n");
			return -1;
		}
		//Trying to create root directory
		if (createRootDir() != 0) {
			printf("Can not create root directory\n");
			return -1;
		}
		//Create an inode table
		if (make_inode_table() != 0) {
			printf("Could not create inode table\n");
			return -1;
		}
		//Create memory for root directory inode table
		inode_t *inode = malloc(ADJUST((MAX_FILES+1)*sizeof(inode_t)));
		read_blocks(INODE_TABLE, INODE_TABLE_SIZE, inode);
		if (inode == NULL) {
			return -1;
		}
		//Inode points to directory
		inode[0].size = DIRECTORY_SIZE * BLOCKSIZE;
		inode[0].link_cnt = DIRECTORY_SIZE;
		inode[0].mode = 1;
		//Check if we need to use the indirect ptr
		if (DIRECTORY_SIZE > 12) {
			inode[0].indirect_ptr = find_free_bit();
			allocate(inode[0].indirect_ptr);
			unsigned int *buff = malloc(BLOCKSIZE);
			write_blocks(START + inode[0].indirect_ptr, 1, buff);
			free(buff);
		}
		//assign the pointers the location of directory files
		int k;
		for (k = 0; k < DIRECTORY_SIZE; k++) {
			if (k > 11) {
				unsigned int *buff = malloc(BLOCKSIZE);
				read_blocks(START + inode[0].indirect_ptr, 1, buff);
				buff[k - 12] = DIRECTORY_LOCATION + k;
				write_blocks(START + inode[0].indirect_ptr, 1, buff);
				free(buff);
			} else {
				inode[0].direct_ptr[k] = DIRECTORY_LOCATION + k;
			}
		}
		//Free memory after updating inode
		write_blocks(INODE_TABLE, INODE_TABLE_SIZE, inode);
		free(inode);
		/*File system already exists; initialize it.*/
	} else if (fresh == 0) {
		if (init_disk(FILENAME, BLOCKSIZE, NUM_BLOCKS) != 0) {
			printf("Cannot initialize disk\n");
			return -1;
		}
	}

	/*Creating memory for data structures*/
	int *superblock = malloc(BLOCKSIZE * SUPERBLOCK_SIZE);

	if (superblock == NULL) {
		printf("Cannot allocate memory\n");
		return -1;
	}
	read_blocks(SUPERBLOCK, SUPERBLOCK_SIZE, superblock);

	root_dir = malloc(ADJUST(sizeof(dir_entry)*MAX_FILES));

	if (root_dir == NULL) {
		printf("Cannot allocate memory\n");
		return -1;
	}
	read_blocks(DIRECTORY_LOCATION, DIRECTORY_SIZE, root_dir);

	inode_table = malloc(ADJUST(sizeof(inode_t)*(MAX_FILES+1)));

	if (inode_table == NULL) {
		printf("Cannot allocate memory\n");
		return -1;
	}
	/*Assign all other variables*/
	read_blocks(INODE_TABLE, INODE_TABLE_SIZE, inode_table);
	number_of_files = 0;
	fd_table = NULL;
	return 0;
}

/*Get next existing file name form directory*/
int sfs_getnextfilename(char* filename)
{
	if (location_in_directory == MAX_FILES)
	{
		return 0;
	}
	strncpy(filename, root_dir[location_in_directory].filename, MAXFILENAME + 1);
	location_in_directory++;
	return 1;
}

/*Get the size of a requested file*/
int sfs_getfilesize(const char* path)
{
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		if (strncmp(root_dir[i].filename, path, MAXFILENAME + 1) == 0) {
			unsigned int inode = root_dir[i].inode;
			unsigned int size = inode_table[inode].size;
			return size;
		}
	}
	return -1;
}



/*Open a file */
int sfs_fopen(char *name) {
	//Filters -
	// 1) If file name is given to be too long
	// 2) If file hasnt been set up
	//
	if (strlen(name) > MAXFILENAME) {
		printf("File name too long, please keep under 15\n");
		return -1;
	}
	if (root_dir == NULL) {
		printf("Trying to open uninitialized system!\n");
		return -1;
	}
	/*Case if file exists*/
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		if (strncmp(root_dir[i].filename, name, MAXFILENAME + 1) == 0) {
			if (root_dir[i].inode == END_OF_FILE) {
				printf( "Cant find link!!!");
				return -1;
			}
			int j, entry_point = -1;
			//Check to see if file is already open
			for (j = 0; j < number_of_files; j++) {
				if (fd_table[j]&& root_dir[i].inode== fd_table[j]->inode && root_dir[i].inode != END_OF_FILE) {
					return j;
					}
				}
				//creating file descriptor for file
			for (j = 0; j < number_of_files; j++) {
				if (fd_table[j] == NULL) {
					fd_table[j] = malloc(sizeof(fd_table_t));
					entry_point = j;
					break;
				}
			}
			//Creating new entry in FDT
			if (entry_point == -1) {
				if (fd_table == NULL)
					fd_table = malloc(sizeof(fd_table_t*));
				else
					fd_table = realloc(fd_table,
							(1 + number_of_files) * (sizeof(fd_table_t*)));
				fd_table[number_of_files] = (fd_table_t *) malloc(
						sizeof(fd_table_t));
				entry_point = number_of_files;
				number_of_files++;
			}
			//Insert information
			fd_table_t *new = fd_table[entry_point];
			if (new == NULL) {
				fprintf(stderr, "Error opening requested file\n");
				return -1;
			}
			//Update information
			new->rw_ptr = inode_table[root_dir[i].inode].size;
			new->inode = root_dir[i].inode;
			return entry_point;
		}
	}
	/*Case if file does not already exist*/
	for (i = 0; i < MAX_FILES; i++) {
		//Find available slot in root dir
		if (strncmp(root_dir[i].filename, "\0", 1) == 0&& root_dir[i].inode == END_OF_FILE) {
			int entry = -1;
			int j;
			//Create space in fd for table
			for (j = 0; j < number_of_files; j++) {
				if (fd_table[j] == NULL) {
					fd_table[j] = malloc(sizeof(fd_table_t));
					entry = j;
					break;
				}
			}
			//Create new entry in fdt.
			if (entry == -1) {
				if (fd_table == NULL)
				fd_table = malloc(sizeof(fd_table_t*));
				else
				fd_table = realloc(fd_table,
						(1 + number_of_files) * (sizeof(fd_table_t*)));

				fd_table[number_of_files] = (fd_table_t *) malloc(
						sizeof(fd_table_t));
				entry = number_of_files;
				number_of_files++;
			}
			//Create inode for new entry
			fd_table_t *newEntry = fd_table[entry];

			if (newEntry == NULL) {
				printf( "Can't create file\n");
				return -1;
			}
			int inode = -1;
			int k;
			//Find next free inode
			for (k = 1; k < MAX_FILES + 1; k++)
			{
				if (inode_table[k].mode == 0) {
					inode_table[k].mode = 1;
					inode = k;
					break;
				}
			}
			//Check if inode is full
			if (inode == -1) {
				printf("INODE table is full!\n");
				return -1;
			}
			//Find bit to write file to
			int writeLoc = find_free_bit();
			if (writeLoc == -1)
			return -1;

			allocate(writeLoc);
			//update file descriptor entry
			newEntry->rw_ptr = 0;
			newEntry->inode = inode;

			//update the root directory
			strncpy(root_dir[i].filename, name, MAXFILENAME + 1);
			root_dir[i].inode = inode;
			write_blocks(DIRECTORY_LOCATION, DIRECTORY_SIZE, root_dir);

			//update inode table
			inode_table[inode].size = 0;
			inode_table[inode].link_cnt = 1;
			inode_table[inode].mode = 1;
			inode_table[inode].direct_ptr[0] = writeLoc;
			write_blocks(INODE_TABLE, INODE_TABLE_SIZE, inode_table);
			return entry;
	}
}
	return -1;
}

/*Close a file*/
int sfs_fclose(int fileID) {
	//check file is open and has not already been closed
	if (fileID >= number_of_files || fd_table[fileID] == NULL) {
		printf("Trying to open a closed file\n");
		return -1;
	}
	free(fd_table[fileID]);
	fd_table[fileID] = NULL;
	return 0;
}

/*Write to a file*/
int sfs_fwrite(int fileID, const char *buf, int length) {
	//if invalid file or requested return -1
	if (fileID >= number_of_files || fd_table[fileID] == NULL || buf == NULL|| length < 0) {
		printf("Bad write request\n");
		return -1; //return -1 on failure
	}
	int length_to_write = length;

	fd_table_t *file_to_write = fd_table[fileID];
	inode_t *inode = &(inode_table[file_to_write->inode]);
	if (file_to_write->inode == END_OF_FILE) {
		printf("Bad inode link");
		return -1;
	}
	char *disk_buffer = malloc(BLOCKSIZE);
	int block = (file_to_write->rw_ptr) / BLOCKSIZE; //get block location to write to
	int bytes = (file_to_write->rw_ptr) % BLOCKSIZE; //get exact byte location to write to
	int block_end = (inode->size) / BLOCKSIZE; //get end of file block

	unsigned int location_to_write;
	int offset = 0;
	//Check if reached max file size
	if (block > 139){
		printf("Write exceeded max file size\n");
		return -1;
	}
	// Use indirect ptrs
	else if (block > 11){
		unsigned int *buffer_for_indirect_ptr = malloc(BLOCKSIZE);
		read_blocks(START + inode->indirect_ptr, 1, buffer_for_indirect_ptr);
		location_to_write = buffer_for_indirect_ptr[block - 12];
		free(buffer_for_indirect_ptr);
	} else
		// Find block location from direct ptr.
		location_to_write = inode->direct_ptr[block];
	if (location_to_write == -1) {
		printf("Cant find location to write");
		return -1;
	}
	while (length > 0)
	{
		read_blocks(START + location_to_write, 1, disk_buffer);
		int write_nember_bytes;
		//check how much to write to block
		if (BLOCKSIZE - bytes < length) {
			write_nember_bytes = BLOCKSIZE - bytes;
		} else{
			write_nember_bytes = length;
		}
		memcpy(&disk_buffer[bytes], &buf[offset], write_nember_bytes);
		write_blocks(START + location_to_write, 1, disk_buffer);
		length -= (write_nember_bytes);
		offset += (write_nember_bytes);
		bytes = 0;
		block++;
		if (length > 0)
				{
			if (block > 139) {
				printf("Cant write more than exceeded max file size\n");
				return -1;
			}
			//Use indirect ptr
			else if (block_end < block) {
				if (block == 12 && inode->indirect_ptr == END_OF_FILE) {
					int indirPtr = find_free_bit();
					allocate(indirPtr);
					inode->indirect_ptr = indirPtr;
				}
				//find next location to write to
				int next = find_free_bit();
				allocate(next);
				if (next == -1){
				  return -1;
				}
				location_to_write = next;
				//Update INODE
				if (block > 11){
					unsigned int *new_buffer = malloc(BLOCKSIZE);
					read_blocks(START + inode->indirect_ptr, 1, new_buffer);
					new_buffer[block - 12] = location_to_write;
					write_blocks(START + inode->indirect_ptr, 1, new_buffer);
					free(new_buffer);
				} else
					inode->direct_ptr[block] = location_to_write;
				//update link count
				inode->link_cnt++;
			} else {
				//update INODE
				if (block > 11){
					unsigned int *new_buffer = malloc(BLOCKSIZE);
					read_blocks(START + inode->indirect_ptr, 1, new_buffer);
					location_to_write = new_buffer[block - 12];
					free(new_buffer);
				} else
					location_to_write = inode->direct_ptr[block];
			}
		}
	}
	if (file_to_write->rw_ptr + length_to_write > inode->size) {
		inode->size = file_to_write->rw_ptr + length_to_write;
	}
	//update rw ptr
	file_to_write->rw_ptr += length_to_write;
	write_blocks(INODE_TABLE, INODE_TABLE_SIZE, inode_table);
	free(disk_buffer);
	return length_to_write;
}

/*Read from a file*/
int sfs_fread(int fileID, char *buf, int length)
{
	if (fd_table[fileID] == NULL || length < 0 || fileID >= number_of_files|| buf == NULL) {
		printf("Bad read request\n");
		return -1;
	}
	fd_table_t *file_to_read = fd_table[fileID];
	inode_t*inode = &(inode_table[file_to_read->inode]);
	//check to see if file descriptor is valid
	if (file_to_read->inode == END_OF_FILE) {
		printf("Trying to read  bad inode \n");
		return -1;
	}
	if (file_to_read->rw_ptr + length > inode->size) {
		length = inode->size - file_to_read->rw_ptr;
	}
	int name_to_read = length;
	char *disk_buffer = malloc(BLOCKSIZE);

	int block = (file_to_read->rw_ptr) / BLOCKSIZE;
	int bytes = (file_to_read->rw_ptr) % BLOCKSIZE;
	int block_end = (inode->size) / BLOCKSIZE;
	unsigned int read_location;
	int offset = 0;
	if (block > 139) {
		printf("Read exceeded max file size\n");
		return -1;
	} else if (block > 11){
		unsigned int *indirect_buffer = malloc(BLOCKSIZE);
		read_blocks(START + inode->indirect_ptr, 1, indirect_buffer);
		read_location = indirect_buffer[block - 12];
		free(indirect_buffer);
	} else
		read_location = inode->direct_ptr[block];

	while (length > 0) {
		read_blocks(START + read_location, 1, disk_buffer);
		int bytesRead;

		//Find how much needs to be read
		if (BLOCKSIZE - bytes < length) {
			bytesRead = BLOCKSIZE - bytes;
		} else
			bytesRead = length;

		memcpy(&buf[offset], &disk_buffer[bytes], bytesRead);

		length -= (bytesRead);
		offset += (bytesRead);
		bytes = 0;

		if (length > 0)
				{
			block++;
			if (block_end < block) {
				return -1;
			}
			if (block > 139) {
				printf("Read exceeded max file size\n");
				return -1;
			} else if (block > 11) {
				unsigned int *next_buffer = malloc(BLOCKSIZE);
				read_blocks(START + inode->indirect_ptr, 1, next_buffer);
				read_location = next_buffer[block - 12];
				free(next_buffer);
			} else
				read_location = inode->direct_ptr[block];
		}
	}
	free(disk_buffer);
	//update r/w pointer in descriptor table
	file_to_read->rw_ptr += name_to_read;
	return name_to_read;
}

/*Seeks to find a position on a file*/
int sfs_fseek(int fileID, int offset)
{
	if (fileID >= number_of_files || fd_table[fileID] == NULL|| inode_table[fd_table[fileID]->inode].size < offset) {
		printf("Cannot seek to this part of file\n");
		return -1;
	}
	//move rw ptr
	fd_table[fileID]->rw_ptr = offset;
	return 0;
}



/*Remove file from disk*/
int sfs_remove(char *file) //remove file from disk
{
	int i;
	for (i = 0; i < MAX_FILES; i++)
	{
		//look for file
		//if found remove from disk and free up space
		if (strncmp(root_dir[i].filename, file, MAXFILENAME + 1) == 0 && root_dir[i].inode != END_OF_FILE) {
	    //update root directory
	    dir_entry *removed_entry = &(root_dir[i]);
		int inode = removed_entry->inode;
		strcpy(removed_entry->filename, "\0");
		inode_t *removed_inode = &(inode_table[inode]);
		removed_entry->inode = END_OF_FILE;
		int k;
		if (removed_inode->link_cnt > 12)
		{
			unsigned int *buff = malloc(BLOCKSIZE);
			read_blocks(START + removed_inode->indirect_ptr, 1, buff);
			//update i-node link count
			for (k = 0; k < removed_inode->link_cnt - 12; k++) {
				set_free(buff[k]);
			}
			free(buff);

			removed_inode->link_cnt = removed_inode->link_cnt - 12;
			set_free(removed_inode->indirect_ptr);
			removed_inode->indirect_ptr = END_OF_FILE;
		}
		for (k = 0; k < 12; k++) {
			set_free(removed_inode->direct_ptr[k]);
			removed_inode->direct_ptr[k] = END_OF_FILE;
		}
		//update i-node data
		removed_inode->mode = 0;
		removed_inode->link_cnt = 0;
		removed_inode->size = 0;
		write_blocks(INODE_TABLE, INODE_TABLE_SIZE, inode_table);
		return 0;
	}
}
	printf("Cant find this file!\n");
	return -1;
}
