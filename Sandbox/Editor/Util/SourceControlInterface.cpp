////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   SourceControlInterface.cpp
//  Version:     v1.00
//  Created:     1/9/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SourceControlInterface.h"

//////////////////////////////////////////////////////////////////////////
CSourceControlInterface::CSourceControlInterface()
{
	m_pSCM = 0;
}

//////////////////////////////////////////////////////////////////////////
CSourceControlInterface::~CSourceControlInterface()
{
	if (m_pSCM)
		m_pSCM->Release();
}

//////////////////////////////////////////////////////////////////////////
ISourceControl* CSourceControlInterface::GetSCMInterface()
{
	if(!gSettings.ssafeParams.SourceControlOn)
		return this;
	std::vector<IClassDesc*> classes;
	GetIEditor()->GetClassFactory()->GetClassesBySystemID( ESYSTEM_CLASS_SCM_PROVIDER,classes );
	for (int i = 0; i < classes.size(); i++)
	{
		IClassDesc *pClass = classes[i];
		ISourceControl *pSCM = NULL;
		HRESULT hRes = pClass->QueryInterface( __uuidof(ISourceControl),(void **)&pSCM );
		if (!FAILED(hRes) && pSCM)
		{
			if (gSettings.ssafeParams.SourceControlPluginName == pClass->ClassName())
				return pSCM;
		}
	}
	return this;
	/*
	std::vector<IClassDesc*> classes;
	GetIEditor()->GetClassFactory()->GetClassesBySystemID( ESYSTEM_CLASS_SCM_PROVIDER,classes );
	for (int i = 0; i < classes.size(); i++)
	{
		IClassDesc *pClass = classes[i];
		ISourceControlProvider *pSCM = NULL;
		HRESULT hRes = pClass->QueryInterface( __uuidof(ISourceControl),&pSCM );
		if (!FAILED(hRes))
		{
		}
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
uint32 CSourceControlInterface::GetFileAttributes( const char *filename )
{
	CCryFile file;
	if (!file.Open(filename,"rb"))
		return SCC_FILE_ATTRIBUTE_INVALID;

	if (file.IsInPak())
	{
		return SCC_FILE_ATTRIBUTE_READONLY|SCC_FILE_ATTRIBUTE_INPAK;
	}

	uint32 attributes = 0;
	
	const char *sFullFilename = file.GetAdjustedFilename();
	DWORD dwAttrib = ::GetFileAttributes(sFullFilename);
	if (dwAttrib != INVALID_FILE_ATTRIBUTES)
	{
		attributes = SCC_FILE_ATTRIBUTE_NORMAL;
		if (dwAttrib & FILE_ATTRIBUTE_READONLY)
			attributes |= SCC_FILE_ATTRIBUTE_READONLY;
		return attributes;
	}

	return SCC_FILE_ATTRIBUTE_INVALID;
}
