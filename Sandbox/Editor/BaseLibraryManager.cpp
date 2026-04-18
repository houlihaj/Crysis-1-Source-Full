////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   baselibrarymanager.cpp
//  Version:     v1.00
//  Created:     10/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BaseLibraryManager.h"

#include "BaseLibrary.h"
#include "BaseLibraryItem.h"
#include "ErrorReport.h"

//////////////////////////////////////////////////////////////////////////
// CBaseLibraryManager implementation.
//////////////////////////////////////////////////////////////////////////
CBaseLibraryManager::CBaseLibraryManager()
{
	m_bUniqNameMap = false;
	m_bUniqGuidMap = true;
	GetIEditor()->RegisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryManager::~CBaseLibraryManager()
{
	ClearAll();
	GetIEditor()->UnregisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::ClearAll()
{
	// Delete all items from all libraries.
	for (int i = 0; i < m_libs.size(); i++)
	{
		m_libs[i]->RemoveAllItems();
	}

	m_itemsGuidMap.clear();
	m_itemsNameMap.clear();
	m_libs.clear();
}

//////////////////////////////////////////////////////////////////////////
IDataBaseLibrary* CBaseLibraryManager::FindLibrary( const CString &library )
{
	for (int i = 0; i < m_libs.size(); i++)
	{
		if (stricmp(library,m_libs[i]->GetName()) == 0)
		{
			return m_libs[i];
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::FindItem( REFGUID guid ) const
{
	CBaseLibraryItem* pMtl = stl::find_in_map( m_itemsGuidMap,guid,(CBaseLibraryItem*)0 );
	return pMtl;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::SplitFullItemName( const CString &fullItemName,CString &libraryName,CString &itemName )
{
	int p;
	p = fullItemName.Find( '.' );
	if (p < 0)
	{
		libraryName = "";
		itemName = fullItemName;
		return;
	}
	libraryName = fullItemName.Mid(0,p);
	itemName = fullItemName.Mid(p+1);
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::FindItemByName( const CString &fullItemName )
{
	CString libraryName,itemName;
	SplitFullItemName( fullItemName,libraryName,itemName );

	CBaseLibraryItem *pItem = stl::find_in_map(m_itemsNameMap,itemName,0);
	return pItem;
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::LoadItemByName( const CString &fullItemName )
{
	CString libraryName, itemName;
	SplitFullItemName( fullItemName, libraryName, itemName );

	if (!FindLibrary(libraryName))
	{
		CString fileName = GetLibsPath() + libraryName + ".xml";;
		fileName.Replace( ' ','_' );
		LoadLibrary(fileName);
	}
	return stl::find_in_map(m_itemsNameMap, itemName, 0);
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::CreateItem( IDataBaseLibrary *pLibrary )
{
	assert( pLibrary );

	// Add item to this library.
	TSmartPtr<CBaseLibraryItem> pItem = MakeNewItem();
	pLibrary->AddItem( pItem );
	return pItem;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::DeleteItem( IDataBaseItem* pItem )
{
	assert( pItem );

	UnregisterItem( (CBaseLibraryItem*)pItem );
	if (pItem->GetLibrary())
	{
		pItem->GetLibrary()->RemoveItem( pItem );
	}

	// Delete all objects from object manager that have 
	//GetIEditor()->GetObjectManager()->GetObjects( objects );
}

//////////////////////////////////////////////////////////////////////////
IDataBaseLibrary* CBaseLibraryManager::LoadLibrary( const CString &inFilename )
{
	CString filename = Path::GetRelativePath( Path::GamePathToFullPath(inFilename) );
	// If library is already loaded ignore it.
	for (int i = 0; i < m_libs.size(); i++)
	{
		if (stricmp(filename,m_libs[i]->GetFilename()) == 0)
		{
			Error( _T("Loading Duplicate Library: %s"),(const char*)filename );
			return 0;
		}
	}

	TSmartPtr<CBaseLibrary> pLib = MakeNewLibrary();
	if (!pLib->Load( filename ))
	{
		Error( _T("Failed to Load Item Library: %s"),(const char*)filename );
		return 0;
	}
	if (FindLibrary(pLib->GetName()) != 0)
	{
		Error( _T("Loading Duplicate Library: %s"),(const char*)pLib->GetName() );
		return 0;
	}
	m_libs.push_back( pLib );
	return pLib;
}

//////////////////////////////////////////////////////////////////////////
IDataBaseLibrary* CBaseLibraryManager::AddLibrary( const CString &library, bool bSetFullFilename )
{
	// Check if library with same name already exist.
	IDataBaseLibrary* pBaseLib = FindLibrary(library);
	if (pBaseLib)
		return pBaseLib;

	CBaseLibrary *lib = MakeNewLibrary();
	lib->SetName( library );

	// Set filename of this library.
	// Make a filename from name of library.
	CString filename = library;
	filename.Replace( ' ','_' );
	// according to timur doesnt matter if by this time the libs path is set or not.
	// therefore it will be delayed later in order to get the proper game path when overriding.
	if (bSetFullFilename) {
		filename = GetLibsPath() + filename + ".xml";
	} else {
		filename = filename + ".xml";
	}
	lib->SetFilename( filename );

	m_libs.push_back( lib );
	return lib;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::DeleteLibrary( const CString &library )
{
	for (int i = 0; i < m_libs.size(); i++)
	{
		if (stricmp(library,m_libs[i]->GetName()) == 0)
		{
			CBaseLibrary *pLibrary = m_libs[i];
			// Check if not level library, they cannot be deleted.
			if (!pLibrary->IsLevelLibrary())
			{
				for (int j = 0; j < pLibrary->GetItemCount(); j++)
				{
					UnregisterItem( (CBaseLibraryItem*)pLibrary->GetItem(j) );
				}
				pLibrary->RemoveAllItems();
				m_libs.erase( m_libs.begin() + i );
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IDataBaseLibrary* CBaseLibraryManager::GetLibrary( int index ) const
{
	assert( index >= 0 && index < m_libs.size() );
	return m_libs[index];
};

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::SaveAllLibs()
{
	for (int i = 0; i < GetLibraryCount(); i++)
	{
		// Check if library is modified.
		IDataBaseLibrary *pLibrary = GetLibrary(i);
		if (pLibrary->IsLevelLibrary())
			continue;
		if (pLibrary->IsModified())
		{
			CString libsPath = GetLibsPath();
			if (!libsPath.IsEmpty())
				CFileUtil::CreateDirectory( libsPath );

			if (pLibrary->Save())
			{
				pLibrary->SetModified(false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::Serialize( XmlNodeRef &node,bool bLoading )
{
	CString rootNodeName = GetRootNodeName();
	if (bLoading)
	{
		CString libsPath = GetLibsPath();

		XmlNodeRef libs = node->findChild( rootNodeName );
		if (libs)
		{
			for (int i = 0; i < libs->getChildCount(); i++)
			{
				// Load only library name.
				XmlNodeRef libNode = libs->getChild(i);
				if (strcmp(libNode->getTag(),"LevelLibrary") == 0)
				{
					m_pLevelLibrary->Serialize( libNode,bLoading );
				}
				else
				{
					CString libName;
					if (libNode->getAttr( "Name",libName ))
					{
						// Load this library.
						CString filename = libName;
						filename.Replace( ' ','_' );
						filename = GetLibsPath() + filename + ".xml";
						if (!FindLibrary(libName))
						{
							LoadLibrary( filename );
						}
					}
				}
			}
		}
	}
	else
	{
		// Save all libraries.
		XmlNodeRef libs = node->newChild( rootNodeName );
		for (int i = 0; i < GetLibraryCount(); i++)
		{
			IDataBaseLibrary* pLib = GetLibrary(i);
			if (pLib->IsLevelLibrary())
			{
				// Level libraries are saved in in level.
				XmlNodeRef libNode = libs->newChild( "LevelLibrary" );
				pLib->Serialize( libNode,bLoading );
			}
			else
			{
				// Save only library name.
				XmlNodeRef libNode = libs->newChild( "Library" );
				libNode->setAttr( "Name",pLib->GetName() );
			}
		}
		SaveAllLibs();
	}
}

//////////////////////////////////////////////////////////////////////////
CString CBaseLibraryManager::MakeUniqItemName( const CString &srcName )
{
	// Remove all numbers from the end of name.
	CString typeName = srcName;
	int len = typeName.GetLength();
	while (len > 0 && isdigit(typeName[len-1]))
		len--;

	typeName = typeName.Left(len);

	CString tpName = typeName;
	int num = 0;
	
	
	IDataBaseItemEnumerator *pEnum = GetItemEnumerator();
	for (IDataBaseItem *pItem = pEnum->GetFirst(); pItem != NULL; pItem = pEnum->GetNext())
	{
		const char *name = pItem->GetName();
		if (strncmp(name,tpName,len) == 0)
		{
			int n = atoi(name+len) + 1;
			num = MAX( num,n );
		}
	}
	pEnum->Release();
	CString str;
	str.Format( "%s%d",(const char*)typeName,num );
	return str;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::Validate()
{
	IDataBaseItemEnumerator *pEnum = GetItemEnumerator();
	for (IDataBaseItem *pItem = pEnum->GetFirst(); pItem != NULL; pItem = pEnum->GetNext())
	{
		pItem->Validate();
	}
	pEnum->Release();
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::RegisterItem( CBaseLibraryItem *pItem,REFGUID newGuid )
{
	assert(pItem);

	if (m_bUniqGuidMap)
	{
		bool bNewItem = true;
		REFGUID oldGuid = pItem->GetGUID();
		if (!GuidUtil::IsEmpty(oldGuid))
		{
			bNewItem = false;
			m_itemsGuidMap.erase( oldGuid );
		}
		if (GuidUtil::IsEmpty(newGuid))
			return;
		CBaseLibraryItem *pOldItem = stl::find_in_map( m_itemsGuidMap,newGuid,(CBaseLibraryItem*)0 );
		if (!pOldItem)
		{
			pItem->m_guid = newGuid;
			m_itemsGuidMap[newGuid] = pItem;
			pItem->m_bRegistered = true;

			// Notify listeners.
			NotifyItemEvent(pItem,EDB_ITEM_EVENT_ADD);
		}
		else
		{
			if (pOldItem != pItem)
			{
				ReportDuplicateItem( pItem,pOldItem );
			}
		}
	}
	if (m_bUniqNameMap)
	{
		if (!pItem->GetName().IsEmpty())
		{
			CBaseLibraryItem *pOldItem = stl::find_in_map( m_itemsNameMap,pItem->GetName(),(CBaseLibraryItem*)0 );
			if (!pOldItem)
			{
				m_itemsNameMap[pItem->GetName()] = pItem;
				pItem->m_bRegistered = true;
				// Notify listeners.
				NotifyItemEvent(pItem,EDB_ITEM_EVENT_ADD);
			}
			else
			{
				if (pOldItem != pItem)
				{
					ReportDuplicateItem( pItem,pOldItem );
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::RegisterItem( CBaseLibraryItem *pItem )
{
	assert(pItem);

	if (m_bUniqGuidMap)
	{
		if (GuidUtil::IsEmpty(pItem->GetGUID()))
			return;
		CBaseLibraryItem *pOldItem = stl::find_in_map( m_itemsGuidMap,pItem->GetGUID(),(CBaseLibraryItem*)0 );
		if (!pOldItem)
		{
			m_itemsGuidMap[pItem->GetGUID()] = pItem;
			pItem->m_bRegistered = true;

			// Notify listeners.
			NotifyItemEvent(pItem,EDB_ITEM_EVENT_ADD);
		}
		else
		{
			if (pOldItem != pItem)
			{
				ReportDuplicateItem( pItem,pOldItem );
			}
		}
	}

	if (m_bUniqNameMap)
	{
		if (!pItem->GetName().IsEmpty())
		{
			CBaseLibraryItem *pOldItem = stl::find_in_map( m_itemsNameMap,pItem->GetName(),(CBaseLibraryItem*)0 );
			if (!pOldItem)
			{
				m_itemsNameMap[pItem->GetName()] = pItem;
				pItem->m_bRegistered = true;
				// Notify listeners.
				NotifyItemEvent(pItem,EDB_ITEM_EVENT_ADD);
			}
			else
			{
				if (pOldItem != pItem)
				{
					ReportDuplicateItem( pItem,pOldItem );
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::SetRegisteredFlag( CBaseLibraryItem *pItem,bool bFlag )
{
	pItem->m_bRegistered = bFlag;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::ReportDuplicateItem( CBaseLibraryItem *pItem,CBaseLibraryItem *pOldItem )
{
	CString sLibName;
	if (pOldItem->GetLibrary())
		sLibName = pOldItem->GetLibrary()->GetName();
	CErrorRecord err;
	err.pItem = pItem;
	err.error.Format( "Item %s with duplicate GUID to loaded item %s ignored",(const char*)pItem->GetFullName(),(const char*)pOldItem->GetFullName() );
	GetIEditor()->GetErrorReport()->ReportError( err );
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::UnregisterItem( CBaseLibraryItem *pItem )
{
	// Notify listeners.
	NotifyItemEvent(pItem,EDB_ITEM_EVENT_DELETE);

	if (m_bUniqGuidMap)
		m_itemsGuidMap.erase( pItem->GetGUID() );
	if (m_bUniqNameMap && !pItem->GetName().IsEmpty())
		m_itemsNameMap.erase( pItem->GetName() );

	pItem->m_bRegistered = false;
}

//////////////////////////////////////////////////////////////////////////
CString CBaseLibraryManager::MakeFullItemName( IDataBaseLibrary *pLibrary,const CString &group,const CString &itemName )
{
	assert(pLibrary);
	CString name = pLibrary->GetName() + ".";
	if (!group.IsEmpty())
		name += group + ".";
	name += itemName;
	return name;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::GatherUsedResources( CUsedResources &resources )
{
	IDataBaseItemEnumerator *pEnum = GetItemEnumerator();
	for (IDataBaseItem *pItem = pEnum->GetFirst(); pItem != NULL; pItem = pEnum->GetNext())
	{
		pItem->GatherUsedResources( resources );
	}
	pEnum->Release();
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItemEnumerator* CBaseLibraryManager::GetItemEnumerator()
{
	if (m_bUniqNameMap)
		return new CDataBaseItemEnumerator<ItemsNameMap>(&m_itemsNameMap);
	else
		return new CDataBaseItemEnumerator<ItemsGUIDMap>(&m_itemsGuidMap);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
	case eNotify_OnBeginNewScene:
		SetSelectedItem(0);
		ClearAll();
		break;
	case eNotify_OnBeginSceneOpen:
		SetSelectedItem(0);
		ClearAll();
		break;
	case eNotify_OnMissionChange:
		SetSelectedItem(0);
		break;
	case eNotify_OnCloseScene:
		SetSelectedItem(0);
		ClearAll();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::RenameItem( CBaseLibraryItem *pItem,const CString &newName,const CString &oldName )
{
	if (!oldName.IsEmpty())
	{
		m_itemsNameMap.erase(oldName);
	}
	if (!newName.IsEmpty())
		m_itemsNameMap[newName] = pItem;
	OnItemChanged(pItem);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::AddListener( IDataBaseManagerListener *pListener )
{
	stl::push_back_unique(m_listeners,pListener);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::RemoveListener( IDataBaseManagerListener *pListener )
{
	stl::find_and_erase(m_listeners,pListener);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::NotifyItemEvent( IDataBaseItem *pItem,EDataBaseItemEvent event )
{
	// Notify listeners.
	if (!m_listeners.empty())
	{
		for (int i = 0; i < m_listeners.size(); i++)
			m_listeners[i]->OnDataBaseItemEvent(pItem,event);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::OnItemChanged( IDataBaseItem *pItem )
{
	NotifyItemEvent(pItem,EDB_ITEM_EVENT_CHANGED);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::SetSelectedItem( IDataBaseItem *pItem )
{
	if (m_pSelectedItem == pItem)
		return;
	m_pSelectedItem = (CBaseLibraryItem*)pItem;
	NotifyItemEvent(m_pSelectedItem,EDB_ITEM_EVENT_SELECTED);
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::GetSelectedItem() const
{
	return m_pSelectedItem;
}
