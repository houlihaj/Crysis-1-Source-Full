////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   baselibrarymanager.h
//  Version:     v1.00
//  Created:     10/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __baselibrarymanager_h__
#define __baselibrarymanager_h__
#pragma once

#include "IDataBaseItem.h"
#include "IDataBaseLibrary.h"
#include "IDataBaseManager.h"

class CBaseLibraryItem;
class CBaseLibrary;

/** Manages all Libraries and Items.
*/
class CBaseLibraryManager : public TRefCountBase<IDataBaseManager>, public IEditorNotifyListener
{
public:
	CBaseLibraryManager();
	~CBaseLibraryManager();

	//! Clear all libraries.
	virtual void ClearAll();

	//////////////////////////////////////////////////////////////////////////
	// IDocListener implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );

	//////////////////////////////////////////////////////////////////////////
	// Library items.
	//////////////////////////////////////////////////////////////////////////
	//! Make a new item in specified library.
	virtual IDataBaseItem* CreateItem( IDataBaseLibrary *pLibrary );
	//! Delete item from library and manager.
	virtual void DeleteItem( IDataBaseItem* pItem );

	//! Find Item by its GUID.
	virtual IDataBaseItem* FindItem( REFGUID guid ) const;
	virtual IDataBaseItem* FindItemByName( const CString &fullItemName );
	virtual IDataBaseItem* LoadItemByName( const CString &fullItemName );

	virtual IDataBaseItemEnumerator* GetItemEnumerator();

	//////////////////////////////////////////////////////////////////////////
	// Set item currently selected.
	virtual void SetSelectedItem( IDataBaseItem *pItem );
	// Get currently selected item.
	virtual IDataBaseItem* GetSelectedItem() const;

	//////////////////////////////////////////////////////////////////////////
	// Libraries.
	//////////////////////////////////////////////////////////////////////////
	//! Add Item library.
	virtual IDataBaseLibrary* AddLibrary( const CString &library, bool bSetFullFilename=false );
	virtual void DeleteLibrary( const CString &library );
	//! Get number of libraries.
	virtual int GetLibraryCount() const { return m_libs.size(); };
	//! Get Item library by index.
	virtual IDataBaseLibrary* GetLibrary( int index ) const;

	//! Find Items Library by name.
	virtual IDataBaseLibrary* FindLibrary( const CString &library );

	//! Load Items library.
	virtual IDataBaseLibrary* LoadLibrary( const CString &filename );

	//! Save all modified libraries.
	virtual void SaveAllLibs();

	//! Serialize property manager.
	virtual void Serialize( XmlNodeRef &node,bool bLoading );

	//! Export items to game.
	virtual void Export( XmlNodeRef &node ) {};

	//void AddNotifyListener( 

	//! Returns unique name base on input name.
	virtual CString MakeUniqItemName( const CString &name );
	virtual CString MakeFullItemName( IDataBaseLibrary *pLibrary,const CString &group,const CString &itemName );

	//! Root node where this library will be saved.
	virtual CString GetRootNodeName() = 0;
	//! Path to libraries in this manager.
	virtual CString GetLibsPath() = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Validate library items for errors.
	void Validate();

	//////////////////////////////////////////////////////////////////////////
	void GatherUsedResources( CUsedResources &resources );

	virtual void AddListener( IDataBaseManagerListener *pListener );
	virtual void RemoveListener( IDataBaseManagerListener *pListener );

	//////////////////////////////////////////////////////////////////////////
	virtual void RegisterItem( CBaseLibraryItem *pItem,REFGUID newGuid );
	virtual void RegisterItem( CBaseLibraryItem *pItem );
	virtual void UnregisterItem( CBaseLibraryItem *pItem );

	// Only Used internally.
	void RenameItem( CBaseLibraryItem *pItem,const CString &newName,const CString &oldName );

	// Called by items to indicated that they have been modified.
	// Sends item changed event to listeners.
	void OnItemChanged( IDataBaseItem *pItem );

protected:
	void SplitFullItemName( const CString &fullItemName,CString &libraryName,CString &itemName );
	void NotifyItemEvent( IDataBaseItem *pItem,EDataBaseItemEvent event );
	void SetRegisteredFlag( CBaseLibraryItem *pItem,bool bFlag );

	//////////////////////////////////////////////////////////////////////////
	// Must be overriden.
	//! Makes a new Item.
	virtual CBaseLibraryItem* MakeNewItem() = 0;
	virtual CBaseLibrary* MakeNewLibrary() = 0;
	//////////////////////////////////////////////////////////////////////////

	virtual void ReportDuplicateItem( CBaseLibraryItem *pItem,CBaseLibraryItem *pOldItem );

protected:
	bool m_bUniqGuidMap;
	bool m_bUniqNameMap;

	//! Array of all loaded entity items libraries.
	std::vector<TSmartPtr<CBaseLibrary> > m_libs;

	// There is always one current level library.
	TSmartPtr<CBaseLibrary> m_pLevelLibrary;

	// GUID to item map.
	typedef std::map<GUID,TSmartPtr<CBaseLibraryItem>,guid_less_predicate> ItemsGUIDMap;
	ItemsGUIDMap m_itemsGuidMap;

	// Case insensitive name to items map.
	typedef std::map<CString,CBaseLibraryItem*,stl::less_stricmp<CString> > ItemsNameMap;
	ItemsNameMap m_itemsNameMap;

	std::vector<IDataBaseManagerListener*> m_listeners;

	// Currently selected item.
	_smart_ptr<CBaseLibraryItem> m_pSelectedItem;
};

//////////////////////////////////////////////////////////////////////////
template <class TMap>
class CDataBaseItemEnumerator : public IDataBaseItemEnumerator
{
	TMap *m_pMap;
	typename TMap::iterator m_iterator;

public:
	CDataBaseItemEnumerator( TMap *pMap )
	{
		assert(pMap);
		m_pMap = pMap;
		m_iterator = m_pMap->begin();
	}
	virtual void Release() { delete this; };
	virtual IDataBaseItem* GetFirst()
	{
		m_iterator = m_pMap->begin();
		if (m_iterator == m_pMap->end())
			return 0;
		return m_iterator->second;
	}
	virtual IDataBaseItem* GetNext()
	{
		if (m_iterator != m_pMap->end())
			m_iterator++;
		if (m_iterator == m_pMap->end())
			return 0;
		return m_iterator->second;
	}
};

#endif // __baselibrarymanager_h__
