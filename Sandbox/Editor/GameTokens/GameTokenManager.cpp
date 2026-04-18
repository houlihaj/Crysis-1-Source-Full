////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   particlemanager.cpp
//  Version:     v1.00
//  Created:     17/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GameTokenManager.h"

#include "GameTokenItem.h"
#include "GameTokenLibrary.h"

#include "GameEngine.h"
#include "GameExporter.h"

#include "DataBaseDialog.h"
#include "GameTokenDialog.h"

#define GAMETOCKENS_LIBS_PATH "/Libs/GameTokens/"

//////////////////////////////////////////////////////////////////////////
// CGameTokenManager implementation.
//////////////////////////////////////////////////////////////////////////
CGameTokenManager::CGameTokenManager()
{
	m_pLevelLibrary = (CBaseLibrary*)AddLibrary( "Level" );
	m_pLevelLibrary->SetLevelLibrary( true );
	m_bUniqGuidMap = false;
	m_bUniqNameMap = true;	
}

//////////////////////////////////////////////////////////////////////////
CGameTokenManager::~CGameTokenManager()
{
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenManager::ClearAll()
{
	CBaseLibraryManager::ClearAll();
	m_pLevelLibrary = (CBaseLibrary*)AddLibrary( "Level" );
	m_pLevelLibrary->SetLevelLibrary( true );
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem* CGameTokenManager::MakeNewItem()
{
	return new CGameTokenItem;
}
//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CGameTokenManager::MakeNewLibrary()
{
	return new CGameTokenLibrary(this);
}
//////////////////////////////////////////////////////////////////////////
CString CGameTokenManager::GetRootNodeName()
{
	return "GameTokensLibrary";
}
//////////////////////////////////////////////////////////////////////////
CString CGameTokenManager::GetLibsPath()
{
	if (m_libsPath.IsEmpty())
		m_libsPath = Path::GetGameFolder()+GAMETOCKENS_LIBS_PATH;
	return m_libsPath;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenManager::Serialize( XmlNodeRef &node,bool bLoading )
{
	CBaseLibraryManager::Serialize( node,bLoading );
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenManager::Export( XmlNodeRef &node )
{
	XmlNodeRef libs = node->newChild( "GameTokensLibraryReferences" );
	for (int i = 0; i < GetLibraryCount(); i++)
	{
		IDataBaseLibrary* pLib = GetLibrary(i);
		// Level libraries are saved in level.
		if (pLib->IsLevelLibrary())
			continue;
		// For Non-Level libs (Global) we save a reference (name)
		// as these are stored in the Game/Libs directory
		XmlNodeRef libNode = libs->newChild( "Library" );
		libNode->setAttr( "Name",pLib->GetName() );
	}	
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenManager::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	__super::OnEditorNotifyEvent(event);


	switch (event)
	{
	case eNotify_OnBeginGameMode:
	case eNotify_OnEndGameMode:
		// Restore default values on modified tokens.
		{
			for (ItemsNameMap::iterator it = m_itemsNameMap.begin(); it != m_itemsNameMap.end(); ++it)
			{
				CGameTokenItem* pToken = (CGameTokenItem*)it->second;
				TFlowInputData data;
				pToken->GetValue(data);
				pToken->SetValue(data, true);
			}
		}
		break;
	}
}
