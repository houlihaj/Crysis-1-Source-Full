////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   PrefabObject.cpp
//  Version:     v1.00
//  Created:     13/11/2003 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PrefabObject.h"

#include "ObjectManager.h"
#include "Prefabs\PrefabManager.h"
#include "Prefabs\PrefabItem.h"
#include "PrefabPanel.h"

#include "..\Viewport.h"
#include "..\DisplaySettings.h"
#include "Settings.h"

//////////////////////////////////////////////////////////////////////////
// CPrefabObject implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CPrefabObject,CBaseObject)

namespace
{
	int s_rollupId = 0;
	CPrefabPanel *s_panel = 0;
}

//////////////////////////////////////////////////////////////////////////
CPrefabObject::CPrefabObject()
{
	SetColor( RGB(255,220,0) ); // Yellowish
	ZeroStruct(m_prefabGUID);

	m_bbox.min = m_bbox.max = Vec3(0,0,0);
	m_bBBoxValid = false;
	UseMaterialLayersMask(true);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::Done()
{
	GetIEditor()->SuspendUndo();
	DeleteAllPrefabObjects();
	GetIEditor()->ResumeUndo();
	CBaseObject::Done();

	ZeroStruct(m_prefabGUID);
	m_prefabName = "";
	m_pPrefabItem = 0;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	bool res = CBaseObject::Init( ie,prev,file );
	if (prev)
	{
		// Cloning.
		SetPrefab( ((CPrefabObject*)prev)->m_pPrefabItem,false );
	}
	else if (!file.IsEmpty())
	{
		SetPrefab( GuidUtil::FromString(file),false );
	}
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::BeginEditParams( IEditor *ie,int flags )
{
	CBaseObject::BeginEditParams( ie,flags );

	if (!s_panel)
	{
		s_panel = new CPrefabPanel;
		s_rollupId = ie->AddRollUpPage( ROLLUP_OBJECTS,"Prefab Parameters",s_panel );
	}
	if (s_panel)
		s_panel->SetObject( this );
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::EndEditParams( IEditor *ie )
{
	if (s_panel)
	{
		ie->RemoveRollUpPage( ROLLUP_OBJECTS,s_rollupId );
	}
	s_rollupId = 0;
	s_panel = 0;

	CBaseObject::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::Display( DisplayContext &dc )
{
	DrawDefault(dc,GetColor());

	dc.PushMatrix( GetWorldTM() );

	bool bSelected = IsSelected();
	if (bSelected)
	{
		dc.SetSelectedColor();
		dc.DrawWireBox( m_bbox.min,m_bbox.max );
		
		dc.DepthWriteOff();
		dc.SetSelectedColor( 0.2f );
		dc.DrawSolidBox( m_bbox.min,m_bbox.max );
		dc.DepthWriteOn();
	}
	else
	{
		if (gSettings.viewports.bAlwaysDrawPrefabBox)
		{
			if (IsFrozen())
				dc.SetFreezeColor();
			else
				dc.SetColor( GetColor(),0.2f );
			
			dc.DepthWriteOff();;
			dc.DrawSolidBox( m_bbox.min,m_bbox.max );
			dc.DepthWriteOn();

			if (IsFrozen())
				dc.SetFreezeColor();
			else
				dc.SetColor( GetColor() );
			dc.DrawWireBox( m_bbox.min,m_bbox.max );
		}
	}
	dc.PopMatrix();

	if (bSelected || gSettings.viewports.bAlwaysDrawPrefabInternalObjects || IsHighlighted() || IsOpen())
	{
		if (HaveChilds())
		{
			int numObjects = GetChildCount();
			for (int i = 0; i < numObjects; i++)
			{
				RecursivelyDisplayObject( GetChild(i),dc );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::RecursivelyDisplayObject( CBaseObject *obj,DisplayContext &dc )
{
	if (!obj->CheckFlags(OBJFLAG_PREFAB))
		return;

	BBox bbox;
	obj->GetBoundBox( bbox );
	if (dc.flags & DISPLAY_2D)
	{
		if (dc.box.IsIntersectBox( bbox ))
		{
			obj->Display( dc );
		}
	}
	else
	{
		if (dc.camera && dc.camera->IsAABBVisible_F( AABB(bbox.min,bbox.max) ))
		{
			obj->Display( dc );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	int numObjects = obj->GetChildCount();
	for (int i = 0; i < numObjects; i++)
	{
		RecursivelyDisplayObject( obj->GetChild(i),dc );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::Serialize( CObjectArchive &ar )
{
	CBaseObject::Serialize( ar );

	if (ar.bLoading)
	{
		// Loading.
		CString prefabName = m_prefabName;
		GUID prefabGUID = m_prefabGUID;
		ar.node->getAttr( "PrefabName",prefabName );
		ar.node->getAttr( "PrefabGUID",prefabGUID );
		SetPrefab( prefabGUID,false );
		uint32 nLayersMask = GetMaterialLayersMask();
		if(nLayersMask)
			SetMaterialLayersMask( nLayersMask );
	}
	else
	{
		ar.node->setAttr( "PrefabGUID",m_prefabGUID );
		ar.node->setAttr( "PrefabName",m_prefabName );
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CPrefabObject::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	// Do not export.
	return 0;
}

//////////////////////////////////////////////////////////////////////////
inline void RecursivelySendEventToPrefabChilds( CBaseObject *obj,ObjectEvent event )
{
	for (int i = 0; i < obj->GetChildCount(); i++)
	{
		CBaseObject *c = obj->GetChild(i);
		if (c->CheckFlags(OBJFLAG_PREFAB))
		{
			c->OnEvent(event);
			if (c->GetChildCount() > 0)
			{
				RecursivelySendEventToPrefabChilds( c,event );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::OnEvent( ObjectEvent event )
{
	switch (event)
	{
	case EVENT_PREFAB_REMAKE:
		if (m_pPrefabItem && GetIEditor()->GetPrefabManager()->GetSelectedItem() == m_pPrefabItem)
			SetPrefab( m_pPrefabItem,true );
		break;
	};
	// Send event to all prefab childs.
	RecursivelySendEventToPrefabChilds( this,event );
	CBaseObject::OnEvent( event );
}

//////////////////////////////////////////////////////////////////////////
inline void RecursivelyGetAllPrefabChilds( CBaseObject *obj,std::vector<CBaseObjectPtr> &childs )
{
	for (int i = 0; i < obj->GetChildCount(); i++)
	{
		CBaseObject *c = obj->GetChild(i);
		if (c->CheckFlags(OBJFLAG_PREFAB))
		{
			childs.push_back(c);
			RecursivelyGetAllPrefabChilds( c,childs );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::DeleteAllPrefabObjects()
{
	std::vector<CBaseObjectPtr> childs;
	RecursivelyGetAllPrefabChilds( this,childs );
	for (int i = 0; i < childs.size(); i++)
	{
		GetObjectManager()->DeleteObject(childs[i]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::SetPrefab( REFGUID guid,bool bForceReload )
{
	if (m_prefabGUID == guid && bForceReload == false)
		return;

	m_prefabGUID = guid;

	//m_fullPrototypeName = prototypeName;
	CPrefabManager *pManager = GetIEditor()->GetPrefabManager();
	CPrefabItem *pPrefab = (CPrefabItem*)pManager->FindItem( guid );
	if (pPrefab)
	{
		SetPrefab( pPrefab,bForceReload );
	}
	else
	{
		if (m_prefabName.IsEmpty())
			m_prefabName = "Unknown Prefab";

		CErrorRecord err;
		err.error.Format( "Cannot find Prefab %s with GUID: %s for Object %s",(const char*)m_prefabName,GuidUtil::ToString(guid),(const char*)GetName() );
		err.pObject = this;
		err.severity = CErrorRecord::ESEVERITY_WARNING;
		GetIEditor()->GetErrorReport()->ReportError(err);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::SetPrefab( CPrefabItem *pPrefab,bool bForceReload )
{
	assert( pPrefab );

	if (pPrefab == m_pPrefabItem && !bForceReload)
		return;

	StoreUndo( "Set Prefab" );

	// Delete all child objects.
	DeleteAllPrefabObjects();

	m_pPrefabItem = pPrefab;
	m_prefabGUID = pPrefab->GetGUID();
	m_prefabName = pPrefab->GetFullName();

	// Make objects from this prefab.
	XmlNodeRef objects = pPrefab->GetObjectsNode();
	if (!objects)
	{
		CErrorRecord err;
		err.error.Format( "Prefab %s does not contain objects",(const char*)m_prefabName );
		err.pObject = this;
		err.severity = CErrorRecord::ESEVERITY_WARNING;
		GetIEditor()->GetErrorReport()->ReportError(err);
		return;
	}

	CObjectLayer *pThisLayer = GetLayer();

	//////////////////////////////////////////////////////////////////////////
	// Spawn objects.
	//////////////////////////////////////////////////////////////////////////
	CObjectArchive ar( GetObjectManager(),objects,true );
	ar.EnableProgressBar(false); // No progress bar is shown when loading objects.
	ar.MakeNewIds( true );
	ar.LoadObjects( objects );
	GetObjectManager()->ForceID(GetId().Data1);//force using this ID, incremental 
	ar.ResolveObjects();
	int numObjects = ar.GetLoadedObjectsCount();
	for (int i = 0; i < numObjects; i++)
	{
		CBaseObject *obj = ar.GetLoadedObject(i);
		if (obj)
		{
			// Only attach parent objects from prefab.
			if (!obj->GetParent())
				AttachChild( obj,false );
			RecursivelySetObjectInPrefab( obj );
		}
	}
	GetObjectManager()->ForceID(0);//disable
	InvalidateBBox();
}

//////////////////////////////////////////////////////////////////////////
CPrefabItem* CPrefabObject::GetPrefab() const
{
	return m_pPrefabItem;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::RecursivelySetObjectInPrefab( CBaseObject *object )
{
	object->SetFlags( OBJFLAG_PREFAB );
	object->SetLayer( GetLayer() );
	object->SetGroup(this);

	int numChilds = object->GetChildCount();
	for (int i = 0; i < numChilds; i++)
	{
		RecursivelySetObjectInPrefab( object->GetChild(i) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx )
{
	CBaseObject *pFromParent = pFromObject->GetParent();
	if (pFromParent)
	{
		CBaseObject *pChildParent = ctx.FindClone( pFromParent );
		if (pChildParent)
			pChildParent->AttachChild( this,false );
		else
			pFromParent->AttachChild( this,false );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabObject::HitTest( HitContext &hc )
{
	if (IsOpen())
	{
		return CGroup::HitTest( hc );
	}
	else
	{
		if (CGroup::HitTest( hc ))
		{
			hc.object = this;
			return true;
		}
/*
		Vec3 pnt;

		Vec3 raySrc = hc.raySrc;
		Vec3 rayDir = hc.rayDir;

		Matrix34 invertTM = GetWorldTM();
		invertTM.Invert();
		raySrc = invertTM.TransformPoint( raySrc );
		rayDir = invertTM.TransformVector(rayDir).GetNormalized();

		if (m_bbox.IsIntersectRay( raySrc,rayDir,pnt ))
		{
			// World space distance.
			float dist = hc.raySrc.GetDistance(GetWorldTM().TransformPoint(pnt));
			hc.dist = dist;
			hc.object = this;
			return true;
		}
		*/
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::ExtractAll()
{
	int i;
	// Clone all child objects.
	CSelectionGroup sel;
	for (i = 0; i < GetChildCount(); i++)
	{
		sel.AddObject( GetChild(i) );
	}
	CSelectionGroup newSel;
	sel.Clone( newSel );

	GetIEditor()->ClearSelection();
	for (i = 0; i < newSel.GetCount(); i++)
	{
		CBaseObject *pClone = newSel.GetObject(i);
		pClone->DetachThis(); // Detach from parent.
		GetIEditor()->SelectObject( pClone );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::ExtractObject( CBaseObject *pObj )
{
	int i;
	// Clone all child objects.
	CSelectionGroup sel;
	sel.AddObject( pObj );

	CSelectionGroup newSel;
	sel.Clone( newSel );

	GetIEditor()->ClearSelection();
	for (i = 0; i < newSel.GetCount(); i++)
	{
		CBaseObject *pClone = newSel.GetObject(i);
		pClone->DetachThis(); // Detach from parent.
		GetIEditor()->SelectObject( pClone );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::AddObjectToPrefab( CBaseObject *pObj, bool isMultyAdding )
{
	if (!m_pPrefabItem)
		return;
	if (pObj->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		if(((CPrefabObject*)pObj)->m_pPrefabItem == m_pPrefabItem)
		{
			Warning( "Object has the same prefab item");
			return;
		}
	if (pObj->CheckFlags(OBJFLAG_PREFAB))
	{
		Warning( "Object %s already in prefab",(const char*)pObj->GetName() );
		return;
	}

	//pObj->StoreUndo( "Add Object(s) To Prefab" );
	if(!isMultyAdding)
		StoreUndo( "Add Object To Prefab" );

	if (!pObj->IsChildOf(this))
	{
		AttachChild(pObj);
	}
	pObj->SetFlags(OBJFLAG_PREFAB);
	pObj->SetLayer( GetLayer() );
	pObj->SetGroup(this);
	if(!isMultyAdding)
	{
		m_pPrefabItem->UpdateFromPrefabObject( this );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::RemoveObjectFromPrefab( CBaseObject *pObj )
{
	if (!m_pPrefabItem)
		return;
	if (pObj && pObj->CheckFlags(OBJFLAG_PREFAB) && pObj->IsChildOf(this))
	{
		//StoreUndo( "Remove Object(s) From Prefab" );

		pObj->ClearFlags(OBJFLAG_PREFAB);
		pObj->SetGroup(NULL);
		pObj->DetachThis();
		//m_pPrefabItem->UpdateFromPrefabObject( this );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::UpdatePrefab()
{
	if (m_pPrefabItem)
	{
		m_pPrefabItem->UpdateFromPrefabObject( this );
	}
}

//////////////////////////////////////////////////////////////////////////
static void Prefab_RecursivelyGetBoundBox( CBaseObject *object,BBox &box,const Matrix34 &parentTM )
{
	if (!object->CheckFlags(OBJFLAG_PREFAB))
		return;

	Matrix34 worldTM = parentTM * object->GetLocalTM();
	BBox b;
	object->GetLocalBounds( b );
	b.SetTransformedAABB( worldTM,b );
	box.Add(b.min);
	box.Add(b.max);

	int numChilds = object->GetChildCount();
	if (numChilds > 0)
	{
		for (int i = 0; i < numChilds; i++)
		{
			Prefab_RecursivelyGetBoundBox( object->GetChild(i),box,worldTM );
		}
	}
}

/////////////////////////////////////////////////////////////////////////
void CPrefabObject::CalcBoundBox()
{
	Matrix34 identityTM;
	identityTM.SetIdentity();

	// Calc local bounds box..
	BBox box;
	box.Reset();

	int numChilds = GetChildCount();
	for (int i = 0; i < numChilds; i++)
	{
		if (GetChild(i)->CheckFlags(OBJFLAG_PREFAB))
		{
			Prefab_RecursivelyGetBoundBox( GetChild(i),box,identityTM );
		}
	}

	if (numChilds == 0)
	{
		box.min = Vec3(-1,-1,-1);
		box.max = Vec3(1,1,1);
	}

	m_bbox = box;
	m_bBBoxValid = true;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::AttachChild( CBaseObject* child,bool bKeepPos )
{
	CBaseObject::AttachChild( child,bKeepPos );
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::RemoveChild( CBaseObject *child )
{
	CBaseObject::RemoveChild( child );
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::SetMaterialLayersMask( uint32 nLayersMask )
{
	for (int i = 0; i < GetChildCount(); i++)
		if (GetChild(i)->CheckFlags(OBJFLAG_PREFAB))
			GetChild(i)->SetMaterialLayersMask(nLayersMask);

	CBaseObject::SetMaterialLayersMask(nLayersMask);
}