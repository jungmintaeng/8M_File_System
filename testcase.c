#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include "hw2.h"
#include "FileSystem.h"

#define DIR_NUM_MAX		100

extern FileSysInfo		*pFileSysInfo;

void PrintInodeBitmap(void)
{
	int i;
	int count;
	int* pBitmap;

	count = BLOCK_SIZE/sizeof(int);
	pBitmap = (int*) pFileSysInfo->pInodeBitmap;
	printf("Inode bitmap: ");
	for (i = 0; i < count;i++)
		printf("%d", pBitmap[i]);
	printf("\n");
}

void PrintBlockBitmap(void)
{
	int i;
	int count;
	int* pBitmap;

	count = BLOCK_SIZE/sizeof(int);
	pBitmap = (int*) pFileSysInfo->pBlockBitmap;
	printf("Block bitmap");
	for (i = 0; i < count;i++)
		printf("%d", pBitmap[i]);
	printf("\n");
}

void ReadInode(InodeInfo* inodeInfo, int inodeNo)
{
	char* pBuf = NULL;
	InodeInfo* pMem = NULL;
	int block = pFileSysInfo->inodeListStart + inodeNo / NUM_OF_INODE_PER_BLK;
	int inode = inodeNo % NUM_OF_INODE_PER_BLK;

	pBuf = (char*)malloc(BLOCK_SIZE);

	BufRead(block,pBuf);
	pMem = (InodeInfo*)pBuf;
	memcpy(inodeInfo, &pMem[inode], sizeof(InodeInfo));
}

void ListDirContentsAndSize(const char* dirName)
{
	int i ;
	int count;
	DirEntry pDirEntry[DIR_NUM_MAX];
	InodeInfo pInodeInfo;

	EnumerateDirStatus(dirName, pDirEntry, &count);

	printf("[%s]Sub-directory:\n", dirName);
	for (i = 0;i < count;i++)
	{
		if (pDirEntry[i].type == FILE_TYPE_FILE){
			ReadInode(&pInodeInfo, pDirEntry[i].inodeNum);
			printf("\t name:%s, inode no:%d, type:file, size:%d, blocks:%d\n", pDirEntry[i].name, pDirEntry[i].inodeNum, pInodeInfo.size, pInodeInfo.blocks);
		}
		else if (pDirEntry[i].type == FILE_TYPE_DIR)
				printf("\t name:%s, inode no:%d, type:directory\n", pDirEntry[i].name, pDirEntry[i].inodeNum);
		else
		{
			assert(0);
		}
	}
}
void ListDirContents(const char* dirName)
{
	int i ;
	int count;
	DirEntry pDirEntry[DIR_NUM_MAX];

	EnumerateDirStatus(dirName, pDirEntry, &count);
	printf("[%s]Sub-directory:\n", dirName);
	for (i = 0;i < count;i++)
	{
		if (pDirEntry[i].type == FILE_TYPE_FILE)
			printf("\t name:%s, inode no:%d, type:file\n", pDirEntry[i].name, pDirEntry[i].inodeNum);
		else if (pDirEntry[i].type == FILE_TYPE_DIR)
				printf("\t name:%s, inode no:%d, type:directory\n", pDirEntry[i].name, pDirEntry[i].inodeNum);
		else
		{
			assert(0);
		}
	}
}


void TestCase1(void)
{
	int i;
	char dirName[NAME_LEN_MAX];

	printf(" ---- Test Case 1 ----\n");

	MakeDir("/tmp");
	MakeDir("/usr");
	MakeDir("/etc");
	MakeDir("/home");
	/* make home directory */
	for (i = 0;i < 8;i++)
	{
		memset(dirName, 0, NAME_LEN_MAX);
		sprintf(dirName, "/home/user%d", i);
		MakeDir(dirName);
	}
	/* make etc directory */
	for (i = 0;i < 24;i++)
	{
		memset(dirName, 0, NAME_LEN_MAX);
		sprintf(dirName, "/etc/dev%d", i);
		MakeDir(dirName);
	}
	ListDirContents("/home");
	ListDirContents("/etc");

	/* remove subdirectory of etc directory */
	for (i = 23;i >= 0;i--)
	{
		memset(dirName, 0, NAME_LEN_MAX);
		sprintf(dirName, "/etc/dev%d", i);
		RemoveDir(dirName);
	}
	ListDirContents("/etc");
}


void TestCase2(void)
{
	int i, j;
	int fd;
	char fileName[NAME_LEN_MAX];
	char dirName[NAME_LEN_MAX];

	printf(" ---- Test Case 2 ----\n");

	ListDirContents("/home");
	/* make home directory */
	for (i = 0;i < 8;i++)
	{
		for (j = 0;j < 9;j++)
		{
			memset(fileName, 0, NAME_LEN_MAX);
			sprintf(fileName, "/home/user%d/file%d", i,j);
			fd = OpenFile(fileName, OPEN_FLAG_CREATE);
			CloseFile(fd);
		}
	}

	for (i = 0;i < 8;i++)
	{
		memset(dirName, 0, NAME_LEN_MAX);
		sprintf(dirName, "/home/user%d", i);
		ListDirContents(dirName);
	}
}


void TestCase3(void)
{
	int i;
	int fd;
	char fileName[NAME_LEN_MAX];
	char pBuffer1[512];
	char pBuffer2[512];

	printf(" ---- Test Case 3 ----\n");
	for (i = 0;i < 9;i++)
	{
		memset(fileName, 0, NAME_LEN_MAX);
		sprintf(fileName, "/home/user7/file%d", i);
		fd = OpenFile(fileName, OPEN_FLAG_CREATE);
		memset(pBuffer1, 0, 512);
		strcpy(pBuffer1, fileName);
		WriteFile(fd, pBuffer1, 512);
		CloseFile(fd);
	}
	for (i = 0;i < 9;i++)
	{
		memset(fileName, 0, NAME_LEN_MAX);
		sprintf(fileName, "/home/user7/file%d", i);
		fd = OpenFile(fileName, OPEN_FLAG_READWRITE);

		memset(pBuffer1, 0, 512);
		strcpy(pBuffer1, fileName);

		memset(pBuffer2, 0, 512);
		ReadFile(fd, pBuffer2, 512);

		if (strcmp(pBuffer1, pBuffer2))
		{
			printf("TestCase 3: error\n");
			exit(0);
		}
		CloseFile(fd);
	}
	printf(" ---- Test Case 3: files written/read----\n");
	ListDirContents("/home/user7");
}


void TestCase4(void)
{
	int i;
	int fd;
	char fileName[NAME_LEN_MAX];
	char pBuffer[1024];

	printf(" ---- Test Case 4 ----\n");
	for (i = 0;i < 9;i++)
	{
		if (i%2 == 0)
		{
			memset(fileName, 0, NAME_LEN_MAX);
			sprintf(fileName, "/home/user7/file%d", i);
			RemoveFile(fileName);
		}
	}
	printf(" ---- Test Case 4: files of even number removed ----\n");

	for (i = 0;i < 9;i++)
	{
		if (i%2)
		{
			memset(fileName, 0, NAME_LEN_MAX);
			sprintf(fileName, "/home/user7/file%d", i);
			fd = OpenFile(fileName, OPEN_FLAG_READWRITE);

			memset(pBuffer, 0, 1024);
			strcpy(pBuffer, fileName);
			WriteFile(fd, pBuffer, 513);
			CloseFile(fd);
		}
	}

	printf(" ---- Test Case 4: files of odd number overwritten ----\n");
	ListDirContents("/home/user7");

	for (i = 0;i < 9;i++)
	{
		if (i%2 == 0)
		{
			memset(fileName, 0, NAME_LEN_MAX);
			sprintf(fileName, "/home/user7/file%d", i);
			fd = OpenFile(fileName, OPEN_FLAG_CREATE);

			memset(pBuffer, 0, 1024);
			strcpy(pBuffer, fileName);
			WriteFile(fd, pBuffer, 513);
			WriteFile(fd, pBuffer, 513);
			CloseFile(fd);
		}
	}
	printf(" ---- Test Case 4: files of even number re-created & written ----\n");
	ListDirContents("/home/user7");
}

void TestCase5(void)
{
	printf(" ---- Test Case 5 ----\n");

	ListDirContentsAndSize("/home/user7");
}

int main(int argc, char** argv)
{
	int TcNum;

	if (argc < 3)
	{
ERROR:
		printf("usage: a.out [format | readwrite] [1-5])\n");
		return -1;
	}
	if (strcmp(argv[1], "format") == 0)
		Mount(MT_TYPE_FORMAT);
	else if (strcmp(argv[1], "readwrite") == 0)
		Mount(MT_TYPE_READWRITE);
	else
		goto ERROR;

	TcNum = atoi(argv[2]);

	DevResetDiskAccessCount();	

	switch (TcNum)
	{
	case 1:
		TestCase1();
		PrintInodeBitmap(); PrintBlockBitmap();
		break;
	case 2:
		TestCase2();
		PrintInodeBitmap(); PrintBlockBitmap();
		break;
	case 3:
		TestCase3();
		PrintInodeBitmap(); PrintBlockBitmap();
		break;
	case 4:
		TestCase4();
		PrintInodeBitmap(); PrintBlockBitmap();
		break;
	case 5:
		TestCase5();
		PrintInodeBitmap(); PrintBlockBitmap();
		break;
	default:
		Unmount();
		goto ERROR;
	}
	Unmount();

	printf("the number of disk access counts is %d\n", DevGetDiskReadCount() + DevGetDiskWriteCount());

	return 0;
}
