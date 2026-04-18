////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MaterialManager.h
//  Version:     v1.00
//  Created:     22/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __materialmanager_h__
#define __materialmanager_h__
#pragma once

#include "BaseLibraryManager.h"
#include "Material.h"

#include <I3DEngine.h>

class CMaterial;
class CMaterialLibrary;

#define MATERIAL_FILE_EXT ".mtl"
#define MATERIALS_PATH "materials/"

/** Manages all entity prototypes and material libraries.
*/
class CRYEDIT_API CMaterialManager : public CBaseLibraryManager, public IMaterialManagerListener
{
public:
	//! Notification callback.
	typedef Functor0 NotifyCallback;

	CMaterialManager( CRegistrationContext &regCtx );
	~CMaterialManager();

	void Set3DEngine( I3DEngine *p3DEngine );

	// Clear all prototypes and material libraries.
	void ClearAll();

	//////////////////////////////////////////////////////////////////////////
	// Materials.
	//////////////////////////////////////////////////////////////////////////

	// Loads material.
	CMaterial* LoadMaterial( const CString &sMaterialName,bool bMakeIfNotFound=true );

	// Creates a new material from a xml node.
	CMaterial* CreateMaterial( const CString &sMaterialName,XmlNodeRef &node=XmlNodeRef(),int nMtlFlags=0 );

	//! Export property manager to game.
	void Export( XmlNodeRef &node );
	int ExportLib( CMaterialLibrary *pLib,XmlNodeRef &libNode );

	virtual void SetSelectedItem( IDataBaseItem *pItem );
	void SetCurrentMaterial( CMaterial *pMtl );
	//! Get currently active material.
	CMaterial* GetCurrentMaterial() const;

	//! Serialize property manager.
	virtual void Serialize( XmlNodeRef &node,bool bLoading );

	virtual void SaveAllLibs();

	//////////////////////////////////////////////////////////////////////////
	// IMaterialManagerListener implementation
	//////////////////////////////////////////////////////////////////////////
	// Called when material manager tries to load a material.
	virtual IMaterial* OnLoadMaterial( const char *sMtlName,bool bForceCreation=false );
	virtual void OnCreateMaterial( IMaterial *pMaterial );
	virtual void OnDeleteMaterial( IMaterial *pMaterial );
	//////////////////////////////////////////////////////////////////////////

	// Convert filename of material file into the name of the material.
	CString FilenameToMaterial( const CString &filename );

	// Convert name of the material to the filename.
	CString MaterialToFilename( const CString &sMaterialName );

	//////////////////////////////////////////////////////////////////////////
	// Convert 3DEngine IMaterial to Editor's CMaterial pointer.
	CMaterial* FromIMaterial( IMaterial *pMaterial );

	// Open File selection dialog to create a new material.
	CMaterial* SelectNewMaterial( int nMtlFlags,const char *sStartPath=NULL );

	// Synchronize material between 3dsMax and editor.
	void SyncMaterialEditor();

	//////////////////////////////////////////////////////////////////////////
	void GotoMaterial( CMaterial *pMaterial );
	void GotoMaterial( IMaterial *pMaterial );

protected:
	// Delete specified material, erases material file, and unassigns from all objects.
	bool DeleteMaterial( CMaterial *pMtl );

	void OnEditorNotifyEvent( EEditorNotifyEvent event );

	virtual CBaseLibraryItem* MakeNewItem();
	virtual CBaseLibrary* MakeNewLibrary();
	//! Root node where this library will be saved.
	virtual CString GetRootNodeName();
	//! Path to libraries in this manager.
	virtual CString GetLibsPath();
	virtual void ReportDuplicateItem( CBaseLibraryItem *pItem,CBaseLibraryItem *pOldItem );

	void RegisterCommands( CRegistrationContext &regCtx );
	void Command_Create();
	void Command_CreateMulti();
	void Command_Delete();
	void Command_AssignToSelection();
	void Command_ResetSelection();
	void Command_SelectAssignedObjects();
	void Command_SelectFromObject();

	// For material syncing with 3dsMax.
	void PickPreviewMaterial(HWND hWndCaller);
	void InitMatSender();

protected:
	CString m_libsPath;
	
	// Currently selected material, in material browser.
	_smart_ptr<CMaterial> m_pCurrentMaterial;
	// IMaterial is needed to not let 3Dengine release selected IMaterial
	_smart_ptr<IMaterial> m_pCurrentEngineMaterial;

	bool m_bMaterialsLoaded;
	I3DEngine *m_p3DEngine;

	class CMaterialSender* m_MatSender;
};

#endif // __materialmanager_h__
