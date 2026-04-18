////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   objectloader.cpp
//  Version:     v1.00
//  Created:     15/8/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ObjectLoader.h"
#include "ObjectManager.h"
#include "Util\PakFile.h"

//////////////////////////////////////////////////////////////////////////
// CObjectArchive Implementation.
//////////////////////////////////////////////////////////////////////////
CObjectArchive::CObjectArchive( IObjectManager *objMan,XmlNodeRef xmlRoot,bool loading )
{
	m_objectManager = objMan;
	bLoading = loading;
	bUndo = false;
	m_bMakeNewIds = false;
	node = xmlRoot;
	m_pCurrentErrorReport = GetIEditor()->GetErrorReport();
	m_pGeometryPak = NULL;
	m_pCurrentObject = NULL;
	m_bNeedResolveObjects = false;
	m_bProgressBarEnabled = true;
}

//////////////////////////////////////////////////////////////////////////
CObjectArchive::~CObjectArchive()
{
	if (m_pGeometryPak)
		delete m_pGeometryPak;
	// Always make sure objects are resolved when loading from archive.
	if (bLoading && m_bNeedResolveObjects)
	{
		ResolveObjects();
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::SetResolveCallback( CBaseObject *fromObject,REFGUID objectId,ResolveObjRefFunctor1 &func )
{
	if (objectId == GUID_NULL)
	{
		func( 0 );
		return;
	}

	CBaseObject *object = m_objectManager->FindObject( objectId );
	if (object && !m_bMakeNewIds)
	{
		// Object is already resolved. immidiatly call callback.
		func( object );
	}
	else
	{
		Callback cb;
		cb.fromObject = fromObject;
		cb.func1 = func;
		m_resolveCallbacks.insert( Callbacks::value_type(objectId,cb) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::SetResolveCallback( CBaseObject *fromObject,REFGUID objectId,ResolveObjRefFunctor2 &func,uint userData )
{
	if (objectId == GUID_NULL)
	{
		func( 0,userData );
		return;
	}

	CBaseObject *object = m_objectManager->FindObject( objectId );
	if (object && !m_bMakeNewIds)
	{
		// Object is already resolved. immidiatly call callback.
		func( object,userData );
	}
	else
	{
		Callback cb;
		cb.fromObject = fromObject;
		cb.func2 = func;
		cb.userData = userData;
		m_resolveCallbacks.insert( Callbacks::value_type(objectId,cb) );
	}
}

//////////////////////////////////////////////////////////////////////////
GUID CObjectArchive::ResolveID( REFGUID id )
{
	return stl::find_in_map( m_IdRemap,id,id );
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::ResolveObjects()
{
	int i;

	if (!bLoading)
		return;

	{
		CWaitProgress wait( "Loading Objects",false );
		if (m_bProgressBarEnabled)
			wait.Start();

		GetIEditor()->SuspendUndo();
		//////////////////////////////////////////////////////////////////////////
		// Serialize All Objects from XML.
		//////////////////////////////////////////////////////////////////////////
		int numObj = m_loadedObjects.size();
		for (i = 0; i < numObj; i++)
		{
			SLoadedObjectInfo &obj = m_loadedObjects[i];
			m_pCurrentErrorReport->SetCurrentValidatorObject( obj.pObject );
			node = obj.xmlNode;

			if (m_bProgressBarEnabled)
				wait.Step( (i*100)/numObj );
			//wait.SetText( obj.pObject->GetName() );

			obj.pObject->Serialize( *this );

			// Objects can be added to the list here (from Groups).
			numObj = m_loadedObjects.size();
		}
		m_pCurrentErrorReport->SetCurrentValidatorObject( NULL );
		//////////////////////////////////////////////////////////////////////////
		GetIEditor()->ResumeUndo();
	}

	// Sort objects by sort order.
	std::sort( m_loadedObjects.begin(),m_loadedObjects.end() );

	// Then rearrange to parent-first, if same sort order.
	for (i = 0; i < m_loadedObjects.size(); i++)
	{
		if (m_loadedObjects[i].pObject->GetParent())
		{
			// Find later in array.
			for (int j = i+1; j < m_loadedObjects.size() && m_loadedObjects[j].nSortOrder == m_loadedObjects[i].nSortOrder; j++)
			{
				if (m_loadedObjects[j].pObject == m_loadedObjects[i].pObject->GetParent())
				{
					// Swap the objects.
					std::swap(m_loadedObjects[i], m_loadedObjects[j]);
					i--;
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Resolve objects GUIDs
	//////////////////////////////////////////////////////////////////////////
	for (Callbacks::iterator it = m_resolveCallbacks.begin(); it != m_resolveCallbacks.end(); it++)
	{
		Callback &cb = it->second;
		GUID objectId = it->first;
		if (!m_IdRemap.empty())
		{
			objectId = stl::find_in_map( m_IdRemap,objectId,objectId );
		}
		CBaseObject *object = m_objectManager->FindObject(objectId);
		if (!object)
		{
			CString from;
			if (cb.fromObject)
			{
				from = cb.fromObject->GetName();
			}
			// Cannot resolve this object id.
			CErrorRecord err;
			err.error.Format( _T("Unresolved ObjectID: %s, Referenced from Object %s"),GuidUtil::ToString(objectId),
													(const char*)from );
			err.severity = CErrorRecord::ESEVERITY_ERROR;
			err.flags = CErrorRecord::FLAG_OBJECTID;
			err.pObject = cb.fromObject;
			GetIEditor()->GetErrorReport()->ReportError(err);

			//Warning( "Cannot resolve ObjectID: %s\r\nObject with this ID was not present in loaded file.\r\nFor instance Trigger referencing another object which is not loaded in Level.",GuidUtil::ToString(objectId) );
			continue;
		}
		m_pCurrentErrorReport->SetCurrentValidatorObject( object );
		// Call callback with this object.
		if (cb.func1)
			(cb.func1)( object );
		if (cb.func2)
			(cb.func2)( object,cb.userData );
	}
	m_resolveCallbacks.clear();
	//////////////////////////////////////////////////////////////////////////

	{
		CWaitProgress wait( "Creating Objects",false );
		if (m_bProgressBarEnabled)
			wait.Start();
		//////////////////////////////////////////////////////////////////////////
		// Serialize All Objects from XML.
		//////////////////////////////////////////////////////////////////////////
		int numObj = m_loadedObjects.size();
		for (i = 0; i < numObj; i++)
		{
			SLoadedObjectInfo &obj = m_loadedObjects[i];
			m_pCurrentErrorReport->SetCurrentValidatorObject( obj.pObject );

			if (m_bProgressBarEnabled)
				wait.Step( (i*100)/numObj );
			//wait.SetText( obj.pObject->GetName() );

			obj.pObject->CreateGameObject();
		}
		m_pCurrentErrorReport->SetCurrentValidatorObject( NULL );
		//////////////////////////////////////////////////////////////////////////
	}

	//////////////////////////////////////////////////////////////////////////
	// Call PostLoad on all these objects.
	//////////////////////////////////////////////////////////////////////////
	{
		int numObj = m_loadedObjects.size();
		for (i = 0; i < numObj; i++)
		{
			SLoadedObjectInfo &obj = m_loadedObjects[i];
			m_pCurrentErrorReport->SetCurrentValidatorObject( obj.pObject );
			node = obj.xmlNode;
			obj.pObject->PostLoad( *this );
		}
	}

	m_bNeedResolveObjects = false;
	m_pCurrentErrorReport->SetCurrentValidatorObject( NULL );
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::SaveObject( CBaseObject *pObject,bool bSaveInGroupObjects,bool bSaveInPrefabObjects )
{
	// Not save objects in prefabs.
	if (!bSaveInPrefabObjects && pObject->CheckFlags(OBJFLAG_PREFAB))
		return;
	
	// Not save objects in group.
	if ((!bSaveInGroupObjects && pObject->GetGroup()) && !bSaveInPrefabObjects)
		return;

	if (m_savedObjects.find(pObject) == m_savedObjects.end())
	{
		m_pCurrentObject = pObject;
		m_savedObjects.insert( pObject );
		// If this object was not saved before.
		XmlNodeRef objNode = node->newChild( "Object" );
		XmlNodeRef prevRoot = node;
		node = objNode;
		pObject->Serialize( *this );
		node = prevRoot;
	}
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectArchive::LoadObject( XmlNodeRef &objNode,CBaseObject *pPrevObject )
{
	XmlNodeRef prevNode = node;
	node = objNode;
	CBaseObject *pObject;
	
	pObject = m_objectManager->NewObject( *this,pPrevObject,m_bMakeNewIds );
	if (pObject)
	{
		SLoadedObjectInfo obj;
		obj.nSortOrder = pObject->GetClassDesc()->GameCreationOrder();
		obj.pObject = pObject;
		obj.xmlNode = node;
		m_loadedObjects.push_back( obj );
		m_bNeedResolveObjects = true;
	}
	node = prevNode;
	return pObject;
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::LoadObjects( XmlNodeRef &rootObjectsNode )
{
	int numObjects = rootObjectsNode->getChildCount();
	for (int i = 0; i < numObjects; i++)
	{
		XmlNodeRef objNode = rootObjectsNode->getChild(i);
		LoadObject( objNode );
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::ReportError( CErrorRecord &err )
{
	if (m_pCurrentErrorReport)
		m_pCurrentErrorReport->ReportError( err );
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::SetErrorReport( CErrorReport *errReport )
{
	if (errReport)
		m_pCurrentErrorReport = errReport;
	else
		m_pCurrentErrorReport = GetIEditor()->GetErrorReport();
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::ShowErrors()
{
	GetIEditor()->GetErrorReport()->Display();
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::MakeNewIds( bool bEnable )
{
	m_bMakeNewIds = bEnable;
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::RemapID( REFGUID oldId,REFGUID newId )
{
	m_IdRemap[oldId] = newId;
}

//////////////////////////////////////////////////////////////////////////
CPakFile* CObjectArchive::GetGeometryPak( const char *sFilename )
{
	if (m_pGeometryPak)
		return m_pGeometryPak;
	m_pGeometryPak = new CPakFile;
	m_pGeometryPak->Open( sFilename );
	return m_pGeometryPak;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectArchive::GetCurrentObject()
{
	return m_pCurrentObject;
}
