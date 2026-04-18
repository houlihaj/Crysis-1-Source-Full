#ifndef __RESFILE_H__
#define __RESFILE_H__

#include "CryName.h"

#define IDRESHEADER		(('K'<<24)+('C'<<16)+('P'<<8)+'C')
#define RESVERSION  2

enum EArc
{
  eARC_NONE     = 0,
  eARC_RLE      = 1,
  eARC_LZSS     = 2
};



// Resource header
struct SFileResHeader
{
  uint hid;
  int  ver;
  uint optimize_mode;
  int num_files;
#ifdef PS2
  int ofs_first;
#else  
  long ofs_first;
#endif  
	AUTO_STRUCT_INFO
};

#define MAX_FILE_NAME 256

// Each file entry in resource
struct SFileDirEntry_name
{
  int size;
  int offset;
  int offnext;
  int offprev;
  int typeID;
  EArc earc;       // eARC_
  char cName[MAX_FILE_NAME];
	AUTO_STRUCT_INFO
};

struct SCacheUser
{
  void *data;
	AUTO_STRUCT_INFO
};

// Intrinsic file entry
struct SDirEntry
{
  CCryName Name;
  int size;
  int offset;
  int offsetHeader;
  int curseek;
  EArc earc;       // eARC_
  SCacheUser user;
  ushort flags;
  short typeID;
  short ref;
  short refOrg;

  SDirEntry()
  {
    memset(this, 0, sizeof(SDirEntry));
    ref = -1;
    refOrg = -1;
  }
  const char *GetName()
  {
    return Name.c_str();
  }
	AUTO_STRUCT_INFO
};

typedef std::map<CCryName,SDirEntry*> ResFilesMap;
typedef ResFilesMap::iterator ResFilesMapItor;

// Resource access types
#define RA_READ   1
#define RA_WRITE  2
#define RA_CREATE 4


// Resource files flags
#define RF_NOTSAVED 1
#define RF_DELETED 2
#define RF_INTRINSIC 4
#define RF_CACHED 8
#define RF_BIG 0x10
#define RF_TEMPDATA 0x20

// Resource optimize flags
#define RO_HEADERS_IN_BEGIN 1
#define RO_HEADERS_IN_END 2
#define RO_HEADER_FILE 4
#define RO_SORT_ALPHA_ASC 8
#define RO_SORT_ALPHA_DESC 0x10

#define MAX_OPEN_RESFILES 16

class CResFile
{
private:
  char m_name[1024];
  char *m_szAccess;
  FILE* m_handle;
  ResFilesMap m_dir;
  int m_typeaccess;
	bool m_swapendian;
  bool m_bDirty;
  uint m_optimize;
  char m_ermes[1024];
  int m_version;
  int m_holesSize;

  static CResFile m_Root;
  static int m_nNumOpenResources;
  CResFile *m_Next;
  CResFile *m_Prev;

  bool mfActivate(bool bFirstTime);
  void mfDeactivate();

  _inline void Relink(CResFile* Before)
  {
    if (m_Next && m_Prev)
    {
      m_Next->m_Prev = m_Prev;
      m_Prev->m_Next = m_Next;
    }
    m_Next = Before->m_Next;
    Before->m_Next->m_Prev = this;
    Before->m_Next = this;
    m_Prev = Before;
  }
  _inline void Unlink()
  {
    if (!m_Next || !m_Prev)
      return;
    m_Next->m_Prev = m_Prev;
    m_Prev->m_Next = m_Next;
    m_Next = m_Prev = NULL;
  }
  _inline void Link(CResFile* Before)
  {
    if (m_Next || m_Prev)
      return;
    m_Next = Before->m_Next;
    Before->m_Next->m_Prev = this;
    Before->m_Next = this;
    m_Prev = Before;
  }

public:
  CResFile(const char* name);
  CResFile();
  ~CResFile();

  char* mfGetError(void);
  void mfSetError(const char *er);
  const char *mfGetFileName() {return m_name;}

  bool mfOpen(int type);
  bool mfClose();
  bool mfFlush(uint64 WriteTime=0);
  bool mfOptimize(uint type);

  int mfGetNumFiles() { return m_dir.size(); }

  int mfFileRead(SDirEntry *de);
  int mfFileRead(const char* name);
  int mfFileRead(CCryName name);

  int mfFileRead(SDirEntry *de, void* data);
  int mfFileRead(CCryName name, void* data);
  int mfFileRead(const char* name, void* data);

  int mfFileWrite(CCryName name, void* data);

  void* mfFileRead2(SDirEntry *de, int size);
  void* mfFileRead2(CCryName name, int size);
  void* mfFileRead2(const char* name, int size);

  void  mfFileRead2(SDirEntry *de, int size, void *buf);
  void  mfFileRead2(CCryName name, int size, void *buf);
  void  mfFileRead2(const char *szName, int size, void *buf);

  void* mfFileGetBuf(SDirEntry *de);
  void* mfFileGetBuf(CCryName name);
  void* mfFileGetBuf(char* name);

  int mfFileSeek(SDirEntry *de, int offs, int type);
  int mfFileSeek(CCryName name, int offs, int type);
  int mfFileSeek(char* name, int offs, int type);

  int mfFileLength(SDirEntry *de);
  int mfFileLength(CCryName name);
  int mfFileLength(char* name);

  int mfFileAdd(SDirEntry* de);

  bool mfIsDirty() { return m_bDirty; }

  //int mfFileDelete(SDirEntry *de);
  //int mfFileDelete(CCryName name);
  //int mfFileDelete(char* name);

  bool mfFileExist(CCryName name);
  bool mfFileExist(const char* name);

  SDirEntry *mfGetEntry(CCryName name);
  int mfGetDirectory(TArray<SDirEntry *>& Dir);

  FILE *mfGetHandle() { return m_handle; }
  int mfGetResourceSize();
  int mfGetHolesSize();

  uint64 mfGetModifTime();
  bool mfSetModifTime(uint64 MTime);

  int Size();
};

#endif //  __RESFILE_H__
