#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/sem.h>

#include "hw2.h"
#include "Disk.h"


#define STACK_SIZE 1024

#define CLONE_VM        0x00000100      /* set if VM shared between processes */
#define CLONE_FS        0x00000200      /* set if fs info shared between processes */
#define CLONE_FILES     0x00000400      /* set if open files shared between processes */
#define CLONE_SIGHAND   0x00000800      /* set if signal handlers and blocked signals shared */


int childPid = 0;	//데몬의 pid가 들어갈 전역변수(BufInit에서 초기화 후 다른 함수에서도
//접근가능하도록 전역변수로 설정
int flag = TRUE;

void BufInsertToHead(Buf* pBuf, int blkno, BufList listNum);
void BufInsertToTail(Buf* pBuf, int blkno, BufList listNum);
Buf* BufFind(int blkno);
Buf* BufGetNewBuffer(void);
void BufDeleteBuf(Buf* pBuf);
void BufMoveToCleanList(Buf* pBuf);
void Initialize_Buffer(Buf* pBuf);
void InsertToFreeList(Buf* pBuf);
void MoveToLruTail(Buf* pBuf);
void BufDaemon(void);
void LockMainThread()
{
	while(flag)
	{
		//waiting...
	}
}
void UnlockMainThread()
{
	flag = FALSE;
}
void BufFinish();



///////////////////////////기존함수들

void BufInit(void)
{
	int index = 0;
	Buf* pBuf[10];
	char* pStack = malloc(STACK_SIZE);
	int flags = SIGCHLD|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_VM;

	DevOpenDisk();

	for(index = 0; index < 10; index++) //버퍼를 초기화
	{
		pBuf[index] = malloc(sizeof(Buf));
		pBuf[index]->pMem = malloc(BLOCK_SIZE);
		memset(pBuf[index]->pMem, 0 , BLOCK_SIZE);
		Initialize_Buffer(pBuf[index]);
		InsertToFreeList(pBuf[index]);
	}

	childPid = clone(BufDaemon, (char*)pStack + STACK_SIZE, flags, NULL);
	LockMainThread();
	flag = TRUE;
}

void BufRead(int blkno, void* pData)
//BufFind()를 사용한다.
//hit가 발생한 경우 해당 버퍼를 lruList의 tail로 이동시킨다.
{
	Buf* Target = BufFind(blkno);

	if(Target != NULL)
	{
		memcpy(pData, Target->pMem, BLOCK_SIZE);
		MoveToLruTail(Target);
	}
	else
	{
		Target = BufGetNewBuffer();
		Target->blkno = blkno;
		BufInsertToTail(Target, blkno, BUF_LIST_CLEAN);
		DevReadBlock(blkno, Target->pMem);
		memcpy(pData, Target->pMem, BLOCK_SIZE);
	}
}

void BufWrite(int blkno, void* pData)
{
	Buf* Target = BufFind(blkno);

	if(Target != NULL)
	{
		memcpy(Target->pMem, pData, BLOCK_SIZE);
		MoveToLruTail(Target);
		if(Target->state == BUF_STATE_CLEAN)
		{
			BufDeleteBuf(Target);
			Target->state = BUF_STATE_DIRTY;
			BufInsertToTail(Target, blkno, BUF_LIST_DIRTY);
		}
	}
	else
	{
		Target = BufGetNewBuffer();
		Target->state = BUF_STATE_DIRTY;
		memcpy(Target->pMem, pData, BLOCK_SIZE);
		BufInsertToTail(Target, blkno, BUF_LIST_DIRTY);
	}
}

void MemoryCopy(void* pTarget, void* pData)
{
	int i;
	for(i = 0; i < BLOCK_SIZE; i++)
	{
		*((char*)pTarget + i) = *((char*)pData + i);
	}
}

void BufSync(void)
{
	Buf* Target = ppObjListHead[BUF_LIST_DIRTY];
	Buf* SaveTarget;
	int blkno;

	while(Target != NULL)
	{
		Target = ppObjListHead[BUF_LIST_DIRTY];
		blkno = Target->blkno;
		SaveTarget = Target;
		DevWriteBlock(blkno, SaveTarget->pMem);
		Target = Target->poNext;
		BufMoveToCleanList(SaveTarget);
		SaveTarget->state = BUF_STATE_CLEAN;
	}
}

void Initialize_Buffer(Buf* pBuf)
{
	pBuf->atime = 0;
	pBuf->blkno = BLKNO_INVALID;
	pBuf->state = BUF_STATE_CLEAN;
	pBuf->phNext = NULL;
	pBuf->phPrev = NULL;
	pBuf->plNext = NULL;
	pBuf->plPrev = NULL;
	pBuf->poNext = NULL;
	pBuf->poPrev = NULL;
}

void InsertToFreeList(Buf* pBuf)
{
	if(pBuf == NULL)
	{
		printf("pBuf가 널입니다\n");
		return;
	}
	if((ppObjListHead[BUF_LIST_FREE] == NULL) && (ppObjListTail[BUF_LIST_FREE] == NULL))
	{
		Initialize_Buffer(pBuf);
		ppObjListHead[BUF_LIST_FREE] = ppObjListTail[BUF_LIST_FREE] = pBuf;
	}
	else
	{
		Initialize_Buffer(pBuf);
		ppObjListTail[BUF_LIST_FREE]->poNext = pBuf;
		pBuf->poPrev = ppObjListTail[BUF_LIST_FREE];
		ppObjListTail[BUF_LIST_FREE] = pBuf;
	}
}

void MoveToLruTail(Buf* pBuf)
{
	if((pBuf == pLruListHead) && (pBuf == pLruListTail))
	{
		//아무행동도 하지 않음
	}
	else if(pBuf == pLruListHead)
	{
		pLruListHead = pBuf->plNext;
		pLruListHead->plPrev = NULL;
		pLruListTail->plNext = pBuf;
		pBuf->plPrev = pLruListTail;
		pBuf->plNext = NULL;
		pLruListTail = pBuf;
	}
	else if(pBuf == pLruListTail)
	{
		//아무행동도 하지 않아도됨
	}
	else
	{
		pBuf->plPrev->plNext = pBuf->plNext;
		pBuf->plNext->plPrev = pBuf->plPrev;
		pLruListTail->plNext = pBuf;
		pBuf->plPrev = pLruListTail;
		pBuf->plNext = NULL;
		pLruListTail = pBuf;
	}
}

void BufInsertToHead(Buf* pBuf, int blkno, BufList listNum)
//InsertObjectToHead --> Hash의 tail, ObjectList의 tail로
{
	int HashTblNum = blkno % HASH_TBL_SIZE;

	if(pBuf == NULL)
	{
		printf("\npBuf가 널값입니다\n");
		return;
	}

	pBuf->blkno = blkno;

	//해시테이블 삽입
	if((ppHashTail[HashTblNum] == NULL) && (ppHashHead[HashTblNum] == NULL))
	{
		//해시테이블이 비었을 경우
		ppHashHead[HashTblNum] = ppHashTail[HashTblNum] = pBuf;
	}
	else
	{
		ppHashTail[HashTblNum]->phNext = pBuf;
		pBuf->phPrev = ppHashTail[HashTblNum];
		ppHashTail[HashTblNum] = pBuf;
	}

	//list tail
	if((ppObjListHead[listNum] == NULL) && (ppObjListTail[listNum] == NULL))
	{
		ppObjListHead[listNum] = ppObjListTail[listNum] = pBuf;
	}
	else
	{
		ppObjListTail[listNum]->poNext = pBuf;
		pBuf->poPrev = ppObjListTail[listNum];
		ppObjListTail[listNum] = pBuf;
	}

	//LRU List에 삽입
	if((pLruListHead == NULL) && (pLruListTail == NULL))
	{
		pLruListHead = pLruListTail = pBuf;
	}
	else
	{
		pLruListTail->plNext = pBuf;
		pBuf->plPrev = pLruListTail;
		pLruListTail = pBuf;
	}
}

void BufInsertToTail(Buf* pBuf, int blkno, BufList listNum)
//InsertObjectToTail --> Hash head, ObjList Tail
{
	int HashTblNum = blkno % HASH_TBL_SIZE;

	if(pBuf == NULL)
	{
		printf("\npBuf가 널값입니다.\n");
		return;
	}

	pBuf->blkno = blkno;

	//해시테이블 head에 삽입
	if((ppHashHead[HashTblNum] == NULL) && (ppHashTail[HashTblNum] == NULL))
	{
		ppHashHead[HashTblNum] = ppHashTail[HashTblNum] = pBuf;
	}
	else
	{
		ppHashHead[HashTblNum]->phPrev = pBuf;
		pBuf->phNext = ppHashHead[HashTblNum];
		ppHashHead[HashTblNum] = pBuf;
	}

	//list tail
	if((ppObjListHead[listNum] == NULL) && (ppObjListTail[listNum] == NULL))
	{
		ppObjListHead[listNum] = ppObjListTail[listNum] = pBuf;
	}
	else
	{
		ppObjListTail[listNum]->poNext = pBuf;
		pBuf->poPrev = ppObjListTail[listNum];
		ppObjListTail[listNum] = pBuf;
	}

	//LRU List에 삽입
	if((pLruListHead == NULL) && (pLruListTail == NULL))
	{
		pLruListHead = pLruListTail = pBuf;
	}
	else
	{
		pLruListTail->plNext = pBuf;
		pBuf->plPrev = pLruListTail;
		pLruListTail = pBuf;
	}
}

Buf* BufFind(int blkno) //GetObject
{
	if(pLruListHead == NULL)
		return NULL;
	Buf* Cursor = pLruListHead;
	while(Cursor->blkno != blkno)
	{
		Cursor = Cursor->plNext;

		if(Cursor == NULL)
			break;
	}

	//반복문을 탈출한 경우는 두가지
	//1. 블럭넘버를 찾았다. --> 블럭 리턴
	//2. 찾지 못하고 lru리스트의 끝에 도달했다. --> 널 리턴

	return Cursor;
}

Buf* BufGetNewBuffer(void) //GetNewObjectFromFreeList/////
{
	if((ppObjListHead[BUF_LIST_FREE] == NULL) || (ppObjListTail[BUF_LIST_FREE] == NULL))
	{
		return NULL;
	}

	Buf* newTail = ppObjListTail[BUF_LIST_FREE]->poPrev;
	Buf* returnBuf = NULL;

	returnBuf = ppObjListTail[BUF_LIST_FREE];

	if(newTail == NULL)
	{
		Initialize_Buffer(returnBuf);
		ppObjListHead[BUF_LIST_FREE] = ppObjListTail[BUF_LIST_FREE] = NULL;
	}
	else
	{
		Initialize_Buffer(returnBuf);
		newTail->poNext = NULL;
		ppObjListTail[BUF_LIST_FREE] = newTail;
	}

	if(GetNumOfBuffersInFreeList() == 1)
	{
		usleep(300);
		kill(childPid, SIGCONT);
		LockMainThread();
		flag = TRUE;
	}

	return returnBuf;
}

void BufDeleteBuf(Buf* pBuf) //DeleteObject
{
	if(pBuf == NULL)
	{
		printf("삭제 실패!\n");
		return;
	}

	int HashTblNum = pBuf->blkno % HASH_TBL_SIZE;
	int ObjListNum;

	Buf* oPrevBuf = pBuf->poPrev;
	Buf* oNextBuf = pBuf->poNext;
	Buf* hPrevBuf = pBuf->phPrev;
	Buf* hNextBuf = pBuf->phNext;


	if(pBuf->state == BUF_STATE_CLEAN)
	{
		ObjListNum = BUF_LIST_CLEAN;
	}
	else
	{
		ObjListNum = BUF_LIST_DIRTY;
	}

	if((pBuf == ppObjListHead[ObjListNum]) && (pBuf == ppObjListTail[ObjListNum]))
	{
		ppObjListHead[ObjListNum] = ppObjListTail[ObjListNum] = NULL;
	}
	else if(pBuf == ppObjListHead[ObjListNum])
	{
		oNextBuf->poPrev = NULL;
		ppObjListHead[ObjListNum] = oNextBuf;
	}
	else if(pBuf == ppObjListTail[ObjListNum])
	{
		oPrevBuf->poNext = NULL;
		ppObjListTail[ObjListNum] = oPrevBuf;
	}
	else
	{
		oPrevBuf->poNext = oNextBuf;
		oNextBuf->poPrev = oPrevBuf;
	}

	if((pBuf == ppHashHead[HashTblNum]) && (pBuf == ppHashTail[HashTblNum]))
	{
		ppHashHead[HashTblNum] = ppHashTail[HashTblNum] = NULL;
	}
	else if(pBuf == ppHashHead[HashTblNum])
	{
		hNextBuf->phPrev = NULL;
		ppHashHead[HashTblNum] = hNextBuf;
	}
	else if(pBuf == ppHashTail[HashTblNum])
	{
		hPrevBuf->phNext = NULL;
		ppHashTail[HashTblNum] = hPrevBuf;
	}
	else
	{
		hPrevBuf->phNext = hNextBuf;
		hNextBuf->phPrev = hPrevBuf;
	}

	//lru

	if((pBuf == pLruListHead)&& (pBuf == pLruListTail))
	{
		pLruListHead = pLruListTail = NULL;
	}
	else if(pBuf == pLruListHead)
	{
		pLruListHead = pBuf->plNext;
		pLruListHead->plPrev = NULL;
	}
	else if(pBuf == pLruListTail)
	{
		pLruListTail = pBuf->plPrev;
		pLruListTail->plNext = NULL;
	}
	else
	{
		pBuf->plPrev->plNext = pBuf->plNext;
		pBuf->plNext->plPrev = pBuf->plPrev;
	}

	pBuf->phNext = NULL;
	pBuf->phPrev = NULL;
	pBuf->poNext = NULL;
	pBuf->poPrev = NULL;
	pBuf->plNext = NULL;
	pBuf->plPrev = NULL;
}

void BufMoveToCleanList(Buf* pBuf)
{
	if(pBuf == NULL)
	{
		printf("Move to Clean List 실패!\n");
		return;
	}

	int ObjListNum = BUF_LIST_DIRTY;

	Buf* oPrevBuf = pBuf->poPrev;
	Buf* oNextBuf = pBuf->poNext;

	if((pBuf == ppObjListHead[ObjListNum]) && (pBuf == ppObjListTail[ObjListNum]))
	{
		ppObjListHead[ObjListNum] = ppObjListTail[ObjListNum] = NULL;
	}
	else if(pBuf == ppObjListHead[ObjListNum])
	{
		//if(oNextBuf != NULL)
			oNextBuf->poPrev = NULL;
		ppObjListHead[ObjListNum] = oNextBuf;
	}
	else if(pBuf == ppObjListTail[ObjListNum])
	{
		//if(oPrevBuf != NULL)
			oPrevBuf->poNext = NULL;;
		ppObjListTail[ObjListNum] = oPrevBuf;
	}
	else
	{	//if(oPrevBuf != NULL)
			oPrevBuf->poNext = oNextBuf;
		//if(oNextBuf != NULL)
			oNextBuf->poPrev = oPrevBuf;
	}

	pBuf->poNext = NULL;
	pBuf->poPrev = NULL;

	//list tail
	if((ppObjListHead[BUF_LIST_CLEAN] == NULL) && (ppObjListTail[BUF_LIST_CLEAN] == NULL))
	{
		ppObjListHead[BUF_LIST_CLEAN] = ppObjListTail[BUF_LIST_CLEAN] = pBuf;
	}
	else
	{
		ppObjListTail[BUF_LIST_CLEAN]->poNext = pBuf;
		pBuf->poPrev = ppObjListTail[BUF_LIST_CLEAN];
		ppObjListTail[BUF_LIST_CLEAN] = pBuf;
	}
}

void BufDaemon(void)
{
	Buf* LruBuf;
	Buf* NextBuf;

	while(1)
	{
		LruBuf = pLruListHead;
		NextBuf = pLruListHead;

		while(GetNumOfBuffersInFreeList() < 3)//2일 때 Lru를 프리리스트로
		{										//이동하면 3/10 -> 30%가 됨
			LruBuf = NextBuf;
			NextBuf = LruBuf->plNext;
			if(LruBuf->state == BUF_STATE_DIRTY)
			{
				DevWriteBlock(LruBuf->blkno, LruBuf->pMem);
			}
			BufDeleteBuf(LruBuf);
			InsertToFreeList(LruBuf);
		}

		usleep(500);
		UnlockMainThread();
		kill(getpid(), SIGSTOP);
	}
}

//////////////////추가함수들
/*
 * GetBufInfoByListNum: Get all buffers in a list specified by listnum.
 *                      This function receives a memory pointer to "ppBufInfo" that can contain the buffers.
 */
extern void GetBufInfoByListNum(BufList listnum, Buf** ppBufInfo, int* pNumBuf)
{
	int i , NumOfBuffers = 0;
	Buf* Cursor = ppObjListHead[listnum];
	if(Cursor == NULL)
	{
		NumOfBuffers = 0;
		*pNumBuf = NumOfBuffers;
	}
	else if(Cursor == ppObjListTail[listnum])
	{
		NumOfBuffers = 1;
		*pNumBuf = NumOfBuffers;
	}
	else
	{
		while(Cursor != NULL)
		{
			Cursor = Cursor->poNext;
			NumOfBuffers++;
		}
		*pNumBuf = NumOfBuffers;
	}

	Cursor = ppObjListHead[listnum];

	for(i = 0 ; i < NumOfBuffers ; i++)
	{
		ppBufInfo[i] = Cursor;
		Cursor = Cursor->poNext;
	}
}

/*
 * GetBufInfoByListNum: Get all buffers in a list specified by a hash index value.
 *                      This function receives a memory pointer to "ppBufInfo" that can contain the buffers.
 */
extern void GetBufInfoByHashIndex(int index, Buf** ppBufInfo, int* pNumBuf)
{
	int i,NumOfBuffers = 0;
	Buf* Cursor = ppHashHead[index];
	if(Cursor == NULL)
	{
		NumOfBuffers = 0;
		*pNumBuf = NumOfBuffers;
	}
	else if(Cursor == ppHashTail[index])
	{
		NumOfBuffers = 1;
		*pNumBuf = NumOfBuffers;
	}
	else
	{
		while(Cursor != NULL)
		{
			Cursor = Cursor->phNext;
			NumOfBuffers++;
		}
		*pNumBuf = NumOfBuffers;
	}
	Cursor = ppHashHead[index];

	for(i = 0 ; i < NumOfBuffers ; i++)
	{
		ppBufInfo[i] = Cursor;
		Cursor = Cursor->phNext;
	}
}

/*
 * GetBufInfoInLruList: Get all buffers in a list specified at the LRU list.
 *                         This function receives a memory pointer to "ppBufInfo" that can contain the buffers.
 */
extern void GetBufInfoInLruList(Buf** ppBufInfo, int* pNumBuf)
{
	int i,NumOfBuffers = 0;
	Buf* Cursor = pLruListHead;

	if(Cursor == NULL)
	{
		NumOfBuffers = 0;
		*pNumBuf = NumOfBuffers;
	}
	else if(Cursor == pLruListTail)
	{
		NumOfBuffers = 1;
		*pNumBuf = NumOfBuffers;
	}
	else
	{
		while(Cursor != NULL)
		{
			Cursor = Cursor->plNext;
			NumOfBuffers++;
		}

		*pNumBuf = NumOfBuffers;
	}

	Cursor = pLruListHead;

	for(i = 0 ; i < NumOfBuffers ; i++)
	{
		ppBufInfo[i] = Cursor;
		Cursor = Cursor->plNext;
	}
}

/*
 *             GetNumOfBuffersInFreeList: Get the number of buffers in free list.
 */
extern int GetNumOfBuffersInFreeList(void)
{
	int NumOfBuffers = 0;
	Buf* Cursor = ppObjListHead[BUF_LIST_FREE];
	if(Cursor == NULL)
	{
		return 0;
	}
	else if(Cursor == ppObjListTail[BUF_LIST_FREE])
	{
		return 1;
	}
	else
	{
		while(Cursor != NULL)
		{
			Cursor = Cursor->poNext;
			NumOfBuffers++;
		}

		return NumOfBuffers;
	}
}

void BufFinish()
{

}
