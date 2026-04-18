////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   particlelibrary.cpp
//  Version:     v1.00
//  Created:     17/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleLibrary.h"
#include "ParticleItem.h"
#include "ErrorReport.h"

//////////////////////////////////////////////////////////////////////////
// CParticleLibrary implementation.
//////////////////////////////////////////////////////////////////////////
bool CParticleLibrary::Save()
{
	XmlNodeRef root = CreateXmlNode( "ParticleLibrary" );
	Serialize( root,false );
	bool bRes = SaveXmlNode( root, GetFilename() );
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
bool CParticleLibrary::Load( const CString &filename )
{
	if (filename.IsEmpty())
		return false;

	SetFilename( filename );
	XmlParser parser;
	XmlNodeRef root = parser.parse( filename );
	if (!root)
		return false;

	Serialize( root,true );

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CParticleLibrary::Serialize( XmlNodeRef &root,bool bLoading )
{
	if (bLoading)
	{
		// Loading.
		CString name = GetName();
		root->getAttr( "Name",name );
		SetName( name );
		for (int i = 0; i < root->getChildCount(); i++)
		{
			XmlNodeRef itemNode = root->getChild(i);
			// Only accept nodes with correct name.
			if (stricmp(itemNode->getTag(),"Particles") != 0)
				continue;
			CString effectName = name + "." + itemNode->getAttr("Name");
			IParticleEffect* pEffect = GetIEditor()->Get3DEngine()->FindParticleEffect(effectName, "", "", false);
			if (pEffect)
			{
				CParticleItem *pItem = new CParticleItem(pEffect);
				AddItem( pItem );
				pItem->AddAllChildren();
				pItem->CBaseLibraryItem::SetName(pEffect->GetBaseName());
			}
			else
			{
				CParticleItem *pItem = new CParticleItem;
				AddItem( pItem );
				GetIEditor()->GetErrorReport()->SetCurrentValidatorItem(pItem);
				CBaseLibraryItem::SerializeContext ctx( itemNode,bLoading );
				pItem->Serialize( ctx );
				GetIEditor()->GetErrorReport()->SetCurrentValidatorItem(NULL);
			}
		}
		SetModified(false);
	}
	else
	{
		// Saving.
		root->setAttr( "Name",GetName() );
		root->setAttr( "SandboxVersion",(const char*)GetIEditor()->GetFileVersion().ToFullString() );
		// Serialize prototypes.
		for (int i = 0; i < GetItemCount(); i++)
		{
			CParticleItem *pItem = (CParticleItem*)GetItem(i);
			// Save materials with childs under thier parent table.
			if (pItem->GetParent())
				continue;

			XmlNodeRef itemNode = root->newChild( "Particles" );
			CBaseLibraryItem::SerializeContext ctx( itemNode,bLoading );
			GetItem(i)->Serialize( ctx );
		}
	}
}