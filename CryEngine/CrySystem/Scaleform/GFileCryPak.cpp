#include "StdAfx.h"


#ifndef EXCLUDE_SCALEFORM_SDK


#include "GFileCryPak.h"

#include <ISystem.h>
#include <ICryPak.h>


GFileCryPak::GFileCryPak( const char* pPath )
: m_pPak( 0 )
, m_fileHandle( 0 )
, m_relativeFilePath( pPath )
{
	m_pPak = gEnv->pCryPak;
	assert( m_pPak );
	m_fileHandle = m_pPak->FOpen( pPath, "rb" );
	//assert( m_fileHandle );
}


GFileCryPak::~GFileCryPak()
{
	if( m_fileHandle )
		Close();
}


const char*	GFileCryPak::GetFilePath()
{
	return( m_relativeFilePath.c_str() );
}


Bool GFileCryPak::IsValid()
{
	return( m_fileHandle != 0 ? true : false );
}


Bool GFileCryPak::IsWritable()
{
	// writeable files are not supported!
	return( false );
}


SInt GFileCryPak::Tell()
{
	assert( m_fileHandle );
	return( m_pPak->FTell( m_fileHandle ) );
}


SInt64 GFileCryPak::LTell()
{
	assert( m_fileHandle );
	return( m_pPak->FTell( m_fileHandle ) );
}


SInt GFileCryPak::GetLength()
{
	assert( m_fileHandle );
	return( m_pPak->FGetSize( m_fileHandle ) );
}


SInt64 GFileCryPak::LGetLength()
{
	assert( m_fileHandle );
	return( m_pPak->FGetSize( m_fileHandle ) );
}


SInt GFileCryPak::GetErrorCode()
{	
	// We cannot blindly return errno as file might have come from a pak.
	// If we have a file handle the file should be good to use, otherwise return errno as file 
	// was not in pak and could also no be accessed on disc.
	return m_fileHandle ? 0 : m_pPak->FGetErrno();
}


SInt GFileCryPak::Write( const UByte* pBuf, SInt numBytes )
{
	// writeable files are not supported!
	return( -1 );
}


SInt GFileCryPak::Read( UByte* pBuf, SInt numBytes )
{
	assert( m_fileHandle );
	SInt res( m_pPak->FReadRaw( pBuf, 1, numBytes, m_fileHandle ) );
	return( m_pPak->FError( m_fileHandle ) ? -1 : res );
}


SInt GFileCryPak::SkipBytes( SInt numBytes )
{
	assert( m_fileHandle );
	SInt res( m_pPak->FSeek( m_fileHandle, numBytes, SEEK_CUR ) == 0 ? 1 : -1 );
	if( m_pPak->FError( m_fileHandle ) )
		res = -1;
	else if( m_pPak->FEof( m_fileHandle ) )
		res = 0;
	return( res );
}


SInt GFileCryPak::BytesAvailable()
{
	assert( m_fileHandle );
	return( m_pPak->FGetSize( m_fileHandle ) - m_pPak->FTell( m_fileHandle ) );
}


Bool GFileCryPak::Flush()
{
	return( true );
}


SInt GFileCryPak::Seek( SInt offset, SInt origin )
{
	assert( m_fileHandle );
	SInt res( m_pPak->FSeek( m_fileHandle, offset, origin ) );
	if( m_pPak->FError( m_fileHandle ) )
		res = 1;
	return( res );
}


SInt64 GFileCryPak::LSeek( SInt64 offset, SInt origin )	
{
	return( Seek( offset, origin ) );
}


Bool GFileCryPak::ChangeSize( SInt newSize )
{
	// writeable files are not supported!
	return( false );
}

SInt GFileCryPak::CopyFromStream( GFile* pStream, SInt byteSize )	
{
	// writeable files are not supported!
	return( -1 );
}


Bool GFileCryPak::Close()
{
	assert( m_fileHandle );	
	Bool res( m_pPak->FClose( m_fileHandle ) == 0 ? true : false );
	m_fileHandle = 0;
	return( res );
}


#endif // #ifndef EXCLUDE_SCALEFORM_SDK