////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   pathutil.h
//  Version:     v1.00
//  Created:     5/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Utility functions to simplify working with paths.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __pathutil_h__
#define __pathutil_h__
#pragma once

#include <ICryPak.h>

namespace Path
{
	//! Convert a path to the uniform form.
	inline CString ToUnixPath( const CString& strPath )
	{
		CString str = strPath;
		str.Replace( '\\','/' );
		return str;
	}

	//! Split full file name to path and filename
	//! @param filepath [IN] Full file name inclusing path.
	//! @param path [OUT] Extracted file path.
	//! @param file [OUT] Extracted file (with extension).
	inline void Split( const CString &filepath,CString &path,CString &file )
	{
		char path_buffer[_MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		_splitpath( filepath,drive,dir,fname,ext );
		_makepath( path_buffer,drive,dir,0,0 );
		path = path_buffer;
		_makepath( path_buffer,0,0,fname,ext );
		file = path_buffer;
	}

	//! Split full file name to path and filename
	//! @param filepath [IN] Full file name inclusing path.
	//! @param path [OUT] Extracted file path.
	//! @param filename [OUT] Extracted file (without extension).
	//! @param ext [OUT] Extracted files extension.
	inline void Split( const CString &filepath,CString &path,CString &filename,CString &fext )
	{
		char path_buffer[_MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		_splitpath( filepath,drive,dir,fname,ext );
		_makepath( path_buffer,drive,dir,0,0 );
		path = path_buffer;
		filename = fname;
		fext = ext;
	}

	//! Extract extension from full specified file path.
	inline CString GetExt( const CString &filepath )
	{
		char ext[_MAX_EXT];
		_splitpath( filepath,0,0,0,ext );
		if (ext[0] == '.')
			return ext+1;
		
		return ext;
	}

	//! Extract path from full specified file path.
	inline CString GetPath( const CString &filepath )
	{
		char path_buffer[_MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		_splitpath( filepath,drive,dir,0,0 );
		_makepath( path_buffer,drive,dir,0,0 );
		return path_buffer;
	}

	//! Extract file name with extension from full specified file path.
	inline CString GetFile( const CString &filepath )
	{
		char path_buffer[_MAX_PATH];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		_splitpath( filepath,0,0,fname,ext );
		_makepath( path_buffer,0,0,fname,ext );
		return path_buffer;
	}

	//! Extract file name without extension from full specified file path.
	inline CString GetFileName( const CString &filepath )
	{
		char fname[_MAX_FNAME];
		_splitpath( filepath,0,0,fname,0 );
		return fname;
	}

	//! Removes the trailing backslash from a given path.
	inline CString RemoveBackslash( const CString &path )
	{
		if (path.IsEmpty())
			return path;

		int iLenMinus1 = path.GetLength()-1;
		char cLastChar = path[iLenMinus1];
		
		if(cLastChar=='\\' || cLastChar == '/')
			return path.Mid( 0,iLenMinus1 );

		return path;
	}

	//! add a backslash if needed
	inline CString AddBackslash( const CString &path )
	{
		if(path.IsEmpty() || path[path.GetLength()-1] == '\\')
			return path;

		return path + "\\";
	}
	//! add a slash if needed
	inline CString AddSlash( const CString &path )
	{
		if(path.IsEmpty() || path[path.GetLength()-1] == '/')
			return path;

		return path + "/";
	}

	//! Replace extension for givven file.
	inline CString ReplaceExtension( const CString &filepath,const CString &ext )
	{
		char path_buffer[_MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		_splitpath( filepath,drive,dir,fname,0 );
		_makepath( path_buffer,drive,dir,fname,ext );
		return path_buffer;
	}

	//! Replace extension for givven file.
	inline CString RemoveExtension( const CString &filepath )
	{
		char path_buffer[_MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		_splitpath( filepath,drive,dir,fname,0 );
		_makepath( path_buffer,drive,dir,fname,0 );
		return path_buffer;
	}

	//! Makes a fully specified file path from path and file name.
	inline CString Make( const CString &path,const CString &file )
	{
		return AddBackslash(path) + file;
	}

	//! Makes a fully specified file path from path and file name.
	inline CString Make( const CString &dir,const CString &filename,const CString &ext )
	{
		char path_buffer[_MAX_PATH];
		_makepath( path_buffer,NULL,dir,filename,ext );
		return path_buffer;
	}

	//! Makes a fully specified file path from path and file name.
	inline CString MakeFullPath( const CString &relativePath )
	{
		return relativePath;
	}

	//////////////////////////////////////////////////////////////////////////
	inline CString GetRelativePath( const CString &fullPath, bool bRelativeToGameFolder = false )
	{
		if (fullPath.IsEmpty())
			return "";

		char rootpath[_MAX_PATH];
		GetCurrentDirectory(sizeof(rootpath),rootpath);
		if( bRelativeToGameFolder == true )
		{
			strcat(rootpath, "\\");
			strcat(rootpath, PathUtil::GetGameFolder().c_str());
		}

		// Create relative path
		char path[_MAX_PATH];
		char srcpath[_MAX_PATH];
		strcpy( srcpath,fullPath );
		if (FALSE == PathRelativePathTo( path,rootpath,FILE_ATTRIBUTE_DIRECTORY,srcpath, NULL))
			return fullPath;

		CString relPath = path;
		relPath.TrimLeft("\\/.");
		return relPath;
	}

	//////////////////////////////////////////////////////////////////////////
	// Description:
	// given the source relative game path, constructs the full path to the file according to the flags.
	// Ex. Objects/box will be converted to C:\Test\GXme\Objects\box.cgf
	CString GamePathToFullPath( const CString &path );

	//////////////////////////////////////////////////////////////////////////
	// Description:
	//    Make a game correct path out of any input path.
	inline CString MakeGamePath( const CString &path )
	{
		CString fullpath = Path::GamePathToFullPath(path);
		CString rootDataFolder = Path::AddBackslash(PathUtil::GetGameFolder().c_str());
		if (fullpath.GetLength() > rootDataFolder.GetLength() && strnicmp(fullpath,rootDataFolder,rootDataFolder.GetLength()) == 0)
		{
			fullpath = fullpath.Right(fullpath.GetLength()-rootDataFolder.GetLength());
			fullpath.Replace('\\','/'); // Slashes use for game files.
			return fullpath;
		}
		fullpath = GetRelativePath(path);
		if (fullpath.IsEmpty())
		{
			fullpath = path;
		}
		fullpath.Replace('\\','/'); // Slashes use for game files.
		return fullpath;
	}

	inline CString GetGameFolder() 
	{
		return (gEnv->pCryPak->GetGameFolder()).c_str();
	}
};

#endif // __pathutil_h__
