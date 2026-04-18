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

#ifndef __controllertcb_h__
#define __controllertcb_h__
#pragma once

#include "TCBSpline.h"




// CJoint animation.
struct CControllerTCB : public IController
{

	uint32 GetID() const {
		return m_ID;
	}
	size_t SizeOfThis() const	{
		return sizeof(CControllerTCB) + m_posTrack.sizeofThis() +  m_rotTrack.sizeofThis() + m_sclTrack.sizeofThis();
	} 

	f32 NTime2KTime(int32 GAID,f32 ntime);
	Status4 GetOPS (int GAID, f32 t, Quat& rot, Vec3& pos, Diag33& scl);
	Status4 GetOP (int GAID, f32 t, Quat& rot, Vec3& pos);
	uint32 GetO(int GAID, f32 t, Quat& rot );
	uint32 GetP(int GAID, f32 t, Vec3& pos );
	uint32 GetS(int GAID, f32 t, Diag33& scl );
	Quat GetOrientationByKey (uint32 t) { return Quat(IDENTITY); };

	virtual EControllerInfo GetControllerType() 
	{
		return eTCB;
	}

	CInfo GetControllerInfo() const
	{
		CInfo info;
		info.numKeys = 0;
		return info;
	}

	virtual size_t GetRotationKeysNum() {
		return 0;
	}

	virtual size_t GetPositionKeysNum() {
		return 0;
	}

	Status4 m_active;
	uint32 m_ID;
	spline::TCBSpline<Vec3> m_posTrack;
	spline::TCBAngleAxisSpline m_rotTrack;
	spline::TCBSpline<Vec3> m_sclTrack;

	CControllerTCB()
	{
		m_active.ops = 0;
		m_ID=0xaaaa5555;
	}
};


#endif // __controllertcb_h__

