#ifndef __HUMANBLENDING_H__
#define __HUMANBLENDING_H__

#pragma once

#define NUMMD (0x100)

#include "IAnimatedCharacter.h"

class CHumanBlending : public IAnimationBlending
{
public:
	virtual SAnimationBlendingParams *Update(IEntity *pIEntity, Vec3 DesiredBodyDirection, Vec3 DesiredMoveDirection, f32 fDesiredMovingSpeed);

	void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->AddContainer(m_arrAbsDesiredBodyDirection);
		s->AddContainer(m_arrAbsDesiredMoveDirection);
		s->AddContainer(m_arrMotionPath);
	}
	
	CHumanBlending()
	{
		m_arrAbsDesiredBodyDirection.resize(NUMMD);
		for (uint32 i=0; i<NUMMD; i++)
			m_arrAbsDesiredBodyDirection[i]=Vec3(0,1,0);

		m_arrAbsDesiredMoveDirection.resize(NUMMD);
		for (uint32 i=0; i<NUMMD; i++)
			m_arrAbsDesiredMoveDirection[i]=Vec3(0,1,0);


		m_OldYawRadiant=0.0f;
		m_OldLeftRightParam=0.0f;

		m_arrMotionPath.resize(0x100);
		for (uint32 i=0; i<0x100; i++)
			m_arrMotionPath[i]=Vec3(ZERO);

		m_smoothStrafeParam = 0.0f;
	}

	std::vector<Vec3> m_arrAbsDesiredBodyDirection;
	std::vector<Vec3> m_arrAbsDesiredMoveDirection;
	f32 m_OldYawRadiant;
	f32 m_OldLeftRightParam;
	
	std::vector<Vec3> m_arrMotionPath;

	f32 m_smoothStrafeParam;

	SAnimationBlendingParams m_params;
};

#endif
