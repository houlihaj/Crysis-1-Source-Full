////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   ReadOnlyChunkFile.h
//  Version:     v1.00
//  Created:     15/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ReadOnlyChunkFile.h"

#define MAX_CHUNKS_NUM 10000

inline bool ChunkCompareFileOffset( const CReadOnlyChunkFile::ChunkDesc &d1,const CReadOnlyChunkFile::ChunkDesc &d2 )
{
	return d1.hdr.FileOffset < d2.hdr.FileOffset;
}
inline bool ChunkCompareId( const CReadOnlyChunkFile::ChunkDesc &d1,const CReadOnlyChunkFile::ChunkDesc &d2 )
{
	return d1.hdr.ChunkID < d2.hdr.ChunkID;
}

//////////////////////////////////////////////////////////////////////////
// ChunkDesc compare by ChunkId operators.
//////////////////////////////////////////////////////////////////////////
inline bool operator<( const CReadOnlyChunkFile::ChunkDesc &d1,const CReadOnlyChunkFile::ChunkDesc &d2 ) { return d1.hdr.ChunkID < d2.hdr.ChunkID; }
inline bool operator>( const CReadOnlyChunkFile::ChunkDesc &d1,const CReadOnlyChunkFile::ChunkDesc &d2 ) { return d1.hdr.ChunkID > d2.hdr.ChunkID; }
inline bool operator==( const CReadOnlyChunkFile::ChunkDesc &d1,const CReadOnlyChunkFile::ChunkDesc &d2 ) { return d1.hdr.ChunkID == d2.hdr.ChunkID; }
inline bool operator!=( const CReadOnlyChunkFile::ChunkDesc &d1,const CReadOnlyChunkFile::ChunkDesc &d2 ) { return d1.hdr.ChunkID != d2.hdr.ChunkID; }
//////////////////////////////////////////////////////////////////////////


CReadOnlyChunkFile::CReadOnlyChunkFile( bool bNoWarningMode )
{
	m_pFileBuffer = 0;
	m_fileHeader.FileType				= FileType_Geom;
	m_fileHeader.ChunkTableOffset		= -1;
	m_fileHeader.Version				= GeomFileVersion;
	strcpy(m_fileHeader.Signature,FILE_SIGNATURE);

	m_bNoWarningMode = bNoWarningMode;

	m_nLastChunkId = 0;
	m_bOwnFileBuffer = false;

	m_hFile = 0;
}

CReadOnlyChunkFile::~CReadOnlyChunkFile()
{
	FreeBuffer();

	if (m_hFile)
		gEnv->pCryPak->FClose(m_hFile);
	m_hFile = 0;
}

//////////////////////////////////////////////////////////////////////////
void CReadOnlyChunkFile::FreeBuffer()
{
	if (m_pFileBuffer && m_bOwnFileBuffer)
		delete []m_pFileBuffer;
	m_pFileBuffer = 0;
	m_bOwnFileBuffer = 0;
}

// retrieves the raw chunk header, as it appears in the file
const CReadOnlyChunkFile::ChunkHeader& CReadOnlyChunkFile::GetChunkHeader(int nChunkIdx)const
{
	return m_chunks[nChunkIdx].hdr;
}

//////////////////////////////////////////////////////////////////////////
CReadOnlyChunkFile::ChunkDesc* CReadOnlyChunkFile::GetChunk( int nIndex )
{
	assert( nIndex >= 0 && nIndex < (int)m_chunks.size() );
	return &m_chunks[nIndex];
}

// returns the raw data of the i-th chunk
const void* CReadOnlyChunkFile::GetChunkData(int nChunkIdx)const
{
	assert (nChunkIdx >= 0 && nChunkIdx < NumChunks());
	if (nChunkIdx>= 0 && nChunkIdx < NumChunks())
	{
		return m_chunks[nChunkIdx].data;
	}
	else
		return 0;
}

// number of chunks
int CReadOnlyChunkFile::NumChunks()const
{
	return (int)m_chunks.size();
}

// number of chunks of the specified type
int CReadOnlyChunkFile::NumChunksOfType (ChunkTypes nChunkType)const
{
	int nResult = 0;
	for (int i = 0; i < NumChunks(); ++i)
	{
		if (m_chunks[i].hdr.ChunkType == nChunkType)
			++nResult;
	}
	return nResult;
}

//////////////////////////////////////////////////////////////////////////
// calculates the chunk size, based on the very next chunk with greater offset
// or the end of the raw data portion of the file
int CReadOnlyChunkFile::GetChunkSize(int nChunkIdx) const
{
	assert (nChunkIdx >= 0 && nChunkIdx < NumChunks());
	return m_chunks[nChunkIdx].size;
}

//////////////////////////////////////////////////////////////////////////
CReadOnlyChunkFile::ChunkDesc* CReadOnlyChunkFile::FindChunkByType( int type )
{
	int nchunks = m_chunks.size();
	if (nchunks > 0)
	{
		ChunkDesc *chunks = &m_chunks[0];
		for (unsigned int i = 0; i < nchunks; i++)
		{
			if (chunks[i].hdr.ChunkType == type)
			{
				return &chunks[i];
			}
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CReadOnlyChunkFile::ChunkDesc* CReadOnlyChunkFile::FindChunkById( int id )
{
	ChunkDesc chunkToFind;
	chunkToFind.hdr.ChunkID = id;
	std::vector<ChunkDesc>::iterator it = stl::binary_find( m_chunks.begin(),m_chunks.end(),chunkToFind );
	if (it != m_chunks.end())
	{
		ChunkDesc* pChunk = &*it;
		return pChunk;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CReadOnlyChunkFile::ReadChunkTableFromBuffer( const void *pBuffer,int nBufferSize )
{
  unsigned i;

	FreeBuffer();

  //char * pData = m_pFileBuffer = (char *)pMemBlock->GetData();
  m_pFileBuffer = (char*)pBuffer;
	m_bOwnFileBuffer = false;

  memcpy(m_pFileBuffer,pBuffer,nBufferSize);
  char * pData = m_pFileBuffer;

  memcpy(&m_fileHeader, pData, sizeof(m_fileHeader));
  SwapEndian( m_fileHeader );
  pData += m_fileHeader.ChunkTableOffset;

  int nChunkTableOffset = m_fileHeader.ChunkTableOffset;

  int n_chunks = *(int*)pData;
  pData += 4;
  if(n_chunks < 0 || n_chunks > MAX_CHUNKS_NUM)
    return false;

  m_nLastChunkId = 0;

  CHUNK_HEADER *chunks = (CHUNK_HEADER*)(m_pFileBuffer + nChunkTableOffset+4); //new CHUNK_HEADER[n_chunks];
  SwapEndian( chunks,n_chunks );

  m_chunks.clear();
  m_chunks.resize(n_chunks);

  int nextFileOffset,chunkSize;
  for (i = 0; i < n_chunks; i++)
  {
    ChunkDesc &ch = m_chunks[i];
    ch.hdr = chunks[i];

    // Find last chunk id.
    if (ch.hdr.ChunkID > m_nLastChunkId)
      m_nLastChunkId = ch.hdr.ChunkID;
  }

  std::sort( m_chunks.begin(),m_chunks.end(),ChunkCompareFileOffset );

  ChunkDesc *sortedChunks = &m_chunks[0];
  for (i = 0; i < n_chunks; i++)
  {
    if (i+1 < n_chunks)
      nextFileOffset = sortedChunks[i+1].hdr.FileOffset;
    else
      nextFileOffset = nBufferSize;

      assert(1);

    ChunkDesc &cd = sortedChunks[i];
    chunkSize = nextFileOffset - cd.hdr.FileOffset;

    cd.size = chunkSize;
    cd.data = m_pFileBuffer + cd.hdr.FileOffset;
  }

  // Resort chunks again now by Id, for faster queryies later.
  std::sort( m_chunks.begin(),m_chunks.end(),ChunkCompareId );

  //delete []chunks;
  return true;
}

//////////////////////////////////////////////////////////////////////////
bool CReadOnlyChunkFile::Read( const char *filename )
{
	if (m_hFile)
		gEnv->pCryPak->FClose(m_hFile);
	m_hFile = 0;
	FreeBuffer();

	int nOpenFlags = 0;
	if (m_bNoWarningMode)
		nOpenFlags |= ICryPak::FOPEN_HINT_QUIET;

	m_hFile = gEnv->pCryPak->FOpen(filename,"rb",nOpenFlags );
	if (!m_hFile)
		return false;

	size_t nFileSize = 0;
	m_pFileBuffer = (char*)gEnv->pCryPak->FGetCachedFileData(m_hFile,nFileSize);
	m_bOwnFileBuffer = false;
	if (!m_pFileBuffer)
		return false;

	if (!ReadChunkTableFromBuffer( m_pFileBuffer,nFileSize ))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CReadOnlyChunkFile::ReadFromMemBlock( const void * pData, int nDataSize )
{
	if (!ReadChunkTableFromBuffer( pData, nDataSize ))
	{
		return false;
	}
	return true;
}
