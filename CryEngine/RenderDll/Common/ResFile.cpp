// ResFile.cpp : implementation file
//

#include "StdAfx.h"
#include "ResFile.h"

#include "LZSS.H"

CResFile CResFile::m_Root;
int CResFile::m_nNumOpenResources=0;

namespace
{
  CryFastLock g_cResLock;
}

void CResFile::mfDeactivate()
{
  Unlink();
  if (m_handle)
  {
    iSystem->GetIPak()->FClose(m_handle);
    m_handle = NULL;
    m_nNumOpenResources--;
  }
}

bool CResFile::mfActivate(bool bFirstTime)
{
  AUTO_LOCK(g_cResLock); // Not thread safe without this

  Relink(&m_Root);
  if (m_handle)
    return true;
  if (m_nNumOpenResources > MAX_OPEN_RESFILES)
  {
    CResFile *rf = m_Root.m_Prev;
    assert(rf && rf->m_handle);
    rf->mfDeactivate();
  }

  if (!bFirstTime && m_szAccess[0] == 'w')
  {
    char szAcc[16];
    strcpy(szAcc, m_szAccess);
    szAcc[0] = 'r';
		m_handle = iSystem->GetIPak()->FOpen(m_name, szAcc, ICryPak::FOPEN_HINT_DIRECT_OPERATION );
  }
  else
    m_handle = iSystem->GetIPak()->FOpen(m_name, m_szAccess, ICryPak::FOPEN_HINT_DIRECT_OPERATION );
  if (!m_handle)
  {
    sprintf(m_ermes, "CResFile::Activate - Can't open resource file <%s>", m_name);
    Unlink();
    return false;
  }
  m_nNumOpenResources++;
  return true;
}

CResFile::CResFile()
{
  m_name[0] = 0;
  m_handle = NULL;
  m_ermes[0] = 0;
  m_holesSize = 0;
	m_swapendian = false;

  m_Next = NULL;
  m_Prev = NULL;
  m_bDirty = false;

  if (!m_Root.m_Next)
  {
    m_Root.m_Next = &m_Root;
    m_Root.m_Prev = &m_Root;
  }
}

CResFile::CResFile(const char* name)
{
  strcpy(m_name, name);
  m_handle = NULL;
  m_ermes[0] = 0;
  m_holesSize = 0;
	m_swapendian = false;

  m_Next = NULL;
  m_Prev = NULL;
  m_bDirty = true;

  if (!m_Root.m_Next)
  {
    m_Root.m_Next = &m_Root;
    m_Root.m_Prev = &m_Root;
  }
}

CResFile::~CResFile()
{
  mfClose();
}

void CResFile::mfSetError(const char *er)
{
  strcpy(m_ermes, er);
}

char* CResFile::mfGetError(void)
{
  if (m_ermes[0])
    return m_ermes;
  else
    return NULL;
}

int CResFile::mfGetResourceSize()
{
  if (!m_handle)
    return 0;

  AUTO_LOCK(g_cResLock); // Not thread safe without this

  iSystem->GetIPak()->FSeek(m_handle, 0, SEEK_END);
  int length = iSystem->GetIPak()->FTell(m_handle);
  iSystem->GetIPak()->FSeek(m_handle, 0, SEEK_SET);

  return length;
}

uint64 CResFile::mfGetModifTime()
{
  mfActivate(false);
  return iSystem->GetIPak()->GetModificationTime(m_handle);
}

bool CResFile::mfSetModifTime(uint64 MTime)
{
#if !defined(LINUX)
  mfDeactivate();
  DWORD error = 0;
  char szDestBuf[256];
  const char *szDest = iSystem->GetIPak()->AdjustFileName(m_name, szDestBuf, 0);
#if defined(PS3)
	FILETIME ft = *((FILETIME*)&MTime);
	SetFileTime(szDest,&ft);
#else
  HANDLE hdst = CreateFile(szDest, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  if (hdst == INVALID_HANDLE_VALUE)
  {
    assert(0);
    /*error = GetLastError();
    LPVOID lpMsgBuf;
    FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | 
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      error,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR) &lpMsgBuf,
      0, NULL );
    LocalFree(lpMsgBuf);*/
    return false;
  }
  else
  {
    FILETIME ft = *((FILETIME*)&MTime);
    SetFileTime(hdst,NULL,NULL,&ft);
    CloseHandle(hdst);
  }
#endif//PS3
  return true;
#else
  return false;
#endif//LINUX
}

int CResFile::mfGetHolesSize()
{
  if (!m_handle)
    return 0;
  return m_holesSize;
}

bool CResFile::mfFileExist(CCryName name)
{
  SDirEntry *de = mfGetEntry(name);
  if (!de)
    return false;
  assert(name == de->Name);
  return true;
}

bool CResFile::mfFileExist(const char* name)
{
  bool bFound = CCryName::find(name);
  if (!bFound)
    return false;
  SDirEntry *de = mfGetEntry(CCryName(name));
  if (!de)
    return false;
  return true;
}

bool CResFile::mfOpen(int type)
{
  SFileResHeader frh;
  SFileDirEntry_name fden;

  PROFILE_FRAME(Resource_Open);

  if (m_name[0] == 0)
  {
    strcpy (m_ermes, "CResFile::Open - No Resource name");
    return false;
  }
  if (type == RA_READ)
    m_szAccess = "rb";
  else
  if (type == (RA_WRITE|RA_READ))
    m_szAccess = "r+b";
  else
  if (type & RA_CREATE)
    m_szAccess = "w+b";
  else
  {
    sprintf (m_ermes, "CResFile::Open - Invalind access mode for file <%s>", m_name);
    return false;
  }
  mfActivate(true);

  AUTO_LOCK(g_cResLock); // Not thread safe without this

  if (!m_handle)
  {
    if (type & (RA_WRITE | RA_CREATE))
    {
      char name[256];
      iSystem->GetIPak()->AdjustFileName(m_name, name, 0);
      FILE *statusdst = fopen(name, "rb");
      if (statusdst)
      {
/*
#if defined(LINUX)
        struct stat64 st;
#else
        struct __stat64 st;
#endif
        int result = _fstat64(fileno(statusdst), &st);
        if (result == 0)
        {
#if defined(LINUX)
          if (st.st_mode & FILE_ATTRIBUTE_READONLY)
            chmod(name, 0x777);//set to full access
#else
          if (!(st.st_mode & _S_IWRITE))
            _chmod(name, _S_IREAD | _S_IWRITE);
#endif
        }
						*/
        fclose(statusdst);
        m_ermes[0] = 0;

				SetFileAttributes( name,FILE_ATTRIBUTE_ARCHIVE );

        mfActivate(true);
      }
    }
    if (!m_handle)
    {
      sprintf(m_ermes, "CResFile::Open - Can't open resource file <%s>", m_name);
      return false;
    }
  }
  m_typeaccess = type;

  if (type & RA_READ)
  {
		// Detect file endianness automatically.
    if ( iSystem->GetIPak()->FRead(&frh, 1, m_handle, false) != 1)
    {
      sprintf (m_ermes, "CResFile::Open - Error read header in resource file <%s>", m_name);
      return false;
    }
	#ifdef NEED_ENDIAN_SWAP
		if (SwapEndianValue(frh.hid) == IDRESHEADER)
		{
			m_swapendian = true;
			SwapEndian(frh);
		}
	#endif
    if (frh.hid != IDRESHEADER)
    {
      sprintf (m_ermes, "CResFile::Open - Error header-id in resource file <%s>", m_name);
      return false;
    }
    if (frh.ver != RESVERSION)
    {
      sprintf (m_ermes, "CResFile::Open - Error version number in resource file <%s>", m_name);
      return false;
    }
    m_version = frh.ver;
    if (!frh.num_files)
    {
      sprintf (m_ermes, "CResFile::Open - Empty resource file <%s>", m_name);
      return false;
    }

    int curseek = frh.ofs_first;
    int nums = 0;

    while (curseek >= 0)
    {
      if (nums >= frh.num_files)
      {
        sprintf (m_ermes, "CResFile::Open - Invalid count of directory files in resource file <%s>", m_name);
        return false;
      }
      iSystem->GetIPak()->FSeek(m_handle, curseek, SEEK_SET);
      if (iSystem->GetIPak()->FReadRaw(&fden, 1, sizeof(fden)-MAX_FILE_NAME+1, m_handle) != sizeof(fden)-MAX_FILE_NAME+1)
      {
        sprintf (m_ermes, "CResFile::Open - Error read directory in resource file <%s>", m_name);
        return false;
      }
      SDirEntry *de = new SDirEntry;
			if (m_swapendian)
				SwapEndian(fden);
      iSystem->GetIPak()->FReadRaw(&fden.cName[1], 1, fden.cName[0], m_handle);
      fden.cName[fden.cName[0]+1] = 0;
      de->Name = CCryName(&fden.cName[1]);
      de->size = fden.size;
      de->offset = fden.offset;
      de->curseek = 0;
      de->earc = fden.earc;
      de->typeID = fden.typeID;
      de->flags = 0;
      de->user.data = NULL;
      /*if (de->Name == CCryName("$deleted$"))
      {
        de->flags |= RF_DELETED;
        m_holesSize += de->size;
      }*/
      nums++;
      curseek = fden.offnext;
      m_dir.insert(ResFilesMapItor::value_type(de->Name, de));
    }
    if (nums != frh.num_files)
    {
      sprintf (m_ermes, "CResFile::Open - Invalid count of directory files in resource file <%s>", m_name);
      return false;
    }
  }
  else
  {
    frh.hid = IDRESHEADER;
    frh.ver = RESVERSION;
    frh.num_files = 0;
    frh.ofs_first = -1;
    m_version = RESVERSION;
    if (iSystem->GetIPak()->FWrite(&frh, 1, sizeof(frh), m_handle) != sizeof(frh))
    {
      sprintf (m_ermes, "CResFile::Open - Error write header in resource file <%s>", m_name);
      return false;
    }
  }

  return true;
}

bool CResFile::mfClose(void)
{
  AUTO_LOCK(g_cResLock); // Not thread safe without this

  if (m_handle)
    mfFlush(0);
  ResFilesMapItor itor = m_dir.begin();
  while(itor != m_dir.end())
  {
    SDirEntry* de = itor->second;
		char *pPtr = (char*)de->user.data;
    SAFE_DELETE_ARRAY(pPtr);
    delete de;
    itor++;
  }
  m_dir.clear();

  mfDeactivate();

  return true;
}

SDirEntry *CResFile::mfGetEntry(CCryName name)
{
  ResFilesMapItor it = m_dir.find(name);
  if (it != m_dir.end())
    return it->second;
  return NULL;
}

int CResFile::mfFileRead(SDirEntry *de)
{
  int size;

  if (de->user.data)
    return de->size;

  mfActivate(false);

  AUTO_LOCK(g_cResLock); // Not thread safe without this

  if (de->earc != eARC_NONE)
  {
    iSystem->GetIPak()->FSeek(m_handle, de->offset, SEEK_SET);
    iSystem->GetIPak()->FRead(&size, 1, m_handle, m_swapendian);
    if (size >= 10000000)
      return 0;
    de->user.data = new byte [size];
    if (!de->user.data )
    {
      sprintf (m_ermes, "CResFile::mfFileRead - Couldn't allocate %i memory for file <%s> in resource file <%s>", size, de->Name.c_str(), m_name);
      return 0;
    }
    byte* buf = new byte [de->size];
    if (!buf)
    {
      sprintf (m_ermes, "CResFile::mfFileRead - Couldn't allocate %i memory for file <%s> in resource file <%s>", de->size, de->Name.c_str(), m_name);
      return 0;
    }
    iSystem->GetIPak()->FReadRaw(buf, de->size-4, 1, m_handle);
    switch (de->earc)
    {
      case eARC_LZSS:
        Decodem(buf, (byte *)de->user.data, de->size-4);
        break;

    }
    delete [] buf;
    return size;
  }

  de->user.data = new byte [de->size];
  if (!de->user.data)
  {
    sprintf (m_ermes, "CResFile::mfFileRead - Couldn't allocate %i memory for file <%s> in resource file <%s>", de->size, de->Name.c_str(), m_name);
    return 0;
  }
  iSystem->GetIPak()->FSeek(m_handle, de->offset, SEEK_SET);
  if (iSystem->GetIPak()->FReadRaw(de->user.data, 1, de->size, m_handle) != de->size)
  {
    sprintf (m_ermes, "CResFile::mfFileRead - Error reading file <%s> in resource file <%s>", de->Name.c_str(), m_name);
    return 0;
  }

  return de->size;
}

int CResFile::mfFileRead(CCryName name)
{
  SDirEntry *de = mfGetEntry(name);

  if (!de)
  {
    sprintf (m_ermes, "CResFile::mfFileRead - invalid file id in resource file <%s>", m_name);
    return 0;
  }
  return mfFileRead(de);
}

int CResFile::mfFileRead(const char* name)
{
  return mfFileRead(CCryName(name));
}

int CResFile::mfFileRead(SDirEntry *de, void* data)
{
  int size;

  if (!data)
  {
    sprintf (m_ermes, "CResFile::mfFileRead - invalid data for file <%s> in resource file <%s>", de->GetName(), m_name);
    return 0;
  }

  mfActivate(false);

  AUTO_LOCK(g_cResLock); // Not thread safe without this

  if (de->earc != eARC_NONE)
  {
    iSystem->GetIPak()->FSeek(m_handle, de->offset, SEEK_SET);
    iSystem->GetIPak()->FRead(&size, 1, m_handle, m_swapendian);
    byte* buf = new byte [de->size];
    if ( !buf )
    {
      sprintf (m_ermes, "CResFile::mfFileRead - Couldn't allocate %i memory for file <%s> in resource file <%s>", de->size, de->GetName(), m_name);
      return 0;
    }
    iSystem->GetIPak()->FReadRaw(buf, de->size-4, 1, m_handle);
    switch (de->earc)
    {
      case eARC_LZSS:
        Decodem(buf, (byte *)data, de->size-4);
        break;

    }
    delete [] buf;
    return size;
  }

  iSystem->GetIPak()->FSeek(m_handle, de->offset, SEEK_SET);
  if (iSystem->GetIPak()->FReadRaw(data, 1, de->size, m_handle) != de->size)
  {
    sprintf (m_ermes, "CResFile::mfFileRead - Error reading file <%s> in resource file <%s>", de->GetName(), m_name);
    return 0;
  }

  return de->size;
}

int CResFile::mfFileRead(CCryName name, void* data)
{
  SDirEntry *de = mfGetEntry(name);

  if (!de)
  {
    sprintf (m_ermes, "CResFile::mfFileRead - invalid file number in resource file <%s>", m_name);
    return 0;
  }
  return mfFileRead(de, data);
}

int CResFile::mfFileRead(const char* name, void* data)
{
  return mfFileRead(CCryName(name), data);
}

int CResFile::mfFileWrite(CCryName name, void* data)
{
  SDirEntry *de = mfGetEntry(name);

  if (!de)
  {
    sprintf (m_ermes, "CResFile::mfFileWrite - invalid file number in resource file <%s>", m_name);
    return 0;
  }
  if (!data)
  {
    sprintf (m_ermes, "CResFile::mfFileWrite - invalid data for file <%s> in resource file <%s>", de->GetName(), m_name);
    return 0;
  }

  mfActivate(false);

  if (de->earc != eARC_NONE)
  {
    assert(0);
    return 0;
  }

  iSystem->GetIPak()->FSeek(m_handle, de->offset, SEEK_SET);
  if (iSystem->GetIPak()->FWrite(data, 1, de->size, m_handle) != de->size)
  {
    sprintf (m_ermes, "CResFile::mfFileWrite - Error reading file <%s> in resource file <%s>", de->GetName(), m_name);
    return 0;
  }

  return de->size;
}

void* CResFile::mfFileRead2(SDirEntry *de, int size)
{
  void* buf;

  buf = new byte [size];
  if (!buf)
  {
    sprintf (m_ermes, "CResFile::mfFileRead2 - Couldn't allocate %i memory for file <%s> in resource file <%s>", size, de->GetName(), m_name);
    return NULL;
  }
  if (de->user.data )
  {
    memcpy (buf, (byte *)(de->user.data)+de->curseek, size);
    de->curseek += size;
    return buf;
  }
  mfActivate(false);

  AUTO_LOCK(g_cResLock); // Not thread safe without this

  iSystem->GetIPak()->FSeek(m_handle, de->offset+de->curseek, SEEK_SET);
  if (iSystem->GetIPak()->FReadRaw(buf, 1, size, m_handle) != size)
  {
    sprintf (m_ermes, "CResFile::mfFileRead2 - Error reading file <%s> in resource file <%s>", de->GetName(), m_name);
    return NULL;
  }
  de->curseek += size;

  return buf;
}

void* CResFile::mfFileRead2(CCryName name, int size)
{
  SDirEntry *de = mfGetEntry(name);
  if (!de)
  {
    sprintf (m_ermes, "CResFile::mfFileRead2 - invalid file id in resource file <%s>", m_name);
    return NULL;
  }
  return mfFileRead2(de, size);
}

void* CResFile::mfFileRead2(const char* name, int size)
{
  return mfFileRead2(CCryName(name), size);
}

void CResFile::mfFileRead2(SDirEntry *de, int size, void *buf)
{
  if (!buf)
  {
    sprintf (m_ermes, "CResFile::mfFileRead2 - Buffer is invalid for file %s in resource file <%s>", de->GetName(), m_name);
    return;
  }
  if (de->user.data)
  {
    memcpy (buf, (byte *)(de->user.data)+de->curseek, size);
    de->curseek += size;
    return;
  }
  mfActivate(false);

  iSystem->GetIPak()->FSeek(m_handle, de->offset+de->curseek, SEEK_SET);
  if (iSystem->GetIPak()->FReadRaw(buf, 1, size, m_handle) != size)
  {
    sprintf (m_ermes, "CResFile::mfFileRead2 - Error reading file <%s> in resource file <%s>", de->GetName(), m_name);
    return;
  }
  de->curseek += size;

  return;
}

void CResFile::mfFileRead2(CCryName name, int size, void *buf)
{
  SDirEntry *de = mfGetEntry(name);
  if (!de)
  {
    sprintf (m_ermes, "CResFile::mfFileRead2 - invalid file id in resource file <%s>", m_name);
    return;
  }
  return mfFileRead2(de, size, buf);
}

void CResFile::mfFileRead2(const char *szName, int size, void *buf)
{
  mfFileRead2(CCryName(szName), size, buf);
}

void* CResFile::mfFileGetBuf(SDirEntry *de)
{
  return de->user.data;
}

void* CResFile::mfFileGetBuf(CCryName name)
{
  SDirEntry *de = mfGetEntry(name);
  if (!de)
  {
    sprintf (m_ermes, "CResFile::mfFileGetBuf - invalid file id in resource file <%s>", m_name);
    return NULL;
  }
  return mfFileGetBuf(de);
}

void* CResFile::mfFileGetBuf(char* name)
{
  return mfFileGetBuf(CCryName(name));
}

int CResFile::mfFileSeek(SDirEntry *de, int ofs, int type)
{
  int m;

  mfActivate(false);

  AUTO_LOCK(g_cResLock); // Not thread safe without this

  switch ( type )
  {
    case SEEK_CUR:
      de->curseek += ofs;
      m = iSystem->GetIPak()->FSeek(m_handle, de->offset+de->curseek, SEEK_SET);
      break;

    case SEEK_SET:
      m = iSystem->GetIPak()->FSeek(m_handle, de->offset+ofs, SEEK_SET);
      de->curseek = ofs;
      break;

    case SEEK_END:
      de->curseek = de->size-ofs;
      m = iSystem->GetIPak()->FSeek(m_handle, de->offset+de->curseek, SEEK_SET);
      break;

    default:
    sprintf (m_ermes, "CResFile::mfFileSeek - invalid seek type in resource file <%s>", m_name);
    return -1;
  }

  return m;
}

int CResFile::mfFileSeek(CCryName name, int ofs, int type)
{
  SDirEntry *de = mfGetEntry(name);

  if (!de)
  {
    sprintf (m_ermes, "CResFile::mfFileSeek - invalid file id in resource file <%s>", m_name);
    return -1;
  }
  return mfFileSeek(de, ofs, type);
}

int CResFile::mfFileSeek(char* name, int ofs, int type)
{
  return mfFileSeek(CCryName(name), ofs, type);
}

int CResFile::mfFileLength(SDirEntry *de)
{
  return de->size;
}

int CResFile::mfFileLength(CCryName name)
{
  SDirEntry *de = mfGetEntry(name);

  if (!de)
  {
    sprintf (m_ermes, "CResFile::mfFileLength - invalid file id in resource file <%s>", m_name);
    return -1;
  }
  return mfFileLength(de);
}

int CResFile::mfFileLength(char* name)
{
  return mfFileLength(CCryName(name));
}


int CResFile::mfFileAdd(SDirEntry* de)
{
  const char *str = de->GetName();
  if (m_typeaccess == RA_READ)
  {
    sprintf (m_ermes, "CResFile::mfFileAdd - Operation 'Add' during RA_READ access mode in resource file <%s>", m_name);
    return -1;
  }
  if (de->size!=0 && (de->user.data || de->ref>=0))
  {
    SDirEntry *newDE = new SDirEntry;
    *newDE = *de;
    newDE->flags |= RF_NOTSAVED;
    m_dir.insert(ResFilesMapItor::value_type(de->Name, newDE));
    m_bDirty = true;
  }
  return m_dir.size();
}

bool CResFile::mfFlush(uint64 WriteTime)
{
  SFileResHeader frh;
  SFileDirEntry_name fden;
  int m = 0;
  int size;

  PROFILE_FRAME(Resource_Flush);

  if (m_typeaccess == RA_READ)
  {
    sprintf (m_ermes, "CResFile::mfFlush - Operation 'Flush' during RA_READ access mode in resource file <%s>", m_name);
    return false;
  }
  if (!m_bDirty)
    return true;
  m_bDirty = false;
  mfActivate(false);

  AUTO_LOCK(g_cResLock); // Not thread safe without this

  iSystem->GetIPak()->FSeek(m_handle, 0, SEEK_SET);
  iSystem->GetIPak()->FRead(&frh, 1, m_handle, m_swapendian);

  int prevseek = -1;
  int curseek = frh.ofs_first;
  int nums = 0;

  while (curseek >= 0)
  {
    iSystem->GetIPak()->FSeek(m_handle, curseek, SEEK_SET);
    if (iSystem->GetIPak()->FReadRaw(&fden, 1, sizeof(fden)-MAX_FILE_NAME, m_handle) != sizeof(fden)-MAX_FILE_NAME)
    {
      sprintf (m_ermes, "CResFile::mfFlush - Error reading directory in resource file <%s>", m_name);
      return false;
    }
		if (m_swapendian)
			SwapEndian(fden);
    prevseek = curseek;
    nums++;
    curseek = fden.offnext;
    m = 1;
  }
  if (nums == m_dir.size())
    return true;
  if (nums > (int)m_dir.size())
  {
    sprintf (m_ermes, "CResFile::mfFlush - Error number directory files in resource file <%s>", m_name);
    return false;
  }

  iSystem->GetIPak()->FSeek(m_handle, 0, SEEK_END);
  int nextseek = iSystem->GetIPak()->FTell(m_handle);
  if (!m)
    frh.ofs_first = nextseek;
  else
  {
    iSystem->GetIPak()->FSeek(m_handle, prevseek, SEEK_SET);
    fden.offnext = nextseek;
    iSystem->GetIPak()->FWrite(&fden, 1, sizeof(fden)-MAX_FILE_NAME, m_handle);
  }

  ResFilesMapItor itor = m_dir.begin();
  while(itor != m_dir.end())
  {
    SDirEntry* de = itor->second;
    if (de->flags & RF_NOTSAVED)
    {
      de->flags &= ~RF_NOTSAVED;
      size = strlen(de->GetName());
      fden.cName[0] = size;
      memcpy(&fden.cName[1], de->GetName(), size);
      size += sizeof(SFileDirEntry_name)-MAX_FILE_NAME+1;
      fden.size = de->size;
      if (de->ref >= 0)
      {
        ResFilesMapItor it = m_dir.begin();
        int n = 0;
        for(it=m_dir.begin(); it!=m_dir.end(); it++)
        {
          SDirEntry* d = it->second;
          if (d->refOrg == de->ref)
          {
            fden.offset = d->offset;
            fden.typeID = d->typeID;
            de->offset = d->offset;
            fden.offnext = nextseek+size;
            fden.size = de->size = d->size;
            break;
          }
          n++;
        }
        assert(n < m_dir.size());
      }
      else
      {
        fden.offset = nextseek+size;
        de->offset = fden.offset;
        fden.offnext = fden.offset+fden.size;
      }
      fden.offprev = prevseek;
      de->offsetHeader = nextseek;
      fden.earc = de->earc;
      fden.typeID = de->typeID;
      prevseek = nextseek;
      iSystem->GetIPak()->FSeek(m_handle, nextseek, SEEK_SET);
      nextseek = fden.offnext;
      if (fden.earc != eARC_NONE)
      {
        byte* buf = new byte [fden.size*2+128];
        if (!buf)
        {
          sprintf (m_ermes, "CResFile::mfFlush - Cannot allocate %i bytes for flushing file entry <%s> in resource file <%s>", fden.size, fden.cName, m_name);
          return false;
        }
        switch (fden.earc)
        {
          case eARC_LZSS:
            {
              if (de->ref < 0)
              {
                int sizeEnc = Encodem((byte *)de->user.data, &buf[4], de->size);
                *(int *)buf = fden.size;
                de->size = fden.size = sizeEnc+4;
                fden.offnext = fden.offset+fden.size;
                nextseek = fden.offnext;
              }
              if (iSystem->GetIPak()->FWrite(&fden, 1, size, m_handle) != size)
              {
                sprintf (m_ermes, "CResFile::mfFlush - Failed writing directory entry in resource file <%s>", m_name);
                return false;
              }
              if (de->ref < 0)
              {
                if (iSystem->GetIPak()->FWrite(buf, 1, fden.size, m_handle) != fden.size)
                {
                  sprintf (m_ermes, "CResFile::mfFlush - Failed flushing file entry <%s> in resource file <%s>", de->GetName(), m_name);
                  return false;
                }
              }
            }
            break;

          case eARC_RLE:
            break;
        }
        delete [] buf;
      }
      else
      {
        if (iSystem->GetIPak()->FWrite(&fden, 1, size, m_handle) != size)
        {
          sprintf (m_ermes, "CResFile::mfFlush - Error write directory in resource file <%s>", m_name);
          return false;
        }
        if (de->ref < 0)
        {
          if (!de->user.data || (iSystem->GetIPak()->FWrite(de->user.data, 1, de->size, m_handle) != de->size))
          {
            sprintf (m_ermes, "CResFile::mfFlush - Error write file <%s> in resource file <%s>", de->GetName(), m_name);
            return false;
          }
        }
      }

      if (de->flags & RF_TEMPDATA)
			{
				char *pPtr = (char*)de->user.data;
        SAFE_DELETE_ARRAY(pPtr);
			}
      de->user.data = NULL;
    }
    itor++;
  }
  if (prevseek != -1)
  {
    iSystem->GetIPak()->FSeek(m_handle, prevseek, SEEK_SET);
    fden.offnext = -1;
    iSystem->GetIPak()->FWrite(&fden, 1, sizeof(fden)-MAX_FILE_NAME, m_handle);
  }
  frh.num_files = m_dir.size();
  iSystem->GetIPak()->FSeek(m_handle, 0, SEEK_SET);
  iSystem->GetIPak()->FWrite(&frh, 1, sizeof(frh), m_handle);
  iSystem->GetIPak()->FFlush(m_handle);
  if (WriteTime != 0)
  {
    mfSetModifTime(WriteTime);
  }

  return true;
}

bool CResFile::mfOptimize(uint type)
{
  return true;
}

int CResFile::Size()
{
  int nSize = sizeof(CResFile);

  nSize += m_dir.size() * sizeof(SDirEntry);

  return nSize;
}

/*int CResFile::mfFileDelete(SDirEntry *de)
{
  SFileDirEntry_name fden;

  mfActivate(false);

  de->flags |= RF_DELETED;
  iSystem->GetIPak()->FSeek(m_handle, de->offsetHeader, SEEK_SET);
  iSystem->GetIPak()->FReadRaw(&fden, 1, sizeof(fden)-MAX_FILE_NAME, m_handle);
	if (m_swapendian)
		SwapEndian(fden);
  strcpy(fden.Name, "$deleted$");
  iSystem->GetIPak()->FSeek(m_handle, de->offsetHeader, SEEK_SET);
  iSystem->GetIPak()->FWrite(&fden, 1, sizeof(fden)-MAX_FILE_NAME+sizeof("$deleted$"), m_handle);

  return m_dir.size();
}

int CResFile::mfFileDelete(CCryName name)
{
  SDirEntry *de = mfGetEntry(name);
  if (!de)
  {
    sprintf (m_ermes, "CResFile::mfFileDelete - error file number in resource file <%s>", m_name);
    return -1;
  }
  return mfFileDelete(de);
}

int CResFile::mfFileDelete(char* name)
{
  return mfFileDelete(CCryName(name));
}*/

int CResFile::mfGetDirectory(TArray<SDirEntry *>& Dir)
{
  ResFilesMapItor itor;
  for(itor=m_dir.begin(); itor!=m_dir.end(); itor++)
  {
    SDirEntry* de = itor->second;
    if (de->flags & RF_DELETED)
      continue;
    Dir.AddElem(de);
  }
  return Dir.Num();
}

//======================================================================

void fpStripExtension (const char *in, char *out)
{
  ptrdiff_t len = strlen(in)-1;

  if(len<=1)
  {
    strcpy(out, in);
    return;
  }

  while (in[len])
  {
    if (in[len]=='.')
    {
      int n = len;
      while(in[n] != 0)
      {
        if (in[n] == '+')
        {
          strcpy(out, in);
          return;
        }
        n++;
      }
      break;
    }
    len--;
    if (!len)
    {
      strcpy(out, in);
      return;
    }
  }
  strncpy(out, in, len);
  out[len] = 0;
}

const char *fpGetExtension (const char *in)
{
  ptrdiff_t len = strlen(in)-1;
  while (len)
  {
    if (in[len]=='.')
      return &in[len];
    len--;
  }
  return NULL;
}

void fpAddExtension (char *path, char *extension)
{
  char    *src;
  src = path + strlen(path) - 1;

  while (*src != '/' && src != path)
  {
    if (*src == '.')
      return;                 // it has an extension
    src--;
  }

  strcat (path, extension);
}

void fpConvertDOSToUnixName( char *dst, const char *src )
{
  while ( *src )
  {
    if ( *src == '\\' )
      *dst = '/';
    else
      *dst = *src;
    dst++; src++;
  }
  *dst = 0;
}

void fpConvertUnixToDosName( char *dst, const char *src )
{
  while ( *src )
  {
    if ( *src == '/' )
      *dst = '\\';
    else
      *dst = *src;
    dst++; src++;
  }
  *dst = 0;
}

void fpUsePath (char *name, char *path, char *dst)
{
  char c;

  if (!path)
  {
    strcpy(dst, name);
    return;
  }

  strcpy(dst, path);
  if ( (c=path[strlen(path)-1]) != '/' && c!='\\')
    strcat(dst, "/");
  strcat(dst, name);
}

#include "TypeInfo_impl.h"
#include "CryName_info.h"
#include "ResFile_info.h"
#include "Textures/Image/dds_info.h"
