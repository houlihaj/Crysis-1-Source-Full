////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FacialInstance.h
//  Version:     v1.00
//  Created:     18/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FacialInstance_h__
#define __FacialInstance_h__
#pragma once

#include "FaceEffector.h"
#include "FaceState.h"
#include "FaceAnimation.h"
#include "EyeMovementFaceAnim.h"
class CCharInstance;

//////////////////////////////////////////////////////////////////////////
// Instance of facial animation model.
//////////////////////////////////////////////////////////////////////////
class CFacialInstance : public IFacialInstance, public _i_reference_target_t
{
public:
	CFacialInstance( CFacialModel* pModel, CSkinInstance* pChar,CCharInstance* pMaster );

	//////////////////////////////////////////////////////////////////////////
	// IFacialInstance interface
	//////////////////////////////////////////////////////////////////////////
	virtual IFacialModel* GetFacialModel();
	virtual IFaceState*   GetFaceState();

	virtual uint32 StartEffectorChannel(IFacialEffector* pEffector,float fWeight,float fFadeTime,float fLifeTime=0,int nRepeatCount=0);
	virtual void StopEffectorChannel(uint32 nChannelID, float fFadeOutTime);
	virtual void PreviewEffector( IFacialEffector *pEffector,float fWeight,float fBalance=0.0f );
	virtual void PreviewEffectors( IFacialEffector **pEffectors,float *fWeights,float *fBalances,int nEffectorsCount );

	virtual IFacialAnimSequence *LoadSequence( const char *sSequenceName, bool addToCache=true );
	virtual void PrecacheFacialExpression(const char *sSequenceName);
	virtual void PlaySequence( IFacialAnimSequence *pSequence,EFacialSequenceLayer layer,bool bExclusive=false,bool bLooping=false );
	virtual void StopSequence( EFacialSequenceLayer layer );
	virtual bool IsPlaySequence( IFacialAnimSequence *pSequence, EFacialSequenceLayer layer );
	virtual void PauseSequence( EFacialSequenceLayer layer, bool bPaused );
	virtual void SeekSequence( EFacialSequenceLayer layer,float fTime );
	virtual void LipSyncWithSound( uint32 nSoundId, bool bStop=false );
	virtual void OnExpressionLibraryLoad();

	virtual void EnableProceduralFacialAnimation( bool bEnable );

	virtual void SetForcedRotations(int numForcedRotations, CFacialAnimForcedRotationEntry* forcedRotations);

	virtual void SetMasterCharacter(ICharacterInstance* pMasterInstance);
	virtual void TemporarilyEnableBoneRotationSmoothing();
	//////////////////////////////////////////////////////////////////////////

	void StopAllSequences();

	CFacialModel* GetModel() const { return m_pModel; }
	CFaceState* GetState() { return m_pFaceState; }
	CSkinInstance* GetCharacter() const { return m_pCharacter; }
	CCharInstance* GetMasterCharacter() const { return m_pMasterInstance; }

	CFacialAnimationContext* GetAnimContext() { return m_pAnimContext; }
	IFacialEffector* FindEffector( const char *sName );

	// Called every time facial animation needs to be recalculated.
	void UpdatePlayingSequences( float fDeltaTimeSec, const QuatTS& rAnimLocationNext );
	void Update( float fDeltaTimeSec, const QuatTS& rAnimLocationNext );
	
	void ApplyProceduralFaceBehaviour( Vec3 &vCamPos );
	void UpdateProceduralFaceBehaviour();

	uint32 SizeOfThis()
	{
		uint32 size = sizeof(CFacialInstance);
		size += m_forcedRotations.capacity() * sizeof(CFacialAnimForcedRotationEntry);
		if (m_pFaceState)
			size += m_pFaceState->SizeOfThis();

		return size;
	}

private:
	void Reset();
	void UpdateCurrentSequence(IFacialAnimSequence* pPreviousSequence);

	struct LayerInfo
	{
		LayerInfo(): sequence(0), bExclusive(false), bLooping(false) {}
		LayerInfo(IFacialAnimSequence* sequence, bool bExclusive, bool bLooping)
			: sequence(sequence), bExclusive(bExclusive), bLooping(bLooping) {}

		_smart_ptr<IFacialAnimSequence> sequence;
		bool bExclusive;
		bool bLooping;
	};

	LayerInfo m_layers[eFacialSequenceLayer_COUNT];
	int m_currentLayer;

	CSkinInstance *m_pCharacter; // Character for this face instance.
	CCharInstance* m_pMasterInstance; // == m_pCharacter, unless char is attachment, then master attachment.
	_smart_ptr<CFacialModel> m_pModel;

	_smart_ptr<CFaceState> m_pFaceState;
	_smart_ptr<CFacialAnimationContext> m_pAnimContext;
	_smart_ptr<CEyeMovementFaceAnim> m_pEyeMovement;

	//////////////////////////////////////////////////////////////////////////
	struct tProceduralFace
	{
		Vec3	m_JitterEyesPositions[4];
		int		m_nCurrEyesJitter;
		float	m_fEyesJitterScale;
		float	m_fLastJitterTime;
		float	m_fJitterTimeChange;
		float	m_fBlinkingTime;
		_smart_ptr<CFacialEffector> m_pBlink;
		bool	m_bEyesBlinking;
		bool	m_bEnabled;
	};
	tProceduralFace m_tProcFace;

	DynArray<CFacialAnimForcedRotationEntry> m_forcedRotations;
};


#endif // __FacialInstance_h__
