////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   GameTokenItem.h
//  Version:     v1.00
//  Created:     10/11/2003 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GameTokenItem_h__
#define __GameTokenItem_h__
#pragma once

#include "BaseLibraryItem.h"
#include <IFlowSystem.h>

struct IGameToken;
struct IGameTokenSystem;

/*! CGameTokenItem contain definition of particle system spawning parameters.
 *	
 */
class CRYEDIT_API CGameTokenItem : public CBaseLibraryItem
{
public:
	CGameTokenItem();
	~CGameTokenItem();

	virtual EDataBaseItemType GetType() const { return EDB_TYPE_GAMETOKEN; };

	virtual void SetName( const CString &name );
	void Serialize( SerializeContext &ctx );

	//! Called after particle parameters where updated.
	void Update();
	CString GetTypeName() const;
	CString GetValueString() const;
	void SetValueString( const char* sValue );
	//! Retrieve value, return false if value cannot be restored
	bool GetValue(TFlowInputData& data) const;
	//! Set value, if bUpdateGTS is true, also the GameTokenSystem's value is set
	void SetValue(const TFlowInputData& data, bool bUpdateGTS=false);

	bool SetTypeName( const char* typeName );
	void SetDescription( const CString &sDescription ) { m_description = sDescription; };
	CString GetDescription() const { return m_description; };

private:
	IGameTokenSystem* m_pTokenSystem;
	TFlowInputData m_value;
	CString m_description;
	CString m_cachedFullName; 
	// we cache the fullname otherwise our d'tor cannot delete the item from the GameTokenSystem
	// because before the d'tor is called the library smart ptr is set to zero sometimes
	// (see CBaseLibrary::RemoveAllItems()) 
	// this whole inconsistency exists because we have cyclic dependencies
	// CBaseLibraryItem and CBaseLibrary using smart pointers
};

#endif // __GameTokenItem_h__


