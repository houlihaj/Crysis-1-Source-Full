////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MusicManager.h
//  Version:     v1.00
//  Created:     22/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __musicmanager_h__
#define __musicmanager_h__
#pragma once

#include "BaseLibraryManager.h"
#include <IMusicSystem.h>

class CMusicThemeLibrary;
class CMusicThemeLibItem;

// Data-struct (which needs to be passed to SetData())
struct SMusicData
{
	TPatternDefVec vecPatternDef;
	TThemeMap mapThemes;
};

/** Manages all entity prototypes and material libraries.
*/
class CRYEDIT_API CMusicManager : public CBaseLibraryManager
{
public:
	//! Notification callback.
	CMusicManager();
	~CMusicManager();

	// Clear all prototypes and material libraries.
	void ClearAll();

	// Propogate changes in music manager to the engine music system.
	void UpdateMusicSystemData();
	// Update only this 1 pattern in music system.
	void UpdatePattern( struct SPatternDef *pPatternDef );

	//! Serialize property manager.
	virtual void Serialize( XmlNodeRef &node,bool bLoading );

	//! Export property manager to game.
	void Export( XmlNodeRef &node );

	//! Return Dynamic Music data definition.
	SMusicData* GetMusicData() { return m_pMusicData; }

protected:
	void ReleaseMusicData();
	virtual CBaseLibraryItem* MakeNewItem();
	virtual CBaseLibrary* MakeNewLibrary();
	//! Root node where this library will be saved.
	virtual CString GetRootNodeName();
	//! Path to libraries in this manager.
	virtual CString GetLibsPath();

	CString m_libsPath;

	// Stores all music data.
	SMusicData *m_pMusicData;
};

#endif // __musicmanager_h__
