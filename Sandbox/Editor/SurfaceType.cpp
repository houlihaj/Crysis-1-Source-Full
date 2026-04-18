////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   SurfaceType.cpp
//  Version:     v1.00
//  Created:     19/11/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Surface type class implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SurfaceType.h"
#include "GameEngine.h"
#include "VegetationMap.h"
#include "VegetationObject.h"
#include "Material/MaterialManager.h"

//////////////////////////////////////////////////////////////////////////
CSurfaceType::CSurfaceType()
{
	m_detailScale[0] = 1;
	m_detailScale[1] = 1;
	m_projAxis = ESFT_Z;
}

//////////////////////////////////////////////////////////////////////////
CSurfaceType::~CSurfaceType()
{
	m_detailScale[0] = 1;
	m_detailScale[1] = 1;
}

//////////////////////////////////////////////////////////////////////////
void CSurfaceType::Serialize( CXmlArchive &xmlAr )
{
	if (xmlAr.bLoading)
	{
		XmlNodeRef sfType = xmlAr.root;

		// Name
		sfType->getAttr( "Name",m_name );
		sfType->getAttr( "DetailTexture",m_detailTexture );
		sfType->getAttr( "DetailScaleX",m_detailScale[0] );
		sfType->getAttr( "DetailScaleY",m_detailScale[1] );
		sfType->getAttr( "DetailMaterial",m_material );
		sfType->getAttr( "ProjectAxis",m_projAxis );
		sfType->getAttr( "Bumpmap",m_bumpmap );

		if (!m_detailTexture.IsEmpty() && m_material.IsEmpty())
		{
			// For backward compatability create a material for this detail texture.
			CMaterial *pMtl = GetIEditor()->GetMaterialManager()->CreateMaterial( Path::RemoveExtension(m_detailTexture),XmlNodeRef(),0 );
			pMtl->AddRef();
			pMtl->SetShaderName( "Terrain.Layer" );
			pMtl->GetShaderResources().m_Textures[EFTT_DIFFUSE].m_Name = m_detailTexture;
			pMtl->Update();
			m_material = pMtl->GetName();
		}
	}
	else
	{
		////////////////////////////////////////////////////////////////////////
		// Storing
		////////////////////////////////////////////////////////////////////////
		XmlNodeRef sfType = xmlAr.root;

		// Name
		sfType->setAttr( "Name",m_name );
		sfType->setAttr( "DetailTexture",m_detailTexture );
		sfType->setAttr( "DetailScaleX",m_detailScale[0] );
		sfType->setAttr( "DetailScaleY",m_detailScale[1] );
		sfType->setAttr( "DetailMaterial",m_material );
		sfType->setAttr( "ProjectAxis",m_projAxis );
		sfType->setAttr( "Bumpmap",m_bumpmap );

		switch (m_projAxis)
		{
		case 0:
			sfType->setAttr( "ProjAxis","X" );
			break;
		case 1:
			sfType->setAttr( "ProjAxis","Y" );
			break;
		case 2:
		default:
			sfType->setAttr( "ProjAxis","Z" );
		};

		/*
		XmlNodeRef sfDetObjs = sfType->newChild( "DetailObjects" );
		
		for (int i = 0; i < m_detailObjects.size(); i++)
		{
			XmlNodeRef sfDetObj = sfDetObjs->newChild( "Object" );
			sfDetObj->setAttr( "Name",m_detailObjects[i] );
		}
		*/

		SaveVegetationIds( sfType );
	}
}

//////////////////////////////////////////////////////////////////////////
void CSurfaceType::SaveVegetationIds( XmlNodeRef &node )
{
	CVegetationMap *pVegMap = GetIEditor()->GetVegetationMap();
	// Go thru all vegetation groups, and see who uses us.
	for (int i = 0; i < pVegMap->GetObjectCount(); i++)
	{
		CVegetationObject *pObject = pVegMap->GetObject(i);
		if (pObject->IsUsedOnTerrainLayer( m_name ))
		{
			XmlNodeRef nodeVeg = node->newChild( "VegetationGroup" );
			nodeVeg->setAttr( "Id",pObject->GetId() );
			nodeVeg->setAttr( "GUID",pObject->GetGUID() );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSurfaceType::SetMaterial( const CString &mtl )
{
	m_material = mtl;
}
