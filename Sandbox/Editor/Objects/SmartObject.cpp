////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   smartobject.h
//  Version:     v1.00
//  Created:     3/7/2006 by Dejan Pavlovski
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SmartObject.h"

#include "IRenderAuxGeom.h"
#include "..\Viewport.h"
#include "Settings.h"
#include "PanelTreeBrowser.h"
#include "SmartObjects/SmartObjectsEditorDialog.h"

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE( CSmartObject, CEntity )


namespace
{
	CPanelTreeBrowser* s_treePanelPtr = NULL;
	int s_treePanelId = 0;
}


CSmartObject::CSmartObject()
{
	m_entityClass = "SmartObject";
	m_pStatObj = NULL;
	m_pHelperMtl = NULL;
	m_pClassTemplate = NULL;

	UseMaterialLayersMask( false );
}

CSmartObject::~CSmartObject()
{
	if ( m_pStatObj && m_pStatObj != (IStatObj*) -1 )
		m_pStatObj->Release();
}

//////////////////////////////////////////////////////////////////////////
void CSmartObject::Done()
{
	__super::Done();
}

bool CSmartObject::Init( IEditor* ie, CBaseObject* prev, const CString& file )
{
	SetColor( RGB(255,255,0) );
	bool res = __super::Init( ie,prev,file );

	return res;
}

float CSmartObject::GetRadius()
{
	return 0.5f;
}

void CSmartObject::SetScale( const Vec3& scale )
{
	// Ignore scale.
}

void CSmartObject::Display( DisplayContext& dc )
{
	const Matrix34& wtm = GetWorldTM();

	if ( IsFrozen() )
		dc.SetFreezeColor();
	else
		dc.SetColor( GetColor() );

	if ( !GetIStatObj() )
	{
		dc.RenderObject( STATOBJECT_ANCHOR, wtm );
	}
	else
	{
		float color[4];
		color[0] = dc.GetColor().r * (1.0f/255.0f);
		color[1] = dc.GetColor().g * (1.0f/255.0f);
		color[2] = dc.GetColor().b * (1.0f/255.0f);
		color[3] = dc.GetColor().a * (1.0f/255.0f);

		Matrix34 tempTm = wtm;
		SRendParams rp;
		rp.pMatrix = &tempTm;
		rp.AmbientColor = ColorF(color[0],color[1],color[2], 1);
		rp.fAlpha = color[3];
		rp.nDLightMask = 0xFFFF;
		rp.dwFObjFlags |= FOB_TRANS_MASK;
		//rp.nShaderTemplate = EFT_HELPER;
		m_pStatObj->Render( rp );
	}

	dc.SetColor( GetColor() );

	if ( IsSelected() || IsHighlighted() )
	{
		dc.PushMatrix(wtm);
		if ( !GetIStatObj() )
		{
			float r = GetRadius();
			dc.DrawWireBox( -Vec3(r,r,r), Vec3(r,r,r) );
		}
		else
		{
			dc.DrawWireBox( m_pStatObj->GetBoxMin(), m_pStatObj->GetBoxMax() );
		}
		dc.PopMatrix();

	// this is now done in CEntity::DrawDefault
	//	if ( gEnv->pAISystem )
	//		gEnv->pAISystem->DrawSOClassTemplate( GetIEntity() );
	}

	DrawDefault( dc );
}

bool CSmartObject::HitTest( HitContext& hc )
{
	if ( GetIStatObj() )
	{
		float hitEpsilon = hc.view->GetScreenScaleFactor( GetWorldPos() ) * 0.01f;
		float hitDist;

		float fScale = GetScale().x;
		BBox boxScaled;
		GetLocalBounds( boxScaled );
		boxScaled.min *= fScale;
		boxScaled.max *= fScale;

		Matrix34 invertWTM = GetWorldTM();
		invertWTM.Invert();

		Vec3 xformedRaySrc = invertWTM.TransformPoint(hc.raySrc);
		Vec3 xformedRayDir = invertWTM.TransformVector(hc.rayDir);
		xformedRayDir.Normalize();

		Vec3 intPnt;
		// Check intersection with bbox edges.
		if (boxScaled.RayEdgeIntersection( xformedRaySrc,xformedRayDir,hitEpsilon,hitDist,intPnt ))
		{
			hc.dist = xformedRaySrc.GetDistance(intPnt);
			return true;
		}

		return false;
	}

	Vec3 origin = GetWorldPos();
	float radius = GetRadius();

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross( w );
	float d = w.GetLength();

	if (d < radius + hc.distanceTollerance)
	{
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
}

void CSmartObject::GetLocalBounds( BBox& box )
{
	if ( GetIStatObj() )
	{
		box.min = m_pStatObj->GetBoxMin();
		box.max = m_pStatObj->GetBoxMax();
	}
	else
	{
		float r = GetRadius();
		box.min = -Vec3(r,r,r);
		box.max = Vec3(r,r,r);
	}
}

//////////////////////////////////////////////////////////////////////////
const IStatObj* CSmartObject::GetIStatObj()
{
	if ( m_pStatObj == (IStatObj*) -1 )
		return NULL;

	if ( m_pStatObj )
		return m_pStatObj;

	IStatObj** pStatObjects = NULL;
	if ( gEnv->pAISystem )
		gEnv->pAISystem->GetSOClassTemplateIStatObj( GetIEntity(), pStatObjects );
	if ( pStatObjects )
	{
		m_pStatObj = pStatObjects[0];
		m_pStatObj->AddRef();
		return m_pStatObj;
	}

	m_pStatObj = (IStatObj*) -1;
	return NULL;

	// Try to load the object specified in the SO class template
	m_pClassTemplate = NULL;
	CString classes;
	IVariable* pVar = GetProperties() ? GetProperties()->FindVariable("soclasses_SmartObjectClass") : NULL;
	if ( pVar )
	{
		pVar->Get( classes );
		if ( classes.IsEmpty() )
			return NULL;

		SetStrings setClasses;
		CSOLibrary::String2Classes( (const char*) classes, setClasses );

		SetStrings::iterator it, itEnd = setClasses.end();
		for ( it = setClasses.begin(); it != itEnd; ++it )
		{
			CSOLibrary::VectorClassData::iterator itClass = CSOLibrary::FindClass( *it );
			if ( itClass == CSOLibrary::GetClasses().end() )
				continue;
			m_pClassTemplate = itClass->pClassTemplateData;
			if ( m_pClassTemplate && !m_pClassTemplate->model.empty() )
				break;
		}
	}

	if ( m_pClassTemplate && !m_pClassTemplate->model.empty() )
	{
		m_pStatObj = GetIEditor()->Get3DEngine()->LoadStatObj( "Editor/Objects/" + m_pClassTemplate->model );
		if ( !m_pStatObj )
		{
			CLogFile::FormatLine( "Error: Load Failed: %s", (const char*) m_pClassTemplate->model );
			return NULL;
		}
		m_pStatObj->AddRef();
		if ( GetHelperMaterial() )
			m_pStatObj->SetMaterial( GetHelperMaterial() );
	}

	return m_pStatObj;
}

#define HELPER_MATERIAL "Editor/Objects/Helper"

//////////////////////////////////////////////////////////////////////////
IMaterial* CSmartObject::GetHelperMaterial()
{
	if ( !m_pHelperMtl )
		m_pHelperMtl = GetIEditor()->Get3DEngine()->GetMaterialManager()->LoadMaterial( HELPER_MATERIAL );
	return m_pHelperMtl;
};

//////////////////////////////////////////////////////////////////////////
void CSmartObject::OnPropertyChange( IVariable *var )
{
	if ( m_pStatObj )
	{
		IStatObj* pOld = (IStatObj*) GetIStatObj();
		m_pStatObj = NULL;
		GetIStatObj();
		if ( pOld )
			pOld->Release();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSmartObject::BeginEditParams( IEditor *ie,int flags )
{
	m_properties->AddOnSetCallback( functor(*this,&CSmartObject::OnPropertyChange) );

	__super::BeginEditParams( ie,flags );

/*
	if ( gSettings.bGeometryBrowserPanel )
	{
		CString prefabName = "";
		CString model;
		IVariable* pVar = GetProperties()->FindVariable("object_Model");
		if ( pVar )
		{
			pVar->Get( prefabName );
			if (!prefabName.IsEmpty())
			{
				if (!s_treePanelPtr)
				{
					s_treePanelPtr = &s_treePanel;
					//s_treePanel = new CPanelTreeBrowser;
					int flags = CPanelTreeBrowser::NO_DRAGDROP|CPanelTreeBrowser::NO_PREVIEW|CPanelTreeBrowser::SELECT_ONCLICK;
					s_treePanelPtr->Create( functor(*this,&CSmartObject::OnFileChange),GetClassDesc()->GetFileSpec(),AfxGetMainWnd(),flags );
				}
				if (s_treePanelId == 0)
					s_treePanelId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,_T("Model"),s_treePanelPtr,false );
			}
		}

		if (s_treePanelPtr)
		{
			s_treePanelPtr->SetSelectCallback( functor(*this,&CSmartObject::OnFileChange) );
			s_treePanelPtr->SelectFile( prefabName );
		}
	}
*/
}

//////////////////////////////////////////////////////////////////////////
void CSmartObject::EndEditParams( IEditor *ie )
{
	m_properties->RemoveOnSetCallback( functor(*this,&CSmartObject::OnPropertyChange) );

	__super::EndEditParams( ie );

/*
	if (s_treePanelId != 0)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,s_treePanelId );
		s_treePanelId = 0;
	}
*/

	Reload( true );
}
