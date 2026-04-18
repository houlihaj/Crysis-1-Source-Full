////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   SequenceObject.cpp
//  Version:     v1.00
//  Created:     05/02/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: CSequenceObject implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ObjectManager.h"

#include "Entity.h"
#include "Geometry\EdMesh.h"
#include "Material\Material.h"
#include "Material\MaterialManager.h"
#include "Settings.h"
#include "CryEditDoc.h"

#include <IEntitySystem.h>
#include <IEntityRenderState.h>


#include "SequenceObject.h"



//////////////////////////////////////////////////////////////////////////
// CSequenceObject implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CSequenceObject,CBaseObject)


//////////////////////////////////////////////////////////////////////////
CSequenceObject::CSequenceObject()
{
	m_pSequence = 0;
}


//////////////////////////////////////////////////////////////////////////
bool CSequenceObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	SetColor( RGB(127,127,255) );

	if(!file.IsEmpty())
		SetName(file);

	if (IsCreateGameObjects())
	{
		if (prev)
		{
			CSequenceObject * sequenceObj = (CSequenceObject*)prev;
		}
	}

	SetTextureIcon( GetClassDesc()->GetTextureIconId() );

	// Must be after SetSequence call.
	bool res = CBaseObject::Init( ie,prev,file );
	
	if (prev)
	{
		CSequenceObject *sequenceObj = (CSequenceObject*)prev;
		m_bbox = sequenceObj->m_bbox;
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CSequenceObject::CreateGameObject()
{
	if(!m_pSequence)
	{
		m_pSequence = GetIEditor()->GetMovieSystem()->FindSequence(GetName());
		if(!m_pSequence)
			m_pSequence = GetIEditor()->GetMovieSystem()->CreateSequence(GetName());
		GetIEditor()->Notify(eNotify_OnUpdateTrackView);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSequenceObject::Done()
{
	m_pSequence = GetIEditor()->GetMovieSystem()->FindSequence(GetName());
	if(m_pSequence)
		GetIEditor()->GetMovieSystem()->RemoveSequence(m_pSequence);
	
	m_pSequence = 0; // for undo

	GetIEditor()->Notify(eNotify_OnUpdateTrackView);
	
	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
void CSequenceObject::SetName( const CString &name )
{
	if(!m_pSequence)
	{
		CBaseObject::SetName(name);
		return;
	}

	IAnimSequence * pSequence = GetIEditor()->GetMovieSystem()->FindSequence(name);
	if(!pSequence)
	{
		// do not change order of next 2 lines.
		m_pSequence->SetName(name);
		CBaseObject::SetName(name);
		//
		GetIEditor()->Notify(eNotify_OnUpdateTrackView);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequenceObject::GetBoundBox( BBox &box )
{
	box.SetTransformedAABB( GetWorldTM(),BBox(Vec3(-1,-1,-1),Vec3(1,1,1)) );
}

//////////////////////////////////////////////////////////////////////////
void CSequenceObject::GetLocalBounds( BBox &box )
{
	box.min = Vec3(-1,-1,-1);
	box.max = Vec3(1,1,1);
}


//////////////////////////////////////////////////////////////////////////
void CSequenceObject::Display( DisplayContext &dc )
{
	/*
	const Matrix34 &wtm = GetWorldTM();
	Vec3 wp = wtm.GetTranslation();

	if (IsSelected())
		dc.SetSelectedColor();
	else if (IsFrozen())
		dc.SetFreezeColor();
	else
		dc.SetColor( GetColor() );

	dc.PushMatrix( wtm );
	BBox box;
	GetLocalBounds(box);
	dc.DrawWireBox( box.min,box.max);
	dc.PopMatrix();
	*/

	DrawDefault( dc );
}


//////////////////////////////////////////////////////////////////////////
void CSequenceObject::Serialize( CObjectArchive &ar )
{
	CBaseObject::Serialize( ar );

	if (ar.bLoading)
	{
		
	}
	else
	{
		m_pSequence = GetIEditor()->GetMovieSystem()->FindSequence(GetName());
		if(m_pSequence)
		{
			XmlNodeRef sequenceNode=ar.node->newChild("Sequence");
			m_pSequence->Serialize(sequenceNode, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequenceObject::PostLoad( CObjectArchive &ar )
{
	XmlNodeRef sequenceNode = ar.node->findChild("Sequence");
	if (m_pSequence != NULL && sequenceNode != NULL)
	{
		m_pSequence->Serialize(sequenceNode, true);
		GetIEditor()->Notify(eNotify_OnUpdateTrackView);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequenceObject::SetModified()
{
	IAnimSequence * pSequence = m_pSequence;
	m_pSequence = GetIEditor()->GetMovieSystem()->FindSequence(GetName());
	if(m_pSequence!=pSequence)
		GetIEditor()->Notify(eNotify_OnUpdateTrackView);
	
	CBaseObject::SetModified();
}