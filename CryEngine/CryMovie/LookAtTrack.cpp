////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2007.
// -------------------------------------------------------------------------
//  File name:   selecttrack.cpp
//  Version:     v1.00
//  Created:     23/7/2002 by Lennert Schneider.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "LookAtTrack.h"

//////////////////////////////////////////////////////////////////////////
void CLookAtTrack::SerializeKey( ILookAtKey &key,XmlNodeRef &keyNode,bool bLoading )
{
	if (bLoading)
	{
		const char *szSelection;
		szSelection= keyNode->getAttr( "node" );
		bool bAllowAdditionalTransforms;
		if (!keyNode->getAttr("relative", bAllowAdditionalTransforms))
			bAllowAdditionalTransforms = false;
		int boneSet;
		if (!keyNode->getAttr("boneSet", boneSet))
			boneSet = -1;
		if (boneSet < 0 || boneSet > eLookAtKeyBoneSet_COUNT)
			boneSet = eLookAtKeyBoneSet_HeadEyes;

		strncpy( key.szSelection,szSelection,sizeof(key.szSelection) );
		key.szSelection[sizeof(key.szSelection)-1] = 0;
		key.bAllowAdditionalTransforms = bAllowAdditionalTransforms;
		key.boneSet = ELookAtKeyBoneSet(boneSet);
	}
	else
	{
		keyNode->setAttr( "node",key.szSelection);
		keyNode->setAttr( "relative",key.bAllowAdditionalTransforms );
		keyNode->setAttr( "boneSet",int(key.boneSet) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CLookAtTrack::GetKeyInfo( int key,const char* &description,float &duration )
{
	assert( key >= 0 && key < (int)m_keys.size() );
	CheckValid();
	description = 0;
	duration = m_keys[key].fDuration;
	if (strlen(m_keys[key].szSelection) > 0)
		description = m_keys[key].szSelection;
}