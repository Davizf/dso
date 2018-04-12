/*
 * OPERATING SYSTEMS DESING - 16/17
 *
 * @file 	filesystem.c
 * @brief 	Implementation of the core file system funcionalities and auxiliary functions.
 * @date	01/03/2017
 */

#include "include/filesystem.h"		// Headers for the core functionality
#include "include/auxiliary.h"		// Headers for auxiliary functions
#include "include/metadata.h"		// Type and structure declaration of the file system
#include "include/crc.h"			// Headers for the CRC functionality

#include <math.h>

/*
 * @brief 	Generates the proper file system structure in a storage device, as designed by the student.
 * @return 	0 if success, -1 otherwise.
 */
int mkFS(long deviceSize)
{
	if (deviceSize < MIN_FILESYSTEM_SIZE) {
		printf("ERROR: Disk too small\n");
		return -1;
	} else if (deviceSize > MAX_FILESYSTEM_SIZE) {
		printf("ERROR: Disk too big\n");
		return -1;
	}

    int i;
	int fd;
	fd = open(DISK_NAME, O_RDWR);

	if (fd < 0) {
		perror("Error: 'disk.dat' disk file not found");
		return -1;
	}

    unsigned int deviceBlocks = floor(deviceSize/BLOCK_SIZE);
    printf("deviceBlocks = %u\n", deviceBlocks);
    
    // unsigned int inodeBlocks = (unsigned int)ceil((MAX_FILESYSTEM_OBJECTS_SUPPORTED*sizeof(inode_t))/BLOCK_SIZE);
    unsigned int inodeBlocks = 1;
    printf("Blocks for inodes = %u\n", inodeBlocks);

    // unsigned int inodeMapBlocks = (unsigned int)ceil((MAX_FILESYSTEM_OBJECTS_SUPPORTED*(unsigned int)sizeof(char))/BLOCK_SIZE);
    unsigned int inodeMapBlocks = 1;
    printf("inodeMapBlocks = %u\n", inodeMapBlocks);
    
    // unsigned int dataMapBlocks = ceil(deviceBlocks*sizeof(char)/BLOCK_SIZE);
    unsigned int dataMapBlocks = 1;
    printf("dataMapBlocks = %u\n", dataMapBlocks);

	unsigned int dataBlockNum = deviceBlocks - 1 - inodeMapBlocks - dataMapBlocks - inodeBlocks; 
    if(dataBlockNum < 0) {
        printf("ERROR no data blocks\n");
        return -1;
    } else {
        printf("dataBlockNum = %d\n", dataBlockNum);
    }

	sblock.magicNum 		 = MAGIC_NUMBER;
	sblock.inodeMapNumBlocks = inodeMapBlocks;
	sblock.dataMapNumBlock 	 = dataMapBlocks;
	sblock.numInodes 		 = MAX_FILESYSTEM_OBJECTS_SUPPORTED;
	sblock.firstInode 		 = 1 + inodeMapBlocks + dataMapBlocks;
	sblock.dataBlockNum 	 = dataBlockNum;
	sblock.firstDataBlock 	 = 1 + inodeMapBlocks + dataMapBlocks + inodeBlocks;
	sblock.deviceSize 		 = deviceSize;
	memset(sblock.padding, '0', sizeof(sblock.padding));

	bwrite(DISK_NAME, 0, (char*)&sblock);

	i_map = (char*)malloc(MAX_FILESYSTEM_OBJECTS_SUPPORTED*sizeof(char));

	for (i = 0; i < sblock.numInodes; i++) {
		printf("Initializing i_map[%d]\n", i);
        i_map[i] = 0;
	}

	b_map = (char*)malloc(dataBlockNum*sizeof(char));

	for (i = 0; i < sblock.dataBlockNum; i++) {
		printf("Initializing b_map[%d]\n", i);
        b_map[i] = 0;
	}

	for (i = 0; i < sblock.numInodes; i++) {
		printf("Initializing inodes[%d]\n", i);
		memset(&(inodes[i]), 0, sizeof(inode_t));
	}

	unmountFS();

	return 0;
}

/*
 * @brief 	Mounts a file system in the simulated device.
 * @return 	0 if success, -1 otherwise.
 */
int mountFS(void)
{
	int i;
	/* Read disk block 1 and store it into sblock */
	bread(DISK_NAME, 0, (char*)&(sblock));

	/* Read from disk inode map */
	for (i = 0; i < sblock.inodeMapNumBlocks; i++) {
		bread(DISK_NAME, 1+i, ((char *)i_map + i*BLOCK_SIZE));
	}

	/* Read disk block map */
	for (i = 0; i < sblock.dataMapNumBlock; i++) {
		bread(DISK_NAME, 1+i+sblock.inodeMapNumBlocks, ((char *)b_map + i*BLOCK_SIZE));
	}

	/* Read inodes from disk */
	for (i = 0; i < (sblock.numInodes*sizeof(inode_t)/BLOCK_SIZE); i++) {
		bread(DISK_NAME, i+sblock.firstInode, ((char *)inodes + i*BLOCK_SIZE));
	}

	return 0;
}

/*
 * @brief 	Unmounts the file system from the simulated device.
 * @return 	0 if success, -1 otherwise.
 */
int unmountFS(void)
{
	int i;
	for (i = 0; i < sblock.numInodes; i++) {
		if(inodes_x[i].opened == 1) {
			return -1;
		}
	}

	fssync();
	return 0;
}

/*
 * @brief	Creates a new file, provided it it doesn't exist in the file system.
 * @return	0 if success, -1 if the file already exists, -2 in case of error.
 */
int createFile(char *fileName)
{
	if(fileName == NULL) {return -2;}           /* Error with file name*/
    if(fileexists(fileName) == 1) {return -1;}  /* File name already exists */

    int b_id, inode_id;

	inode_id = ialloc();
	if(inode_id < 0) {return inode_id;}
	b_id = alloc();
	if(b_id < 0) {ifree(inode_id); return b_id;}

	strcpy(inodes[inode_id].name, fileName);
	inodes[inode_id].undirectBlock = b_id;

	inodes_x[inode_id].position = 0;
	inodes_x[inode_id].opened = 1;

	return 0;
}

/*
 * @brief	Deletes a file, provided it exists in the file system.
 * @return	0 if success, -1 if the file does not exist, -2 in case of error..
 */
int removeFile(char *fileName)
{
	int inode_id;

	inode_id = namei(fileName);
	if (inode_id < 0) return -1;

	bfree(inodes[inode_id].undirectBlock);
	memset(&(inodes[inode_id]), 0, sizeof(inode_t));
	ifree(inode_id);

	return 0;
}

/*
 * @brief	Opens an existing file.
 * @return	The file descriptor if possible, -1 if file does not exist, -2 in case of error..
 */
int openFile(char *fileName)
{
	int inode_id;

	inode_id = namei(fileName);
	if (inode_id < 0) {
		return inode_id;
	}

	inodes_x[inode_id].position = 0;
	inodes_x[inode_id].opened = 1;

	return inode_id;
}

/*
 * @brief	Closes a file.
 * @return	0 if success, -1 otherwise.
 */
int closeFile(int fileDescriptor)
{
	if (fileDescriptor < 0) {
		return -1;
	}

	inodes_x[fileDescriptor].position = 0;
	inodes_x[fileDescriptor].opened = 0;

	return 0;
}

/*
 * @brief	Reads a number of bytes from a file and stores them in a buffer.
 * @return	Number of bytes properly read, -1 in case of error.
 */
/*
int readFile(int fileDescriptor, void *buffer, int numBytes)
{
	char b[BLOCK_SIZE];
	int b_id;

	if (inodes_x[fileDescriptor].position+numBytes > inodes[fileDescriptor].size) {
		numBytes = inodes[fileDescriptor].size - inodes_x[fileDescriptor].position;
	}

	if (numBytes <= 0) {
		return -1;
	}

	b_id = bmap(fileDescriptor, inodes_x[fileDescriptor].position);
	bread(DISK_NAME, b_id, b);
	memmove(buffer, b+inodes_x[fileDescriptor].position, numBytes);
	inodes_x[fileDescriptor].position += numBytes;

	return numBytes;
}
*/

/*
 * @brief	Writes a number of bytes from a buffer and into a file.
 * @return	Number of bytes properly written, -1 in case of error.
 */
/*
int writeFile(int fileDescriptor, void *buffer, int numBytes)
{
	char b[BLOCK_SIZE];
	int b_id;

	if (inodes_x[fileDescriptor].position+numBytes > BLOCK_SIZE) {
		numBytes = BLOCK_SIZE - inodes_x[fileDescriptor].position;
	}

	if (numBytes <= 0) {
		return -1;
	}

	b_id = bmap(fileDescriptor, inodes_x[fileDescriptor].position);
	bread(DISK_NAME, b_id, b);
	memmove(b+inodes_x[fileDescriptor].position, buffer, numBytes);
	bwrite(DISK_NAME, b_id, b);
	inodes_x[fileDescriptor].position += numBytes;

	return numBytes;
}
*/

/*
 * @brief	Modifies the position of the seek pointer of a file.
 * @return	0 if succes, -1 otherwise.
 */
int lseekFile(int fileDescriptor, long offset, int whence)
{
	return -1;
}

/*
 * @brief 	Verifies the integrity of the file system metadata.
 * @return 	0 if the file system is correct, -1 if the file system is corrupted, -2 in case of error.
 */
int checkFS(void)
{
	return -2;
}

/*
 * @brief 	Verifies the integrity of a file.
 * @return 	0 if the file is correct, -1 if the file is corrupted, -2 in case of error.
 */
int checkFile(char *fileName)
{
	return -2;
}


int ialloc(void) {
	int i;
	/* To search for a free inode */
	for (i = 0; i < sblock.numInodes; i++) {
		if (i_map[i] == 0) {
			/* inode busy right now */
			i_map[i] = 1;
			/* Default values for the inode */
			memset(&(inodes[i]), 0, sizeof(inode_t));
			/* Return the inode identification */
			return i;
		}
	}
	return -1;
}

int alloc(void) {
	int i;
	char buffer[BLOCK_SIZE];

	for (i = 0; i < sblock.dataBlockNum; i++) {
		if (b_map[i] == 0) {
			/* busy block right now */
			b_map[i] = 1;
			/* default values for the block */
			memset(buffer, 0, BLOCK_SIZE);
			bwrite(DISK_NAME, i+sblock.firstDataBlock, buffer);
			/* it returns the block id */
			return i;
		}
	}
	return -1;
}

int ifree(int inode_id) {
	/* to check the inode_id vality */
	if (inode_id > sblock.numInodes) {
		return -1;
	}

	/* free inode */
	i_map[inode_id] = 0;

	return 0;
}

int bfree(int block_id) {
	/* to check the inode_id vality */
	if (block_id > sblock.dataBlockNum) {
		return -1;
	}

	/* free inode */
	b_map[block_id] = 0;

	return 0;
}

int namei(char *fname) {
	int i;
	/* seek for the inode with name <fname> */
	for (i = 0; i < sblock.numInodes; i++) {
		if (!strcmp(inodes[i].name, fname)) {
			return i;
		}
	}
	return -1;
}
/*
int bmap(int inode_id, int offset) {
	// check for if it is a valid inode ID */
//	if (inode_id > sblock.numInodes) {
//		return -1;
//	}
//
//	/* return the inode block */
//	if (offset < BLOCK_SIZE) {
//		return inodes[inode_id].directBlock;
//	}
//
//	return -1;
//}


int fssync(void) {
	int i;
	/* Write block 1 from sblock into disk */
	bwrite(DISK_NAME, 0, (char*)&sblock);

	/* Write inode map to disk */
	for (i = 0; i < sblock.inodeMapNumBlocks; i++) {
		bwrite(DISK_NAME, 1+i, ((char *)i_map + i*BLOCK_SIZE));
	}

	/* Write block map to disk */
	for (i = 0; i < sblock.dataMapNumBlock; i++) {
		bwrite(DISK_NAME, 1+i+sblock.inodeMapNumBlocks, ((char *)b_map + i*BLOCK_SIZE));
	}

	/* Write inodes to disk */
	for (i = 0; i < (sblock.numInodes*sizeof(inode_t)/BLOCK_SIZE); i++) {
		bwrite(DISK_NAME, i+sblock.firstInode, ((char *)inodes + i*BLOCK_SIZE));
	}

	return 1;
}

int fileexists(char *filename) {
    int i;

    for (i = 0; i < sblock.numInodes; i++) {
        if((i_map[i] == 0) && (strcmp(inodes[i].name, filename) == 0)) {
            return 1;
        }
    }

    return 0;
}
