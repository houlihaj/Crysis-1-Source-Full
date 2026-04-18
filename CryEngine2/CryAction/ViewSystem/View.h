/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: View System interfaces.
  
 -------------------------------------------------------------------------
  History:
  - 24:9:2004 : Created by Filippo De Luca

*************************************************************************/
#ifndef __VIEW_H__
#define __VIEW_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IViewSystem.h"

class CGameObject;

class CView : public IView
{
public:

	CView(ISystem *pSystem);
	virtual ~CView();

	//shaking
	struct SShake
	{
		bool updating;
		bool flip;
		bool doFlip;
		bool groundOnly;
		
		int ID;

		float nextShake;
		float duration;
		float maxDuration;

		float frequency;
		float ratio;

		float randomness;
		
		Quat goalShake;
		Vec3 goalShakeVector;
				
		Ang3 amount;
		Vec3 amountVector;

		Quat shakeQuat;
		Vec3 shakeVector;

		SShake(int shakeID)
		{
			memset(this,0,sizeof(SShake));

			goalShake.SetIdentity();
			shakeQuat.SetIdentity();

			randomness = 0.5f;

			ID = shakeID;
		}
	};


	// IView
	virtual void Update(float frameTime, bool isActive);
	virtual void ProcessShaking(float frameTime);
	virtual void ProcessShake(SShake *pShake,float frameTime);
	virtual void ResetShaking();
	//FIXME: keep CGameObject *  or use IGameObject *?
	virtual void LinkTo(IGameObject *follow);
	virtual void LinkTo(IEntity* follow);
	virtual unsigned int GetLinkedId() {return m_linkedTo;};
	virtual void SetCurrentParams( SViewParams &params ) { m_viewParams = params; };
	virtual const SViewParams * GetCurrentParams() {return &m_viewParams;}
	virtual void SetViewShake(Ang3 shakeAngle,Vec3 shakeShift,float duration,float frequency,float randomness,int shakeID, bool bFlipVec = true, bool bUpdateOnly=false, bool bGroundOnly=false);
	// ~IView

	void Serialize(TSerialize ser);
	CCamera& GetCamera() { return m_camera; };

	void GetMemoryStatistics(ICrySizer * s);

protected:
	CGameObject * GetLinkedGameObject();
	IEntity* GetLinkedEntity();

	bool m_active;
	EntityId m_linkedTo;
	
	SViewParams m_viewParams;
	CCamera m_camera;

	ISystem *m_pSystem;
	
	std::vector<SShake> m_shakes;
};

#endif //__VIEW_H__
