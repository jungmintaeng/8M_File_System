#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FileSystem.h"
#include "Disk.h"

#define INVALID_NUMBER -1
#define MAXNUM_OF_BLOCKS FS_DISK_CAPACITY / BLOCK_SIZE

#define BIT (8*sizeof(byte))
#define BITMAP_NOTFOUND -1

typedef enum{false=0, true} bool;
typedef unsigned char byte;

FileSysInfo* pFileSysInfo = NULL;

void	Mount(MountType type) //Mount에서 BufInit 하면 안됨
{
	int i , j;  //for문 돌리기 위한 변수

	if(type == MT_TYPE_FORMAT)
	{
		DevCreateDisk();
		FileSysInit();

		pFileSysInfo = (FileSysInfo*)malloc(BLOCK_SIZE);
		pFileSysInfo->pBlockBitmap = (char*)malloc(BLOCK_SIZE);
		pFileSysInfo->pInodeBitmap = (char*)malloc(BLOCK_SIZE);
		pFileSysInfo->blockBitmapStart = 2;
		pFileSysInfo->blocks = BLOCK_SIZE;
		pFileSysInfo->dataStart = 11;
		pFileSysInfo->diskCapacity = FS_DISK_CAPACITY / BLOCK_SIZE;
		pFileSysInfo->inodeBitmapStart = 1;
		pFileSysInfo->inodeListStart = 3;
		pFileSysInfo->numAllocBlocks = 5;
		pFileSysInfo->numAllocInodes = 1;
		pFileSysInfo->numFreeBlocks = 8192-5;
		pFileSysInfo->numFreeInodes = FS_INODE_COUNT - 1;
		pFileSysInfo->numInodes = FS_INODE_COUNT;
		pFileSysInfo->rootInodeNum = 0;

		for(i = 0; i < FS_DISK_CAPACITY / BLOCK_SIZE; i++)
			BlockBitmapReset(i);

		for(i = 0; i <= 3; i++)
			BlockBitmapSet(i);

		BlockBitmapSet(11);

		for(i = 0; i < FS_INODE_COUNT; i++)
			InodeBitmapReset(i);

		InodeBitmapSet(0);

		InodeInfo inode[16];

		for(i = 0; i < 16; i++)
		{
			inode[i].blocks = 0;
			inode[i].mode = INVALID_NUMBER;
			inode[i].size = 0;
			inode[i].type = INVALID_NUMBER;
			for(j = 0; j < 12; j++)
				inode[i].directPtr[j] = INVALID_NUMBER;
		}

		for(i = 4; i < 11; i++)
			DevWriteBlock(i, inode);

		inode[0].blocks = 1;
		inode[0].mode = FILE_MODE_READWRITE;
		inode[0].type = FILE_TYPE_DIR;
		inode[0].size = inode[0].blocks * BLOCK_SIZE;
		inode[0].directPtr[0] = 11;

		DevWriteBlock(3, inode);

		DirBlock* block_11 = malloc(BLOCK_SIZE);

		DirEntry dirEntries[16];

		for(i = 0; i < 12; i++)
		{
			dirEntries[i].inodeNum = INVALID_NUMBER;
			memset(dirEntries[i].name, 0 , 56);
			dirEntries[i].type = INVALID_NUMBER;
		}

		memcpy(block_11->dirEntries, dirEntries, 16);

		for(i = 12 ; i < pFileSysInfo->diskCapacity; i++)
			DevWriteBlock(i, block_11);

		block_11->dirEntries[0].inodeNum = 0;
		strcpy(block_11->dirEntries[0].name, ".");
		block_11->dirEntries[0].type = FILE_TYPE_DIR;

		DevWriteBlock(11, block_11);

		DevWriteBlock(0, pFileSysInfo);
		DevWriteBlock(1, pFileSysInfo->pInodeBitmap);
		DevWriteBlock(2, pFileSysInfo->pBlockBitmap);
	}
	else if(type == MT_TYPE_READWRITE)
	{
		FileSysInit();

		pFileSysInfo = (FileSysInfo*)malloc(BLOCK_SIZE);
		pFileSysInfo->pBlockBitmap = (char*)malloc(BLOCK_SIZE);
		pFileSysInfo->pInodeBitmap = (char*)malloc(BLOCK_SIZE);

		void* Temp1 = malloc(BLOCK_SIZE);
		void* Temp2 = malloc(BLOCK_SIZE);
		void* Temp3 = malloc(BLOCK_SIZE);

		DevReadBlock(0, Temp1);
		pFileSysInfo = (FileSysInfo*)Temp1;
		DevReadBlock(1, Temp2);
		pFileSysInfo->pInodeBitmap = (char*)Temp2;
		DevReadBlock(2, Temp3);
		pFileSysInfo->pBlockBitmap = (char*)Temp3;
	}
}

void	Unmount(void)
{
	FileSysFinish();
	DevWriteBlock(0, pFileSysInfo);
	DevWriteBlock(1, pFileSysInfo->pInodeBitmap);
	DevWriteBlock(2, pFileSysInfo->pBlockBitmap);
	free(pFileSysInfo);
}
