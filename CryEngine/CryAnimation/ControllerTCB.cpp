////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   controllertcb.h
//  Version:     v1.00
//  Created:     12/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: TCB controller implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CharacterManager.h"


f32 CControllerTCB::NTime2KTime(int32 GAID,f32 ntime)
{
	//assert(ntime>=0 && ntime<=1);
	int32 numAnims=int32(g_AnimationManager.m_arrGlobalAnimations.size());
	assert(GAID>=0);
	assert(GAID<numAnims);
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GAID];
	f32 duration	=rGlobalAnimHeader.m_fEndSec-rGlobalAnimHeader.m_fStartSec;		
	f32 start			=rGlobalAnimHeader.m_fStartSec;		
	f32 key				= ntime*TICKS_PER_SECOND*duration + start*TICKS_PER_SECOND;
	return key;
}

Status4 CControllerTCB::GetOPS (int GAID, f32 ntime, Quat& rot, Vec3& pos, Diag33& scl)
{
	f32 key = NTime2KTime(GAID,ntime);
	if (m_active.o)
	{
		Quat out;
		m_rotTrack.interpolate( key, out );
		rot = !out;
	}

	if (m_active.p)
	{
		Vec3 out;
		m_posTrack.interpolate( key,out );
		pos=out/100.0f;	// Position controller from Max must be scalled 100 times down.
	}

	if (m_active.s) 
	{
		Vec3 out;
		m_sclTrack.interpolate( key,out );
		scl=Diag33(out);
	}
	return m_active;
}

Status4 CControllerTCB::GetOP (int GAID, f32 ntime, Quat& rot, Vec3& pos)
{
	f32 key = NTime2KTime(GAID,ntime);
	if (m_active.o)
	{
		Quat out;
		m_rotTrack.interpolate( key, out );
		rot = !out;
	}

	if (m_active.p)
	{
		Vec3 out;
		m_posTrack.interpolate( key,out );
		pos=out/100.0f;	// Position controller from Max must be scalled 100 times down.
	}

	return m_active;
}



uint32 CControllerTCB::GetO(int GAID, f32 ntime, Quat& rot )
{
	f32 key = NTime2KTime(GAID,ntime);
	if (m_active.o)
	{
		Quat out;
		m_rotTrack.interpolate( key, out );
		rot = !out;
	}
	return m_active.o;
}

uint32 CControllerTCB::GetP(int GAID, f32 ntime, Vec3& pos )
{
	f32 key = NTime2KTime(GAID,ntime);
	if (m_active.p)
	{
		Vec3 out;
		m_posTrack.interpolate( key,out );
		pos=out/100.0f;	// Position controller from Max must be scalled 100 times down.
	}
	return m_active.p;
}

uint32 CControllerTCB::GetS(int GAID, f32 ntime, Diag33& scl )
{
	f32 key = NTime2KTime(GAID,ntime);
	if (m_active.s)
	{
		Vec3 out;
		m_sclTrack.interpolate( key,out );
		scl=Diag33(out);
	}
	return m_active.s;
}

