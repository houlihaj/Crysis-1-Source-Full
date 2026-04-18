////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2007.
// -------------------------------------------------------------------------
//  File name:   LookAtTrack.h
//  Version:     v1.00
//  Created:     14/8/2007 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __LOOKATTRACK_H__
#define __LOOKATTRACK_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include "IMovieSystem.h"
#include "AnimTrack.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
/** Look at target track, keys represent new lookat targets for entity.
*/
class CLookAtTrack : public TAnimTrack<ILookAtKey>
{
public:
	EAnimTrackType GetType() { return ATRACK_LOOKAT; };
	EAnimValue GetValueType() { return AVALUE_LOOKAT; };

	void GetKeyInfo( int key,const char* &description,float &duration );
	void SerializeKey( ILookAtKey &key,XmlNodeRef &keyNode,bool bLoading );
};

#endif // __LOOKATTRACK_H__