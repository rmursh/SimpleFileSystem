/**
 * Author: Razi Murshed
 * McGill ID: 260516333
 * Description: Header file for the API written for a Simple File System
 * Date - 17th November, 2015
 */
#define MAXFILENAME 15  //Maximum length of the name of the file
int mksfs(int fresh);  //creates the file system
int sfs_getnextfilename(char* filename); //get the name of the next file in directory
int sfs_getfilesize(const char* path); //get the size of the given file
int sfs_fopen(char *name); //opens the given file
int sfs_fclose(int fileID); //closes the given file
int sfs_fwrite(int fileID, const char *buf, int length); // write buf characters into disk
int sfs_fread(int fileID, char *buf, int length); // read characters from disk into buf
int sfs_fseek(int fileID, int offset); // seek to the location from beginning
int sfs_remove(char *file); // removes a file from the filesystem











