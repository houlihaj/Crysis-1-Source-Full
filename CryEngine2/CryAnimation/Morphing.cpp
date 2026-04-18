//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:Morphing.cpp
//  Implementation of the Morphing class
//
//	History:
//	June 16, 2007: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <CryHeaders.h>
#include "FacialAnimation/FacialInstance.h"

#include "Model.h"
#include "CharacterInstance.h"


void CMorphing::InitMorphing( CCharacterModel* pModel )
{

	m_pModel=pModel;
	m_LinearMorphSequence=-1;
	m_fMorphVertexDrag = 1.0f;
	assert(m_pModel);
}



//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//! Start the specified by parameters morph target
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void CMorphing::StartMorph (const char* szMorphTarget,const CryCharMorphParams& Params)
{
	RunMorph (szMorphTarget, Params);
}
//! Start the specified by parameters morph target
void CMorphing::StartMorph (int nMorphTargetId, const CryCharMorphParams& params)
{
	RunMorph (nMorphTargetId, params);
}



//! Finds the morph with the given id and sets its relative time.
//! Returns false if the operation can't be performed (no morph)
bool CMorphing::SetMorphTime (const char* szMorphTarget, f32 fTime)
{
	assert(m_pModel);
	CModelMesh* pModelMesh = m_pModel->GetModelMesh(0);
	int32 nMorphTargetId = pModelMesh->FindMorphTarget(szMorphTarget);
	if ( nMorphTargetId<0 )
	{
		AnimWarning("SetMorphTime: Morph Target \"%s\" Not Found for character \"%s\"", szMorphTarget, m_pModel->GetModelFilePath()); 
		return 0;
	}

	CryModEffMorph* pEffector = GetMorphTarget(nMorphTargetId);
	if (pEffector)
	{
		pEffector->setTime (fTime);
		return true;
	}
	return 0;
}


//! Finds the morph with the given id and sets its relative time.
//! Returns false if the operation can't be performed (no morph)
bool CMorphing::SetMorphTime (int nMorphTargetId, float fTime)
{
	CryModEffMorph* pEffector = GetMorphTarget(nMorphTargetId);
	if (pEffector)
	{
		pEffector->setTime (fTime);
		return true;
	}
	return false;
}


void CMorphing::RunMorph (int nMorphTargetId, const CryCharMorphParams&Params)
{
	// find the first free slot in the array of morph target, or create a new one and start morphing there
	unsigned nMorphEffector, nMorphEffectorSize = m_arrMorphEffectors.size();
	for (nMorphEffector = 0; nMorphEffector < nMorphEffectorSize; ++nMorphEffector)
	{
		if (!m_arrMorphEffectors[nMorphEffector].isActive())
		{
			m_arrMorphEffectors[nMorphEffector].StartMorph (nMorphTargetId, Params);
			return;
		}
	}
	m_arrMorphEffectors.resize (m_arrMorphEffectors.size()+1);
	m_arrMorphEffectors.back().StartMorph (nMorphTargetId, Params);
}

int32 CMorphing::RunMorph (const char* szMorphTarget,const CryCharMorphParams&Params,bool bShowNotFoundWarning)
{
	assert(szMorphTarget);

	assert(m_pModel);
	CModelMesh* pSkinning = m_pModel->GetModelMesh(0);
	int32 nMorphTargetId = pSkinning->FindMorphTarget(szMorphTarget);
	if ( nMorphTargetId>=0)
	{
		RunMorph (nMorphTargetId, Params);
		return nMorphTargetId;
	}
	return nMorphTargetId;
}

void CMorphing::UpdateMorphEffectors (CFacialInstance* pFacialInstance, float fDeltaTimeSec, const QuatTS& rAnimLocationNext, float fZoomAdjustedDistanceFromCamera )
{
	if (pFacialInstance)
		pFacialInstance->UpdatePlayingSequences(fDeltaTimeSec, rAnimLocationNext);

	uint32 numMorphTargets = m_arrMorphEffectors.size();

	for (uint32 nMorphTarget=0; nMorphTarget<numMorphTargets; ++nMorphTarget)
	{
		CryModEffMorph& rMorph = m_arrMorphEffectors[nMorphTarget];
		rMorph.Tick(fDeltaTimeSec);
	}

	// clean up the array of morph targets from unused ones
	while ( !m_arrMorphEffectors.empty() && !m_arrMorphEffectors.back().isActive() )
		m_arrMorphEffectors.pop_back();

	// Only update facial animation if we are close - this will add morph effectors to the list
	// that will last only one frame. Therefore if we don't update them, the morphs will be
	// removed automatically.
	if (fZoomAdjustedDistanceFromCamera < Console::GetInst().ca_FacialAnimationRadius)
	{
		if (pFacialInstance)
			pFacialInstance->Update( fDeltaTimeSec, rAnimLocationNext );
	}
}


bool CMorphing::StopMorph (const char* szMorphTarget)
{
	assert(m_pModel);
	CModelMesh* pModelMesh = m_pModel->GetModelMesh(0);
	int32 nMorphTargetId = pModelMesh->FindMorphTarget(szMorphTarget);
	unsigned numStopped = 0;
	if (nMorphTargetId>=0) 
	{
		for (;;)
		{
			CryModEffMorph* pEffector = GetMorphTarget(nMorphTargetId);
			if (!pEffector)
				break;
			pEffector->stop();
			++numStopped;
		}
	}

	return numStopped > 0;
}


void CMorphing::StopAllMorphs()
{
	m_arrMorphEffectors.clear();
}


//! Set morph speed scale
//! Finds the morph target with the given id, sets its morphing speed and returns true;
//! if there's no such morph target currently playing, returns false
bool CMorphing::SetMorphSpeed (const char* szMorphTarget, float fSpeed)
{
	CModelMesh* pModelMesh = m_pModel->GetModelMesh(0);
	int32 nMorphTargetId = pModelMesh->FindMorphTarget(szMorphTarget);
	if ( nMorphTargetId>=0 )
	{
		CryModEffMorph* pEffector = GetMorphTarget(nMorphTargetId);
		if (pEffector)
			pEffector->setSpeed (fSpeed);
	}
	return true;
}

//returns the morph target effector, or NULL if no such effector found
CryModEffMorph* CMorphing::GetMorphTarget(int nMorphTargetId)
{
	unsigned nMorphEffectorSize = m_arrMorphEffectors.size();
	for (uint32 nMorphEffector=0; nMorphEffector < nMorphEffectorSize; ++nMorphEffector)
	{
		if (m_arrMorphEffectors[nMorphEffector].getMorphTargetId() == nMorphTargetId)
			return &m_arrMorphEffectors[nMorphEffector];
	}
	return NULL;
}



//! freezes all currently playing morphs at the point they're at
void CMorphing::FreezeAllMorphs()
{
	size_t numMorphEffectors = m_arrMorphEffectors.size();
	for (uint32 i=0; i<numMorphEffectors; i++)
		m_arrMorphEffectors[i].freeze();
}
