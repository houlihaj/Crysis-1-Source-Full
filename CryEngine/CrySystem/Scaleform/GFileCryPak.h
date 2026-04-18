//////////////////////////////////////////////////////////////////////////////
// A wrapper around Scaleform's file interface to delegate all I/O to ICryPak
//////////////////////////////////////////////////////////////////////////////

#ifndef _GFILE_CRYPAK_H_
#define _GFILE_CRYPAK_H_

#pragma once


#ifndef EXCLUDE_SCALEFORM_SDK


#include <GFile.h>


struct ICryPak;


class GFileCryPak : public GFile
{
public:
	GFileCryPak( const char* pPath );
	virtual ~GFileCryPak();

	virtual const char* GetFilePath();
	virtual Bool IsValid();
	virtual Bool IsWritable();
	virtual SInt Tell();
	virtual SInt64 LTell();
	virtual SInt GetLength();
	virtual SInt64 LGetLength();
	virtual	SInt GetErrorCode();
	virtual	SInt Write( const UByte* pBuf, SInt numBytes );
	virtual	SInt Read( UByte* pBuf, SInt numBytes );
	virtual	SInt SkipBytes( SInt numBytes );
	virtual	SInt BytesAvailable();
	virtual	Bool Flush();
	virtual SInt Seek( SInt offset, SInt origin = SEEK_SET );
	virtual SInt64 LSeek( SInt64 offset, SInt origin = SEEK_SET );
	virtual Bool ChangeSize( SInt newSize );
	virtual SInt CopyFromStream( GFile *pStream, SInt byteSize );
	virtual Bool Close();

private:
	ICryPak* m_pPak;
	FILE* m_fileHandle;
	string m_relativeFilePath;
};


#endif // #ifndef EXCLUDE_SCALEFORM_SDK


#endif // #ifndef _GFILE_CRYPAK_H_