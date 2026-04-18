////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   pakfile.h
//  Version:     v1.00
//  Created:     30/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __pakfile_h__
#define __pakfile_h__
#pragma once

// forward references.
struct ICryArchive;
TYPEDEF_AUTOPTR(ICryArchive);

#include "CryMemFile.h"			// CCryMemFile

/*!	CPakFile Wraps game implementation of ICryArchive.
		Used for storing multiple files into zip archive file.
*/
class CPakFile
{
public:
	CPakFile();
	~CPakFile();
	//! Opens archive for writing.
	explicit CPakFile( const char *filename );
	//! Opens archive for writing.
	bool Open( const char *filename,bool bAbsolutePath=true );
	//! Opens archive for reading only.
	bool OpenForRead( const char *filename );

	void Close();
	//! Adds or update file in archive.
	bool UpdateFile( const char *filename,CCryMemFile &file,bool bCompress=true );
	//! Adds or update file in archive.
	bool UpdateFile( const char *filename,CMemoryBlock &mem,bool bCompress=true,int nCompressLevel=ICryArchive::LEVEL_BETTER );
	//! Adds or update file in archive.
	bool UpdateFile( const char *filename,void *pBuffer,int nSize,bool bCompress=true,int nCompressLevel=ICryArchive::LEVEL_BETTER );
	//! Remove file from archive.
	bool RemoveFile( const char *filename );
	//! Remove dir from archive.
	bool RemoveDir( const char *directory );

	//! Return archive of this pak file wrapper.
	ICryArchive* GetArchive() { return m_pArchive; };
private:
	ICryArchive_AutoPtr m_pArchive;
};

#endif // __pakfile_h__
