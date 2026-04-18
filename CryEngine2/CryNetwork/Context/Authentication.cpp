/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  Helper classes for authentication
 -------------------------------------------------------------------------
 History:
 - 26/07/2004   : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "Authentication.h"
#include "ITimer.h"
#include "IDataProbe.h"

ILINE static uint32 GetRandomNumber()
{
	IDataProbe * pDataProbe = GetISystem()->GetIDataProbe();
	if (pDataProbe)
		return pDataProbe->GetRand();
	else
	{
		NetWarning( "Using low quality random numbers: security compromised" );
		return (rand()<<16) | rand();
	}
}

SAuthenticationSalt::SAuthenticationSalt() :
	fTime( gEnv->pTimer->GetCurrTime() ),
	nRand( GetRandomNumber() )
{
}

void SAuthenticationSalt::SerializeWith( TSerialize ser )
{
	ser.Value( "fTime", fTime );
	ser.Value( "nRand", nRand );
}

CWhirlpoolHash SAuthenticationSalt::Hash( const string& password ) const
{
	char n1[32];
	char n2[32];
	sprintf(n1, "%f", fTime);
	sprintf(n2, "%.8x", nRand);
	string buffer = password + ":" + n1 + ":" + n2;
	return CWhirlpoolHash( buffer );
}
