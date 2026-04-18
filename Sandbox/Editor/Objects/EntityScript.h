////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   EntityScript.h
//  Version:     v1.00
//  Created:     10/12/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: EntityScript definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __EntityScript_h__
#define __EntityScript_h__

#if _MSC_VER > 1000
#pragma once
#endif

#define PROPERTIES_TABLE "Properties"
#define PROPERTIES2_TABLE "PropertiesInstance"
#define FIRST_ENTITY_CLASS_ID 200

// forward declaration
class CEntity;
struct IScriptTable;
struct IEntityClass;
struct IScriptObject;

#define EVENT_PREFIX "Event_"

//////////////////////////////////////////////////////////////////////////
enum EEntityScriptFlags
{
	ENTITY_SCRIPT_SHOWBOUNDS      = 0x0001,
	ENTITY_SCRIPT_ABSOLUTERADIUS  = 0x0002,
	ENTITY_SCRIPT_ICONONTOP       = 0x0004,
	ENTITY_SCRIPT_DISPLAY_ARROW   = 0x0008,
};

/*!
 *  CEntityScript holds information about Entity lua script.
 */
class CEntityScript : public CRefCountBase
{
public:
	CEntityScript( IEntityClass *pClass );
	virtual ~CEntityScript();

	//////////////////////////////////////////////////////////////////////////
	IEntityClass* GetClass() const { return m_pClass; }

	//! Get name of entity script.
	CString GetName() const;
	// Get file of script.
	CString GetFile() const;

	//////////////////////////////////////////////////////////////////////////
	// Flags.
	int GetFlags() const { return m_nFlags; }

	int GetMethodCount() const { return m_methods.size(); }
	const CString& GetMethod( int index ) const { return m_methods[index]; }

	//////////////////////////////////////////////////////////////////////////
	int	GetEventCount();
	const CString& GetEvent( int i );

	//////////////////////////////////////////////////////////////////////////
	int	GetEmptyLinkCount();
	const CString& GetEmptyLink( int i );

	//////////////////////////////////////////////////////////////////////////
	//! Get properties of this sacript.
	CVarBlock* GetProperties() const { return m_properties; }
	CVarBlock* GetProperties2() const { return m_properties2; }
	//////////////////////////////////////////////////////////////////////////

	bool	Load();
	void	Reload();
	bool	IsValid() const { return m_valid; };

	//! Marks script not valid, must be loaded on next access.
	void Invalidate() { m_valid = false; };

	//! Takes current values of properties from Entity and put it to entity table.
	void	SetProperties( IEntity *entity,CVarBlock *properties,bool bCallUpdate );
	void	SetProperties2( IEntity *entity,CVarBlock *properties,bool bCallUpdate );
	void  CallOnPropertyChange( IEntity *ientity );

	//! Setup entity target events table
	void	SetEventsTable( CEntity *entity );

	//! Run method.
	void	RunMethod( IEntity *entity,const CString &method );
	void	SendEvent( IEntity *entity,const CString &event );

	// Edit methods.
	void	GotoMethod( const CString &method );
	void	AddMethod( const CString &method );

	//! Get visual object for this entity script.
	const CString&	GetVisualObject() { return m_visualObject; };

	// Retrieve texture icon associated with this entity class.
	int GetTextureIcon() { return m_nTextureIcon; };

	//! Check if entity of this class can be used in editor.
	bool IsUsable() const { return m_usable; }

	// Set class as placable or not.
	void SetUsable( bool usable ) { m_usable = usable; }

private:
	bool ParseScript();
	int FindLineNum( const CString &line );

	//! Put var block to script properties.
	void VarToScriptObject( IVariable *var,IScriptTable *obj );

	IEntityClass *m_pClass;

	bool m_valid;

	bool m_haveEventsTable;
	
	//! True if entity script have update entity
	bool m_bUpdatePropertiesImplemented;

	CString m_visualObject;
	int m_visibilityMask;
	int m_nTextureIcon;

	bool m_usable;

	//! Array of methods in this script.
	std::vector<CString> m_methods;

	//! Array of events supported by this script.
	std::vector<CString> m_events;

	// Create empty links.
	std::vector<CString> m_emptyLinks;

	int m_nFlags;
	TSmartPtr<CVarBlock> m_properties;
	TSmartPtr<CVarBlock> m_properties2;
};

typedef TSmartPtr<CEntityScript> CEntityScriptPtr;

/*!
 *	CEntityScriptRegistry	manages all known CEntityScripts instances.
 */
class CEntityScriptRegistry
{
public:
	CEntityScriptRegistry();
	~CEntityScriptRegistry();

	CEntityScript* Find( const CString &name );
	void	Insert( CEntityScript *script );

	void LoadScripts();
	void Reload();

	//! Get all scripts as array.
	void	GetScripts( std::vector<CEntityScript*> &scripts );

	static CEntityScriptRegistry* Instance();
	static void Release();

private:
	StdMap<CString,CEntityScriptPtr> m_scripts;
	static CEntityScriptRegistry* m_instance;
};

#endif // __EntityScript_h__
