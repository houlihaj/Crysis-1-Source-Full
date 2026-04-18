////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	10/9/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  default-modelskeleton of the model   
/////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CRY_MODELSKELETON
#define _CRY_MODELSKELETON

#include "ModelJoint.h"      //embedded



enum EIdxMap
{
	eIM_BipIdx,
	eIM_PelvisIdx,
	eIM_Spine0Idx,
	eIM_Spine1Idx,
	eIM_Spine2Idx,
	eIM_Spine3Idx,
	eIM_NeckIdx,
	eIM_HeadIdx,

	eIM_LeftEyeIdx,
	eIM_RightEyeIdx,

	eIM_RClavicleIdx,
	eIM_RUpperArmIdx,
	eIM_RForeArmIdx,
	eIM_RHandIdx,
	eIM_WeaponBoneIdx,

	eIM_LClavicleIdx,
	eIM_LUpperArmIdx,
	eIM_LForeArmIdx,
	eIM_LHandIdx,
	eIM_LHandFinger2Idx,

	eIM_LThighIdx,
	eIM_LKneeIdx,
	eIM_LKneeEndIdx,
	eIM_LCalfIdx,
	eIM_LFootIdx,
	eIM_LHeelIdx,
	eIM_LToe0Idx,
	eIM_LToe0NubIdx,

	eIM_RThighIdx,
	eIM_RKneeIdx,
	eIM_RKneeEndIdx,
	eIM_RCalfIdx,
	eIM_RFootIdx,
	eIM_RHeelIdx,
	eIM_RToe0Idx,
	eIM_RToe0NubIdx,

	eIM_HunterRootIdx,
	eIM_HunterFrontLegL01Idx,
	eIM_HunterFrontLegL12Idx,
	eIM_HunterFrontLegR01Idx,
	eIM_HunterFrontLegR12Idx,
	eIM_HunterBackLegL01Idx,
	eIM_HunterBackLegL13Idx,
	eIM_HunterBackLegR01Idx,
	eIM_HunterBackLegR13Idx,

	// insert new value id BEFORE last
	eIM_Last
};



class CModelSkeleton : public _reference_target_t
{
	static const int ELastValue = eIM_Last;

public:

	CModelSkeleton()
	{
		memset(m_IdxArray, -1, sizeof(m_IdxArray));
	}

	void SetIKJointID();

	CModelJoint* GetParent (uint32 i) 
	{
		int32 pidx = m_arrModelJoints[i].m_idxParent;
		if (pidx>=0)
			return &m_arrModelJoints[pidx];
		else
			return 0;
	}

	// return bone index from the index map
	inline uint16 GetIdx(EIdxMap val)
	{
		return m_IdxArray[val];
	}

	uint32 SizeOfThis()
	{
		uint32 nSize = sizeof(CModelSkeleton) + m_JointMap.GetMapSize();
		for (uint32 i = 0, end = m_arrModelJoints.size(); i < end; ++i)
		{
			nSize += m_arrModelJoints[i].SizeOfThis();
		}

		return nSize;
	}

	// helper function. return has legs model or not.
	bool IsHuman();

	// This is the bone hierarchy. All the bones of the hierarchy are present in this array
	std::vector<CModelJoint> m_arrModelJoints;

	CNameCRCMap	m_JointMap;

	int16	m_IdxArray[ELastValue]; //Index list with important joints. Needed for IK
	std::vector<int16>	m_arrRThighChilds;
	std::vector<int16>	m_arrLThighChilds;
	std::vector<int16>	m_arrRShoulderChilds;
	std::vector<int16>	m_arrLShoulderChilds;
	std::vector<int16>	m_arrMirrorJoints;
};



#endif
