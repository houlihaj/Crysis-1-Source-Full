////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   EyeMovementFaceAnim.h
//  Version:     v1.00
//  Created:     20/6/2005 by Michael S.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __EYEMOVEMENTFACEANIM_H__
#define __EYEMOVEMENTFACEANIM_H__

class CFacialInstance;
class CSkinInstance;
class CFacialEffectorsLibrary;
class CFaceState;

class CEyeMovementFaceAnim : public _i_reference_target_t
{
public:
	CEyeMovementFaceAnim(CFacialInstance* pInstance);

	void Update(float fDeltaTimeSec, const QuatTS& rAnimLocationNext, CSkinInstance* pCharacter, CFacialEffectorsLibrary* pEffectorsLibrary, CFaceState* pFaceState);
	void OnExpressionLibraryLoad();

private:
	enum DirectionID
	{
		DirectionNONE = -1,

		DirectionEyeUp = 0,
		DirectionEyeUpRight,
		DirectionEyeRight,
		DirectionEyeDownRight,
		DirectionEyeDown,
		DirectionEyeDownLeft,
		DirectionEyeLeft,
		DirectionEyeUpLeft,

		DirectionCOUNT
	};

	enum EyeID
	{
		EyeLeft,
		EyeRight,

		EyeCOUNT
	};

	enum EffectorID
	{
		EffectorEyeLeftDirectionEyeUp,
		EffectorEyeLeftDirectionEyeUpRight,
		EffectorEyeLeftDirectionEyeRight,
		EffectorEyeLeftDirectionEyeDownRight,
		EffectorEyeLeftDirectionEyeDown,
		EffectorEyeLeftDirectionEyeDownLeft,
		EffectorEyeLeftDirectionEyeLeft,
		EffectorEyeLeftDirectionEyeUpLeft,
		EffectorEyeRightDirectionEyeUp,
		EffectorEyeRightDirectionEyeUpRight,
		EffectorEyeRightDirectionEyeRight,
		EffectorEyeRightDirectionEyeDownRight,
		EffectorEyeRightDirectionEyeDown,
		EffectorEyeRightDirectionEyeDownLeft,
		EffectorEyeRightDirectionEyeLeft,
		EffectorEyeRightDirectionEyeUpLeft,

		EffectorCOUNT
	};

	EffectorID EffectorFromEyeAndDirection(EyeID eye, DirectionID direction)
	{
		assert(eye >= 0 && eye < EyeCOUNT);
		assert(direction >= 0 && direction < DirectionCOUNT);
		return static_cast<EffectorID>(eye * DirectionCOUNT + direction);
	}

	EyeID EyeFromEffector(EffectorID effector)
	{
		assert(effector >= 0 && effector < EffectorCOUNT);
		return static_cast<EyeID>(effector / DirectionCOUNT);
	}

	DirectionID DirectionFromEffector(EffectorID effector)
	{
		assert(effector >= 0 && effector < EffectorCOUNT);
		return static_cast<DirectionID>(effector % DirectionCOUNT);
	}

	void InitialiseChannels();
	uint32 GetChannelForEffector(EffectorID effector);
	uint32 CreateChannelForEffector(EffectorID effector);
	void UpdateEye(const QuatTS& rAnimLocationNext, EyeID eye, const QuatT& additionalRotation);
	DirectionID FindEyeDirection(EyeID eye);
	void InitialiseBoneIDs();
	void FindLookAngleAndStrength(EyeID eye, float& angle, float& strength, const QuatT& additionalRotation);
	void DisplayDebugInfoForEye(const QuatTS& rAnimLocationNext, EyeID eye, const string& text);
	void CalculateEyeAdditionalRotation(CSkinInstance* pCharacter, CFaceState* pFaceState, CFacialEffectorsLibrary* pEffectorsLibrary, QuatT additionalRotation[EyeCOUNT]);

	CFacialInstance* m_pInstance;
	static const char* szEffectorNames[EffectorCOUNT];
	static const char* szEyeBoneNames[EyeCOUNT];

	class CChannelEntry
	{
	public:
		CChannelEntry(): id(~0), loadingAttempted(false) {}
		int id;
		bool loadingAttempted;
	};

	CChannelEntry m_channelEntries[EffectorCOUNT];
	int m_eyeBoneIDs[EyeCOUNT];
	bool m_bInitialized;
};

#endif //__EYEMOVEMENTFACEANIM_H__
