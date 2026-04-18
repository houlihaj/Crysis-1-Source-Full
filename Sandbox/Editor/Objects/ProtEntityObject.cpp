////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   protentityobject.cpp
//  Version:     v1.00
//  Created:     24/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ProtEntityObject.h"

#include "EntityPrototype.h"
#include "EntityPrototypeManager.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CProtEntityObject,CEntity)

//////////////////////////////////////////////////////////////////////////
// CProtEntityObject implementation.
//////////////////////////////////////////////////////////////////////////
CProtEntityObject::CProtEntityObject()
{
	ZeroStruct(m_prototypeGUID);
	m_prototypeName = "Unknown Archetype";
}

//////////////////////////////////////////////////////////////////////////
bool CProtEntityObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	bool result = CEntity::Init( ie,prev,"" );

	if (prev)
	{
		CProtEntityObject *pe = (CProtEntityObject*)prev;
		if (pe->m_prototype)
			SetPrototype( pe->m_prototype,true );
		else
		{
			m_prototypeGUID = pe->m_prototypeGUID;
			m_prototypeName = pe->m_prototypeName;
		}
	}
	else if (!file.IsEmpty())
	{
		SetPrototype( GuidUtil::FromString(file) );
		SetUniqName( m_prototypeName );
	}

	//////////////////////////////////////////////////////////////////////////
	// Disable variables that are in archetype.
	//////////////////////////////////////////////////////////////////////////
	mv_castShadow.SetFlags( mv_castShadow.GetFlags()|IVariable::UI_DISABLED );
	mv_motionBlurAmount.SetFlags( mv_motionBlurAmount.GetFlags()|IVariable::UI_DISABLED );
	mv_ratioLOD.SetFlags( mv_ratioLOD.GetFlags()|IVariable::UI_DISABLED );
	mv_ratioViewDist.SetFlags( mv_ratioViewDist.GetFlags()|IVariable::UI_DISABLED );
	mv_recvWind.SetFlags( mv_recvWind.GetFlags()|IVariable::UI_DISABLED );

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::Done()
{
	if (m_prototype)
		m_prototype->RemoveUpdateListener( functor(*this,&CProtEntityObject::OnPrototypeUpdate) );
	m_prototype = 0;
	ZeroStruct(m_prototypeGUID);
	CEntity::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CProtEntityObject::CreateGameObject()
{
	if (m_prototype)
	{
		return CEntity::CreateGameObject();
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::SpawnEntity()
{
	if (m_prototype)
	{
		CEntity::SpawnEntity();
	}
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::SetPrototype( REFGUID guid,bool bForceReload )
{
	if (m_prototypeGUID == guid && bForceReload == false)
		return;

	m_prototypeGUID = guid;

	//m_fullPrototypeName = prototypeName;
	CEntityPrototypeManager *protMan = GetIEditor()->GetEntityProtManager();
	CEntityPrototype *prototype = protMan->FindPrototype( guid );
	if (!prototype)
	{
		m_prototypeName = "Unknown Archetype";

		CErrorRecord err;
		err.error.Format( "Cannot find Entity Archetype: %s for Entity %s",GuidUtil::ToString(guid),(const char*)GetName() );
		err.pObject = this;
		err.severity = CErrorRecord::ESEVERITY_WARNING;
		GetIEditor()->GetErrorReport()->ReportError(err);
		//Warning( "Cannot find Entity Archetype: %s for Entity %s",GuidUtil::ToString(guid),(const char*)GetName() );
		return;
	}
	SetPrototype( prototype,bForceReload );
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::BeginEditParams( IEditor *ie,int flags )
{
	CEntity::BeginEditParams( ie,flags );
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::EndEditParams( IEditor *ie )
{
	CEntity::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::BeginEditMultiSelParams( bool bAllOfSameType )
{
	CEntity::BeginEditMultiSelParams( bAllOfSameType );
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::EndEditMultiSelParams()
{
	CEntity::EndEditMultiSelParams();
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::Serialize( CObjectArchive &ar )
{
	if (ar.bLoading)
	{
		// Serialize name at first.
		CString name;
		ar.node->getAttr( "Name",name );
		SetName( name );

		// Loading.
		GUID guid;
		ar.node->getAttr( "Prototype",guid );
		SetPrototype( guid );
		if (ar.bUndo)
		{
			SpawnEntity();
		}
	}
	else
	{
		// Saving.
		ar.node->setAttr( "Prototype",m_prototypeGUID );
	}
	CEntity::Serialize( ar );
	
	if (ar.bLoading)
	{
		// If baseobject loading ovverided some variables.
		SyncVariablesFromPrototype();
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CProtEntityObject::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	return CEntity::Export( levelPath,xmlNode );
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::OnPrototypeUpdate()
{
	// Callback from prototype.
	OnPropertyChange(0);

	if (m_prototype)
	{
		m_prototypeName = m_prototype->GetName();
		SyncVariablesFromPrototype();
	}
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::SetPrototype( CEntityPrototype *prototype,bool bForceReload )
{
	assert( prototype );

	if (prototype == m_prototype && !bForceReload)
		return;

	bool bRespawn = m_entity != 0;

	StoreUndo( "Set Archetype" );

	if (m_prototype)
		m_prototype->RemoveUpdateListener( functor(*this,&CProtEntityObject::OnPrototypeUpdate) );

	m_prototype = prototype;
	m_prototype->AddUpdateListener( functor(*this,&CProtEntityObject::OnPrototypeUpdate) );
	m_properties = 0;
	m_prototypeGUID = m_prototype->GetGUID();
	m_prototypeName = m_prototype->GetName();

	SyncVariablesFromPrototype();

	SetClass( m_prototype->GetEntityClassName(),bForceReload,true );
	if (bRespawn)
		SpawnEntity();
}

//////////////////////////////////////////////////////////////////////////
void CProtEntityObject::SyncVariablesFromPrototype()
{
	if (m_prototype)
	{
		CVarBlock *pVarsInPrototype = m_prototype->GetObjectVarBlock();
		mv_castShadow.CopyValue( pVarsInPrototype->FindVariable(mv_castShadow.GetName()) );
		mv_motionBlurAmount.CopyValue( pVarsInPrototype->FindVariable(mv_motionBlurAmount.GetName()) );
		mv_ratioLOD.CopyValue( pVarsInPrototype->FindVariable(mv_ratioLOD.GetName()) );
		mv_ratioViewDist.CopyValue( pVarsInPrototype->FindVariable(mv_ratioViewDist.GetName()) );
		mv_recvWind.CopyValue( pVarsInPrototype->FindVariable(mv_recvWind.GetName()) );
	}
}