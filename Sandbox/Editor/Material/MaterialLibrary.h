////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MaterialLibrary.h
//  Version:     v1.00
//  Created:     22/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __materiallibrary_h__
#define __materiallibrary_h__
#pragma once

#include "BaseLibrary.h"

/** Library of prototypes.
*/
class CRYEDIT_API CMaterialLibrary : public CBaseLibrary
{
public:
	CMaterialLibrary( CBaseLibraryManager *pManager ) : CBaseLibrary(pManager) {};
	virtual bool Save();
	virtual bool Load( const CString &filename );
	virtual void Serialize( XmlNodeRef &node,bool bLoading );

	//////////////////////////////////////////////////////////////////////////
	// CBaseLibrary override.
	//////////////////////////////////////////////////////////////////////////
	void AddItem( IDataBaseItem* item );
	int GetItemCount() const { return m_items.size(); }
	IDataBaseItem* GetItem( int index );
	void RemoveItem( IDataBaseItem* item );
	void RemoveAllItems();
	IDataBaseItem* FindItem( const CString &name );
private:
	std::vector<CBaseLibraryItem*> m_items;
};

#endif // __materiallibrary_h__
