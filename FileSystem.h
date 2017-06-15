#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include "Disk.h"
#include <string.h>


// ------- Caution -----------------------------------------
#define FS_DISK_CAPACITY	(8388608) /* 8M */
#define FS_INODE_COUNT		(128)



#define NUM_OF_INODE_PER_BLK	(BLOCK_SIZE / sizeof(InodeInfo))
#define NUM_OF_DIRENT_PER_BLK	(BLOCK_SIZE / sizeof(DirEntry))



#define MAX_INDEX_OF_DIRBLK		(NUM_OF_DIRENT_PER_BLK)
#define NAME_LEN_MAX			(56)
#define NUM_OF_DIRECT_BLK_PTR	(12)
// ----------------------------------------------------------


typedef enum __openFlag {
	OPEN_FLAG_READWRITE,
	OPEN_FLAG_CREATE
} OpenFlag;


typedef enum __fileType {
    FILE_TYPE_FILE,
    FILE_TYPE_DIR,
    FILE_TYPE_DEV
} FileType;



typedef enum __fileMode {
	FILE_MODE_READONLY,
	FILE_MODE_READWRITE,
	FILE_MODE_EXEC
}FileMode;



typedef struct __dirEntry {
    char name[NAME_LEN_MAX];
    int inodeNum;
    FileType type;
} DirEntry;



typedef enum __mountType {
    MT_TYPE_FORMAT,
    MT_TYPE_READWRITE
} MountType;



typedef struct _fileSysInfo {
	int blocks;				
	int rootInodeNum; 
	int diskCapacity;		
	int numAllocBlocks;		
	int numFreeBlocks;		
	int numInodes;		
	int numAllocInodes;		
	int numFreeInodes;		
	int inodeBitmapStart;	
	int blockBitmapStart;	
	int inodeListStart;		
	int dataStart;			
	char* pInodeBitmap; 	
	char* pBlockBitmap; 	
} FileSysInfo;



typedef struct __inodeInfo {
	int			size;
	FileType	type;
	FileMode	mode;
	int			blocks;
	int			directPtr[NUM_OF_DIRECT_BLK_PTR];	// Direct block pointers
}InodeInfo;

typedef struct __dirBlock {
	DirEntry	dirEntries[NUM_OF_DIRENT_PER_BLK];
}DirBlock;

typedef struct __fileDesc {
	int	valid_bit;
	int	offset;
	int	inodeNo;
}FileDesc;

typedef struct __fileDescTable {
	FileDesc	file[FS_INODE_COUNT];
}FileDescTable;

extern int		OpenFile(const char* szFileName, OpenFlag flag);
extern int		WriteFile(int fileDesc, char* pBuffer, int length);
extern int		ReadFile(int fileDesc, char* pBuffer, int length);
extern int		CloseFile(int fileDesc);
extern int		RemoveFile(const char* szFileName);
extern int		MakeDir(const char* szDirName);
extern int		RemoveDir(const char* szDirName);
extern void		EnumerateDirStatus(const char* szDirName, DirEntry* pDirEntry, int* pNum);
extern void		Mount(MountType type);
extern void		Unmount(void);
extern void 		FileSysInit(void);
extern void		FileSysFinish(void);

extern FileDescTable* pFileDescTable;
extern FileSysInfo* pFileSysInfo;


#endif /* FILESYSTEM_H_ */
