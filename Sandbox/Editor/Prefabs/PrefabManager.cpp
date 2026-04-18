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
#include "PrefabManager.h"

#include "PrefabItem.h"
#include "PrefabLibrary.h"

#include "GameEngine.h"
#include "GameExporter.h"

#include "DataBaseDialog.h"
#include "PrefabDialog.h"

#include "Objects/PrefabObject.h"

#define PREFABS_LIBS_PATH "/Prefabs/"

//////////////////////////////////////////////////////////////////////////
// CPrefabManager implementation.
//////////////////////////////////////////////////////////////////////////
CPrefabManager::CPrefabManager()
{	
	m_pLevelLibrary = (CBaseLibrary*)AddLibrary( "Level" );
	m_pLevelLibrary->SetLevelLibrary( true );
}

//////////////////////////////////////////////////////////////////////////
CPrefabManager::~CPrefabManager()
{
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::ClearAll()
{
	CBaseLibraryManager::ClearAll();

	m_pLevelLibrary = (CBaseLibrary*)AddLibrary( "Level" );
	m_pLevelLibrary->SetLevelLibrary( true );
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem* CPrefabManager::MakeNewItem()
{
	return new CPrefabItem;
}
//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CPrefabManager::MakeNewLibrary()
{
	return new CPrefabLibrary(this);
}
//////////////////////////////////////////////////////////////////////////
CString CPrefabManager::GetRootNodeName()
{
	return "PrefabsLibrary";
}
//////////////////////////////////////////////////////////////////////////
CString CPrefabManager::GetLibsPath()
{
	if (m_libsPath.IsEmpty())
		m_libsPath = Path::GetGameFolder()+PREFABS_LIBS_PATH;
	return m_libsPath;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::Serialize( XmlNodeRef &node,bool bLoading )
{
	CBaseLibraryManager::Serialize( node,bLoading );
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::Export( XmlNodeRef &node )
{
}

//////////////////////////////////////////////////////////////////////////
CPrefabItem* CPrefabManager::MakeFromSelection()
{
	CBaseLibraryDialog *dlg = GetIEditor()->OpenDataBaseLibrary( EDB_TYPE_PREFAB );
	if (dlg && dlg->IsKindOf(RUNTIME_CLASS(CPrefabDialog)))
	{
		CPrefabDialog *pPrefabDialog = (CPrefabDialog*)dlg;
		return pPrefabDialog->GetPrefabFromSelection();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::AddSelectionToPrefab()
{
	CSelectionGroup *pSel = GetIEditor()->GetSelection();
	CPrefabObject * pPrefab = 0;
	int cnt = 0;
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject *pObj = pSel->GetObject(i);
		if(pObj->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			cnt++;
			pPrefab = (CPrefabObject*) pObj;
		}
	}
	if(cnt==0)
	{
		Warning( "Select a prefab and objects");
		return;
	}
	if(cnt>1)
	{
		Warning( "Select only one prefab");
		return;
	}


	std::vector<CBaseObject*>objects;
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject *pObj = pSel->GetObject(i);
		if(pObj!=pPrefab)
			objects.push_back(pObj);
	}

	CUndo undo("Add Objects To Prefab");
	pPrefab->StoreUndo( "Add Objects To Prefab" );

	for (int i = 0; i < objects.size(); i++)
		pPrefab->AddObjectToPrefab(objects[i], true);

	pPrefab->GetPrefab()->UpdateFromPrefabObject( pPrefab );
}