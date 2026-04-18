////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   particleitem.cpp
//  Version:     v1.00
//  Created:     17/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleItem.h"

#include "ParticleLibrary.h"
#include "BaseLibraryManager.h"

#include "ParticleParams.h"

//////////////////////////////////////////////////////////////////////////
CParticleItem::CParticleItem()
{
	m_pParentParticles = 0;
	m_pEffect = GetIEditor()->Get3DEngine()->CreateParticleEffect();
	m_pEffect->AddRef();
	m_bDebugEnabled = true;
	m_bSaveEnabled = false;
}

//////////////////////////////////////////////////////////////////////////
CParticleItem::CParticleItem( IParticleEffect *pEffect )
{
	assert(pEffect);
	m_pParentParticles = 0;
	m_pEffect = pEffect;
	m_name = pEffect->GetBaseName();
	m_bDebugEnabled = true;
	m_bSaveEnabled = false;
}

//////////////////////////////////////////////////////////////////////////
CParticleItem::~CParticleItem()
{
	GetIEditor()->Get3DEngine()->DeleteParticleEffect( m_pEffect );
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::SetName( const CString &name )
{
	if (m_pEffect)
	{
		CString fullname = GetLibrary()->GetName() + "." + name;
		m_pEffect->SetName( fullname );
		CString newname = m_pEffect->GetBaseName();
		if (newname != name)
		{
			CBaseLibraryItem::SetName( newname );
			return;
		}
	}
	CBaseLibraryItem::SetName( name );
}


void CParticleItem::AddAllChildren()
{
	// Add all children recursively.
	for (int i = 0; i < m_pEffect->GetChildCount(); i++)
	{
		CParticleItem *pItem = new CParticleItem( m_pEffect->GetChild(i) );
		GetLibrary()->AddItem( pItem );
		pItem->m_pParentParticles = this;
		m_childs.push_back(pItem);
		pItem->AddAllChildren();
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::Serialize( SerializeContext &ctx )
{
	XmlNodeRef node = ctx.node;
	if (ctx.bLoading)
	{
		if (!ctx.bIgnoreChilds)
			ClearChilds();

		m_pEffect->Serialize( node,true,!ctx.bIgnoreChilds );

		AddAllChildren();
	}
	else
	{
		m_pEffect->Serialize( node,false,true );
	}

	// Do After effect serialization.
	CBaseLibraryItem::Serialize( ctx );
}

//////////////////////////////////////////////////////////////////////////
int CParticleItem::GetChildCount() const
{
	return m_childs.size();
}

//////////////////////////////////////////////////////////////////////////
CParticleItem* CParticleItem::GetChild( int index ) const
{
	assert( index >= 0 && index < m_childs.size() );
	return m_childs[index];
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::AddChild( CParticleItem *pItem )
{
	assert( pItem );
	pItem->m_pParentParticles = this;
	m_childs.push_back(pItem);
	pItem->m_library = m_library;

	// Change name to be without group.
	if (pItem->GetName() != pItem->GetShortName())
	{
		pItem->SetName( m_library->GetManager()->MakeUniqItemName(pItem->GetShortName()) );
	}

	if (m_pEffect)
		m_pEffect->AddChild( pItem->GetEffect() );
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::RemoveChild( CParticleItem *pItem )
{
	assert( pItem );
	TSmartPtr<CParticleItem> refholder = pItem;
	if (stl::find_and_erase( m_childs,pItem ))
	{
		pItem->m_pParentParticles = NULL;
		m_library->RemoveItem(pItem);
	}
	if (m_pEffect)
		m_pEffect->RemoveChild( pItem->GetEffect() );
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::ClearChilds()
{
	// Also delete them from the library.
	for (int i = 0; i < m_childs.size(); i++)
	{
		m_childs[i]->m_pParentParticles = NULL;
		m_library->RemoveItem(m_childs[i]);
	}
	m_childs.clear();

	if (m_pEffect)
		m_pEffect->ClearChilds();
}

/*
//////////////////////////////////////////////////////////////////////////
void CParticleItem::InsertChild( int slot,CParticleItem *pItem )
{
	if (slot < 0)
		slot = 0;
	if (slot > m_childs.size())
		slot = m_childs.size();

	assert( pItem );
	pItem->m_pParentParticles = this;
	pItem->m_library = m_library;

	m_childs.insert( m_childs.begin() + slot,pItem );
	m_pMatInfo->RemoveAllSubMtls();
	for (int i = 0; i < m_childs.size(); i++)
	{
		m_pMatInfo->AddSubMtl( m_childs[i]->m_pMatInfo );
	}
}
*/

//////////////////////////////////////////////////////////////////////////
int CParticleItem::FindChild( CParticleItem *pItem )
{
	for (int i = 0; i < m_childs.size(); i++)
	{
		if (m_childs[i] == pItem)
		{
			return i;
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
CParticleItem* CParticleItem::GetParent() const
{
	return m_pParentParticles;
}

//////////////////////////////////////////////////////////////////////////
IParticleEffect* CParticleItem::GetEffect() const
{
	return m_pEffect;
}

/*
		E/S		0			1
		0/0		0/0		0/0
		0/1		0/1		1/1
		1/0		0/1		1/1
		1/1		0/1		1/1
*/
void CParticleItem::DebugEnable(int iEnable)
{
	if (m_pEffect)
	{
		if (iEnable < 0)
			iEnable = !m_bDebugEnabled;
		m_bDebugEnabled = iEnable != 0;
		m_bSaveEnabled = m_bSaveEnabled || m_pEffect->IsEnabled();
		m_pEffect->SetEnabled(m_bSaveEnabled && m_bDebugEnabled);
		for (int i = 0; i < m_childs.size(); i++)
			m_childs[i]->DebugEnable(iEnable);
	}
}

int CParticleItem::GetEnabledState() const
{
	int nEnabled = m_pEffect->IsEnabled();
	for (int i = 0; i < m_childs.size(); i++)
		if (m_childs[i]->GetEnabledState())
			nEnabled |= 2;
	return nEnabled;
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::GenerateIdRecursively()
{
	GenerateId();
	for (int i = 0; i < m_childs.size(); i++)
	{
		m_childs[i]->GenerateIdRecursively();
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::Update()
{
	// Mark library as modified.
	if (GetLibrary())
		GetLibrary()->SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::SetDefaults()
{
	if (m_pEffect)
	{
		ParticleParams params;
		m_pEffect->SetParticleParams(params);
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::GatherUsedResources( CUsedResources &resources )
{
	if (m_pEffect->GetParticleParams().sTexture.length())
		resources.Add( m_pEffect->GetParticleParams().sTexture.c_str() );
	if (m_pEffect->GetParticleParams().sMaterial.length())
		resources.Add( m_pEffect->GetParticleParams().sMaterial.c_str() );
	if (m_pEffect->GetParticleParams().sGeometry.length())
		resources.Add( m_pEffect->GetParticleParams().sGeometry.c_str() );
	if (m_pEffect->GetParticleParams().sSound.length())
		resources.Add( m_pEffect->GetParticleParams().sSound.c_str() );
}
