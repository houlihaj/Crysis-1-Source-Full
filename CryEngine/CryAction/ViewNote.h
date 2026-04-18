/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2005.
 -------------------------------------------------------------------------	
	
  Description:	First prototype-like implementation of View Note
  
 -------------------------------------------------------------------------
  History:
  - 03:11:2005   : Created by MichaelR
	
*************************************************************************/
#ifndef __VIEWNOTE_H__
#define __VIEWNOTE_H__


struct SWaitInfo;
void ViewNote(IConsoleCmdArgs* args);
void WaitForViewNote(void* state, bool bTimedOut);


#ifdef WIN32

struct SWaitInfo
{
  SWaitInfo() : handle(NULL) 
  {}
  PROCESS_INFORMATION pi;  
  HANDLE handle;
  char* szFile;

};
#endif


//------------------------------------------------------------------------
void ViewNote(IConsoleCmdArgs* args)
{ 
#ifdef WIN32 
  HANDLE hFile;
  SWaitInfo* pInfo = new SWaitInfo;  

  // create temporary file   
  char szFileName[] = "viewnote.txt";      

  char szTmpPath[1024] = "";
  GetTempPath(1024-strlen(szFileName), szTmpPath);

  pInfo->szFile = new char[strlen(szTmpPath)+strlen(szFileName)+1];
  sprintf(pInfo->szFile, "%s%s", szTmpPath, szFileName);

  hFile = CreateFile(pInfo->szFile, GENERIC_READ, 0, NULL, CREATE_ALWAYS, 
    FILE_ATTRIBUTE_NORMAL, NULL );  

  if (hFile == INVALID_HANDLE_VALUE) 
  { 
    CryLog("Could not open file %s (error %d)\n", pInfo->szFile, GetLastError());
    return;
  }
  CloseHandle(hFile);


  // spawn notepad with file opened
  char szCmd[1024];
  sprintf(szCmd, "notepad.exe %s", pInfo->szFile);

  STARTUPINFO si;
  ZeroMemory( &si, sizeof(si) );
  si.cb = sizeof(si);
  si.dwX = 100;
  si.dwY = 100;
  si.dwXSize = 300;
  si.dwYSize = 100;
  si.dwFlags = STARTF_USEPOSITION | STARTF_USESIZE;


  ZeroMemory( &(pInfo->pi), sizeof(pInfo->pi) );

  if( !CreateProcess( NULL, // No module name (use command line). 
    szCmd,				    // Command line. 
    NULL,             // Process handle not inheritable. 
    NULL,             // Thread handle not inheritable. 
    FALSE,            // Set handle inheritance to FALSE. 
    NULL,             // creation flags. 
    NULL,             // Use parent's environment block. 
    NULL,					    // Set starting directory. 
    &si,              // Pointer to STARTUPINFO structure.
    &(pInfo->pi) )    // Pointer to PROCESS_INFORMATION structure.
    ) 
  {    
    CryLog("ViewNote: CreateProcess failed!");
    return;
  }

  // register wait object
  pInfo->handle = RegisterWaitForSingleObjectEx(pInfo->pi.hProcess, (WAITORTIMERCALLBACK)WaitForViewNote, pInfo, INFINITE, WT_EXECUTEONLYONCE);  

#endif  
}


//------------------------------------------------------------------------
void WaitForViewNote(void* state, bool bTimedOut)
{
#ifdef WIN32
  SWaitInfo* pInfo = (SWaitInfo*)state;

  CryLog("WaitForViewNote: notified!");

  if (!pInfo)
    return;

  //CloseHandle( pInfo->pi.hProcess );
  //CloseHandle( pInfo->pi.hThread );  

  if (pInfo->handle)
  {   
    UnregisterWait(pInfo->handle);    
  }

  // reload file into string    
  HANDLE hFile = CreateFile(pInfo->szFile, GENERIC_READ, FILE_SHARE_READ, NULL, 
    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );  

  if (hFile == INVALID_HANDLE_VALUE) 
  { 
    CryLog("Could not open file %s (error %d)\n", pInfo->szFile, GetLastError());
    return;
  }

  unsigned int size = GetFileSize(hFile, NULL);
  if (size == INVALID_FILE_SIZE)
  {
    CryLog("INVALID_FILE_SIZE, returning..");
    return;
  }

  char* buf = new char[size+1];
  DWORD bytesRead;

  if (! ::ReadFile(hFile, buf, size, &bytesRead, NULL))    
  {      
    CryLog("ReadFile failed (error %d)", GetLastError());
    return;
  }

  buf[bytesRead] = '\0';
  CryLog("ViewNote: %s", buf); // finished

  // cleanup
  delete[] buf;
  CloseHandle(hFile);
  DeleteFile(pInfo->szFile);     

  delete[] pInfo->szFile;
  delete pInfo;
#endif
}




#endif //__VIEWNOTE_H__
