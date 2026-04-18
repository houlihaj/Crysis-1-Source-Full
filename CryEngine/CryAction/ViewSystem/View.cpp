/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 20:9:2004 : Created by Filippo De Luca

*************************************************************************/
#include "StdAfx.h"

#include <Cry_Camera.h>
#include "View.h"
#include "GameObjects/GameObject.h"

//FIXME:not very pretty
static ICVar *pCamShakeMult = 0;

//------------------------------------------------------------------------
CView::CView(ISystem *pSystem) :
	m_pSystem(pSystem),
	m_linkedTo(0)
{
	if (!pCamShakeMult)
		pCamShakeMult = gEnv->pConsole->GetCVar("c_shakeMult");
}

//------------------------------------------------------------------------
CView::~CView()
{
}

//------------------------------------------------------------------------
void CView::Update(float frameTime,bool isActive)
{
	//FIXME:some cameras may need to be updated always
	if (!isActive)
		return;

	CGameObject * pLinkedTo = GetLinkedGameObject();
	IEntity* pEntity = pLinkedTo ? 0 : GetLinkedEntity();

	if (pLinkedTo || pEntity)
	{
		m_viewParams.SaveLast();

		CCamera *pSysCam = &m_pSystem->GetViewCamera();

		//process screen shaking
		ProcessShaking(frameTime);
		
		//FIXME:to let the updateView implementation use the correct shakeVector
		m_viewParams.currentShakeShift = m_viewParams.rotation * m_viewParams.currentShakeShift;
		
    m_viewParams.frameTime=frameTime;
		//update view position/rotation
		if (pLinkedTo)
		{
			pLinkedTo->UpdateView(m_viewParams);
		}
		else
		{
			const Matrix34& mat = pEntity->GetWorldTM();
			m_viewParams.position = mat.GetTranslation();
			m_viewParams.rotation = Quat(mat);
		}

		m_viewParams.UpdateBlending(frameTime);

		if (pLinkedTo)
			pLinkedTo->PostUpdateView(m_viewParams);

		float fNearZ  = gEnv->pGame->GetIGameFramework()->GetIViewSystem()->GetDefaultZNear();
		
		//see if the view have to use a custom near clipping plane
		float nearPlane = (m_viewParams.nearplane > 0.01f)?(m_viewParams.nearplane):(fNearZ/*pSysCam->GetNearPlane()*/);
		float farPlane = m_pSystem->GetI3DEngine()->GetMaxViewDistance();
		float fov = m_viewParams.fov < 0.001 ? DEFAULT_FOV : m_viewParams.fov;

		m_camera.SetFrustum(pSysCam->GetViewSurfaceX(),pSysCam->GetViewSurfaceZ(),fov,nearPlane,farPlane);

		//apply shake & set the view matrix
		m_viewParams.rotation *= m_viewParams.currentShakeQuat;
		m_viewParams.rotation.Normalize();
		m_viewParams.position += m_viewParams.currentShakeShift;

		Matrix34 viewMtx(m_viewParams.rotation);
		viewMtx.SetTranslation(m_viewParams.position);
		m_camera.SetMatrix(viewMtx);
	}
	else
	{
		m_linkedTo = 0;
		m_camera.SetPosition(Vec3(1,1,1));
	}
}

#define RANDOM() ((((float)cry_rand()/(float)RAND_MAX)*2.0f)-1.0f)

void CView::SetViewShake(Ang3 shakeAngle,Vec3 shakeShift,float duration,float frequency,float randomness,int shakeID, bool bFlipVec, bool bUpdateOnly, bool bGroundOnly)
{
	float shakeMult(pCamShakeMult->GetFVal());
	if (shakeMult<0.001f)
		return;

	int shakes(m_shakes.size());
	SShake *pSetShake(NULL);

	for (int i=0;i<shakes;++i)
	{
		SShake *pShake = &m_shakes[i];
		if (pShake->ID == shakeID)
		{
			pSetShake = pShake;
			break;
		}
	}

	if (!pSetShake)
	{
		m_shakes.push_back(SShake(shakeID));
		pSetShake = &m_shakes.back();
	}

	if (pSetShake)
	{
		// this can be set dynamically
		pSetShake->frequency = frequency;

		// the following are set on a 'new' shake as well
		if (bUpdateOnly == false)
		{
			pSetShake->amount = shakeAngle * shakeMult;
			pSetShake->amountVector = shakeShift * shakeMult;
			pSetShake->duration = duration;
			pSetShake->randomness = randomness;
			pSetShake->doFlip = bFlipVec;
			pSetShake->groundOnly = bGroundOnly;
		}
	}
}

void CView::ProcessShaking(float frameTime)
{
	m_viewParams.currentShakeQuat.SetIdentity();
	m_viewParams.currentShakeShift.zero();
	m_viewParams.shakingRatio = 0;
	m_viewParams.groundOnly = false;

	int shakes(m_shakes.size());
	for (int i=0;i<shakes;++i)
		ProcessShake(&m_shakes[i],frameTime);
}

void CView::ProcessShake(SShake *pShake,float frameTime)
{
	if (pShake->duration <= 0.0f)
	{
		if (pShake->updating)
		{
			pShake->shakeQuat = Quat::CreateSlerp(pShake->shakeQuat,IDENTITY,frameTime * 5.0f);
			m_viewParams.currentShakeQuat *= pShake->shakeQuat;

			pShake->shakeVector = Vec3::CreateLerp(pShake->shakeVector,ZERO,frameTime * 5.0f);
			m_viewParams.currentShakeShift += pShake->shakeVector;

			float svlen2(pShake->shakeVector.len2());
			bool quatIsIdentity(pShake->shakeQuat.IsEquivalent(IDENTITY,0.0001f));
						
			if (quatIsIdentity && svlen2<0.01f)
			{
				pShake->shakeQuat.SetIdentity();
				pShake->shakeVector.zero();

				pShake->ratio = 0.0f;
				pShake->maxDuration = 0.0f;
				pShake->nextShake = 0.0f;
				pShake->flip = false;

				pShake->updating = false;
			}
		}
	}
	else
	{
		pShake->updating = true;

		//first or new shake?
		if (pShake->ratio < 0.001f || pShake->duration > pShake->maxDuration)
		{
			pShake->maxDuration = pShake->duration;
		}
		
		pShake->ratio = 1.0f - (max(0.0f,pShake->maxDuration - pShake->duration) / pShake->maxDuration);

		float t;
		if (pShake->nextShake <= 0.0f)
		{
			//angular
			pShake->goalShake.SetRotationXYZ(pShake->amount);
			if (pShake->flip)
				pShake->goalShake.Invert();

			//translational
			pShake->goalShakeVector = pShake->amountVector;
			if (pShake->flip)
				pShake->goalShakeVector = -pShake->goalShakeVector;

			if (pShake->doFlip)
				pShake->flip = !pShake->flip;

			//randomize it a little
			float randomAmt(pShake->randomness);
			float len(fabs(pShake->amount.x) + fabs(pShake->amount.y) + fabs(pShake->amount.z));
			len /= 3.0f;
			pShake->goalShake *= Quat::CreateRotationXYZ(Ang3(RANDOM()*len*randomAmt,RANDOM()*len*randomAmt,RANDOM()*len*randomAmt));

			//translational randomization
			len = fabs(pShake->amountVector.x) + fabs(pShake->amountVector.y) + fabs(pShake->amountVector.z);
			len /= 3.0f;
			pShake->goalShakeVector += Vec3(RANDOM()*len*randomAmt,RANDOM()*len*randomAmt,RANDOM()*len*randomAmt);

			//damp & bounce it in a non linear fashion
			t = 1.0f-(pShake->ratio*pShake->ratio);
			pShake->goalShake = Quat::CreateSlerp(pShake->goalShake,IDENTITY,t);
			pShake->goalShakeVector = Vec3::CreateLerp(pShake->goalShakeVector,ZERO,t);

			pShake->nextShake = pShake->frequency;
		}

		pShake->duration = max(0.0f,pShake->duration - frameTime);
		pShake->nextShake = max(0.0f,pShake->nextShake - frameTime);

		t = min(1.0f,frameTime * (1.0f/pShake->frequency));
		pShake->shakeQuat = Quat::CreateSlerp(pShake->shakeQuat,pShake->goalShake,t);
		pShake->shakeQuat.Normalize();
		pShake->shakeVector = Vec3::CreateLerp(pShake->shakeVector,pShake->goalShakeVector,t);

		//for the global shaking ratio keep the biggest
		if (pShake->groundOnly)
			m_viewParams.groundOnly = true;
		m_viewParams.shakingRatio = max(m_viewParams.shakingRatio,pShake->ratio);
		m_viewParams.currentShakeQuat *= pShake->shakeQuat;
		m_viewParams.currentShakeShift += pShake->shakeVector;
	}
}

//------------------------------------------------------------------------
void CView::ResetShaking()
{
	// disable shakes
	std::vector<SShake>::iterator iter = m_shakes.begin();
	std::vector<SShake>::iterator iterEnd = m_shakes.end();
	while (iter != iterEnd)
	{
		SShake& shake = *iter;
		shake.updating = false;
		shake.duration = -1.0f;
		++iter;
	}
}

//------------------------------------------------------------------------
void CView::LinkTo(IGameObject *follow)
{
	m_linkedTo = follow->GetEntityId();
}

//------------------------------------------------------------------------
void CView::LinkTo(IEntity *follow)
{
	m_linkedTo = follow->GetId();
}

//------------------------------------------------------------------------
CGameObject * CView::GetLinkedGameObject()
{
	IEntity * pEntity = gEnv->pEntitySystem->GetEntity( m_linkedTo );

	if (!pEntity) 
		return NULL;

	return (CGameObject*) pEntity->GetProxy( ENTITY_PROXY_USER );
}

//------------------------------------------------------------------------
IEntity * CView::GetLinkedEntity()
{
	IEntity * pEntity = gEnv->pEntitySystem->GetEntity( m_linkedTo );
	return pEntity;
}

void CView::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->Add(m_shakes);
}

void CView::Serialize(TSerialize ser)
{
	if (ser.IsReading())
	{
		ResetShaking();
	}
}