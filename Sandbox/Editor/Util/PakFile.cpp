////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   pakfile.cpp
//  Version:     v1.00
//  Created:     30/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PakFile.h"
#include <ICryPak.h>

//////////////////////////////////////////////////////////////////////////
CPakFile::CPakFile()
{
	m_pArchive = NULL;
}

//////////////////////////////////////////////////////////////////////////
CPakFile::~CPakFile()
{
	Close();
}

//////////////////////////////////////////////////////////////////////////
CPakFile::CPakFile( const char *filename )
{
	m_pArchive = NULL;
	Open( filename );
}

//////////////////////////////////////////////////////////////////////////
void CPakFile::Close()
{

	m_pArchive = NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::Open( const char *filename,bool bAbsolutePath )
{
	if (m_pArchive)
		Close();
	if (bAbsolutePath)
		m_pArchive = GetIEditor()->GetSystem()->GetIPak()->OpenArchive( filename,ICryArchive::FLAGS_ABSOLUTE_PATHS );
	else
		m_pArchive = GetIEditor()->GetSystem()->GetIPak()->OpenArchive( filename );
	if (m_pArchive)
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::OpenForRead( const char *filename )
{
	if (m_pArchive)
		Close();
	//m_pArchive = GetIEditor()->GetSystem()->GetIPak()->OpenArchive( filename,ICryArchive::FLAGS_OPTIMIZED_READ_ONLY|ICryArchive::FLAGS_RELATIVE_PATHS_ONLY|ICryArchive::FLAGS_IGNORE_MODS );
	m_pArchive = GetIEditor()->GetSystem()->GetIPak()->OpenArchive( filename,ICryArchive::FLAGS_OPTIMIZED_READ_ONLY|ICryArchive::FLAGS_ABSOLUTE_PATHS|ICryArchive::FLAGS_IGNORE_MODS );
	if (m_pArchive)
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::UpdateFile( const char *filename,CCryMemFile &file,bool bCompress )
{
	if (m_pArchive)
	{
		int nSize = file.GetLength();

		UpdateFile( filename,file.GetMemPtr(),nSize,bCompress );
		file.Close();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::UpdateFile( const char *filename,CMemoryBlock &mem,bool bCompress,int nCompressLevel )
{
	if (m_pArchive)
	{
		return UpdateFile( filename,mem.GetBuffer(),mem.GetSize(),bCompress,nCompressLevel );
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////
bool CPakFile::UpdateFile( const char *filename,void *pBuffer,int nSize,bool bCompress,int nCompressLevel )
{
	if (m_pArchive)
	{
		if (bCompress)
			return 0 == m_pArchive->UpdateFile( filename,pBuffer,nSize, ICryArchive::METHOD_DEFLATE, nCompressLevel );
		else
			return 0 == m_pArchive->UpdateFile( filename,pBuffer,nSize );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::RemoveFile( const char *filename )
{
	if (m_pArchive)
	{
		return m_pArchive->RemoveFile( filename );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::RemoveDir( const char *directory )
{
	if (m_pArchive)
	{
		return m_pArchive->RemoveDir( directory );
	}
	return false;
}