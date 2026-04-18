////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   GameTokenLibrary.cpp
//  Version:     v1.00
//  Created:     10/11/2003 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GameTokenLibrary.h"
#include "GameTokenItem.h"

//////////////////////////////////////////////////////////////////////////
// CGameTokenLibrary implementation.
//////////////////////////////////////////////////////////////////////////
bool CGameTokenLibrary::Save()
{
	XmlNodeRef root = CreateXmlNode( "GameTokensLibrary" );
	Serialize( root,false );
	bool bRes = SaveXmlNode( root,GetFilename() );
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
bool CGameTokenLibrary::Load( const CString &filename )
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
void CGameTokenLibrary::Serialize( XmlNodeRef &root,bool bLoading )
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
			if (stricmp(itemNode->getTag(),"GameToken") != 0)
				continue;
			CBaseLibraryItem *pItem = new CGameTokenItem;
			AddItem( pItem );

			CBaseLibraryItem::SerializeContext ctx( itemNode,bLoading );
			pItem->Serialize( ctx );
		}
		SetModified(false);
	}
	else
	{
		// Saving.
		root->setAttr( "Name",GetName() );
		// Serialize prototypes.
		for (int i = 0; i < GetItemCount(); i++)
		{
			XmlNodeRef itemNode = root->newChild( "GameToken" );
			CBaseLibraryItem::SerializeContext ctx( itemNode,bLoading );
			GetItem(i)->Serialize( ctx );
		}
	}
}
