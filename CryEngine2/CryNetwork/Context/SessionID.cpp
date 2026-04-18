/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  maintains a globally unique id for game sessions
 -------------------------------------------------------------------------
 History:
 - 27/02/2006   12:34 : Created by Craig Tiller
*************************************************************************/

#include "StdAfx.h"
#include "SessionID.h"

namespace
{

	string GetDigestString( const char * hostname, int counter, time_t start )
	{
		string s;
		s.Format( "%s:%.8x:%.8x", hostname, counter, start );
		return s;
	}

}

CSessionID::CSessionID()
{
}

CSessionID::CSessionID( const char * hostname, int counter, time_t start ) : m_hash(GetDigestString(hostname, counter, start))
{
}

