#ifndef __HW2_H__

#define __HW2_H__


#define HASH_TBL_SIZE    (8)
#define MAX_BUFLIST_NUM  (3)
#define BLKNO_INVALID  (-1)


/* 추가됨: 전체 버퍼의 개수, 즉, 초기에 free list에서 생성될 버퍼의 개수 */
#define MAX_BUF_NUM      (10)


typedef struct Buf Buf;

typedef enum {
	FALSE = 0,
	TRUE = 1
} BOOL;

typedef enum __BufState
{
	BUF_STATE_CLEAN = 0,
	BUF_STATE_DIRTY = 1
} BufState;

typedef enum __BufList
{
    BUF_LIST_DIRTY = 0,
    BUF_LIST_CLEAN = 1,
    BUF_LIST_FREE = 2
} BufList;

struct Buf
{
    int			blkno;
    BufState		state;
    void*		pMem;
    int			atime;
    Buf*		phPrev;	/* the previous pointer in hash linked list */
    Buf*		phNext; /* the next pointer in hash linked list */
    Buf*		poPrev; /* the previous pointer in object linked list */
    Buf*		poNext; /* the next pointer in object linked list */
    Buf*                plPrev;
    Buf*                plNext;
};

Buf*  ppHashHead[HASH_TBL_SIZE];
Buf*  ppHashTail[HASH_TBL_SIZE];

Buf*  ppObjListHead[MAX_BUFLIST_NUM];
Buf*  ppObjListTail[MAX_BUFLIST_NUM];


Buf*  pLruListHead;
Buf*  pLruListTail;



extern void		BufInit(void);
extern void		BufRead(int blkno, void* pData);
extern void		BufWrite(int blkno, void* pData);
extern void		BufSync(void);


/*
 * GetBufInfoByListNum: Get all buffers in a list specified by listnum.
 *                      This function receives a memory pointer to "ppBufInfo" that can contain the buffers.
 */
extern void           GetBufInfoByListNum(BufList listnum, Buf** ppBufInfo, int* pNumBuf);


/*
 * GetBufInfoByListNum: Get all buffers in a list specified by a hash index value.
 *                      This function receives a memory pointer to "ppBufInfo" that can contain the buffers.
 */
extern void           GetBufInfoByHashIndex(int index, Buf** ppBufInfo, int* pNumBuf);

/*
 * GetBufInfoInLruList: Get all buffers in a list specified at the LRU list.
 *                         This function receives a memory pointer to "ppBufInfo" that can contain the buffers.
 */
extern void           GetBufInfoInLruList(Buf** ppBufInfo, int* pNumBuf);

/*
 *             GetNumOfBuffersInFreeList: Get the number of buffers in free list.
 */
extern int            GetNumOfBuffersInFreeList(void);

#endif /* __HW2_H__ */
