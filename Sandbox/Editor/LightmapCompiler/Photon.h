/*=============================================================================
  Photon.h :
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __PHOTON_H__
#define __PHOTON_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Cry_Vector3.h"

//===================================================================================
// Class				:	CTon
// Description			:	Holds a generic particle
struct	CTon 
{
	Vec3			P,N;		// Position and normal
	short			flags;		// Photon flags
};

//===================================================================================
// Class				:	CPhoton
// Description			:	A Photon
struct	CPhoton : public CTon 
{
	Vec3			C;				// The intensity
	unsigned char	theta,phi;		// Photon direction
};


#endif
