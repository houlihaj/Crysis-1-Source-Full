////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   ReadOnlyChunkFile.cpp
//  Version:     v1.00
//  Created:     15/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _READONLY_CHUNK_FILE_READER_HDR_
#define _READONLY_CHUNK_FILE_READER_HDR_

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

class CReadOnlyChunkFile : public IChunkFile
{
public:
	//////////////////////////////////////////////////////////////////////////
	CReadOnlyChunkFile( bool bNoWarningMode=false );
	~CReadOnlyChunkFile();

	void Release() { delete this; };

	bool Read( const char *filename );
	bool ReadFromMemBlock( const void * pData, int nDataSize );
	bool ReadChunkTableFromBuffer( const void *pBuffer,int nBufferSize );
	
	// Write chunks to file.
	bool Write( const char *filename ) { return false; };
	void WriteToMemory( void **pData,int *nSize ) {};

	//! Add chunk to file.
	//! @return ChunkID of added chunk.
	int AddChunk( const CHUNK_HEADER &hdr,void *chunkData,int chunkSize ) { return -1; };
	void SetChunkData( int nChunkId,void *chunkData,int chunkSize ) {};
	void DeleteChunkId( int nChunkId ) {};

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

private:
	bool ReadChunkTable( CCryFile &file,int nChunkTableOffset );
	void FreeBuffer();

private:
	// this variable contains the last error occured in this class
	string m_LastError;

	struct SIdToChunk
	{
		int id;
		int index;

		bool operator<( const SIdToChunk &o ) const { return id < o.id; }
		bool operator>( const SIdToChunk &o ) const { return id > o.id; }
		bool operator==( const SIdToChunk &o ) const { return id == o.id; }
		bool operator!=( const SIdToChunk &o ) const { return id != o.id; }
	};

	int m_nLastChunkId;
	FILE_HEADER m_fileHeader;
	std::vector<ChunkDesc> m_chunks;
	
	char *m_pFileBuffer;
	bool m_bOwnFileBuffer;
	bool m_bNoWarningMode;

	FILE *m_hFile;
};

TYPEDEF_AUTOPTR(CReadOnlyChunkFile);

#endif //_READONLY_CHUNK_FILE_READER_HDR_