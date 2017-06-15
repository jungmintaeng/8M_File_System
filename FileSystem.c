#include <stdio.h>
#include <stdlib.h>
#include "hw2.h"
#include "FileSystem.h"


#define BIT (8*sizeof(byte))
#define BITMAP_NOTFOUND -1

#define INVALID_NUMBER -1
#define MAXNUM_OF_BLOCKS FS_DISK_CAPACITY / BLOCK_SIZE

typedef enum{false=0, true} bool;
typedef unsigned char byte;
int DeleteByInode(int , int);

int BlockBitmapSearch();
int InodeBitmapSearch();
void InodeBitmapSet(int blkno);
void BlockBitmapSet(int blkno);
int InodeBitmapGet(int blkno);
int BlockBitmapGet(int blkno);
void InodeBitmapReset(int blkno);
void BlockBitmapReset(int blkno);
int TableSearch(int inodeNum);
void PrintDirBlockEntries(int blockNum);
void UpdateBlockBitmapInodeList();
int FindInodeByName(int, const char*);
bool IsInodeFull(int inodeNum, int* dirptr, int* dirent);

FileDescTable* pFileDescTable = NULL;


void		FileSysInit(void)
{
	BufInit();
	pFileDescTable = malloc(sizeof(FileDescTable));
	int i;
	for(i = 0 ; i < 128; i++)
		pFileDescTable->file[i].inodeNo = 0;
}

void		FileSysFinish(void)
{
	BufSync();
	free(pFileDescTable);
}

int		OpenFile(const char* szFileName, OpenFlag flag)
{
	InodeInfo aboveInodeBlock[NUM_OF_INODE_PER_BLK];

	int length = strlen(szFileName);
	char* str = (char*)malloc(length + 1);
	char* nameArray[20];
	char* temp;

	int index = 0;		//nameArray에서 새 디렉토리명 인덱스를 담고있음
	int findIndex = 1;	//Cursor
	int aboveInodeNum = 0;

	strncpy(str, szFileName, length); //szDirName -> const

	while(1)
	{
		if(index == 0)
			temp = strtok(str, "/");
		else
			temp = strtok(NULL, "/");
		nameArray[index] = temp;
		if(temp == NULL)
			break;
		index++;
	}

	/*
	 * nameArray[index - 2] = above Directory name
	 *
	 * nameArray[index - 1] = new Directory name
	 */

	int i = 0;

	while(findIndex < index && aboveInodeNum != INVALID_NUMBER)
	{
		BufRead(aboveInodeNum / 16 + 3, aboveInodeBlock);
		for(i = 0; i < NUM_OF_DIRECT_BLK_PTR; i++)
		{
			if(aboveInodeBlock[aboveInodeNum % 16].directPtr[i] < 11)
			{
				continue;
			}

			aboveInodeNum = FindInodeByName(
					aboveInodeBlock[aboveInodeNum % 16].directPtr[i], nameArray[findIndex - 1]);
			if(aboveInodeNum != INVALID_NUMBER)
				break;
		}

		/*
		 * for문 탈출시
		 * 1. 상위 폴더를 찾았을 경우 : aboveInodeNum != INVALID_NUMBER
		 * 2. 못찾았을 경우 : aboveInodeNum == INVALID_NUMBER
		 */

		findIndex++;
	}
	/*
	 * while문 탈출
	 * 1. 상위 폴더까지 찾음 --> aboveInodeNum = 상위폴더의 inodeNum
	 * 2. 못찾음 --> aboveInodeNum = INVALID_NUMBER --> 에러메시지 출력 후 리턴
	 */
	if(aboveInodeNum == INVALID_NUMBER) //while문 탈출 case 2
	{
		return INVALID_NUMBER;
	}

	if(flag == OPEN_FLAG_READWRITE)
	{
		int currentBlock;

		for(i = 0; i < NUM_OF_DIRENT_PER_BLK; i++)
		{
			currentBlock = aboveInodeBlock[aboveInodeNum % NUM_OF_INODE_PER_BLK].directPtr[i];
			if(currentBlock < 11)
				continue;
			else
			{
				if(FindInodeByName(currentBlock, nameArray[index-1]) >= 0)
				{
					break;
				}
			}
		}

		if(currentBlock < 11) // 파일이 존재하지 않는 경우
		{
			printf("\nREADWRITE_FLAG --> 해당 파일이 존재하지 않습니다.\n");
			return INVALID_NUMBER;
		}
		else
		{
			int fileInodeNum;
			fileInodeNum = FindInodeByName(currentBlock, nameArray[index-1]);

			int tableNum = TableSearch(0);
			if(tableNum == INVALID_NUMBER)
			{
				printf("\nfile Desc Table is full\n");
				return -1;
			}
			else
			{
				pFileDescTable->file[tableNum].inodeNo = fileInodeNum;
				pFileDescTable->file[tableNum].offset = 0;
				pFileDescTable->file[tableNum].valid_bit = 1;
			}
			UpdateBlockBitmapInodeList();
			return tableNum;
		}
	}
	else if(flag == OPEN_FLAG_CREATE)
	{
		int currentBlock;

		for(i = 0; i < NUM_OF_DIRENT_PER_BLK; i++)
		{
			currentBlock = aboveInodeBlock[aboveInodeNum % NUM_OF_INODE_PER_BLK].directPtr[i];
			if(currentBlock < 11)
				continue;
			else
			{
				if(FindInodeByName(currentBlock, nameArray[index-1]) >= 0)
				{
					break;
				}
			}
		}

		if(currentBlock < pFileSysInfo->dataStart)//파일이 존재하지 않는 경우
		{
			int* dirPtr = malloc(sizeof(int));
			int* dirEnt = malloc(sizeof(int));

			DirBlock* aboveDirectBlock = malloc(BLOCK_SIZE);
			if(IsInodeFull(aboveInodeNum, dirPtr, dirEnt) == false)
			{
				BufRead(*dirPtr, aboveDirectBlock);/////////////-----------///////////

				aboveDirectBlock->dirEntries[*dirEnt].inodeNum = InodeBitmapSearch();
				strcpy(aboveDirectBlock->dirEntries[*dirEnt].name, nameArray[index - 1]);
				aboveDirectBlock->dirEntries[*dirEnt].type = FILE_TYPE_FILE;

				BufWrite(*dirPtr, aboveDirectBlock);//상위 폴더 다이렉트 엔트리에 추가

				const int newInodeNum = aboveDirectBlock->dirEntries[*dirEnt].inodeNum;
				InodeInfo newInodeBlock[16];

				BufRead(newInodeNum / 16 + 3, newInodeBlock);////////// --------//////////

				newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].blocks = 1;

				for(int num = 0; num < 12; num++)
				{
					newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].directPtr[num] = INVALID_NUMBER;
				}

				newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].directPtr[0] =
						BlockBitmapSearch();

				newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].mode = FILE_MODE_READWRITE;
				newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].size = BLOCK_SIZE;
				newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].type = FILE_TYPE_FILE;

				BufWrite(newInodeNum / 16 + 3, newInodeBlock);/////////--------////////

				char* emptyBlock = (char*)malloc(BLOCK_SIZE);
				for(i = 0; i < BLOCK_SIZE; i++)
				{
					emptyBlock[i] = 0;
				}

				BufWrite(newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].directPtr[0],
						emptyBlock);////-----////

				BlockBitmapSet(newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].directPtr[0]);

				InodeBitmapSet(newInodeNum);

				pFileSysInfo->numAllocBlocks++;
				pFileSysInfo->numAllocInodes++;
				pFileSysInfo->numFreeBlocks--;
				pFileSysInfo->numFreeInodes--;

				//printf("%s 생성 완료 \n", nameArray[index - 1]);
				int tableNum = TableSearch(0);
				if(tableNum == INVALID_NUMBER)
				{
					printf("\nfile Desc Table is full\n");
					return -1;
				}
				else
				{
					pFileDescTable->file[tableNum].inodeNo = newInodeNum;
					pFileDescTable->file[tableNum].offset = 0;
					pFileDescTable->file[tableNum].valid_bit = 1;
				}
				UpdateBlockBitmapInodeList();
				return tableNum;
			}
			else
			{
				//printf("디렉토리가 꽉차서 생성할 수 없음 \n");
				return INVALID_NUMBER;
			}
		}
		else//기존의 파일이 존재하는 경우
		{
			return OpenFile(szFileName, OPEN_FLAG_READWRITE);
		}
	}
}


int		WriteFile(int fileDesc, char* pBuffer, int length)
{
	InodeInfo inode[16];
	int currentOffset = pFileDescTable->file[fileDesc].offset;
	int fileInode = pFileDescTable->file[fileDesc].inodeNo;
	char temp[pFileDescTable->file[fileDesc].offset+length];
	char temp1[BLOCK_SIZE], temp2[BLOCK_SIZE];

	strcpy(temp2, pBuffer);

	BufRead(fileInode / 16 + 3, inode);

	BufRead(inode[fileInode % 16].directPtr[0] , temp1);

	int i = 0;

	for(i = 0; i < currentOffset + length; i++)
	{
		if(currentOffset  + i >= BLOCK_SIZE * 12)
			break;
		if(i < currentOffset)
			temp[i] = temp1[i];
		else
			temp[i] = temp2[i - currentOffset];
	}

	int index = 0;
	int j = 0;

	for(i = 0; i < (currentOffset + length) / BLOCK_SIZE + 1; i++)
	{
		for(j = 0; j < BLOCK_SIZE; j++)
		{
			if(index > (currentOffset + length) / BLOCK_SIZE)
				break;
			temp1[j] = temp[index];
			index ++;
		}

		if(inode[fileInode % 16].directPtr[i] < 11)
		{
			inode[fileInode % 16].directPtr[i] = BlockBitmapSearch();
			BlockBitmapSet(inode[fileInode % 16].directPtr[i]);
			pFileSysInfo->numAllocBlocks++;
			pFileSysInfo->numFreeBlocks--;
			inode[fileInode % 16].blocks++;
			inode[fileInode % 16].size = inode[fileInode % 16].size + BLOCK_SIZE;

			BufWrite(fileInode / 16 + 3, inode);

			BufWrite(inode[fileInode % 16].directPtr[i], temp);
		}
		else
		{
			BufWrite(inode[fileInode % 16].directPtr[i], temp);
		}
	}

	pFileDescTable->file[fileDesc].offset = currentOffset + length;

	return length;
}

int		ReadFile(int fileDesc, char* pBuffer, int length)
{
	InodeInfo inode[16];
	char* temp = malloc(BLOCK_SIZE);
	int fileInode = pFileDescTable->file[fileDesc].inodeNo;

	BufRead(fileInode / 16 + 3, inode);

	BufRead(inode[fileInode % 16].directPtr[0], temp);

	strcpy(pBuffer, temp);

	int nByte = strlen(pBuffer);

	if(nByte >= 0)
	{
		pFileDescTable->file[fileDesc].offset = pFileDescTable->file[fileDesc].offset + length;
		return length;
	}
	else
		return INVALID_NUMBER;
}


int		CloseFile(int fileDesc)
{
	if(pFileDescTable->file[fileDesc].inodeNo == 0 && pFileDescTable->file[fileDesc].offset == 0 && pFileDescTable->file[fileDesc].valid_bit == 0)
		return -1;
	else
	{
		pFileDescTable->file[fileDesc].inodeNo = 0;
		pFileDescTable->file[fileDesc].offset = 0;
		pFileDescTable->file[fileDesc].valid_bit = 0;
		return 1;
	}
}

int		RemoveFile(const char* szFileName)
{
	DirBlock* block = malloc(BLOCK_SIZE);
	DirBlock* DeleteBlock = malloc(BLOCK_SIZE);
	InodeInfo aboveInodeBlock[NUM_OF_INODE_PER_BLK];

	int length = strlen(szFileName);

	char* str = (char*)malloc(length + 1);
	char* nameArray[20];
	char* temp;

	int index = 0;		//nameArray에서 새 디렉토리명 인덱스를 담고있음
	int findIndex = 1;	//Cursor
	int aboveInodeNum = 0;
	int targetInodeNum = INVALID_NUMBER;
	bool deleted = false;

	strncpy(str, szFileName, length); //szDirName -> const

	while(1)
	{
		if(index == 0)
			temp = strtok(str, "/");
		else
			temp = strtok(NULL, "/");
		nameArray[index] = temp;
		if(temp == NULL)
			break;
		index++;
	}

	/*
	 * nameArray[index - 2] = above Directory name
	 *
	 * nameArray[index - 1] = new Directory name
	 */

	int i = 0;


	while(findIndex < index && aboveInodeNum != INVALID_NUMBER)
	{
		BufRead(aboveInodeNum / 16 + 3, aboveInodeBlock);
		for(i = 0; i < NUM_OF_DIRECT_BLK_PTR; i++)
		{
			if(aboveInodeBlock[aboveInodeNum % 16].directPtr[i] < 11)
			{
				continue;
			}

			aboveInodeNum = FindInodeByName(
					aboveInodeBlock[aboveInodeNum % 16].directPtr[i], nameArray[findIndex - 1]);
			if(aboveInodeNum != INVALID_NUMBER)
				break;
		}

		/*
		 * for문 탈출시
		 * 1. 상위 폴더를 찾았을 경우 : aboveInodeNum != INVALID_NUMBER
		 * 2. 못찾았을 경우 : aboveInodeNum == INVALID_NUMBER
		 */

		findIndex++;
	}
	/*
	 * while문 탈출
	 * 1. 상위 폴더까지 찾음 --> aboveInodeNum = 상위폴더의 inodeNum
	 * 2. 못찾음 --> aboveInodeNum = INVALID_NUMBER --> 에러메시지 출력 후 리턴
	 */
	if(aboveInodeNum == INVALID_NUMBER) //while문 탈출 case 2
	{
		//printf("\n올바른 경로가 아닙니다\n");
		return INVALID_NUMBER;
	}

	for(i = 0; i < NUM_OF_DIRENT_PER_BLK; i++)
	{
		int currentBlock = aboveInodeBlock[aboveInodeNum % NUM_OF_INODE_PER_BLK].directPtr[i];
		if(currentBlock < 11)
			continue;
		else
		{
			if(FindInodeByName(currentBlock, nameArray[index-1]) > 0)
			{

				BufRead(currentBlock, block);
				int j;
				for(j = 0; j < 16; j++)
				{
					if(strncmp(block->dirEntries[j].name, nameArray[index-1], strlen(nameArray[index-1])) == 0)
					{
						if(DeleteByInode(block->dirEntries[j].inodeNum, aboveInodeNum) == 0)
						{
							deleted = true;
							break;
						}
						else
							printf("삭제 오류 \n");
					}
				}
				if(deleted)
				{
					UpdateBlockBitmapInodeList();
					return 0;
				}
			}
		}
	}

	return INVALID_NUMBER;
}


int		MakeDir(const char* szDirName)
{
	InodeInfo aboveInodeBlock[NUM_OF_INODE_PER_BLK];
	DirBlock* newDirectBlock = malloc(BLOCK_SIZE);
	DirBlock* aboveDirectBlock = malloc(BLOCK_SIZE);

	int length = strlen(szDirName);

	char* str = (char*)malloc(length + 1);
	char* nameArray[20];
	char* temp;

	int index = 0;		//nameArray에서 새 디렉토리명 인덱스를 담고있음
	int findIndex = 1;	//Cursor
	int aboveInodeNum = 0;

	strncpy(str, szDirName, length); //szDirName -> const

	while(1)
	{
		if(index == 0)
			temp = strtok(str, "/");
		else
			temp = strtok(NULL, "/");
		nameArray[index] = temp;
		if(temp == NULL)
			break;
		index++;
	}

	/*
	 * nameArray[index - 2] = above Directory name
	 *
	 * nameArray[index - 1] = new Directory name
	 */

	int i = 0;

	while(findIndex < index && aboveInodeNum != INVALID_NUMBER)
	{
		BufRead(aboveInodeNum / 16 + 3, aboveInodeBlock);
		for(i = 0; i < NUM_OF_DIRECT_BLK_PTR; i++)
		{
			if(aboveInodeBlock[aboveInodeNum % 16].directPtr[i] < 11)
			{
				continue;
			}

			aboveInodeNum = FindInodeByName(
					aboveInodeBlock[aboveInodeNum % 16].directPtr[i], nameArray[findIndex - 1]);
			if(aboveInodeNum != INVALID_NUMBER)
				break;
		}

		/*
		 * for문 탈출시
		 * 1. 상위 폴더를 찾았을 경우 : aboveInodeNum != INVALID_NUMBER
		 * 2. 못찾았을 경우 : aboveInodeNum == INVALID_NUMBER
		 */

		findIndex++;
	}
	/*
	 * while문 탈출
	 * 1. 상위 폴더까지 찾음 --> aboveInodeNum = 상위폴더의 inodeNum
	 * 2. 못찾음 --> aboveInodeNum = INVALID_NUMBER --> 에러메시지 출력 후 리턴
	 */
	if(aboveInodeNum == INVALID_NUMBER) //while문 탈출 case 2
	{
		//printf("\n올바른 경로가 아닙니다\n");
		return INVALID_NUMBER;
	}

	//상위 폴더에 같은 이름이 있는지 찾기

	BufRead(aboveInodeNum / 16 + 3, aboveInodeBlock);

	for(i = 0; i < NUM_OF_DIRENT_PER_BLK; i++)
	{
		int currentBlock = aboveInodeBlock[aboveInodeNum % NUM_OF_INODE_PER_BLK].directPtr[i];
		if(currentBlock < 11)
			continue;
		else
		{
			if(FindInodeByName(currentBlock, nameArray[index-1]) >= 0)
			{
				//printf("해당 디렉토리는 이미 존재합니다. 생성실패...\n");
				return INVALID_NUMBER;
			}
		}
	}	//for문을 탈출했다면 해당 이름의 디렉토리가 존재하지 않음

	int* dirPtr = malloc(sizeof(int));
	int* dirEnt = malloc(sizeof(int));

	if(IsInodeFull(aboveInodeNum, dirPtr, dirEnt) == false)
	{
		BufRead(*dirPtr, aboveDirectBlock);/////////////-----------///////////

		aboveDirectBlock->dirEntries[*dirEnt].inodeNum = InodeBitmapSearch();
		strcpy(aboveDirectBlock->dirEntries[*dirEnt].name, nameArray[index - 1]);
		aboveDirectBlock->dirEntries[*dirEnt].type = FILE_TYPE_DIR;

		BufWrite(*dirPtr, aboveDirectBlock);//상위 폴더 다이렉트 엔트리에 추가

		const int newInodeNum = aboveDirectBlock->dirEntries[*dirEnt].inodeNum;
		InodeInfo newInodeBlock[16];

		BufRead(newInodeNum / 16 + 3, newInodeBlock);////////// --------//////////

		newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].blocks = 1;

		for(int num = 0; num < 12; num++)
		{
			newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].directPtr[num] = INVALID_NUMBER;
		}

		newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].directPtr[0] =
				BlockBitmapSearch();

		newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].mode = FILE_MODE_READWRITE;
		newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].size = BLOCK_SIZE;
		newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].type = FILE_TYPE_DIR;

		BufWrite(newInodeNum / 16 + 3, newInodeBlock);/////////--------////////

		newDirectBlock->dirEntries[0].inodeNum = newInodeNum;
		strcpy(newDirectBlock->dirEntries[0].name, ".");
		newDirectBlock->dirEntries[0].type = FILE_TYPE_DIR;

		newDirectBlock->dirEntries[1].inodeNum = aboveInodeNum;
		strcpy(newDirectBlock->dirEntries[1].name, "..");
		newDirectBlock->dirEntries[1].type = FILE_TYPE_DIR;

		for(i = 2; i < 16; i++)
		{
			newDirectBlock->dirEntries[i].inodeNum = INVALID_NUMBER;
			newDirectBlock->dirEntries[i].type = INVALID_NUMBER;
		}

		BufWrite(newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].directPtr[0],
				newDirectBlock);////-----////

		BlockBitmapSet(newInodeBlock[newInodeNum % NUM_OF_INODE_PER_BLK].directPtr[0]);

		InodeBitmapSet(newInodeNum);

		pFileSysInfo->numAllocBlocks++;
		pFileSysInfo->numAllocInodes++;
		pFileSysInfo->numFreeBlocks--;
		pFileSysInfo->numFreeInodes--;

		//printf("%s 생성 완료 \n", nameArray[index - 1]);

		UpdateBlockBitmapInodeList();
		return 0;
	}
	else
	{
		//printf("디렉토리가 꽉차서 생성할 수 없음 \n");
		return INVALID_NUMBER;
	}
}

/*
 *this function is made to find directory's or file's inode by its name
 *
 *returns Inode --> Success
 *
 *returns INVALID_NUMBER(-1) when searching is failed
 */
int FindInodeByName(int blkno, const char* name)
{
	DirBlock* targetBlk = malloc(sizeof(DirBlock));
	BufRead(blkno, targetBlk);
	for(int i = 0; i < NUM_OF_DIRENT_PER_BLK; i++)
	{
		if(strncmp(targetBlk->dirEntries[i].name, name, strlen(name)) == 0)
		{
			return targetBlk->dirEntries[i].inodeNum;
		}
	}

	return INVALID_NUMBER;
}

/*
 * this function is made to identify
 * whether one inode's directptrs and
 * their dirents are full or not
 * And when inode is not full,
 * this saves IndexOf ptr, dirent
 * to output parameter
 */
bool IsInodeFull(int inodeNum, int* dirptr, int* dirent)
{
	InodeInfo inodeBlock[16];
	int inodeBlockNum = inodeNum / 16 + 3;
	int i = 0, j = 0;

	DirBlock* block = malloc(BLOCK_SIZE);
	InodeInfo inode;
	bool found = false;
	bool empty = false;

	BufRead(inodeBlockNum, inodeBlock);

	inode = inodeBlock[inodeNum % 16];

	for(i = 0; i < 12; i++)
	{
		if(inode.directPtr[i] < 11)
		{
			empty = true;
			continue;
		}
		else
		{
			BufRead(inode.directPtr[i], block);
			for(j = 0; j < 16; j++)
			{
				if(block->dirEntries[j].name[0] == 0)
				{
					found = true;
					break;
				}
			}
			if(found == true)
				break;
		}
	}

	*dirptr = inode.directPtr[i];
	*dirent = j;

	if(!found && !empty)
	{
		*dirptr = INVALID_NUMBER;
		*dirent = INVALID_NUMBER;
		return true;
	}
	else if(!found && empty)
	{
		for(i = 0; i < 12; i++)
		{
			if(inode.directPtr[i] == INVALID_NUMBER)
			{
				inodeBlock[inodeNum % 16].directPtr[i] = BlockBitmapSearch();
				inodeBlock[inodeNum % 16].blocks++;
				inodeBlock[inodeNum % 16].size = inodeBlock[inodeNum % 16].size + BLOCK_SIZE;
				BufWrite(inodeNum / 16 + 3, inodeBlock);
				pFileSysInfo->numAllocBlocks++;
				pFileSysInfo->numFreeBlocks--;

				BlockBitmapSet(inodeBlock[inodeNum % 16].directPtr[i]);

				*dirptr = inodeBlock[inodeNum % 16].directPtr[i];
				*dirent = 0;

				break;
			}
		}
	}

	UpdateBlockBitmapInodeList();
	return false;
}

int		RemoveDir(const char* szDirName)
{
	DirBlock* block = malloc(BLOCK_SIZE);
	InodeInfo aboveInodeBlock[NUM_OF_INODE_PER_BLK];

	int length = strlen(szDirName);

	char* str = (char*)malloc(length + 1);
	char* nameArray[20];
	char* temp;

	int index = 0;		//nameArray에서 새 디렉토리명 인덱스를 담고있음
	int findIndex = 1;	//Cursor
	int aboveInodeNum = 0;
	bool deleted = false;

	strncpy(str, szDirName, length); //szDirName -> const

	while(1)
	{
		if(index == 0)
			temp = strtok(str, "/");
		else
			temp = strtok(NULL, "/");
		nameArray[index] = temp;
		if(temp == NULL)
			break;
		index++;
	}

	/*
	 * nameArray[index - 2] = above Directory name
	 *
	 * nameArray[index - 1] = new Directory name
	 */

	int i = 0;

	while(findIndex < index && aboveInodeNum != INVALID_NUMBER)
	{
		BufRead(aboveInodeNum / 16 + 3, aboveInodeBlock);
		for(i = 0; i < NUM_OF_DIRECT_BLK_PTR; i++)
		{
			if(aboveInodeBlock[aboveInodeNum % 16].directPtr[i] < 11)
			{
				continue;
			}

			aboveInodeNum = FindInodeByName(
					aboveInodeBlock[aboveInodeNum % 16].directPtr[i], nameArray[findIndex - 1]);
			if(aboveInodeNum != INVALID_NUMBER)
				break;
		}

		/*
		 * for문 탈출시
		 * 1. 상위 폴더를 찾았을 경우 : aboveInodeNum != INVALID_NUMBER
		 * 2. 못찾았을 경우 : aboveInodeNum == INVALID_NUMBER
		 */

		findIndex++;
	}
	/*
	 * while문 탈출
	 * 1. 상위 폴더까지 찾음 --> aboveInodeNum = 상위폴더의 inodeNum
	 * 2. 못찾음 --> aboveInodeNum = INVALID_NUMBER --> 에러메시지 출력 후 리턴
	 */
	if(aboveInodeNum == INVALID_NUMBER) //while문 탈출 case 2
	{
		//printf("\n올바른 경로가 아닙니다\n");
		return INVALID_NUMBER;
	}

	//상위 폴더에 삭제할 디렉토리가 있는지 확인

	BufRead(aboveInodeNum / 16 + 3, aboveInodeBlock);

	for(i = 0; i < NUM_OF_DIRENT_PER_BLK; i++)
	{
		int currentBlock = aboveInodeBlock[aboveInodeNum % NUM_OF_INODE_PER_BLK].directPtr[i];
		if(currentBlock < 11)
			continue;
		else
		{
			if(FindInodeByName(currentBlock, nameArray[index-1]) > 0)
			{

				BufRead(currentBlock, block);
				int j;
				for(j = 0; j < 16; j++)
				{
					if(strncmp(block->dirEntries[j].name, nameArray[index-1], strlen(nameArray[index-1])) == 0)
					{
						if(DeleteByInode(block->dirEntries[j].inodeNum, aboveInodeNum) == 0)
						{
							deleted = true;
							break;
						}
						else
							printf("삭제 오류 \n");
					}
				}
				if(deleted)
				{
					UpdateBlockBitmapInodeList();
					return 0;
				}
			}
		}
	}

	return INVALID_NUMBER;
}

int DeleteByInode(int inodeNum, int aboveInodeNum)
{
	InodeInfo aboveInode[16];
	InodeInfo targetInode[16];
	DirBlock* block = malloc(BLOCK_SIZE);
	int i = 0;

	bool found = false;

	BufRead(inodeNum / 16 + 3, targetInode);

	if(targetInode[inodeNum % 16].blocks != 1)
		return INVALID_NUMBER;
	else
	{
		targetInode[inodeNum % 16].blocks = 0;
		for(i = 0; i < 12; i++)//directPtr 초기화
		{
			if(targetInode[inodeNum % 16].directPtr[i] > 10)
			{
				for(int j = 0; j < NUM_OF_DIRENT_PER_BLK; j ++)
				{
					block->dirEntries[j].inodeNum = INVALID_NUMBER;
					memset(block->dirEntries[j].name, 0, NAME_LEN_MAX);
					block->dirEntries[j].type = INVALID_NUMBER;
				}
				BufWrite(targetInode[inodeNum % 16].directPtr[i], block);
				BlockBitmapReset(targetInode[inodeNum % 16].directPtr[i]);
				pFileSysInfo->numAllocBlocks--;
				pFileSysInfo->numFreeBlocks++;
			}
			targetInode[inodeNum % 16].directPtr[i] = INVALID_NUMBER;
		}
		targetInode[inodeNum % 16].mode = INVALID_NUMBER;
		targetInode[inodeNum % 16].size = 0;
		targetInode[inodeNum % 16].type = INVALID_NUMBER;

		BufWrite(inodeNum / 16 + 3, targetInode);

		InodeBitmapReset(inodeNum);

		pFileSysInfo->numAllocInodes--;
		pFileSysInfo->numFreeInodes++;
		///상위폴더 정리

		BufRead(aboveInodeNum / 16 + 3 , aboveInode);
		for(i = 0; i < 12; i++)
		{
			BufRead(aboveInode[aboveInodeNum % 16].directPtr[i], block);
			for(int j = 0; j < 16; j++)
			{
				if(block->dirEntries[j].inodeNum == inodeNum)
				{
					block->dirEntries[j].inodeNum = INVALID_NUMBER;
					memset(block->dirEntries[j].name, 0, NAME_LEN_MAX);
					block->dirEntries[j].type = INVALID_NUMBER;

					BufWrite(aboveInode[aboveInodeNum % 16].directPtr[i], block);

					found = true;

					bool empty = true;

					for(int k = 0; k < 16; k++)
					{
						if(block->dirEntries[k].name[0] != 0)
							empty = false;
					}

					if(empty)
					{
						for(int k = 0; k < 16; k++)
						{
							block->dirEntries[k].inodeNum = INVALID_NUMBER;
							memset(block->dirEntries[k].name, 0, NAME_LEN_MAX);
							block->dirEntries[k].type = INVALID_NUMBER;
						}
						BufWrite(aboveInode[aboveInodeNum % 16].directPtr[i], block);

						BlockBitmapReset(aboveInode[aboveInodeNum % 16].directPtr[i]);
						BufWrite(2, pFileSysInfo->pBlockBitmap);

						aboveInode[aboveInodeNum % 16].directPtr[i] = INVALID_NUMBER;
						aboveInode[aboveInodeNum % 16].blocks--;
						aboveInode[aboveInodeNum % 16].size =
								aboveInode[aboveInodeNum % 16].size - BLOCK_SIZE;

						BufWrite(aboveInodeNum / 16 + 3 , aboveInode);
					}

					break;
				}
			}

			if(found)
				break;
		}
		UpdateBlockBitmapInodeList();
		return 0;
	}
}

void	EnumerateDirStatus(const char* szDirName, DirEntry* pDirEntry, int* pNum)
{
	DirBlock block;
	DirEntry entry[100];

	InodeInfo inodeBlock[16];

	int targetInodeNum = 0;
	int num = 0;
	int index = 0;
	int findIndex = 0;

	int length = strlen(szDirName);

	char* str = (char*)malloc(length + 1);
	char* nameArray[20];
	char* temp;

	strncpy(str, szDirName, length); //szDirName -> const

	while(1)
	{
		if(index == 0)
			temp = strtok(str, "/");
		else
			temp = strtok(NULL, "/");
		nameArray[index] = temp;
		if(temp == NULL)
			break;
		index++;
	}

	while(findIndex < index && targetInodeNum != INVALID_NUMBER)
	{
		BufRead(targetInodeNum / 16 + 3, inodeBlock);
		for(int i = 0; i < NUM_OF_DIRECT_BLK_PTR; i++)
		{
			if(inodeBlock[targetInodeNum % 16].directPtr[i] < 11)
			{
				continue;
			}

			targetInodeNum = FindInodeByName(
					inodeBlock[targetInodeNum % 16].directPtr[i], nameArray[findIndex]);
			if(targetInodeNum != INVALID_NUMBER)
				break;
		}

		/*
		 * for문 탈출시
		 * 1. 상위 폴더를 찾았을 경우 : aboveInodeNum != INVALID_NUMBER
		 * 2. 못찾았을 경우 : aboveInodeNum == INVALID_NUMBER
		 */

		findIndex++;
	}
	/*
	 * while문 탈출
	 * 1. 상위 폴더까지 찾음 --> aboveInodeNum = 상위폴더의 inodeNum
	 * 2. 못찾음 --> aboveInodeNum = INVALID_NUMBER --> 에러메시지 출력 후 리턴
	 */

	if(targetInodeNum == INVALID_NUMBER)
	{
		printf("Enumerate ERR\n\n");
		return;
	}

	//PrintInodeStatus(targetInodeNum);

	index = 0;

	BufRead(targetInodeNum / 16 + 3, inodeBlock);

	for(int i = 0; i < 12; i++)
	{
		if(inodeBlock[targetInodeNum % 16].directPtr[i] > 10)
		{
			BufRead(inodeBlock[targetInodeNum % 16].directPtr[i], &block);
			for(int j = 0; j < 16; j ++)
			{
				if(block.dirEntries[j].name[0] != 0)
				{
					entry[index] = block.dirEntries[j];
					index++;
					num++;
				}
			}
		}
	}

	*pNum = num;

	for(int i = 0; i < 100 ; i++)
		pDirEntry[i] = entry[i];
}

/*
 * Functions for bitmap
 */
void InodeBitmapSet(int blkno)
{
	char temp=(blkno)%8;
	if(InodeBitmapGet(blkno)==0)
	{
		pFileSysInfo->pInodeBitmap[(blkno)/8]|=0x80>>temp;
	}
}

void BlockBitmapSet(int blkno)
{
	char temp=(blkno)%8;
	if(BlockBitmapGet(blkno)==0)
	{
		pFileSysInfo->pBlockBitmap[(blkno)/8]|=0x80>>temp;
	}
}

int InodeBitmapGet(int blkno)
{
	char temp=(blkno)%8;
	char result=pFileSysInfo->pInodeBitmap[(blkno)/8]&0x80>>temp;
	if(result==0)
		return 0;
	else
		return 1;
}

int BlockBitmapGet(int blkno)
{
	char temp=(blkno)%8;
	char result=pFileSysInfo->pBlockBitmap[(blkno)/8]&0x80>>temp;
	if(result==0)
		return 0;
	else
		return 1;
}

void InodeBitmapReset(int blkno)
{
	char temp=(blkno)%8;
	pFileSysInfo->pInodeBitmap[(blkno)/8]&=(unsigned char)(~0x80>>temp);
	//pFileSysInfo->pInodeBitmap[blkno / BIT] |= (1 << (blkno % BIT));
}

void BlockBitmapReset(int blkno)
{
	char temp=(blkno)%8;
	pFileSysInfo->pBlockBitmap[(blkno)/8]&=(unsigned char)(~0x80>>temp);
}

int InodeBitmapSearch()
{
	int i = 0;
	for(i = 1; i < FS_INODE_COUNT; i++)
		if(InodeBitmapGet(i) == 0)
			return i;
	return INVALID_NUMBER;
}

int BlockBitmapSearch()
{
	int i = 0;
	for(i = 12; i < FS_DISK_CAPACITY / BLOCK_SIZE; i++)
		if(BlockBitmapGet(i) == 0)
			return i;
	return INVALID_NUMBER;
}

int TableSearch(int inodeNum)
{
	int i = 0;
	for(i = 0 ; i < FS_INODE_COUNT; i++)
	{
		if(	pFileDescTable->file[i].inodeNo == inodeNum)
			return i;
	}

	return INVALID_NUMBER;
}

void UpdateBlockBitmapInodeList()
{
	int i = 0,j = 0;
	bool value = false;

	for(i = 3; i < 11; i++)
	{
		for(j = 16 * (i - 3) ; j < 16 * (i - 2); j ++)
		{
			if(InodeBitmapGet(j) == true)
				value = true;
		}

		if(value == true)
			BlockBitmapSet(i);
		else
			BlockBitmapReset(i);

		value  = false;
	}
//
//	if(InodeBitmapGet(0) || InodeBitmapGet(1) || InodeBitmapGet(2) || InodeBitmapGet(3) || InodeBitmapGet(4) || InodeBitmapGet(5) || InodeBitmapGet(6) || InodeBitmapGet(7) ||
//			InodeBitmapGet(8) || InodeBitmapGet(9) || InodeBitmapGet(10) || InodeBitmapGet(11) || InodeBitmapGet(12) || InodeBitmapGet(13) || InodeBitmapGet(14) || InodeBitmapGet(15))
//		BlockBitmapSet(3);
//	if(InodeBitmapGet(16) || InodeBitmapGet(17) || InodeBitmapGet(18) || InodeBitmapGet(19) || InodeBitmapGet(20) || InodeBitmapGet(21) || InodeBitmapGet(22) || InodeBitmapGet(23) ||
//			InodeBitmapGet(24) || InodeBitmapGet(25) || InodeBitmapGet(26) || InodeBitmapGet(27) || InodeBitmapGet(28) || InodeBitmapGet(29) || InodeBitmapGet(30) || InodeBitmapGet(31))
//		BlockBitmapSet(4);
//
}

void PrintInodeStatus()
{
	int i;
	printf("\nInode Bitmap Status : ");
	for(i = 0 ; i < FS_INODE_COUNT; i++)
	{
		printf("%d:%d ",i,InodeBitmapGet(i));
	}
}

void PrintBlockStatus()
{
	int i;
	int temp;
	printf("\nBlock Bitmap Status : ");
	for(i = 0 ; i < FS_INODE_COUNT * 2; i++)
	{
		temp = BlockBitmapGet(i);
		printf("%d:%d ",i,temp);
//		if(i > 10 && temp == 1)
//		{
//			PrintDirBlockEntries(i);
//			printf("\n");
//		}
		if(i % 128 == 0)
			printf("\n");
	}
}

void PrintDirBlockEntries(int blockNum)
{
	DirBlock * block = malloc(BLOCK_SIZE);
	BufRead(blockNum, block);

	printf("\n======================Block [%d] Info=======================", blockNum);
	int i = 0;
	for(i = 0; i < 16; i ++)
	{
		printf("\nDirEntry%d\n", i);
		printf("Inode:%d\n", block->dirEntries[i].inodeNum);
		printf("name:%s\n", block->dirEntries[i].name);
		printf("FileType:%d\n", block->dirEntries[i].type);
	}

	printf("=============================================================\n");
}

void PrintInodeInfo(int inodeNum)
{
	InodeInfo inode[16];
	BufRead(inodeNum / 16 + 3, inode);
	int i;
	printf("\nInode DirPtrInfo\n");
	for(i = 0 ; i < 12; i++)
		printf("%d ",inode[inodeNum % 16].directPtr[i]);
	printf("\n");
}
