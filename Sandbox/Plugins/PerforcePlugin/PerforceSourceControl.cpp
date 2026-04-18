////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   PerforceSourceControl.cpp
//  Version:     v1.00
//  Created:     22 Sen 2004 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <CryModuleDefs.h>
#include "CryFile.h"


#include "Include/ISourceControl.h"
#include "Include/IEditorClassFactory.h"


#include "PerforceSourceControl.h"

#define BBox BBox1
#include "platform_impl.h"


void CMyClientUser::HandleError(Error *e)
{
	/*
	StrBuf  m;
	e->Fmt( &m );
	if ( strstr( m.Text(), "file(s) not in client view." ) )
		e->Clear();
	else if ( strstr( m.Text(), "no such file(s)" ) )
		e->Clear();
	else if ( strstr( m.Text(), "access denied" ) )
		e->Clear();
	*/
	m_e = *e;
}


void CMyClientUser::Init()
{
	m_bIsSetup = false;
	m_bIsPreCreateFile = false;
}

void CMyClientUser::PreCreateFileName(const char * file)
{
	m_bIsPreCreateFile = true;
	strcpy(m_file, file);
	m_findedFile[0]=0;
}


void CMyClientUser::OutputStat( StrDict *varList )
{
	if(m_bIsSetup && !m_bIsPreCreateFile)
		return;

	StrRef var, val;
	*m_action=0;
	*m_headAction=0;

	for( int i = 0; varList->GetVar( i, var, val ); i++ )
	{
		if(m_bIsPreCreateFile)
		{
			if(var=="clientFile")
			{
				char tmpval[MAX_PATH];
				strcpy(tmpval, val.Text());
				char * ch = tmpval;
				while(ch = strchr(ch, '/'))
					*ch = '\\';

				if(!stricmp(tmpval, m_file))
				{
					strcpy(m_findedFile, val.Text());
					m_bIsPreCreateFile = false;
				}
			}
		}
		else
		{
			if(var=="action")
				strcpy(m_action, val.Text());
			if(var=="headAction")
				strcpy(m_headAction, val.Text());
		}
	}
	m_bIsSetup = true;
}

void	CMyClientUser::OutputInfo( char level, const char *data )
{
	if(!m_bIsPreCreateFile)
		return;

	const char * ch = strrchr(data, '/');
	if(ch)
	{
		if(!stricmp(ch+1, m_file))
		{
			strcpy(m_findedFile, ch+1);
			m_bIsPreCreateFile = false;
		}
	}
}

void CMyClientUser::Edit( FileSys *f1, Error *e )
{
	char msrc[4000];
	char mdst[4000];

	char * src=&msrc[0];
	char * dst=&mdst[0];

	f1->Open(FOM_READ, e);
	int size = f1->Read(msrc, 10240, e);
	msrc[size]=0;
	f1->Close(e);

	while(*dst=*src)
	{
		if(!strnicmp(src, "\nDescription", 11))
		{
			src++;
			while(*src!='\n' && *src!='\0')
				src++;
			src++;
			while(*src!='\n' && *src!='\0')
				src++;
			src--;
			strcpy(dst, "\nDescription:\n\t!Sandbox: ");
			dst += 25;
			strcpy(dst, m_desc);
			dst += strlen(m_desc)-1;
		}
		src++;
		dst++;
	}

	f1->Open(FOM_WRITE, e);
	f1->Write(mdst, strlen(mdst), e);
	f1->Close(e);
	
	strcpy(m_desc, m_initDesc);
}



////////////////////////////////////////////////////////////

extern ISystem* g_pSystem;

CPerforceSourceControl::CPerforceSourceControl() : m_ref(0)
{
	m_client.Init( &m_e );
	if ( m_e.Test() )
		g_pSystem->GetILog()->Log("\nPerforce plugin: Failed to connect.");
	else
		g_pSystem->GetILog()->Log("\nPerforce plugin: Connected.");
}



void CPerforceSourceControl::FreeData()
{
	m_client.Final( &m_e );
}


void CPerforceSourceControl::ConvertFileNameCS(char *sDst, const char *sSrcFilename)
{
	*sDst = 0;
	bool bFinded = true;

	char szAdjustedFile[ICryPak::g_nMaxPath];
	strcpy(szAdjustedFile, sSrcFilename);

	//_finddata_t fd;
	WIN32_FIND_DATA fd;

	char csPath[ICryPak::g_nMaxPath]={0};

	char * ch, *ch1;

	ch = strrchr(szAdjustedFile, '\\');
	ch1 = strrchr(szAdjustedFile, '/');
	if(ch < ch1) ch = ch1;
	bool bIsFirstTime = true;

	bool bIsEndSlash = false;
	if(ch && ch-szAdjustedFile+1 == strlen(szAdjustedFile))
		bIsEndSlash = true;

	while(ch)
	{
		//intptr_t handle;
		//handle = gEnv->pCryPak->FindFirst( szAdjustedFile, &fd );
		HANDLE handle = FindFirstFile( szAdjustedFile, &fd );
		//if (handle != -1)
		if(handle != INVALID_HANDLE_VALUE)
		{
			char tmp[ICryPak::g_nMaxPath];
			strcpy(tmp, csPath);
			//strcpy(csPath, fd.name);
			strcpy(csPath, fd.cFileName);
			if(!bIsFirstTime)
				strcat(csPath, "\\");
			bIsFirstTime = false;
			strcat(csPath, tmp);

			//gEnv->pCryPak->FindClose( handle );
			FindClose(handle);
		}
		
		*ch = 0;
		ch = strrchr(szAdjustedFile, '\\');
		ch1 = strrchr(szAdjustedFile, '/');
		if(ch < ch1) ch = ch1;
	}

	if(!*csPath)
		return;

	strcat(szAdjustedFile,"\\");
	strcat(szAdjustedFile, csPath);
	if(bIsEndSlash || strlen(szAdjustedFile) < strlen(sSrcFilename))
		strcat(szAdjustedFile, "\\");

	// if we have only folder on disk find in perforce
	if(strlen(szAdjustedFile) < strlen(sSrcFilename))
	{
		if(IsFileManageable(szAdjustedFile))
		{
			char file[MAX_PATH];
			char clienFile[MAX_PATH]={0};

			bool bCont = true;
			while (bCont)
			{
				strcpy(file, &sSrcFilename[strlen(szAdjustedFile)]);
				char * ch = strchr(file, '/');
				char * ch1 = strchr(file, '\\');
				if(ch < ch1) ch = ch1;

				if(ch)
				{
					*ch=0;
					bFinded = bCont = FindDir(clienFile, szAdjustedFile, file);
				}
				else
				{
					bFinded = FindFile(clienFile, szAdjustedFile, file);
					bCont = false;
				}
				strcpy(szAdjustedFile, clienFile);
				if(bCont && strlen(clienFile)>=strlen(sSrcFilename))
					bCont = false;
			}
		}
	}

	if(bFinded)
		strcpy(sDst, szAdjustedFile);
}

bool CPerforceSourceControl::FindDir(char * clientFile, const char * folder, const char * dir)
{
	char fl[MAX_PATH];
	strcpy(fl, folder );
	strcat(fl, "*" );
	char * argv[] = { fl};
	m_ui.PreCreateFileName(dir);

	m_client.SetArgv( 1, argv );
	m_client.Run( "dirs", &m_ui );
	m_client.WaitTag();

	if(m_ui.m_e.Test())
		return false;

	strcpy(clientFile, folder);
	strcat(clientFile, m_ui.m_findedFile);
	strcat(clientFile, "\\");
	if(*m_ui.m_findedFile)
		return true;

	return false;
}


bool CPerforceSourceControl::FindFile(char * clientFile, const char * folder, const char * file)
{
	char fullPath[MAX_PATH];

	strcpy(fullPath, folder);
	strcat(fullPath, file);

	char fl[MAX_PATH];
	strcpy(fl, folder );
	strcat(fl, "*" );
	char * argv[] = { fl};
	m_ui.PreCreateFileName(fullPath);

	m_client.SetArgv( 1, argv );
	m_client.Run( "fstat", &m_ui );
	m_client.WaitTag();

	if(m_ui.m_e.Test())
		return false;

	strcpy(clientFile, m_ui.m_findedFile);
	if(*clientFile)
		return true;

	return false;
}


bool CPerforceSourceControl::IsFileManageable(const char *sFilename)
{
	bool bRet=false;

	char fl[MAX_PATH];
	strcpy(fl, sFilename);
	//ConvertFileNameCS(fl, sFilename);
	char * argv[] = { fl};
	m_ui.Init();
	m_client.SetArgv( 1, argv );
	m_client.Run( "fstat", &m_ui ); 
	m_client.WaitTag();
	
	if(!m_ui.m_e.IsFatal())
		bRet=true;
	m_ui.m_e.Clear();
	return bRet;
}

bool CPerforceSourceControl::IsFileExistsInDatabase(const char *sFilename)
{
	bool bRet=false;

	char fl[MAX_PATH];
	strcpy(fl, sFilename);
	char * argv[] = { fl};
	m_ui.Init();
	m_client.SetArgv( 1, argv );
	m_client.Run( "fstat", &m_ui ); 
	m_client.WaitTag();
	
	if(!m_ui.m_e.Test())
	{
		if(strcmp(m_ui.m_headAction, "delete"))
			bRet=true;
	}
	m_ui.m_e.Clear();
	return bRet;
}

bool CPerforceSourceControl::IsFileCheckedOutByUser(const char *sFilename)
{
	bool bRet=false;

	char fl[MAX_PATH];
	strcpy(fl, sFilename);
	char * argv[] = { fl};
	m_ui.Init();
	m_client.SetArgv( 1, argv );
	m_client.Run( "fstat", &m_ui ); 
	m_client.WaitTag();
	
	if(!strcmp(m_ui.m_action,"edit") &&  !m_ui.m_e.Test())
		bRet=true;
	m_ui.m_e.Clear();
	return bRet;
}


uint32 CPerforceSourceControl::GetFileAttributesAndFileName( const char *filename, char * FullFileName )
{
	if(FullFileName)
		FullFileName[0]=0;
	CCryFile file;
	if (!file.Open(filename,"rb"))
		return SCC_FILE_ATTRIBUTE_INVALID;

	uint32 attributes = 0;
	const char *sFullFilenameLC = file.GetAdjustedFilename();
	char sFullFilename[ICryPak::g_nMaxPath];
	ConvertFileNameCS(sFullFilename, sFullFilenameLC);

	if(FullFileName)
		strcpy(FullFileName, sFullFilename);

	if (file.IsInPak())
	{
		attributes = SCC_FILE_ATTRIBUTE_READONLY|SCC_FILE_ATTRIBUTE_INPAK;
		//if(m_pIntegrator && m_pIntegrator->FileIsManageable(sFullFilename) && m_pIntegrator->FileExistsInDatabase (sFullFilename))

		if(IsFileManageable(sFullFilename) && IsFileExistsInDatabase (sFullFilename))
		{
			attributes |= SCC_FILE_ATTRIBUTE_MANAGED;
			if(IsFileCheckedOutByUser(sFullFilename))
				attributes |= SCC_FILE_ATTRIBUTE_CHECKEDOUT;
		}
		return attributes;
	}

	DWORD dwAttrib = ::GetFileAttributes(sFullFilename);
	if (dwAttrib != INVALID_FILE_ATTRIBUTES)
	{
		attributes = SCC_FILE_ATTRIBUTE_NORMAL;
		if (dwAttrib & FILE_ATTRIBUTE_READONLY)
			attributes |= SCC_FILE_ATTRIBUTE_READONLY;

		//if(m_pIntegrator)
		{
			if(IsFileManageable (sFullFilename))
			{
				if(IsFileExistsInDatabase (sFullFilename))
				{
					attributes |= SCC_FILE_ATTRIBUTE_MANAGED;
					if(IsFileCheckedOutByUser(sFullFilename))
						attributes |= SCC_FILE_ATTRIBUTE_CHECKEDOUT;
				}
			}
		}
		return attributes;
	}
	return SCC_FILE_ATTRIBUTE_INVALID;
}

uint32 CPerforceSourceControl::GetFileAttributes( const char *filename )
{
	return GetFileAttributesAndFileName(filename, 0);
}


bool CPerforceSourceControl::IsFolder(const char * filename, char * FullFileName)
{
	bool bFolder = false;

	char sFullFilename[ICryPak::g_nMaxPath];
	ConvertFileNameCS(sFullFilename, filename);

	uint32 attr = ::GetFileAttributes(sFullFilename);
	if(attr==INVALID_FILE_ATTRIBUTES)
	{
		if(*sFullFilename && sFullFilename[strlen(sFullFilename)-1]=='\\')
			bFolder = true;
		else
			return false;
	}
	else
		if((attr & FILE_ATTRIBUTE_DIRECTORY))
			bFolder = true;

	if(bFolder )
		strcpy(FullFileName, sFullFilename);

	return bFolder;
}


bool CPerforceSourceControl::Add( const char *filename, const char * desc, int nFlags)
{
	bool bRet = false;

	char FullFileName[MAX_PATH];

	CString	str = filename;
	int curPos = 0;


	CString resToken = str.Tokenize(";",curPos);
	while (!resToken.IsEmpty())
	{
		resToken.Trim();
		bool bFolder = false;
		uint32 attrib = GetFileAttributesAndFileName(resToken, FullFileName);
		if(attrib == SCC_FILE_ATTRIBUTE_INVALID)
			bFolder = IsFolder(resToken, FullFileName);

		/*if(bFolder)
		{
			CNxNString sNamespacePath;
			if (m_pIntegrator->MapManagedPath(FullFileName, sNamespacePath))
			{
				CNxNItem itm;
				if(pInt->GetItem(sNamespacePath, itm))
					if(itm.Import(FullFileName, ""))
						bRet = true;
			}
		}
		else*/ if((attrib!=SCC_FILE_ATTRIBUTE_INVALID) && !(attrib & SCC_FILE_ATTRIBUTE_MANAGED) && IsFileManageable(FullFileName))
		{
			char * argv[] = { FullFileName};
			m_client.SetArgv( 1, argv );
			m_client.Run( "add", &m_ui );
			if(desc)
				strcpy(m_ui.m_desc, desc);
			m_client.SetArgv( 1, argv );
			m_client.Run( "submit", &m_ui );
			if ( !m_ui.m_e.Test() )
				bRet = true;
		}
		resToken = str.Tokenize(";",curPos);
	}


	return bRet;
}

bool CPerforceSourceControl::CheckIn( const char *filename, const char * desc, int nFlags)
{
	bool bRet = false;

	char FullFileName[MAX_PATH];

	CString	str = filename;
	int curPos = 0;

	CString resToken = str.Tokenize(";",curPos);
	while (!resToken.IsEmpty())
	{
		resToken.Trim();

		bool bFolder=false;
		uint32 attrib = GetFileAttributesAndFileName(resToken, FullFileName);
		if(attrib == SCC_FILE_ATTRIBUTE_INVALID)
			bFolder = IsFolder(resToken, FullFileName);

		if(bFolder)
			strcat(FullFileName, "...");

		if(bFolder || ((attrib != SCC_FILE_ATTRIBUTE_INVALID) && (attrib & SCC_FILE_ATTRIBUTE_MANAGED) && (attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT)))
		{
			{
				char * argv[] = { "-c", "default", FullFileName};
				m_client.SetArgv( 3, argv );
				m_client.Run( "reopen", &m_ui );
			}

			if(desc)
				strcpy(m_ui.m_desc, desc);
			{
				char * argv[] = { FullFileName};
				m_client.SetArgv( 1, argv );
				m_client.Run( "submit", &m_ui );
			}

			if ( !m_ui.m_e.Test() )
				bRet=true;
		}
		resToken = str.Tokenize(";",curPos);
	}

	return bRet;
}

bool CPerforceSourceControl::CheckOut( const char *filename, int nFlags)
{
	bool bRet = false;

	char FullFileName[MAX_PATH];

	bool bFolder=false;
	uint32 attrib = GetFileAttributesAndFileName(filename, FullFileName);
	if(attrib == SCC_FILE_ATTRIBUTE_INVALID)
		bFolder = IsFolder(filename, FullFileName);

	if(bFolder)
		strcat(FullFileName, "...");

	if(bFolder || ((attrib & SCC_FILE_ATTRIBUTE_MANAGED) && !(attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT) ))
	{
		char * argv[] = { FullFileName};
		m_client.SetArgv( 1, argv );
		m_client.Run( "edit", &m_ui );
		if ( !m_ui.m_e.Test() )
			bRet=true;
	}
	return bRet;
}

bool CPerforceSourceControl::UndoCheckOut( const char *filename, int nFlags)
{
	bool bRet = false;

	char FullFileName[MAX_PATH];

	bool bFolder=false;
	uint32 attrib = GetFileAttributesAndFileName(filename, FullFileName);
	if(attrib == SCC_FILE_ATTRIBUTE_INVALID)
		bFolder = IsFolder(filename, FullFileName);

	if(bFolder)
		strcat(FullFileName, "...");

	if(bFolder  || ((attrib & SCC_FILE_ATTRIBUTE_MANAGED) && (attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT)))
	{
		char * argv[] = { FullFileName};
		m_client.SetArgv( 1, argv );
		m_client.Run( "revert", &m_ui );
		if ( !m_ui.m_e.Test() )
			bRet=true;
	}

	return bRet;
}

bool CPerforceSourceControl::Rename( const char *filename, const char *newname, const char * desc, int nFlags)
{
	bool bRet = false;
	char FullFileName[MAX_PATH];
	uint32 attrib = GetFileAttributesAndFileName(filename, FullFileName);

	if(!(attrib & SCC_FILE_ATTRIBUTE_MANAGED))
		return true;

	//if(m_pIntegrator->FileCheckedOutByAnotherUser(FullFileName))
		//return false;

	char FullNewFileName[MAX_PATH];
	strcpy(FullNewFileName, newname);

	{
		char * argv[] = { FullFileName, FullNewFileName };
		m_client.SetArgv( 2, argv );
		m_client.Run( "integrate", &m_ui );
	}

	{
		char * argv[] = { FullFileName };
		m_client.SetArgv( 1, argv );
		m_client.Run( "delete", &m_ui );
	}
	if(desc)
		strcpy(m_ui.m_desc, desc);
	{
		char * argv[] = { FullFileName };
		m_client.SetArgv( 1, argv );
		m_client.Run( "submit", &m_ui );
	}

	if(desc)
		strcpy(m_ui.m_desc, desc);
	{
		char * argv[] = { FullNewFileName };
		m_client.SetArgv( 1, argv );
		m_client.Run( "submit", &m_ui );
	}

	if ( !m_ui.m_e.Test() )
		bRet=true;

//p4 integrate source_file target_file
//p4 delete source_file
//p4 submit 


	return bRet;
}

bool CPerforceSourceControl::Delete( const char *filename, const char * desc, int nFlags)
{
	bool bRet = false;
	char FullFileName[MAX_PATH];
	uint32 attrib = GetFileAttributesAndFileName(filename, FullFileName);

	if(!(attrib & SCC_FILE_ATTRIBUTE_MANAGED))
		return true;

	//if(m_pIntegrator->FileCheckedOutByAnotherUser(FullFileName))
		//return false;

	if((attrib & SCC_FILE_ATTRIBUTE_MANAGED) && (attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT))
	{
		char * argv[] = { FullFileName};
		m_client.SetArgv( 1, argv );
		m_client.Run( "revert", &m_ui );
	}

	char * argv[] = { FullFileName};
	m_client.SetArgv( 1, argv );
	m_client.Run( "delete", &m_ui );

	if(desc)
		strcpy(m_ui.m_desc, desc);
	m_client.SetArgv( 1, argv );
	m_client.Run( "submit", &m_ui );
	if ( !m_ui.m_e.Test() )
		bRet = true;

	return bRet;
}


bool CPerforceSourceControl::GetLatestVersion( const char *filename, int nFlags)
{
	bool bRet = false;
	char FullFileName[MAX_PATH];
	uint32 attrib = GetFileAttributesAndFileName(filename, FullFileName);

	bool bFolder=false;
	if(attrib == SCC_FILE_ATTRIBUTE_INVALID)
	{
		bFolder = IsFolder(filename, FullFileName);
		if(!bFolder)
			return true;
	}
	else
		if(!(attrib & SCC_FILE_ATTRIBUTE_MANAGED))
			return true;

	if(bFolder)
		strcat(FullFileName, "...");

	if(attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
	{
		char * argv[] = { FullFileName};
		m_client.SetArgv( 1, argv );
		m_client.Run( "revert", &m_ui ); 
	}
	/*
	else if (attrib & SCC_FILE_ATTRIBUTE_INPAK)
	{
		char * argv[] = { "-f", FullFileName};
		m_client.SetArgv( 2, argv );
		m_client.Run( "sync", &m_ui ); 
	}
	else
	{
		char * argv[] = { FullFileName};
		m_client.SetArgv( 1, argv );
		m_client.Run( "reopen", &m_ui ); 
	}
	*/

	char * argv[] = { "-f", FullFileName};
	m_client.SetArgv( 2, argv );
	m_client.Run( "sync", &m_ui );

	bRet=true;
	return bRet;
}

