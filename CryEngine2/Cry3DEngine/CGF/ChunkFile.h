//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:ChunkFile.h
//  Declaration of class CChunkFile
//
//	History:
//	06/26/2002 :Created by Sergiy Migdalskiy <sergiy@crytek.de>
//
//////////////////////////////////////////////////////////////////////
#ifndef _CHUNK_FILE_READER_HDR_
#define _CHUNK_FILE_READER_HDR_

#include "CryHeaders.h"
#include <smartptr.h>
#include <IChunkFile.h>

////////////////////////////////////////////////////////////////////////
// Chunk file reader. 
// Accesses a chunked file structure through file mapping object.
// Opens a chunk file and checks for its validity.
// If it's invalid, closes it as if there was no open operation.
// Error handling is performed through the return value of open: it must
// be true for successfully open files
////////////////////////////////////////////////////////////////////////

class CChunkFile : public IChunkFile
{
public:
	//////////////////////////////////////////////////////////////////////////
	CChunkFile();
	~CChunkFile();

	void Release() { delete this; };

	bool Read( const char *filename );
	
	// Write chunks to file.
	bool Write( const char *filename );
	void WriteToMemory( void **pData,int *nSize );

	virtual bool ReadFromMemBlock( const void * pData, int nDataSize ) { return false; }
	virtual bool ReadChunkTableFromBuffer( const void *pBuffer,int nBufferSize ) { return false; }

	//! Add chunk to file.
	//! @return ChunkID of added chunk.
	int AddChunk( const CHUNK_HEADER &hdr,void *chunkData,int chunkSize );
	void SetChunkData( int nChunkId,void *chunkData,int chunkSize );
	void DeleteChunkId( int nChunkId );

	ChunkDesc* FindChunkByType( int type );
	ChunkDesc* FindChunkById( int id );

	// returns the file headers
	const FileHeader& GetFileHeader() const { return m_fileHeader; };

	//////////////////////////////////////////////////////////////////////////
	// Old interface.
	//////////////////////////////////////////////////////////////////////////
	// returns the raw data of the i-th chunk
	const void* GetChunkData(int nIndex )const;
	// retrieves the raw chunk header, as it appears in the file
	const ChunkHeader& GetChunkHeader( int nIndex )const;
	// Get chunk description at i-th index.
	ChunkDesc* GetChunk( int nIndex );
	// calculates the chunk size, based on the very next chunk with greater offset
	// or the end of the raw data portion of the file
	int GetChunkSize( int nIndex ) const;
	// number of chunks
	int NumChunks()const;
	// number of chunks of the specified type
	int NumChunksOfType (ChunkTypes nChunkType) const;
	const char* GetLastError()const {return m_LastError;}
	uint32 GetFileSize() const { return m_FileSize; };

private:
	void ReleaseChunks();
	bool ReadChunkTable( CCryFile &file,int nChunkTableOffset );

private:
	// this variable contains the last error occured in this class
	string m_LastError;

	int m_nLastChunkId;
	FILE_HEADER m_fileHeader;
	std::vector<ChunkDesc*> m_chunks;
	typedef std::map<int,ChunkDesc*> ChunkIdMap;
	ChunkIdMap m_chunkIdMap;
	char *m_pInternalData;
	uint32 m_FileSize;
};

TYPEDEF_AUTOPTR(CChunkFile);

#endif