////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   filechangemonitor.cpp
//  Version:     v1.00
//  Created:     15/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FileChangeMonitor.h"

#include "Thread.h"
#include "Util\FileUtil.h"
#include <sys/stat.h>
#include <CryThread.h>

INT64 g_ChangeFileTimeDelta = 0;

//////////////////////////////////////////////////////////////////////////
// Directory monitoring thread.
//////////////////////////////////////////////////////////////////////////
struct CFileChangeMonitorThread : public CThread
{
public:
	std::vector<HANDLE> m_handles;
	MTQueue<CString> m_pendingItems;
	std::vector<CString> m_monitorItems;
	MTQueue<CString> m_files;
	DWORD m_mainThreadId;

	static HANDLE m_killEvent;
	static HANDLE m_updateEvent;

	CFileChangeMonitorThread()
	{
		m_mainThreadId = GetCurrentThreadId();

		// First event in list is KillEvent.
		m_killEvent = ::CreateEvent( NULL,TRUE,FALSE,NULL );
		m_handles.push_back(m_killEvent);

		// Last event in list is UpdateEvent.
		m_updateEvent = ::CreateEvent( NULL,TRUE,FALSE,NULL );
		m_handles.push_back(m_updateEvent);

		m_monitorItems.push_back("");
	}

	~CFileChangeMonitorThread()
	{
		if ( m_killEvent )
		{
			::CloseHandle( m_killEvent );
			m_killEvent = 0;
		}

		if ( m_updateEvent )
		{
			::CloseHandle( m_updateEvent );
			m_updateEvent = 0;
		}
	}

protected:
	void FindChangedFiles( const CString &item );

	void Run()
	{
		CryThreadSetName( -1,"FileMonitor" );
		DWORD dwWaitStatus;

		// If First handle triggers, its quit.
		// If Last handle triggers, its update.
		// Waiting thread.
		while (TRUE) 
		{ 
			int numHandles = m_handles.size();

			// Wait for notification.
			dwWaitStatus = WaitForMultipleObjects( numHandles,&m_handles[0],FALSE,INFINITE );

			if (dwWaitStatus >= WAIT_OBJECT_0 && dwWaitStatus < WAIT_OBJECT_0+numHandles)
			{
				if (dwWaitStatus == WAIT_OBJECT_0)
				{
					// This is Thread Kill event.
					break;
				}

				if (dwWaitStatus == WAIT_OBJECT_0+numHandles-1)
				{
					// This is Thread Update event.

					// Remove update event from back of list
					::CloseHandle( m_updateEvent );
					m_handles.pop_back();

					// Add pending items to list
					while ( !m_pendingItems.empty() )
					{
						const CString item = m_pendingItems.top();
						m_pendingItems.pop();

						// Test for duplicate
						bool bAlreadyExists = false;
						for (std::vector<CString>::iterator iter=m_monitorItems.begin(); iter!=m_monitorItems.end(); ++iter)
						{
							if ( item.CompareNoCase( *iter ) == 0 )
							{
								bAlreadyExists = true;
								break;
							}
						}

						if ( bAlreadyExists == false )
						{
							// Create Notification Event.
							HANDLE dwHandle = FindFirstChangeNotification(item,TRUE,FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_FILE_NAME); // watch file name changes 

							m_handles.push_back( dwHandle );
							m_monitorItems.push_back( item );
						}
					} 

					// Add update event handle to end of handle list
					m_updateEvent = ::CreateEvent( NULL,TRUE,FALSE,NULL );
					m_handles.push_back(m_updateEvent);

					continue;
				}

				// One of objects got triggered, find out which.
				int id = dwWaitStatus - WAIT_OBJECT_0;
				CString item = m_monitorItems[id];

				FindChangedFiles(item);

				// Now the intesting part.. we need to find which file have been changed.
				// The fastest way, is scan directoryto take current time

				if (FindNextChangeNotification(m_handles[id]) == FALSE)
				{
					// Error!!!.
					//ExitProcess(GetLastError());
				}
				// Notify main thread that something have changed.
				PostThreadMessage( m_mainThreadId,WM_FILEMONITORCHANGE,0,0 );
			}
		}

		// Close all change notification handles.
		if ( m_handles.size() > 2) // this is because we always have a kill and an update handle, both not closed here
		{
			for (int i = 1; i < m_handles.size()-1; i++)
			{
				FindCloseChangeNotification(m_handles[i]);
			}
		}
	}
};

HANDLE CFileChangeMonitorThread::m_killEvent = 0;
HANDLE CFileChangeMonitorThread::m_updateEvent = 0;

//////////////////////////////////////////////////////////////////////////
inline void UnixTimeToFileTime(time_t t, LPFILETIME pft)
{
	// Note that LONGLONG is a 64-bit value
	LONGLONG ll;

	ll = Int32x32To64(t, 10000000) + 116444736000000000;
	pft->dwLowDateTime = (DWORD)ll;
	pft->dwHighDateTime = ll >> 32;
}

//////////////////////////////////////////////////////////////////////////
inline bool ScanDirectoryFiles( const CString &root,const CString &path,const CString &fileSpec,
															 std::vector<CFileChangeMonitor::SFileEnumInfo> &files,INT64 currFTime )
{
	bool anyFound = false;
	CString dir = Path::AddBackslash(root + path);

	CString findFilter = Path::Make(dir,fileSpec);
	ICryPak *pIPak = GetIEditor()->GetSystem()->GetIPak();

	// Add all directories.
	CFileFind finder;
	BOOL bWorking = finder.FindFile( Path::Make(dir,fileSpec) );
	while (bWorking)
	{
		bWorking = finder.FindNextFile();

		if (finder.IsDots())
			continue;

		if (!finder.IsDirectory())
		{
			anyFound = true;

			CFileChangeMonitor::SFileEnumInfo fd;
			fd.filename = dir + finder.GetFileName();
			//fd.nFileSize = finder.GetLength();

			//finder.GetCreationTime( &fd.ftCreationTime );
			finder.GetLastAccessTime( &fd.ftLastAccessTime );
			finder.GetLastWriteTime( &fd.ftLastWriteTime );
			
			/*
			HANDLE hFile = CreateFile( fd.filename,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_ARCHIVE,NULL );
			FILETIME f1,f2,f3;
			GetFileTime( hFile,&f1,&f2,&f3 );
			CloseHandle(hFile);

			//INT64 ftime2 = *(INT64*)&fd.ftCreationTime;
			//INT64 ftime3 = *(INT64*)&fd.ftLastAccessTime;
			*/

			INT64 ftime1 = *(INT64*)&fd.ftLastAccessTime;
			INT64 ftime2 = *(INT64*)&fd.ftLastWriteTime;
			INT64 ftime = (ftime1 > ftime2) ? ftime1 : ftime2;

			//INT64 ftime = *(INT64*)&fd.ftLastWriteTime;
			INT64 dt = currFTime - ftime;
			if (dt < 0)
				dt = ftime - currFTime;

			if (dt < g_ChangeFileTimeDelta)
			{
				files.push_back(fd);
			}
		}
	}

	return anyFound;
}

//////////////////////////////////////////////////////////////////////////
// Get directory contents.
//////////////////////////////////////////////////////////////////////////
inline bool ScanDirectoryRecursive( const CString &root,const CString &path,const CString &fileSpec,std::vector<CFileChangeMonitor::SFileEnumInfo> &files, bool recursive,INT64 currFTime )
{
	bool anyFound = false;
	CString dir = Path::AddBackslash(root + path);

	if (ScanDirectoryFiles( root,path,fileSpec,files,currFTime ))
		anyFound = true;

	if (recursive)
	{
		CFileFind finder;
		BOOL bWorking = finder.FindFile( Path::Make(dir,"*.*") );
		while (bWorking)
		{
			bWorking = finder.FindNextFile();

			if (finder.IsDots())
				continue;

			if (finder.IsDirectory())
			{
				// Scan directory.
				if (ScanDirectoryRecursive( root,Path::AddBackslash(path+finder.GetFileName()),fileSpec,files,recursive,currFTime ))
					anyFound = true;
			}
		}
	}
	
	return anyFound;
}

//////////////////////////////////////////////////////////////////////////
void CFileChangeMonitorThread::FindChangedFiles( const CString &item )
{
	/*
	// Open directory handle.
	HANDLE hDir = CreateFile( dir,FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL );
	if (hDir)
	{
		DWORD bufSize = 8*1024;
		DWORD nBytesReturned = 0;
		void *lpBegin = malloc(bufSize);
		if (ReadDirectoryChangesW( hDir,lpBegin,bufSize,TRUE,FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_LAST_ACCESS|FILE_NOTIFY_CHANGE_SIZE,&nBytesReturned,NULL,NULL ) != 0)
		{
			if (nBytesReturned > 0)
			{
				FILE_NOTIFY_INFORMATION *lpInfo = (FILE_NOTIFY_INFORMATION*)lpBegin;
				while (lpInfo)
				{
					WCHAR wcFileName[MAX_PATH+1] = {0};//L"";
					memcpy(	wcFileName, lpInfo->FileName,MIN(MAX_PATH*sizeof(WCHAR),lpInfo->FileNameLength) );

					CString filename = dir + CString(wcFileName);
					m_files.push( filename );

					if (lpInfo->NextEntryOffset)
						lpInfo = (FILE_NOTIFY_INFORMATION*)(((char*)lpInfo) + lpInfo->NextEntryOffset);
					else
						lpInfo = NULL;
				}
			}
		}
		CloseHandle(hDir);
		free(lpBegin);
	}
	return;
	*/

	//CLogFile::WriteLine( "** Searching file" );
	// Get current file time.
	SYSTEMTIME curSysTime;
	FILETIME curFileTime;
	GetSystemTime( &curSysTime );
	SystemTimeToFileTime( &curSysTime,&curFileTime );

	INT64 curftime = *(INT64*)&curFileTime;

	SYSTEMTIME s1,s2;
	FILETIME ft1,ft2;
	ZeroStruct(ft1);
	FileTimeToSystemTime(&ft1,&s1);
	FileTimeToSystemTime(&ft1,&s2);

	s2.wSecond = 5; // 5 second.

	SystemTimeToFileTime(&s1,&ft1);
	SystemTimeToFileTime(&s2,&ft2);
	INT64 deltaT = (*(INT64*)&ft2) - (*(INT64*)&ft1) ; // 5 second.

	g_ChangeFileTimeDelta = deltaT;

	std::vector<CFileChangeMonitor::SFileEnumInfo> files;
	files.reserve( 1000 );
	ScanDirectoryRecursive( item,"","*.*",files,true,curftime );

	for (int i = 0; i < files.size(); i++)
	{
		// This file was written within deltaT time from now, consider it as modified.
		CString filename = files[i].filename;
		m_files.push( filename );
	}
}

//////////////////////////////////////////////////////////////////////////
CFileChangeMonitor::CFileChangeMonitor()
{
	m_thread = new CFileChangeMonitorThread;

	// Start monitoring thread.
	m_thread->Start();
}

//////////////////////////////////////////////////////////////////////////
CFileChangeMonitor::~CFileChangeMonitor()
{
	// Send to thread a kill event.
	StopMonitor();
}

//////////////////////////////////////////////////////////////////////////
bool CFileChangeMonitor::IsDirectory(const char* sFileName)
{
	struct __stat64 my_stat;
	if (_stat64(sFileName, &my_stat) != 0) return false;
	return ((my_stat.st_mode & S_IFDIR) != 0);
}

//////////////////////////////////////////////////////////////////////////
bool CFileChangeMonitor::IsFile(const char* sFileName)
{
	struct __stat64 my_stat;
	if (_stat64(sFileName, &my_stat) != 0) return false;
	return ((my_stat.st_mode & S_IFDIR) == 0);
}

//////////////////////////////////////////////////////////////////////////
bool CFileChangeMonitor::MonitorItem( const CString &sItem )
{
	bool bRet = false;

	bool bIsDirectory = IsDirectory( Path::RemoveBackslash( sItem ) );
	bool bIsFile = IsFile( sItem );

	if ( ( bIsDirectory || bIsFile ) &&
		m_thread && CFileChangeMonitorThread::m_updateEvent != 0 )
	{
		m_thread->m_pendingItems.push( bIsDirectory ? sItem : Path::RemoveBackslash( Path::GetPath( sItem ) ) );

		::SetEvent(CFileChangeMonitorThread::m_updateEvent);
		bRet = true;
	}

	return bRet;
}

//////////////////////////////////////////////////////////////////////////
void CFileChangeMonitor::StopMonitor()
{
	if (m_thread && CFileChangeMonitorThread::m_killEvent != 0)
	{
		::SetEvent(CFileChangeMonitorThread::m_killEvent);
	}
}
	
//////////////////////////////////////////////////////////////////////////
bool CFileChangeMonitor::HaveModifiedFiles() const
{
	return !m_thread->m_files.empty();
}

//////////////////////////////////////////////////////////////////////////
CString CFileChangeMonitor::GetModifiedFile()
{
	CString file;
	if (!m_thread->m_files.empty())
	{
		file = m_thread->m_files.top();
		m_thread->m_files.pop();
	}
	return file;
}