////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   objectloader.h
//  Version:     v1.00
//  Created:     15/8/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __objectloader_h__
#define __objectloader_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "ErrorReport.h"

/** CObjectLoader used to load Bas Object and resolve ObjectId references while loading.
*/
class CObjectArchive
{
public:
	XmlNodeRef node; //!< Current archive node.
	bool bLoading;
	bool bUndo;

	CObjectArchive( IObjectManager *objMan,XmlNodeRef xmlRoot,bool loading );
	~CObjectArchive();

	//! Resolve callback with only one parameter of CBaseObject.
	typedef Functor1<CBaseObject*> ResolveObjRefFunctor1;
	//! Resolve callback with two parameters one is pointer to CBaseObject and second use data integer.
	typedef Functor2<CBaseObject*,unsigned int> ResolveObjRefFunctor2;

	/** Register Object id.
		@param objectId Original object id from the file.
		@param realObjectId Changed object id.
	*/
	//void RegisterObjectId( int objectId,int realObjectId );

	// Return object ID remapped after loading.
	GUID ResolveID( REFGUID id );
	
	//! Set object resolve callback, it will be called once object with specified Id is loaded.
	void SetResolveCallback( CBaseObject *fromObject,REFGUID objectId,ResolveObjRefFunctor1 &func );
	//! Set object resolve callback, it will be called once object with specified Id is loaded.
	void SetResolveCallback( CBaseObject *fromObject,REFGUID objectId,ResolveObjRefFunctor2 &func,uint userData );
	//! Resolve all object ids and call callbacks on resolved objects.
	void ResolveObjects();

	// Save object to archive.
	void SaveObject( CBaseObject *pObject,bool bSaveInGroupObjects=false,bool bSaveInPrefabObjects=false );
	
	//! Load multiple objects from archive.
	void LoadObjects( XmlNodeRef &rootObjectsNode );

	//! Load one object from archive.
	CBaseObject* LoadObject( XmlNodeRef &objNode,CBaseObject *pPrevObject=NULL );

	//////////////////////////////////////////////////////////////////////////
	int GetLoadedObjectsCount() { return m_loadedObjects.size(); }
	CBaseObject* GetLoadedObject( int nIndex ) { return m_loadedObjects[nIndex].pObject; }

	//! If true new loaded objects will be assigned new GUIDs.
	void MakeNewIds( bool bEnable );

	//! Remap object ids.
	void RemapID( REFGUID oldId,REFGUID newId );

	//! Report error during loading.
	void ReportError( CErrorRecord &err );
	//! Assigner different error report class.
	void SetErrorReport( CErrorReport *errReport );
	//! Display collected error reports.
	void ShowErrors();

	void EnableProgressBar( bool bEnable ) { m_bProgressBarEnabled = bEnable; };

	CPakFile* GetGeometryPak( const char *sFilename );
	CBaseObject* GetCurrentObject();

private:
	struct SLoadedObjectInfo
	{
		int nSortOrder;
		_smart_ptr<CBaseObject> pObject;
		XmlNodeRef xmlNode;
		bool operator <( const SLoadedObjectInfo &oi ) const { return nSortOrder < oi.nSortOrder;	}
	};


	IObjectManager* m_objectManager;
	struct Callback {
		ResolveObjRefFunctor1 func1;
		ResolveObjRefFunctor2 func2;
		uint userData;
		TSmartPtr<CBaseObject> fromObject;
		Callback() { func1 = 0; func2 = 0; userData = 0; };
	};
	typedef std::multimap<GUID,Callback,guid_less_predicate> Callbacks;
	Callbacks m_resolveCallbacks;

	// Set of all saved objects to this archive.
	typedef std::set<_smart_ptr<CBaseObject> > ObjectsSet;
	ObjectsSet m_savedObjects;

	//typedef std::multimap<int,_smart_ptr<CBaseObject> > OrderedObjects;
	//OrderedObjects m_orderedObjects;
	std::vector<SLoadedObjectInfo> m_loadedObjects;

	// Loaded objects IDs, used for remapping of GUIDs.
	std::map<GUID,GUID,guid_less_predicate> m_IdRemap;

	//! If true new loaded objects will be assigned new GUIDs.
	bool m_bMakeNewIds;
	CErrorReport *m_pCurrentErrorReport;
	CPakFile *m_pGeometryPak;
	CBaseObject *m_pCurrentObject;

	bool m_bNeedResolveObjects;
	bool m_bProgressBarEnabled;
};

#endif // __objectloader_h__
