////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FaceState.h
//  Version:     v1.00
//  Created:     18/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FaceState.h"

//////////////////////////////////////////////////////////////////////////
void CFaceState::SetNumWeights( int n )
{
	m_weights.resize( n );
	m_balance.resize(n);
	Reset();
}

//////////////////////////////////////////////////////////////////////////
void CFaceState::Reset()
{
	if (m_weights.size() > 0)
		memset( &m_weights[0],0,sizeof(float)*m_weights.size() );
	if (m_balance.size() > 0)
		memset( &m_balance[0],0,sizeof(float)*m_balance.size() );
}
