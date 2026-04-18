////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   AlienBrainSourceControl.cpp
//  Version:     v1.00
//  Created:     22 Sen 2004 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CryFile.h"


#include "Include/ISourceControl.h"
#include "Include/IEditorClassFactory.h"

#include <NxNIntegratorSDK.h>
#include <NxNSmartIntegrator.h>


#include "AlienBrainSourceControl.h"

#define BBox BBox1
#include "platform_impl.h"


extern ISystem* g_pSystem;

CAlienBrainSourceControl::CAlienBrainSourceControl() : m_ref(0)
{
	m_pIntegrator = new CNxNSmartIntegrator();

	if(!m_pIntegrator)
	{
		g_pSystem->GetILog()->Log("\nAlienbrain plugin: new CNxNSmartIntegrator failed.");
		return;
	}
	
	if (!m_pIntegrator->InitInstance(false))
	{
		delete m_pIntegrator;
		g_pSystem->GetILog()->Log("\nAlienbrain plugin: Integrator->InitInstance failed.");
		return;
	}

	g_pSystem->GetILog()->Log("\nAlienbrain plugin: Integrator->InitInstance OK.");
}

void CAlienBrainSourceControl::FreeData()
{
	if(m_pIntegrator)
	{
		m_pIntegrator->ExitInstance();
		delete m_pIntegrator;
	}
}


uint32 CAlienBrainSourceControl::GetFileAttributesAndFileName( const char *filename, char * FullFileName )
{
	if(FullFileName)
		FullFileName[0]=0;
	CCryFile file;
	if (!file.Open(filename,"rb"))
		return SCC_FILE_ATTRIBUTE_INVALID;

	uint32 attributes = 0;
	const char *sFullFilename = file.GetAdjustedFilename();

	if(FullFileName)
		strcpy(FullFileName, sFullFilename);

	if (file.IsInPak())
	{
		attributes = SCC_FILE_ATTRIBUTE_READONLY|SCC_FILE_ATTRIBUTE_INPAK;
		if(m_pIntegrator && m_pIntegrator->FileIsManageable(sFullFilename) && m_pIntegrator->FileExistsInDatabase (sFullFilename))
		{
			attributes |= SCC_FILE_ATTRIBUTE_MANAGED;
			if(m_pIntegrator->FileCheckedOutByUser(sFullFilename))
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

		if(m_pIntegrator)
		{
			if(m_pIntegrator->FileIsManageable (sFullFilename))
			{
				if(m_pIntegrator->FileExistsInDatabase (sFullFilename))
				{
					attributes |= SCC_FILE_ATTRIBUTE_MANAGED;
					if(m_pIntegrator->FileCheckedOutByUser(sFullFilename))
						attributes |= SCC_FILE_ATTRIBUTE_CHECKEDOUT;
				}
			}
		}
		return attributes;
	}

	return SCC_FILE_ATTRIBUTE_INVALID;
}

uint32 CAlienBrainSourceControl::GetFileAttributes( const char *filename )
{
	return GetFileAttributesAndFileName(filename, 0);
}


bool CAlienBrainSourceControl::IsFolder(const char * filename, char * FullFileName)
{
	bool bFolder = false;
	CNxNString manPath;

	uint32 attr = ::GetFileAttributes(filename);
	if(attr==INVALID_FILE_ATTRIBUTES)
		return false;

	if((attr & FILE_ATTRIBUTE_DIRECTORY) && m_pIntegrator->MapManagedPath(filename, manPath))
	{
		strcpy(FullFileName, filename);
		bFolder = true;
	}
	return bFolder;
}


bool CAlienBrainSourceControl::Add( const char *filename, int nFlags)
{
	bool bRet = false;

	char FullFileName[MAX_PATH];

	CString	str = filename;
	int curPos = 0;

	CNxNIntegrator * pInt = m_pIntegrator->GetIntegrator();
	if(pInt)
	{
		CNxNPathCollection paths;

		CString resToken = str.Tokenize(";",curPos);
		while (!resToken.IsEmpty())
		{
			resToken.Trim();
			bool bFolder = false;
			uint32 attrib = GetFileAttributesAndFileName(resToken, FullFileName);
			if(attrib == SCC_FILE_ATTRIBUTE_INVALID)
				bFolder = IsFolder(resToken, FullFileName);

			if(bFolder)
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
			else if((attrib!=SCC_FILE_ATTRIBUTE_INVALID) && !(attrib & SCC_FILE_ATTRIBUTE_MANAGED) && m_pIntegrator->FileIsManageable(FullFileName) && !m_pIntegrator->FileCheckedOutByAnotherUser(FullFileName))
			{
				char folder[MAX_PATH];
				strcpy(folder, FullFileName);
				char * ch = strrchr(folder, '\\');
				CNxNDbFolder * pFolder = 0;
				int i = 0;
				while(ch)
				{
					*ch = 0;
					CNxNString sNamespacePath;
					if (m_pIntegrator->MapManagedPath(folder, sNamespacePath))
						if(pFolder = (CNxNDbFolder *) m_pIntegrator->GetManagedNode(sNamespacePath, CNxNDbFolder::GetType()))
							break;
					ch = strrchr(folder, '\\');
					i++;
				}

				ch++;
				while(i && pFolder)
				{
					pFolder = pFolder->CreateFolder(ch);
					ch += strlen(ch)+1;
					i--;
				}

				//if(pFolder && pFolder->Import(FullFileName))
					//bRet = true;

				//paths.Add(FullFileName);
				CNxNString sNamespacePath;
				if (m_pIntegrator->MapManagedPath(FullFileName, sNamespacePath))
				{
					CNxNItem itm;
					if(pInt->GetItem(sNamespacePath, itm))
						if(itm.Import(FullFileName, ""))
							bRet = true;
				}
			}
			resToken = str.Tokenize(";",curPos);
		}

		/*
		if(!paths.IsEmpty())
		{
			if(pFolder)
				if(pFolder->ImportCollection(paths))
					bRet=true;
		}
		*/
	}

	m_pIntegrator->FlushIfRequired();

	return bRet;
}

bool CAlienBrainSourceControl::CheckIn( const char *filename, int nFlags)
{
	bool bRet = false;
	char FullFileName[MAX_PATH];

	CString	str = filename;
	int curPos = 0;

	CNxNDbNodeList nodeList;

	CString resToken = str.Tokenize(";",curPos);
	while (!resToken.IsEmpty())
	{
		resToken.Trim();

		bool bFolder=false;
		uint32 attrib = GetFileAttributesAndFileName(resToken, FullFileName);
		if(attrib == SCC_FILE_ATTRIBUTE_INVALID)
			bFolder = IsFolder(resToken, FullFileName);

		if(bFolder || ((attrib != SCC_FILE_ATTRIBUTE_INVALID) && (attrib & SCC_FILE_ATTRIBUTE_MANAGED) && (attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT)))
		{
			CNxNString sNamespacePath;
			if (m_pIntegrator->MapManagedPath(FullFileName, sNamespacePath))
			{
				CNxNDbNode *pNode = (CNxNDbNode *) m_pIntegrator->GetManagedNode(sNamespacePath, CNxNDbNode::GetType());
				if(pNode)
					nodeList.Add(pNode);
			}
		}
		resToken = str.Tokenize(";",curPos);
	}

	if(nodeList.HasObjects())
		if(nodeList.CheckIn())
			bRet = true;

	return bRet;
}

bool CAlienBrainSourceControl::CheckOut( const char *filename, int nFlags)
{
	bool bRet = false;
	char FullFileName[MAX_PATH];

	bool bFolder=false;
	uint32 attrib = GetFileAttributesAndFileName(filename, FullFileName);
	if(attrib == SCC_FILE_ATTRIBUTE_INVALID)
		bFolder = IsFolder(filename, FullFileName);

	if(bFolder || ((attrib & SCC_FILE_ATTRIBUTE_MANAGED) && !(attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT) /*&& !m_pIntegrator->FileCheckedOutByAnotherUser(FullFileName)*/))
	{
		CNxNString sNamespacePath;
		if (m_pIntegrator->MapManagedPath(FullFileName, sNamespacePath))
		{
			CNxNDbNode *pNode = (CNxNDbNode*)m_pIntegrator->GetManagedNode(sNamespacePath, CNxNDbNode::GetType());
			if(pNode)
			{
				if(pNode->CheckOut())
					bRet = true;
			}
		}
	}

	return bRet;
}

bool CAlienBrainSourceControl::UndoCheckOut( const char *filename, int nFlags)
{
	bool bRet = false;
	char FullFileName[MAX_PATH];

	bool bFolder=false;
	uint32 attrib = GetFileAttributesAndFileName(filename, FullFileName);
	if(attrib == SCC_FILE_ATTRIBUTE_INVALID)
		bFolder = IsFolder(filename, FullFileName);

	if(bFolder  || ((attrib & SCC_FILE_ATTRIBUTE_MANAGED) && (attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT)))
	{
		CNxNString sNamespacePath;
		if (m_pIntegrator->MapManagedPath(FullFileName, sNamespacePath))
		{
			CNxNDbNode *pNode = (CNxNDbNode*)m_pIntegrator->GetManagedNode(sNamespacePath, CNxNDbNode::GetType());
			if(pNode)
			{
				if(pNode->UndoCheckOut())
					bRet = true;
			}
		}
	}

	return bRet;
}

bool CAlienBrainSourceControl::Rename( const char *filename, const char *newname, int nFlags)
{
	bool bRet = false;
	char FullFileName[MAX_PATH];
	uint32 attrib = GetFileAttributesAndFileName(filename, FullFileName);

	if(!(attrib & SCC_FILE_ATTRIBUTE_MANAGED))
		return true;

	if(m_pIntegrator->FileCheckedOutByAnotherUser(FullFileName))
		return false;

	CNxNString sNamespacePath;
	if (m_pIntegrator->MapManagedPath(FullFileName, sNamespacePath))
	{
		CNxNIntegrator * pInt = m_pIntegrator->GetIntegrator();
		if(pInt)
		{
			CNxNItem itm;
			if(pInt->GetItem(sNamespacePath, itm))
				if(itm.Rename(newname))
					bRet = true;
		}
	}

	return bRet;
}

bool CAlienBrainSourceControl::Delete( const char *filename, int nFlags)
{
	bool bRet = false;
	char FullFileName[MAX_PATH];
	uint32 attrib = GetFileAttributesAndFileName(filename, FullFileName);

	if(!(attrib & SCC_FILE_ATTRIBUTE_MANAGED))
		return true;

	if(m_pIntegrator->FileCheckedOutByAnotherUser(FullFileName))
		return false;

	CNxNString sNamespacePath;
	if (m_pIntegrator->MapManagedPath(FullFileName, sNamespacePath))
	{
		// get integrator from smart integrator
		CNxNIntegrator * pInt = m_pIntegrator->GetIntegrator();
		if(pInt)
		{
			CNxNItem itm;
			if(pInt->GetItem(sNamespacePath, itm))
				if(itm.Delete(false, true))
					bRet = true;
		}
	}

	return bRet;
}


bool CAlienBrainSourceControl::GetLatestVersion( const char *filename, int nFlags)
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

	CNxNString sNamespacePath;
	if (m_pIntegrator->MapManagedPath(FullFileName, sNamespacePath))
	{
		CNxNDbNode *pNode = (CNxNDbNode*)m_pIntegrator->GetManagedNode(sNamespacePath, CNxNDbNode::GetType());
		if(pNode)
			if(pNode->GetLatest())
				bRet = true;
	}

	return bRet;
}

