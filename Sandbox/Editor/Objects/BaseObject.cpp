////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   BaseObject.cpp
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: CBaseObject implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BaseObject.h"
#include "ObjectManager.h"
#include "Group.h"
#include "..\Viewport.h"
#include "..\ObjectPanel.h"
#include "..\PropertiesPanel.h"
#include "..\DisplaySettings.h"
#include "..\Settings.h"

#include "Material\Material.h"
#include "Material\MaterialManager.h"

#include "GizmoManager.h"
#include "UIPanels/TabPanel.h"

#include <IMovieSystem.h>
#include <ITimer.h>

#define LINK_COLOR_PARENT RGB(0,255,255)
#define LINK_COLOR_CHILD RGB(0,0,255)

#define INVALID_POSITION_EPSILON 100000

extern CObjectManager* g_pObjectManager;

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CBaseObject,CObject);

class CMaterialLayersMaskPanelUI;

namespace {
	int s_rollupIndex = 0;
	CObjectPanel* s_objectPanel = 0;

	int s_tabPanelIndex = 0;
	CTabPanel* s_tabPanel = 0;
	
	int s_varsIndex = 0;
	CPropertiesPanel* s_varsPanel = 0;

	CMaterialLayersMaskPanelUI* s_pMaterialLayersMaskPanel = NULL;
	int s_MaterialLayersMaskPanelId = 0;

	struct AutoReleaseAll
	{
		AutoReleaseAll() {};
		~AutoReleaseAll()
		{
			delete s_objectPanel;
			delete s_varsPanel;
		}
	} s_autoReleaseAll;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! Undo object for CBaseObject.
class CUndoBaseObject : public IUndoObject
{
public:
	CUndoBaseObject( CBaseObject *obj,const char *undoDescription )
	{
		// Stores the current state of this object.
		assert( obj != 0 );
		m_undoDescription = undoDescription;
		m_object = obj;
		m_redo = 0;
		m_undo = CreateXmlNode("Undo");
		CObjectArchive ar(GetIEditor()->GetObjectManager(),m_undo,false );
		ar.bUndo = true;
		obj->Serialize( ar );
	}
protected:
	virtual int GetSize()
	{
		/*
		CString m_undoStr,m_redoStr;
		if (m_undo)
			m_undoStr = m_undo->getXML();
		if (m_redo)
			m_redoStr = m_redo->getXML();
		return sizeof(CUndoBaseObject) + m_undoStr.GetLength() + m_redoStr.GetLength() + m_undoDescription.GetLength();
		*/
		return sizeof(CUndoBaseObject);
	}
	virtual const char* GetDescription() { return m_undoDescription; };

	virtual void Undo( bool bUndo )
	{
		GetIEditor()->SuspendUndo();
		if (bUndo)
		{
			m_redo = CreateXmlNode("Redo");
			// Save current object state.
			CObjectArchive ar(GetIEditor()->GetObjectManager(),m_redo,false );
			ar.bUndo = true;
			m_object->Serialize( ar );
		}
		// Undo object state.
		CObjectArchive ar(GetIEditor()->GetObjectManager(),m_undo,true );
		ar.bUndo = true;
		m_object->Serialize( ar );
		GetIEditor()->ResumeUndo();
	}
	virtual void Redo()
	{
		GetIEditor()->SuspendUndo();
		CObjectArchive ar(GetIEditor()->GetObjectManager(),m_redo,true );
		ar.bUndo = true;
		m_object->Serialize( ar );
		GetIEditor()->ResumeUndo();
	}

private:
	CString m_undoDescription;
	CBaseObjectPtr m_object;
	XmlNodeRef m_undo;
	XmlNodeRef m_redo;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! Undo object for CBaseObject.
class CUndoBaseObjectPos : public IUndoObject
{
public:
	CUndoBaseObjectPos( CBaseObject *obj,const char *undoDescription )
	{
		// Stores the current state of this object.
		assert( obj != 0 );
		m_undoDescription = undoDescription;
		m_object = obj;
		
		ZeroStruct(m_redo);
		m_undo.pos = obj->GetPos();
		m_undo.rotate = obj->GetRotation();
		m_undo.scale = obj->GetScale();
		m_undo.color = obj->GetColor();
		m_undo.area = obj->GetArea();
		m_undo.minSpec = obj->GetMinSpec();
	}
protected:
	virtual int GetSize() { return sizeof(*this); }
	virtual const char* GetDescription() { return m_undoDescription; };

	virtual void Undo( bool bUndo )
	{
		if (bUndo)
		{
			m_redo.pos = m_object->GetPos();
			m_redo.rotate = m_object->GetRotation();
			m_redo.scale = m_object->GetScale();
			m_redo.color = m_object->GetColor();
			m_redo.area = m_object->GetArea();
			m_redo.minSpec = m_object->GetMinSpec();
		}
		if (bUndo)
			m_object->SetLocalTM(m_undo.pos,m_undo.rotate,m_undo.scale,TM_UNDO);
		else
			m_object->SetLocalTM(m_undo.pos,m_undo.rotate,m_undo.scale,TM_RESTORE_UNDO);
		m_object->SetColor(m_undo.color);
		m_object->SetArea(m_undo.area);
		m_object->SetMinSpec(m_undo.minSpec);
	}
	virtual void Redo()
	{
		m_object->SetLocalTM(m_redo.pos,m_redo.rotate,m_redo.scale,TM_UNDO);
		m_object->SetColor(m_redo.color);
		m_object->SetArea(m_redo.area);
		m_object->SetMinSpec(m_redo.minSpec);
	}
private:
	CBaseObjectPtr m_object;
	CString m_undoDescription;
	struct StateStruct { 
		Vec3 pos;
		Quat rotate;
		Vec3 scale; 
		COLORREF color; 
		float area; 
		int minSpec;
	};
	StateStruct m_undo;
	StateStruct m_redo;
};

//////////////////////////////////////////////////////////////////////////
class CMaterialLayersMaskPanelUI : public CPropertiesPanel
{
private:
	_smart_ptr<CVarBlock> m_pVarBlock;
	CSmartVariable<bool> mv_frozen;
  CSmartVariable<bool> mv_dynamic_frozen;
	CSmartVariable<bool> mv_wet;
	CSmartVariable<bool> mv_cloak;
	//CSmartVariable<bool> mv_burned;
public:
	CMaterialLayersMaskPanelUI()
	{
		m_pVarBlock = new CVarBlock;

		mv_frozen = false;
		m_pVarBlock->AddVariable( mv_frozen,"Frozen" );
    m_pVarBlock->AddVariable( mv_dynamic_frozen,"Dynamic Frozen" );
		m_pVarBlock->AddVariable( mv_wet,"Wet" );
		m_pVarBlock->AddVariable( mv_cloak,"Cloak" );
		//m_pVarBlock->AddVariable( mv_burned,"Burned" );
	}
	void SetObject( CBaseObject *pObject )
	{
		DeleteVars();
		if (pObject)
		{
			uint32 nMtlLayersMask = pObject->GetMaterialLayersMask();
			mv_frozen = nMtlLayersMask & MTL_LAYER_FROZEN;
      mv_dynamic_frozen = nMtlLayersMask & MTL_LAYER_DYNAMICFROZEN;
      mv_wet = nMtlLayersMask & MTL_LAYER_WET;
			mv_cloak = nMtlLayersMask & MTL_LAYER_CLOAK;
		}
		SetVarBlock( m_pVarBlock,functor(*this,&CMaterialLayersMaskPanelUI::OnVarChange) );
	}
	void ModifyFlag( uint32 &nFlags,uint32 flag,CSmartVariable<bool> &var,IVariable *pVar )
	{
		if (var.GetVar() == pVar)
			nFlags = (var) ? (nFlags | flag) : (nFlags & (~flag));
	}
	void OnVarChange( IVariable *pVar )
	{
		CSelectionGroup *selection = GetIEditor()->GetSelection();
		for (int i = 0; i < selection->GetCount(); i++)
		{
			CBaseObject *pObj = selection->GetObject(i);
			uint32 nFlags = pObj->GetMaterialLayersMask();
			
			ModifyFlag( nFlags,MTL_LAYER_FROZEN,mv_frozen,pVar );
      ModifyFlag( nFlags,MTL_LAYER_DYNAMICFROZEN,mv_dynamic_frozen,pVar );
			ModifyFlag( nFlags,MTL_LAYER_WET,mv_wet,pVar );
			ModifyFlag( nFlags,MTL_LAYER_CLOAK,mv_cloak,pVar );

			pObj->SetMaterialLayersMask( nFlags );
			pObj->SetMaterial( pObj->GetMaterial() ); // Force change of material.
		}
	}
};

//////////////////////////////////////////////////////////////////////////
void CObjectCloneContext::AddClone( CBaseObject *pFromObject,CBaseObject *pToObject )
{
	m_objectsMap[pFromObject] = pToObject;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectCloneContext::FindClone( CBaseObject *pFromObject )
{
	CBaseObject *pTarget = stl::find_in_map( m_objectsMap,pFromObject, (CBaseObject*) NULL );
	return pTarget;
}

//////////////////////////////////////////////////////////////////////////
GUID CObjectCloneContext::ResolveClonedID( REFGUID guid )
{
	CBaseObject *pObject = GetIEditor()->GetObjectManager()->FindObject(guid);
	CBaseObject *pClonedTarget = FindClone( pObject );
	if (!pClonedTarget)
		pClonedTarget = pObject; // If target not cloned, link to original target.

	if (pClonedTarget)
		return pClonedTarget->GetId();
	return GUID_NULL;

}

//////////////////////////////////////////////////////////////////////////
// CBaseObject implementation.
//////////////////////////////////////////////////////////////////////////
CBaseObject::CBaseObject()
{
	m_pos(0,0,0);
	m_rotate.SetIdentity();
	m_scale(1,1,1);

	m_color = RGB(255,255,255);
	m_pMaterial = NULL;

	m_group = 0;

	m_flags = 0;
	m_flattenArea = 0;

	m_numRefs = 0;
	m_guid = GUID_NULL;

	m_layer = 0;

	m_parent = 0;

	m_lookat = 0;
	m_lookatSource = 0;

	m_bMatrixInWorldSpace = false;
	m_bMatrixValid = false;
	m_bWorldBoxValid = false;
	m_bUseMaterialLayersMask = false;

	m_nTextureIcon = 0;
	m_nMaterialLayersMask = 0;
	m_nMinSpec = 0;

	m_worldTM.SetIdentity();
	m_worldBounds.min.Set(0,0,0);
	m_worldBounds.max.Set(0,0,0);

	m_nextListener = m_eventListeners.end();
}

//////////////////////////////////////////////////////////////////////////
IObjectManager*	CBaseObject::GetObjectManager() const
{
	return g_pObjectManager;
};


void CBaseObject::SetClassDesc( CObjectClassDesc *classDesc )
{
	m_classDesc = classDesc;
}

//! Initialize Object.
bool CBaseObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	SetFlags( m_flags&(~OBJFLAG_DELETED) );

	if (prev != 0)
	{
		// Same layer.
		SetLayer( prev->GetLayer() );

		SetUniqName( prev->GetName() );
		SetLocalTM( prev->GetPos(),prev->GetRotation(),prev->GetScale() );
		SetArea( prev->GetArea() );
		SetColor( prev->GetColor() );
		m_nMaterialLayersMask = prev->m_nMaterialLayersMask;
		SetMaterial( prev->GetMaterial() );
		SetMinSpec( prev->GetMinSpec() );

		/*
		// Clone important flags.
		if (prev->CheckFlags(OBJFLAG_SHARED))
			SetFlags(OBJFLAG_SHARED);
		else
			CheckFlags(OBJFLAG_SHARED);
			*/

		// Copy all basic variables.
		CopyVariableValues(prev);
	}

	m_nTextureIcon = m_classDesc->GetTextureIconId();
	return true;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject::~CBaseObject()
{
	for (Childs::iterator c = m_childs.begin(); c != m_childs.end(); c++)
	{
		CBaseObject* child = *c;
		child->m_parent = 0;
	}
	m_childs.clear();
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::Done()
{
	DetachAll();

	SetLookAt(0);
	if (m_lookatSource)
	{
		m_lookatSource->SetLookAt(0);
	}
	SetFlags( m_flags|OBJFLAG_DELETED );

	NotifyListeners( CBaseObject::ON_DELETE );
	m_eventListeners.clear();

	if (m_pMaterial)
	{
		m_pMaterial->Release();
		m_pMaterial = NULL;
	}
	
	m_group = 0;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetName( const CString &name )
{
	if (name == m_name)
		return;

	StoreUndo( "Name" );
	
	m_name = name;
	GetObjectManager()->RegisterObjectName( name );
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetUniqName( const CString &name )
{
	if (name == m_name)
		return;
	SetName( GetObjectManager()->GenUniqObjectName(name) );
}

//////////////////////////////////////////////////////////////////////////
const CString& CBaseObject::GetName() const
{
	return m_name;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsSameClass( CBaseObject *obj )
{
	return GetClassDesc() == obj->GetClassDesc();
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::SetPos( const Vec3 &pos,int nWhyFlags )
{
	if (IsVectorsEqual(m_pos,pos))
		return false;

	//////////////////////////////////////////////////////////////////////////
	// Check if position is bad.
	if (fabs(pos.x) > INVALID_POSITION_EPSILON ||
			fabs(pos.y) > INVALID_POSITION_EPSILON ||
			fabs(pos.z) > INVALID_POSITION_EPSILON ||
			!_finite(pos.x) || !_finite(pos.y) || !_finite(pos.z))
	{
		CryWarning( VALIDATOR_MODULE_EDITOR,VALIDATOR_WARNING,"Object %s, SetPos called with invalid position: (%f,%f,%f)",(const char*)GetName(),pos.x,pos.y,pos.z );
		return false;
	}
	//////////////////////////////////////////////////////////////////////////
	StoreUndo( "Position",true );

	m_pos = pos;
	m_height = pos.z - GetIEditor()->GetTerrainElevation(pos.x,pos.y);
	if (m_bMatrixValid && !(nWhyFlags&TM_NOT_INVALIDATE))
		InvalidateTM( nWhyFlags|TM_POS_CHANGED );
	SetModified();

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::SetRotation( const Quat& rotate,int nWhyFlags )
{
	if (m_rotate.w == rotate.w && m_rotate.v.x == rotate.v.x && m_rotate.v.y == rotate.v.y && m_rotate.v.z == rotate.v.z)
		return false;
		
	StoreUndo( "Rotate",true );
	m_rotate = rotate;
	if (m_bMatrixValid && !(nWhyFlags&TM_NOT_INVALIDATE))
		InvalidateTM( nWhyFlags|TM_ROT_CHANGED );
	SetModified();

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::SetScale( const Vec3 &scale,int nWhyFlags )
{
	if (IsVectorsEqual(m_scale,scale))
		return false;
	
	//////////////////////////////////////////////////////////////////////////
	// Check if position is bad.
	if (scale.x < 0.01 || scale.y < 0.01 || scale.z < 0.01)
	{
		CryWarning( VALIDATOR_MODULE_EDITOR,VALIDATOR_WARNING,"Object %s, SetScale called with invalid scale: (%f,%f,%f)",(const char*)GetName(),scale.x,scale.y,scale.z );
		return false;
	}
	//////////////////////////////////////////////////////////////////////////

	StoreUndo( "Scale",true );
	m_scale = scale;
	if (m_bMatrixValid && !(nWhyFlags&TM_NOT_INVALIDATE))
		InvalidateTM( nWhyFlags|TM_SCL_CHANGED );
	SetModified();
	return true;
}


//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetColor( COLORREF color )
{
	if (color == m_color)
		return;

	StoreUndo( "Color",true );

	m_color = color;
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetArea( float area )
{
	if (m_flattenArea == area)
		return;

	StoreUndo( "Area",true );

	m_flattenArea = area;
	SetModified();
};

/*
//////////////////////////////////////////////////////////////////////////
void CBaseObject::GetWorldBounds( BBox &box )
{
	GetLocalBounds( box );
	m_box = box;

	Vec3 v[8];
	v[0] = Vec3(box.min.x,box.min.y,box.min.z);
	v[1] = Vec3(box.min.x,box.min.y,box.max.z);
	v[2] = Vec3(box.min.x,box.max.y,box.min.z);
	v[3] = Vec3(box.min.x,box.max.y,box.max.z);
	v[4] = Vec3(box.max.x,box.min.y,box.min.z);
	v[5] = Vec3(box.max.x,box.min.y,box.max.z);
	v[6] = Vec3(box.max.x,box.max.y,box.min.z);
	v[7] = Vec3(box.max.x,box.max.y,box.max.z);
	
	box.min.Set(FLT_MAX,FLT_MAX,FLT_MAX);
	box.max.Set(FLT_MIN,FLT_MIN,FLT_MIN);
	for (int i = 0; i < 8; i++)
	{
		box.min.x = std::min(box.min.x,v[i].x);
		box.min.y = std::min(box.min.y,v[i].y);
		box.min.z = std::min(box.min.z,v[i].z);

		box.max.x = std::max(box.max.x,v[i].x);
		box.max.y = std::max(box.max.y,v[i].y);
		box.max.z = std::max(box.max.z,v[i].z);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::GetLocalBounds( BBox &box )
{
	box = m_box;
}
*/

//! Get bounding box of object in world coordinate space.
void CBaseObject::GetBoundBox( BBox &box )
{
	if (!m_bWorldBoxValid)
	{
		GetLocalBounds( m_worldBounds );
		m_worldBounds.SetTransformedAABB( m_worldTM,m_worldBounds );
		m_bWorldBoxValid = true;
	}
	box = m_worldBounds;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::GetLocalBounds( BBox &box )
{
	box.min.Set(0,0,0);
	box.max.Set(0,0,0);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::UpdateUIVars()
{
	if (s_varsPanel)
	{
		s_varsPanel->DeleteVars();
		s_varsPanel->AddVars( GetVarBlock() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::BeginEditParams( IEditor *ie,int flags )
{
	if (!s_objectPanel)
	{
		s_objectPanel = new CObjectPanel(AfxGetMainWnd());
		s_objectPanel->Create( CObjectPanel::IDD,AfxGetMainWnd() );
		s_rollupIndex = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,GetTypeName(),s_objectPanel );
		if (s_rollupIndex == 0)
		{
			delete s_objectPanel;
			s_objectPanel = 0;
			return;
		}
	}

	if (GetVarBlock())
	{
		if (!s_varsPanel)
			s_varsPanel = new CPropertiesPanel( AfxGetMainWnd() );
		else
			s_varsPanel->DeleteVars();
		s_varsPanel->AddVars( GetVarBlock() );
		if (!s_varsIndex)
			s_varsIndex = AddUIPage( CString(GetTypeName()) + " Params",s_varsPanel );
	}

	assert( s_objectPanel != 0 );
	if (s_objectPanel)
	{
		CObjectPanel::SParams uip;
		uip.name = m_name;
		uip.color = m_color;
		uip.area = m_flattenArea;
		//uip.flatten = CheckFlags(OBJFLAG_FLATTEN);
		//uip.shared = CheckFlags(OBJFLAG_SHARED);
		uip.layer = m_layer->GetName();
		uip.helperScale = GetHelperScale();
		uip.minSpec = GetMinSpec();
		s_objectPanel->SetParams( this,uip );
	}

	if (m_bUseMaterialLayersMask)
	{
		if (!s_pMaterialLayersMaskPanel)
		{
			s_pMaterialLayersMaskPanel = new CMaterialLayersMaskPanelUI();
			s_MaterialLayersMaskPanelId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,_T("Material Layers"),s_pMaterialLayersMaskPanel,true,-1,false );
		}
		s_pMaterialLayersMaskPanel->SetMultiSelect(false);
		s_pMaterialLayersMaskPanel->SetObject(this);
	}

	SetFlags( OBJFLAG_EDITING );
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::EndEditParams( IEditor *ie )
{
	OnUIUpdate();

	ClearFlags( OBJFLAG_EDITING );

	if (s_rollupIndex != 0)
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,s_rollupIndex );
	s_rollupIndex = 0;
	s_objectPanel = 0;

	if (s_varsIndex != 0)
		RemoveUIPage( s_varsIndex );

	if (s_pMaterialLayersMaskPanel)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,s_MaterialLayersMaskPanelId );
		s_pMaterialLayersMaskPanel = 0;
		s_MaterialLayersMaskPanelId = 0;
	}

	s_varsIndex = 0;
	s_varsPanel = 0;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::BeginEditMultiSelParams( bool bAllOfSameType )
{
	if (!s_objectPanel)
	{
		s_objectPanel = new CObjectPanel(AfxGetMainWnd());
		s_objectPanel->Create( CObjectPanel::IDD,AfxGetMainWnd() );
		s_rollupIndex = AddUIPage( GetTypeName(),s_objectPanel );
		s_objectPanel->SetMultiSelect( true );
	}

	if (bAllOfSameType)
	{
		if (GetVarBlock())
		{
			if (!s_varsPanel)
				s_varsPanel = new CPropertiesPanel( AfxGetMainWnd() );
			else
				s_varsPanel->DeleteVars();

			// Add all selected objects.
			CSelectionGroup *grp = GetIEditor()->GetSelection();
			for (int i = 0; i < grp->GetCount(); i++)
			{
				CVarBlock *vb = grp->GetObject(i)->GetVarBlock();
				if (vb)
					s_varsPanel->AddVars( vb );
			}
			if (!s_varsIndex)
				s_varsIndex = AddUIPage( CString(GetTypeName()) + " Params",s_varsPanel );
		}

		if (m_bUseMaterialLayersMask)
		{
			if (!s_pMaterialLayersMaskPanel)
			{
				s_pMaterialLayersMaskPanel = new CMaterialLayersMaskPanelUI();
				s_pMaterialLayersMaskPanel->SetObject(0);
				s_MaterialLayersMaskPanelId = AddUIPage( _T("Material Layers"),s_pMaterialLayersMaskPanel );
			}
			s_pMaterialLayersMaskPanel->SetMultiSelect(true);
		}

		/*
		if (m_params != 0 && GetPropertiesTemplate() != 0)
		{
			if (!s_paramsPanel)
			{
				s_paramsPanel = new CPropertiesPanel( AfxGetMainWnd() );
				s_paramsPanel->SetMultiSelect(true);
				s_paramsPanel->SetObject( this ); // Have to be before AddRollup.
				s_paramsIndex = AddUIPage( GetTypeName() + " Properties",s_paramsPanel );
			}
			else
			{
				s_paramsPanel->SetMultiSelect(true);
				s_paramsPanel->SetObject( this );
			}
		}
		*/

	}
	else
	{
		// Different typed objects.
		if (s_pMaterialLayersMaskPanel)
		{
			RemoveUIPage( s_MaterialLayersMaskPanelId );
			s_pMaterialLayersMaskPanel = 0;
			s_MaterialLayersMaskPanelId = 0;
		}
	}
	SetFlags( OBJFLAG_EDITING );
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::EndEditMultiSelParams()
{
	ClearFlags( OBJFLAG_EDITING );

	if (s_rollupIndex != 0)
		RemoveUIPage( s_rollupIndex );
	s_rollupIndex = 0;
	s_objectPanel = 0;

	if (s_varsIndex != 0)
		RemoveUIPage( s_varsIndex );
	s_varsIndex = 0;
	s_varsPanel = 0;

	if (s_pMaterialLayersMaskPanel)
	{
		RemoveUIPage( s_MaterialLayersMaskPanelId );
		s_pMaterialLayersMaskPanel = 0;
		s_MaterialLayersMaskPanelId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::OnUIUpdate()
{
	if (s_objectPanel && !s_objectPanel->IsMultiSelect())
	{
		CObjectPanel::SParams uip;
		s_objectPanel->GetParams( uip );

		if (uip.area != m_flattenArea)
			SetArea( uip.area );
		
		/*
		// First flags.
		if (uip.flatten)
			SetFlags(OBJFLAG_FLATTEN);
		else
			ClearFlags(OBJFLAG_FLATTEN);
		*/

		SetHelperScale( uip.helperScale );

		//Timur[9/11/2002]
		/*
		if (uip.shared)
			SetFlags(OBJFLAG_SHARED);
		else
			ClearFlags(OBJFLAG_SHARED);
			*/

		if (uip.name != m_name)
		{
			// This may also change object id.
			GetObjectManager()->ChangeObjectName( this,uip.name );
		}
		if (uip.color != m_color )
			SetColor( uip.color );

		CObjectLayer *pLayer = GetObjectManager()->GetLayersManager()->FindLayerByName(uip.layer);
		if (pLayer != m_layer)
			SetLayer( pLayer );

		if (uip.minSpec != GetMinSpec())
		{
			StoreUndo( "Change MinSpec",true );
			SetMinSpec(uip.minSpec);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::UpdateEditParams()
{
	if (s_objectPanel && s_objectPanel->GetObject() == this)
	{
		// If its current object update its panel.
		CObjectPanel::SParams uip;
		uip.name = m_name;
		uip.color = m_color;
		uip.area = m_flattenArea;
		//uip.flatten = CheckFlags(OBJFLAG_FLATTEN);
		//uip.shared = CheckFlags(OBJFLAG_SHARED);
		uip.layer = m_layer->GetName();
		uip.helperScale = GetHelperScale();
		uip.minSpec = GetMinSpec();
		s_objectPanel->SetParams( this,uip );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetModified()
{
	/* Not needed! causes refresh of UI everytime object is moved.
	if (CheckFlags(OBJFLAG_EDITING) && s_objectPanel != 0)
	{
		CObjectPanel::SParams uip;
		uip.name = m_name;
		uip.color = m_color;
		uip.area = m_flattenArea;
		uip.flatten = CheckFlags(OBJFLAG_FLATTEN);
		uip.shared = CheckFlags(OBJFLAG_SHARED);
		uip.layer = m_layer;
		s_objectPanel->SetParams( uip );
	}
	*/
}

void CBaseObject::DrawDefault( DisplayContext &dc,COLORREF labelColor )
{
	Vec3 wp = GetWorldPos();

	bool bDisplaySelectionHelper = false;
	if (dc.flags & DISPLAY_SELECTION_HELPERS)
	{
		// Check if this object type is masked for selection.
		if ((GetType() & gSettings.objectSelectMask) && !IsFrozen())
			bDisplaySelectionHelper = true;
		else
		{
			// Object helpers should not be displayed when object is not for selection.
			return;
		}
	}

	// Draw 
	if (dc.flags & DISPLAY_LINKS)
	{
		if (GetParent() && !(GetParent()->GetType() == OBJTYPE_GROUP) && !CheckFlags(OBJFLAG_PREFAB))
		{
			// Draw link between parent and child.
			Vec3 parentPos = GetParent()->GetWorldPos();
			dc.DrawLine( parentPos,wp,LINK_COLOR_PARENT,LINK_COLOR_CHILD );
		}
	}

	if (dc.flags & DISPLAY_BBOX)
	{
		BBox box;
		GetBoundBox( box );
		dc.SetColor( Vec3(1,1,1) );
		dc.DrawWireBox( box.min,box.max );
	}

	if (IsHighlighted())
	{
		DrawHighlight( dc );
	}

	if (IsSelected())
	{
		DrawArea( dc );
		// Draw Axis.
		//DrawAxis( dc,Vec3(0,0,0),AXIS_SIZE );
	}

	if (!(dc.flags & DISPLAY_HIDENAMES) || bDisplaySelectionHelper)
	{
		// Check if our group is not closed.
		if (!m_group || m_group->IsOpen())
		{
			BBox box;
			GetBoundBox( box );

			//p.z = box.max.z + 0.2f;
			if ((dc.flags & DISPLAY_2D) && labelColor == RGB(255,255,255))
			{
				labelColor = RGB(0,0,0);
			}
			DrawLabel( dc,wp,labelColor );
		}
	}

	if (bDisplaySelectionHelper)
	{
		dc.SetColor( GetColor() );
		if (IsHighlighted())
			dc.SetColor( dc.GetSelectedColor() );
		uint32 nPrevState = dc.GetState();
		dc.DepthTestOff();
		float r = dc.view->GetScreenScaleFactor(wp) * 0.006f;
		dc.DrawWireBox( wp-Vec3(r,r,r),wp+Vec3(r,r,r) );
		dc.SetState(nPrevState);
	}

	/*

	//////////////////////////////////////////////////////////////////////////
	// Test.
	int nTexId = GetIEditor()->GetIconManager()->GetIconTexture( "Editor/ObjectIcons/Light.tif" );
	if (nTexId != 0)
	{
		dc.renderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA );
		Vec3 wp = GetWorldPos();
		//dc.CullOff();
		//dc.renderer->DrawLabelImage( Vec3(0,0,0),1000,nTexId );
		//dc.renderer->DrawLabelImage( wp,100,nTexId );
		//dc.renderer->Set2DMode( true,500,500 );
		//dc.renderer->DrawImage( 0,0,100,100,nTexId,0,0,1,1,1,1,1,1 );
		//dc.renderer->Set2DMode( false,500,500 );
		//dc.CullOn();
		CRect rc;
		dc.view->GetClientRect(rc);
		CPoint p = dc.view->WorldToView( wp );
		dc.renderer->Set2DMode( true,rc.right,rc.bottom );
		dc.renderer->DrawImage( p.x-16,p.y+16,32,-32,nTexId,0,0,1,1,1,1,1,1 );
		dc.renderer->Set2DMode( false,rc.right,rc.bottom );
	}
	*/

	if (m_nTextureIcon && gSettings.viewports.bShowIcons)
	{
		if (IsHighlighted())
			dc.SetColor( RGB(255,120,0),0.8f );
		else if (IsSelected())
			dc.SetSelectedColor();
		else if (IsFrozen())
			dc.SetFreezeColor();
		else
			dc.SetColor( RGB(255,255,255),1 );

		int nIconFlags = 0;
		if (CheckFlags(OBJFLAG_SHOW_ICONONTOP))
		{
			BBox box;
			GetBoundBox(box);
			wp.z = box.max.z;
			nIconFlags = DisplayContext::TEXICON_ALIGN_BOTTOM;
		}
		dc.DrawTextureLabel( wp,OBJECT_TEXTURE_ICON_SIZEX,OBJECT_TEXTURE_ICON_SIZEY,m_nTextureIcon,nIconFlags );
	}
}

void CBaseObject::DrawLabel( DisplayContext &dc,const Vec3 &pos,COLORREF labelColor )
{
	float camDist = dc.camera->GetPosition().GetDistance(pos);
	float maxDist = dc.settings->GetLabelsDistance();
	if (camDist < dc.settings->GetLabelsDistance() || (dc.flags & DISPLAY_SELECTION_HELPERS))
	{
		float range = maxDist / 2.0f;
		Vec3 c = Rgb2Vec(labelColor);
		if (IsSelected())
			c = Rgb2Vec(dc.GetSelectedColor());

		float col[4] = { c.x,c.y,c.z,1 };
		if (dc.flags & DISPLAY_SELECTION_HELPERS)
		{
			if (IsHighlighted())
				c = Rgb2Vec(dc.GetSelectedColor());
			col[0] = c.x; col[1] = c.y; col[2] = c.z;
		}
		else if (camDist > range)
		{
			col[3] = col[3] * (1.0f - (camDist - range)/range);
		}

		dc.SetColor( col[0],col[1],col[2],col[3] );
		//dc.renderer->DrawLabelEx( GetPos()+Vec3(0,0,1),1.0f,col,true,true,"%s",(const char*)GetName() );
		//dc.renderer->DrawLabelEx( pos,1.0f,col,true,true,"%s",(const char*)GetName() );
		dc.DrawTextLabel( pos,1,GetName() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DrawHighlight( DisplayContext &dc )
{
	if (!m_nTextureIcon)
	{
		//static int sizeRunner = 0;
		BBox box;
		GetLocalBounds( box );
		dc.SetColor( RGB(255,120,0),0.8f );
		dc.PushMatrix( GetWorldTM() );
		dc.SetLineWidth(3);
		dc.DrawWireBox( box.min,box.max );
		dc.SetLineWidth(0);
		dc.PopMatrix();
	}
}

//////////////////////////////////////////////////////////////////////////
void	CBaseObject::DrawAxis( DisplayContext &dc,const Vec3 &pos,float size )
{
	/*
	dc.renderer->EnableDepthTest(false);
	Vec3 x(size,0,0);
	Vec3 y(0,size,0);
	Vec3 z(0,0,size);

	bool bWorldSpace = false;
	if (dc.flags & DISPLAY_WORLDSPACEAXIS)
		bWorldSpace = true;
	
	Matrix tm = GetWorldTM();
	Vec3 org = tm.TransformPoint( pos );
	
	if (!bWorldSpace)
	{
		tm.NoScale();
		x = tm.TransformVector(x);
		y = tm.TransformVector(y);
		z = tm.TransformVector(z);
	}

	float fScreenScale = dc.view->GetScreenScaleFactor(org);
	x = x * fScreenScale;
	y = y * fScreenScale;
	z = z * fScreenScale;
	
	float col[4] = { 1,1,1,1 };
	float hcol[4] = { 1,0,0,1 };
	dc.renderer->DrawLabelEx( org+x,1.2f,col,true,true,"X" );
	dc.renderer->DrawLabelEx( org+y,1.2f,col,true,true,"Y" );
	dc.renderer->DrawLabelEx( org+z,1.2f,col,true,true,"Z" );

	Vec3 colX(1,0,0),colY(0,1,0),colZ(0,0,1);
	if (s_highlightAxis)
	{
		float col[4] = { 1,0,0,1 };
		if (s_highlightAxis == 1)
		{
			colX.Set(1,1,0);
			dc.renderer->DrawLabelEx( org+x,1.2f,col,true,true,"X" );
		}
		if (s_highlightAxis == 2)
		{
			colY.Set(1,1,0);
			dc.renderer->DrawLabelEx( org+y,1.2f,col,true,true,"Y" );
		}
		if (s_highlightAxis == 3)
		{
			colZ.Set(1,1,0);
			dc.renderer->DrawLabelEx( org+z,1.2f,col,true,true,"Z" );
		}
	}

	x = x * 0.8f;
	y = y * 0.8f;
	z = z * 0.8f;
	float fArrowScale = fScreenScale * 0.07f;
	dc.SetColor( colX );
	dc.DrawArrow( org,org+x,fArrowScale );
	dc.SetColor( colY );
	dc.DrawArrow( org,org+y,fArrowScale );
	dc.SetColor( colZ );
	dc.DrawArrow( org,org+z,fArrowScale );

	//dc.DrawLine( org,org+x,colX,colX );
	//dc.DrawLine( org,org+y,colY,colY );
	//dc.DrawLine( org,org+z,colZ,colZ );

	dc.renderer->EnableDepthTest(true);
	///dc.SetColor( 0,1,1,1 );
	//dc.DrawLine( p,p+dc.view->m_constructionPlane.m_normal*10.0f );
	*/
}

void CBaseObject::DrawArea( DisplayContext &dc )
{
	float area = m_flattenArea;
	if (area > 0)
	{
//		dc.renderer->SetMaterialColor( 0,0,0,1 );
		dc.SetColor(RGB(5,5,255), 1.f);	// make it different color from the AI sight radius
		Vec3 wp = GetWorldPos();
		float z = GetIEditor()->GetTerrainElevation( wp.x,wp.y );
		if (fabs(wp.z-z) < 5)
			dc.DrawTerrainCircle( wp,area,0.2f );
		else
			dc.DrawCircle( wp,area );

		/*
		// Draw area radius.
		dc.renderer->SetPolygonMode(R_WIREFRAME_MODE);
		dc.renderer->SetMaterialColor( 0,1,1,0.2f );
		//dc.renderer->DrawBall( areaPos,area );
		dc.renderer->SetPolygonMode(R_SOLID_MODE);
		*/
	}
}
/*
void CBaseObject::GetMatrix( Matrix &tm,bool noScale )
{
	tm.Identity();
	tm.RotateMatrix( GetAngles() );

	if (!noScale)
	{
		Vec3 s = GetScale();
		if (s.x != 1 && s.y != 1 && s.z != 1)
		{
			Matrix tms;
			tms.Identity();
			tms[0][0] = s.x; tms[1][0] = 0;   tms[2][0] = 0;
			tms[0][1] = 0;   tms[1][1] = s.y; tms[2][1] = 0;
			tms[0][2] = 0;   tms[1][2] = 0;   tms[2][2] = s.z;
			tm = tm*tms;
		}
	}
	Vec3 p = GetPos();
	tm[3][0] = p.x;
	tm[3][1] = p.y;
	tm[3][2] = p.z;
}
*/

//////////////////////////////////////////////////////////////////////////
int CBaseObject::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown)
	{
		Vec3 pos;
		if (GetIEditor()->GetAxisConstrains() != AXIS_TERRAIN)
		{
			pos = view->MapViewToCP(point);
		}
		else
		{
			// Snap to terrain.
			bool hitTerrain;
			pos = view->ViewToWorld( point,&hitTerrain );
			if (hitTerrain)
			{
				pos.z = GetIEditor()->GetTerrainElevation(pos.x,pos.y) + 1.0f;
			}
			pos = view->SnapToGrid(pos);
		}

		SetPos( pos );
		if (event == eMouseLDown)
			return MOUSECREATE_OK;
		return MOUSECREATE_CONTINUE;
	}
	return MOUSECREATE_CONTINUE;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::OnEvent( ObjectEvent event )
{
	switch (event)
	{
	case EVENT_KEEP_HEIGHT:
		{
			float h = m_height;
			float newz = GetIEditor()->GetTerrainElevation( m_pos.x,m_pos.y ) + m_height;
			SetPos( Vec3(m_pos.x,m_pos.y,newz) );
			m_height = h;
		}
		break;
	case EVENT_CONFIG_SPEC_CHANGE:
		UpdateVisibility( !IsHidden() );
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetShared( bool bShared )
{
	//Timur[9/11/2002]
	/*
	if (!GetGroup())
	{
		if (bShared)
			SetFlags(OBJFLAG_SHARED);
		else
			ClearFlags(OBJFLAG_SHARED);
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetHidden( bool bHidden )
{
	if (CheckFlags(OBJFLAG_HIDDEN) != bHidden)
	{
		StoreUndo( "Hide Object" );
		if (bHidden)
			SetFlags(OBJFLAG_HIDDEN);
		else
			ClearFlags(OBJFLAG_HIDDEN);

		GetObjectManager()->InvalidateVisibleList();
		UpdateVisibility( !IsHidden() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetFrozen( bool bFrozen )
{
	if (CheckFlags(OBJFLAG_FROZEN) != bFrozen)
	{
		StoreUndo( "Freeze Object" );
		if (bFrozen)
			SetFlags(OBJFLAG_FROZEN);
		else
			ClearFlags(OBJFLAG_FROZEN);
	}
	if (bFrozen && IsSelected())
	{
		// If frozen must be unselected.
		SetSelected(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetHighlight( bool bHighlight )
{
	if (bHighlight)
    SetFlags(OBJFLAG_HIGHLIGHT);
	else
		ClearFlags(OBJFLAG_HIGHLIGHT);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetSelected( bool bSelect )
{
	if (bSelect)
	{
		SetFlags(OBJFLAG_SELECTED);
		NotifyListeners( ON_SELECT );

		//CLogFile::FormatLine( "Selected: %s, ID=%u",(const char*)m_name,m_id );
	}
	else
	{
		ClearFlags(OBJFLAG_SELECTED);
		NotifyListeners( ON_UNSELECT );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsHiddenBySpec() const
{
	if (!gSettings.bApplyConfigSpecInEditor)
		return false;

	// Check against min spec.
	if (m_nMinSpec == CONFIG_DETAIL_SPEC && gSettings.editorConfigSpec == CONFIG_LOW_SPEC)
		return true;

	return (m_nMinSpec != 0 && gSettings.editorConfigSpec != 0 && m_nMinSpec > gSettings.editorConfigSpec);
}

//////////////////////////////////////////////////////////////////////////
//! Returns true if object hidden.
bool CBaseObject::IsHidden() const
{
	return	(CheckFlags(OBJFLAG_HIDDEN)) ||
					(!m_layer->IsVisible()) ||
					(gSettings.objectHideMask & GetType()) || IsHiddenBySpec();
}
	
//////////////////////////////////////////////////////////////////////////
//! Returns true if object frozen.
bool CBaseObject::IsFrozen() const
{
	return CheckFlags(OBJFLAG_FROZEN) || m_layer->IsFrozen();
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsSelectable() const
{
	// Not selectable if hidden.
	if (IsHidden())
		return false;

	// Not selectable if frozen.
	if (IsFrozen())
		return false;

	// Not selectable if in closed group.
	CGroup* group = GetGroup();
	if (group)
	{
		if (!group->IsOpen())
			return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::Serialize( CObjectArchive &ar )
{
	XmlNodeRef xmlNode = ar.node;

	if (ar.bLoading)
	{
		// Loading.

		int flags = 0;
		int oldFlags = m_flags;
		///xmlNode->getAttr( "Id",m_id );

		CObjectLayer *pLayer = 0;
		// Find layer name.
		CString layerName;
		if (xmlNode->getAttr( "Layer",layerName ))
		{
			pLayer = GetObjectManager()->GetLayersManager()->FindLayerByName(layerName);
		}

		CString name = m_name;
		CString mtlName;
		
		Vec3 pos = m_pos;
		Vec3 scale=m_scale;
		Quat quat = m_rotate;
		Ang3 angles(0,0,0);
		uint32 nMinSpec = m_nMinSpec;

		COLORREF color = m_color;
		float flattenArea = m_flattenArea;
		
		GUID parentId = GUID_NULL;
		GUID lookatId = GUID_NULL;

		xmlNode->getAttr( "Name",name );
		xmlNode->getAttr( "Pos",pos );
		if (!xmlNode->getAttr( "Rotate",quat ))
		{
			// Backward compatability.
			if (xmlNode->getAttr( "Angles",angles ))
			{
				angles = DEG2RAD(angles);
				//angles.z += gf_PI/2;
				quat.SetRotationXYZ(angles);
			}
		}
		xmlNode->getAttr( "Scale",scale );
		xmlNode->getAttr( "ColorRGB",color );
		xmlNode->getAttr( "FlattenArea",flattenArea );
		xmlNode->getAttr( "Flags",flags );
		xmlNode->getAttr( "Parent",parentId );
		xmlNode->getAttr( "LookAt",lookatId );
		xmlNode->getAttr( "Material",mtlName );
		xmlNode->getAttr( "MinSpec",nMinSpec );
		
		if (nMinSpec <= CONFIG_VERYHIGH_SPEC) // Ignore invalid values.
			m_nMinSpec = nMinSpec;

		if (m_bUseMaterialLayersMask)
		{
			uint32 nMask = 0;
			xmlNode->getAttr( "MatLayersMask",nMask );
			m_nMaterialLayersMask = nMask;
		}

		bool bHidden = flags & OBJFLAG_HIDDEN;
		bool bFrozen = flags & OBJFLAG_FROZEN;

		m_flags = flags;
		m_flags &= ~OBJFLAG_PERSISTMASK;
		m_flags |= (oldFlags) & (~OBJFLAG_PERSISTMASK);
		//SetFlags( flags & OBJFLAG_PERSISTMASK );
		m_flags &= ~OBJFLAG_SHARED; // clear shared flag
		m_flags &= ~OBJFLAG_DELETED; // clear deleted flag

		ar.SetResolveCallback( this,parentId,functor(*this,&CBaseObject::ResolveParent) );
		ar.SetResolveCallback( this,lookatId,functor(*this,&CBaseObject::SetLookAt) );
		
		if (name != m_name)
		{
			// This may change object id.
			SetName( name );
		}

		//////////////////////////////////////////////////////////////////////////
		// Check if position is bad.
		if (fabs(pos.x) > INVALID_POSITION_EPSILON ||
				fabs(pos.y) > INVALID_POSITION_EPSILON ||
				fabs(pos.z) > INVALID_POSITION_EPSILON)
		{
			// File Not found.
			CErrorRecord err;
			err.error.Format( "Object %s have invalid position (%f,%f,%f)",(const char*)GetName(),pos.x,pos.y,pos.z );
			err.pObject = this;
			err.severity = CErrorRecord::ESEVERITY_WARNING;
			GetIEditor()->GetErrorReport()->ReportError(err);
		}
		//////////////////////////////////////////////////////////////////////////
		
		if (!ar.bUndo)
			SetLocalTM( pos,quat,scale );
		else
			SetLocalTM( pos,quat,scale,TM_UNDO );

		SetColor( color );
		SetArea( flattenArea );
		SetFrozen( bFrozen );
		SetHidden( bHidden );
		
		if (pLayer)
			SetLayer( pLayer );

		InvalidateTM(0);
		SetModified();

		//////////////////////////////////////////////////////////////////////////
		// Load material.
		//////////////////////////////////////////////////////////////////////////
		CMaterial *pMaterial = NULL;
		if (!mtlName.IsEmpty())
		{
			pMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial( mtlName );
		}
		SetMaterial(pMaterial);
		//////////////////////////////////////////////////////////////////////////
		
		if (ar.bUndo)
		{
			// If we are selected update UI Panel.
			UpdateEditParams();
		}
	}
	else
	{
		// Saving.

		// This attributed only readed by ObjectManager.
		xmlNode->setAttr( "Type",GetTypeName() );

		if (m_layer)
			xmlNode->setAttr( "Layer",m_layer->GetName() );

		xmlNode->setAttr( "Id",m_guid );
		xmlNode->setAttr( "Name",GetName() );

		if (m_parent)
			xmlNode->setAttr( "Parent",m_parent->GetId() );

		if (m_lookat)
			xmlNode->setAttr( "LookAt",m_lookat->GetId() );
		
		if (!IsEquivalent(GetPos(),Vec3(0,0,0),0))
			xmlNode->setAttr( "Pos",GetPos() );
		
		if (!m_rotate.IsIdentity())
			xmlNode->setAttr( "Rotate",m_rotate );
		
		if (!IsEquivalent(GetScale(),Vec3(1,1,1),0))
			xmlNode->setAttr( "Scale",GetScale() );
		
		xmlNode->setAttr( "ColorRGB",GetColor() );

		if (GetArea() != 0)
			xmlNode->setAttr( "FlattenArea",GetArea() );
		
		int flags = m_flags & OBJFLAG_PERSISTMASK;
		if (flags != 0)
			xmlNode->setAttr( "Flags",flags );

		if (m_pMaterial)
			xmlNode->setAttr( "Material",m_pMaterial->GetName() );

		if (m_nMinSpec != 0)
			xmlNode->setAttr( "MinSpec",(uint32)m_nMinSpec );

		if (m_bUseMaterialLayersMask)
		{
			uint32 nMask = m_nMaterialLayersMask;
			xmlNode->setAttr( "MatLayersMask",nMask );
		}
	}

	// Serialize variables after default entity parameters.
	CVarObject::Serialize( xmlNode,ar.bLoading );
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CBaseObject::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	XmlNodeRef objNode = xmlNode->newChild( "Object" );

	objNode->setAttr( "Type",GetTypeName() );
	objNode->setAttr( "Name",GetName() );

	if (m_pMaterial)
		objNode->setAttr( "Material",m_pMaterial->GetName() );
		
	Vec3 pos,scale;
	Quat rotate;
	if (m_parent)
	{
		// Export world coordinates.
		AffineParts ap;
		ap.SpectralDecompose( GetWorldTM() );

		pos = ap.pos;
		rotate = ap.rot;
		scale = ap.scale;
	}
	else
	{
		pos = m_pos;
		rotate = m_rotate;
		scale = m_scale;
	}

	if (!IsEquivalent(pos,Vec3(0,0,0),0))
		objNode->setAttr( "Pos",pos );

	if (!rotate.IsIdentity())
		objNode->setAttr( "Rotate",rotate );

	if (!IsEquivalent(scale,Vec3(1,1,1),0))
		objNode->setAttr( "Scale",scale );

	if (m_nMinSpec != 0)
		objNode->setAttr( "MinSpec",(uint32)m_nMinSpec );

	// Save variables.
	CVarObject::Serialize( objNode,false );

	return objNode;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CBaseObject::FindObject( REFGUID id ) const
{
	return GetObjectManager()->FindObject( id );
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::StoreUndo( const char *UndoDescription,bool minimal )
{
	if (CUndo::IsRecording())
	{
		if (minimal)
			CUndo::Record( new CUndoBaseObjectPos(this,UndoDescription) );
		else
			CUndo::Record( new CUndoBaseObject(this,UndoDescription) );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsCreateGameObjects() const
{
	return GetObjectManager()->IsCreateGameObjects();
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetLayer( CObjectLayer *layer )
{
	if (layer == m_layer)
		return;
	assert( layer != 0 );
	StoreUndo( "Set Layer" );
	m_layer = layer;

	// Set layer for all childs.
	for (int i = 0; i < m_childs.size(); i++)
	{
		m_childs[i]->SetLayer(layer);
	}

	// If object have target, target must also go into this layer.
	if (m_lookat)
	{
		m_lookat->SetLayer(layer);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::HitTestRectBounds( HitContext &hc,const AABB &box )
{
	// transform all 8 vertices into view space.
	CPoint p[8] = 
	{ 
		hc.view->WorldToView(Vec3(box.min.x,box.min.y,box.min.z)),
		hc.view->WorldToView(Vec3(box.min.x,box.max.y,box.min.z)),
		hc.view->WorldToView(Vec3(box.max.x,box.min.y,box.min.z)),
		hc.view->WorldToView(Vec3(box.max.x,box.max.y,box.min.z)),
		hc.view->WorldToView(Vec3(box.min.x,box.min.y,box.max.z)),
		hc.view->WorldToView(Vec3(box.min.x,box.max.y,box.max.z)),
		hc.view->WorldToView(Vec3(box.max.x,box.min.y,box.max.z)),
		hc.view->WorldToView(Vec3(box.max.x,box.max.y,box.max.z))
	};

	CRect objrc,temprc;

	objrc.left = 10000;
	objrc.top = 10000;
	objrc.right = -10000;
	objrc.bottom = -10000;
	// find new min/max values
	for(int i=0; i<8; i++)
	{
		objrc.left = min(objrc.left,p[i].x);
		objrc.right = max(objrc.right,p[i].x);
		objrc.top = min(objrc.top,p[i].y);
		objrc.bottom = max(objrc.bottom,p[i].y);
	}
	if (objrc.IsRectEmpty())
	{
		// Make objrc at least of size 1.
		objrc.bottom += 1;
		objrc.right += 1;
	}
	if (temprc.IntersectRect( objrc,hc.rect ))
	{
		hc.object = this;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::HitTestRect( HitContext &hc )
{
	BBox box;

	if (hc.bUseSelectionHelpers)
	{
		box.min = GetWorldPos();
		box.max = box.min;
	}
	else
	{
		// Retrieve world space bound box.
		GetBoundBox( box );
	}

	return HitTestRectBounds( hc,box );
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CBaseObject::GetChild( int i ) const
{
	assert( i >= 0 && i < m_childs.size() );
	return m_childs[i];
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsChildOf( CBaseObject *node )
{
	CBaseObject *p = m_parent;
	while (p && p != node) {
		p = p->m_parent;
	}
	if (p == node)
		return true;
	return false;
}
	
//////////////////////////////////////////////////////////////////////////
void CBaseObject::AttachChild( CBaseObject* child,bool bKeepPos )
{
	assert( child );
	if (!child)
		return;

	child->StoreUndo( "Attach Child" );

	Matrix34 childTM;
	if (bKeepPos)
	{
		childTM = child->GetWorldTM();
	}

	// If not already attached to this node.
	if (child->m_parent == this)
		return;
  
	// Add to child list first to make sure node not get deleted while reattaching.
	m_childs.push_back( child );
	if (child->m_parent)
		child->DetachThis(bKeepPos);	// Detach node if attached to other parent.	

	//////////////////////////////////////////////////////////////////////////
	child->m_parent = this;	// Assign this node as parent to child node.

	//Timur[14/11/2003] Removed change of layer for child object.
	// child->SetLayer( GetLayer() );
	
	//Timur[9/11/2002]
	/*
	// All child object must have same sharing flag.
	if (CheckFlags(OBJFLAG_SHARED))
		child->SetFlags(OBJFLAG_SHARED);
	else
		child->ClearFlags(OBJFLAG_SHARED);
	*/

	if (bKeepPos)
	{
		//////////////////////////////////////////////////////////////////////////
		// Keep old world space transformation.
		child->SetWorldTM( childTM );
	}
	//else
	child->InvalidateTM(0);

  NotifyListeners( ON_CHILDATTACHED );
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DetachAll( bool bKeepPos )
{
	while (!m_childs.empty())
	{
		CBaseObject* child = *m_childs.begin();
		child->DetachThis(bKeepPos);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DetachThis( bool bKeepPos )
{
	if (m_parent)
	{
		StoreUndo( "DetachThis" );

		Matrix34 worldTM;
		
		if (bKeepPos)
		{
			worldTM = GetWorldTM();
		}
		
		// Copy parent to temp var, erasing child from parent may delete this node if child referenced only from parent.
		CBaseObject* parent = m_parent;
		m_parent = 0;
		parent->RemoveChild( this );
		
		if (bKeepPos)
		{
			// Keep old world space transformation.
			SetLocalTM( worldTM );
		}
		else
			InvalidateTM(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::RemoveChild( CBaseObject *node )
{
	Childs::iterator it = std::find( m_childs.begin(),m_childs.end(),node );
	if (it != m_childs.end())
		m_childs.erase( it );
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::ResolveParent( CBaseObject *parent )
{
	if (parent != m_parent)
	{
		if (parent)
			parent->AttachChild( this,false );
		else
			DetachThis(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::CalcLocalTM( Matrix34 &tm ) const
{
	tm.SetIdentity();

	if (m_lookat)
	{
		Vec3 pos = m_pos;

		if (m_parent)
		{
			// Get our world position.
			pos = m_parent->GetWorldTM().TransformPoint(pos);
		}

		// Calculate local TM matrix differently.
		tm =Matrix34( Matrix33::CreateRotationVDir((m_lookat->GetWorldPos()-pos).GetNormalized()), pos ); 
	}
	else
	{
		tm = Matrix34::Create(m_scale,m_rotate,m_pos);
	}
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CBaseObject::GetWorldTM() const
{
	if (!m_bMatrixValid)
	{
    m_worldTM = GetLocalTM();
		m_bMatrixValid = true;
		m_bMatrixInWorldSpace = false;
		m_bWorldBoxValid = false;
	}
	if (!m_bMatrixInWorldSpace)
	{
		CBaseObject *parent = GetParent();
		if (parent)
		{
			m_worldTM = parent->GetWorldTM() * m_worldTM;
		}
		m_bMatrixInWorldSpace = true;
		m_bWorldBoxValid = false;
	}
	return m_worldTM;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::InvalidateTM( int nWhyFlags )
{
	bool bMatrixWasValid = m_bMatrixValid;

	m_bMatrixInWorldSpace = false;
	m_bMatrixValid = false;
	m_bWorldBoxValid = false;

	// If matrix was valid, ivalidate all childs.
	if (bMatrixWasValid)
	{
		if (m_lookatSource)
			m_lookatSource->InvalidateTM(TM_PARENT_CHANGED);

		// Invalidate matrices off all child objects.
		for (int i = 0; i < m_childs.size(); i++)
		{
			if (m_childs[i] != 0 && m_childs[i]->m_bMatrixValid)
			{
				m_childs[i]->InvalidateTM(TM_PARENT_CHANGED);
			}
		}
		NotifyListeners( ON_TRANSFORM );
	}

	// Notify parent that we were modified.
	if (m_parent)
	{
		m_parent->OnChildModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetLocalTM( const Vec3 &pos,const Quat &rotate,const Vec3& scale, int nWhyFlags )
{
	bool b1 = SetPos( pos,nWhyFlags|TM_NOT_INVALIDATE );
	bool b2 = SetRotation( rotate,nWhyFlags|TM_NOT_INVALIDATE );
	bool b3 = SetScale( scale,nWhyFlags|TM_NOT_INVALIDATE );

	if (b1 || b2 || b3 || nWhyFlags == TM_ANIMATION)
	{
		nWhyFlags = (b1) ? (nWhyFlags|TM_POS_CHANGED) : (nWhyFlags&(~TM_POS_CHANGED));
		nWhyFlags = (b2) ? (nWhyFlags|TM_ROT_CHANGED) : (nWhyFlags&(~TM_ROT_CHANGED));
		nWhyFlags = (b3) ? (nWhyFlags|TM_SCL_CHANGED) : (nWhyFlags&(~TM_SCL_CHANGED));
		
		InvalidateTM( nWhyFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetLocalTM( const Matrix34 &tm,int nWhyFlags )
{
	if (m_lookat)
	{
		SetPos( tm.GetTranslation(),TM_NOT_INVALIDATE );
		InvalidateTM(nWhyFlags);
	}
	else
	{
		AffineParts affineParts;
		affineParts.SpectralDecompose(tm);

		SetLocalTM( affineParts.pos,affineParts.rot,affineParts.scale,nWhyFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetWorldTM( const Matrix34 &tm,int nWhyFlags )
{
	if (GetParent())
	{
		Matrix34 invParentTM = GetParent()->GetWorldTM();
		invParentTM.Invert();
		Matrix34 localTM = invParentTM * tm;
		SetLocalTM( localTM,nWhyFlags );
	}
	else
	{
		SetLocalTM( tm,nWhyFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::UpdateVisibility( bool visible )
{
	/*
	for (int i = 0; i < m_childs.size(); i++)
	{
		m_childs[i]->UpdateVisibility( visible );
	}
	*/
	if (visible == CheckFlags(OBJFLAG_INVISIBLE))
	{
		if (!visible)
			m_flags |= OBJFLAG_INVISIBLE;
		else
			m_flags &= ~OBJFLAG_INVISIBLE;

		NotifyListeners( ON_VISIBILITY );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::AddGizmo( CGizmo *gizmo )
{
	GetObjectManager()->GetGizmoManager()->AddGizmo( gizmo );
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::RemoveGizmo( CGizmo *gizmo )
{
	GetObjectManager()->GetGizmoManager()->RemoveGizmo( gizmo );
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetLookAt( CBaseObject *target )
{
	if (m_lookat == target)
		return;

	StoreUndo( "Change LookAt" );

	if (m_lookat)
	{
		// Unbind current lookat.
		m_lookat->m_lookatSource = 0;
	}
	m_lookat = target;
	if (m_lookat)
	{
		m_lookat->m_lookatSource = this;
	}

	InvalidateTM(0);
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsLookAtTarget() const
{
	return m_lookatSource != 0;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::AddEventListener( const EventCallback &cb )
{
	if (std::find(m_eventListeners.begin(),m_eventListeners.end(),cb) == m_eventListeners.end())
		m_eventListeners.push_back(cb);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::RemoveEventListener( const EventCallback &cb )
{
	std::list<EventCallback>::iterator it = std::find(m_eventListeners.begin(),m_eventListeners.end(),cb);
	if (it != m_eventListeners.end())
	{
		m_nextListener = m_eventListeners.erase(it);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::NotifyListeners( EObjectListenerEvent event )
{
	for (std::list<EventCallback>::iterator it = m_eventListeners.begin(); it != m_eventListeners.end(); it = m_nextListener)
	{
		m_nextListener = it;
		m_nextListener++;
		// Call listener callback.
		(*it)( this,event );
	}
	m_nextListener = m_eventListeners.end();
	/*
	std::list<EventCallback>::iterator next;
	for (std::list<EventCallback>::iterator it = m_eventListeners.begin(); it != m_eventListeners.end(); it = next)
	{
		next = it;
		++next;
		// Call listener callback.
		(*it)( this,event );
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::ConvertFromObject( CBaseObject *object )
{
	SetLocalTM( object->GetLocalTM() );
	SetName( object->GetName() );
	SetLayer( object->GetLayer() );
	SetColor( object->GetColor() );
	m_flattenArea = object->m_flattenArea;
	if (object->GetParent())
	{
		object->GetParent()->AttachChild( this );
	}
	SetMaterial( object->GetMaterial() );
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsPotentiallyVisible() const
{
	if (!m_layer->IsVisible())
		return false;
	if (CheckFlags(OBJFLAG_HIDDEN))
		return false;
	if (gSettings.objectHideMask & GetType())
		return false;

	CGroup *pGroup = GetGroup();
	if (pGroup)
	{
		if (!pGroup->IsPotentiallyVisible())
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
//! Analyze errors for this object.
void CBaseObject::Validate( CErrorReport *report )
{
	// Checks for invalid values in base object.
	
	//////////////////////////////////////////////////////////////////////////
	// Check if position is bad.
	if (fabs(m_pos.x) > INVALID_POSITION_EPSILON ||
		fabs(m_pos.y) > INVALID_POSITION_EPSILON ||
		fabs(m_pos.z) > INVALID_POSITION_EPSILON)
	{
		// File Not found.
		CErrorRecord err;
		err.error.Format( "Object %s have invalid position (%f,%f,%f)",(const char*)GetName(),m_pos.x,m_pos.y,m_pos.z );
		err.pObject = this;
		report->ReportError(err);
	}
	//////////////////////////////////////////////////////////////////////////

	float minScale = 0.01f;
	float maxScale = 1000.0f;
	//////////////////////////////////////////////////////////////////////////
	// Check if position is bad.
	if (m_scale.x < minScale || m_scale.x > maxScale ||
			m_scale.y < minScale || m_scale.y > maxScale ||
			m_scale.z < minScale || m_scale.z > maxScale)
	{
		// File Not found.
		CErrorRecord err;
		err.error.Format( "Object %s have invalid scale (%f,%f,%f)",(const char*)GetName(),m_scale.x,m_scale.y,m_scale.z );
		err.pObject = this;
		report->ReportError(err);
	}
	//////////////////////////////////////////////////////////////////////////

	if (GetMaterial() != NULL && GetMaterial()->IsDummy())
	{
		CErrorRecord err;
		err.error.Format( "Material: %s for object: %s not found,",GetMaterial()->GetName(),(const char*)GetName() );
		err.pObject = this;
		report->ReportError(err);
	}
	
};

//////////////////////////////////////////////////////////////////////////
Ang3 CBaseObject::GetWorldAngles() const
{
	if (m_scale == Vec3(1,1,1))
	{
		Quat q = Quat( GetWorldTM() );
		Ang3 angles = RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(q)));
		return angles;
	}
	else
	{
		Matrix34 tm = GetWorldTM();
		tm.OrthonormalizeFast();
		Quat q = Quat(tm);
		Ang3 angles = RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(q)));
		return angles;
	}
};

//////////////////////////////////////////////////////////////////////////
void CBaseObject::PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx )
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
	for (int i = 0; i < pFromObject->GetChildCount(); i++)
	{
		CBaseObject *pChildObject = pFromObject->GetChild(i);
		CBaseObject *pClonedChild = GetObjectManager()->CloneObject( pChildObject );
		ctx.AddClone( pChildObject,pClonedChild );
	}
	for (int i = 0; i < pFromObject->GetChildCount(); i++)
	{
		CBaseObject *pChildObject = pFromObject->GetChild(i);
		CBaseObject *pClonedChild = ctx.FindClone( pChildObject );
		if (pClonedChild)
		{
			pClonedChild->PostClone( pChildObject,ctx );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::OnChangeGUID( REFGUID newGUID )
{
	m_guid = newGUID;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::GatherUsedResources( CUsedResources &resources )
{
	if (GetVarBlock())
		GetVarBlock()->GatherUsedResources( resources );
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsSimilarObject( CBaseObject *pObject )
{
	if (pObject->GetClassDesc() == GetClassDesc() && pObject->GetRuntimeClass() == GetRuntimeClass())
	{
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetMaterial( CMaterial *mtl )
{
	StoreUndo( "Assign Material" );
	if (m_pMaterial)
	{
		m_pMaterial->Release();
	}
	m_pMaterial = mtl;
	if (m_pMaterial)
	{
		m_pMaterial->AddRef();
	}

	UpdateEditParams();
}

//////////////////////////////////////////////////////////////////////////
int CBaseObject::AddUIPage( const char* sCaption, CDialog *pDialog,bool bAutoDestroy,int iIndex,bool bAutoExpand )
{
	return GetIEditor()->AddRollUpPage(ROLLUP_OBJECTS,sCaption,pDialog,bAutoDestroy,iIndex,bAutoDestroy);
	/*
	if (!s_tabPanel)
	{
		s_tabPanel = new CTabPanel;
		s_tabPanelIndex = GetIEditor()->AddRollUpPage(ROLLUP_OBJECTS,"Properties",s_tabPanel);
	}
	return s_tabPanel->AddPage(sCaption,pDialog,bAutoDestroy);
	*/
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::RemoveUIPage(int iIndex)
{
	GetIEditor()->RemoveRollUpPage(ROLLUP_OBJECTS,iIndex);

	/*
	if (s_tabPanel)
	{
		s_tabPanel->RemovePage(iIndex);
		int nPageCount = s_tabPanel->GetPageCount();
		if (nPageCount == 0)
		{
			GetIEditor()->RemoveRollUpPage(ROLLUP_OBJECTS,s_tabPanelIndex);
			s_tabPanel = 0;
		}
	}
	*/
}
