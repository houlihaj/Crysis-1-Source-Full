////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FacialModel.h
//  Version:     v1.00
//  Created:     10/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FacialModel_h__
#define __FacialModel_h__
#pragma once

#include "FaceEffector.h"

class CCharacterModel;
class CFacialEffectorsLibrary;
class CSkinInstance;
class CCharInstance;
class CFaceState;

//////////////////////////////////////////////////////////////////////////
class CFacialModel : public IFacialModel, public _i_reference_target_t
{
public:
	struct SEffectorInfo
	{
		_smart_ptr<CFacialEffector> pEffector;
		string name;        // morph target name.
		int nMorphTargetId; // positive if effector is a morph target.
	};

	struct SMorphTargetInfo
	{
		string name;
		int nMorphTargetId;
		bool operator<( const SMorphTargetInfo &m ) const { return name < m.name; }
		bool operator>( const SMorphTargetInfo &m ) const { return name > m.name; }
		bool operator==( const SMorphTargetInfo &m ) const { return name == m.name; }
		bool operator!=( const SMorphTargetInfo &m ) const { return name != m.name; }
	};

	CFacialModel( CCharacterModel *pModel );

	size_t SizeOfThis();

	//////////////////////////////////////////////////////////////////////////
	// IFacialModel interface
	//////////////////////////////////////////////////////////////////////////
	int GetEffectorCount() const { return m_effectors.size(); }
	CFacialEffector* GetEffector( int nIndex ) const { return m_effectors[nIndex].pEffector; }
	virtual void AssignLibrary( IFacialEffectorsLibrary *pLibrary );
	virtual IFacialEffectorsLibrary* GetLibrary();
	virtual int GetMorphTargetCount() const;
	virtual const char* GetMorphTargetName(int morphTargetIndex) const;
	//////////////////////////////////////////////////////////////////////////

	// Creates a new FaceState.
	CFaceState* CreateState();

	SEffectorInfo* GetEffectorsInfo() { return &m_effectors[0]; }
	CFacialEffectorsLibrary* GetFacialEffectorsLibrary() {return m_pEffectorsLibrary;}

	//////////////////////////////////////////////////////////////////////////
	// Apply state to the specified character instance.
	void ApplyFaceStateToCharacter( CFaceState* pFaceState,CSkinInstance *pCharacter, CCharInstance* pMasterCharacter, int numForcedRotations, CFacialAnimForcedRotationEntry* forcedRotations, bool blendBoneRotations);

private:
	void InitMorphTargets();

	void ApplyAttachmentEffector( CSkinInstance *pCharacter,CFacialEffector *pEffector,float fWeight );
	void ApplyBoneEffector( CCharInstance* pCharacter,CFacialEffector *pEffector,float fWeight,QuatT* additionalRotPos );

private:
	// Array of facial effectors that apply to this model.
	std::vector<SEffectorInfo> m_effectors;

	_smart_ptr<CFacialEffectorsLibrary> m_pEffectorsLibrary;

	// Maps indices from the face state into the morph target id.
	std::vector<int> m_faceStateIndexToMorphTargetId;
	typedef std::vector<SMorphTargetInfo> MorphTargetsInfo;
	MorphTargetsInfo m_morphTargets;

	// Character model who owns this facial model.
	CCharacterModel *m_pModel;

	bool m_bAttachmentChanged;
};


#endif // __FacialModel_h__
