////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	10/9/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  default-skeleton of the model   
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ModelSkeleton.h"
#include <StlUtils.h>
#include "StringUtils.h"





CModelJoint::~CModelJoint()
{
	IPhysicalWorld *pIPhysicalWorld = g_pIPhysicalWorld;
	IGeomManager* pPhysGeomManager = pIPhysicalWorld?pIPhysicalWorld->GetGeomManager():NULL;

	for(int nLod=0; nLod<2; nLod++)
	{
		if (m_PhysInfo[nLod].pPhysGeom && (INT_PTR)m_PhysInfo[nLod].pPhysGeom!=-1) 
		{
			if ((INT_PTR)m_PhysInfo[nLod].pPhysGeom<0x400) 
			{
				//CryWarning(VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,"Error: trying to release wrong bone phys geometry");
			} 
			else 
				if (pPhysGeomManager)
				{
					phys_geometry* pPhysGeom = m_PhysInfo[nLod].pPhysGeom;
					pPhysGeomManager->UnregisterGeometry(pPhysGeom);
				}
				else
					CryWarning(VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,"todo: delete bones phys");
		}
	}
}



// scales the bone with the given multiplier
void CModelJoint::scale (f32 fScale)
{
	assert(0);
}



size_t CModelJoint::SizeOfThis() const
{
	unsigned nSize = sizeof(CModelJoint);
//	nSize += sizeofVector(m_arrControllersMJoint);
	return nSize;
}





//////////////////////////////////////////////////////////////////////////
// Updates the given lod level bone physics info from the bones found in
// the given chunk.
// THis is required to update the dead body physics info from the lod1 geometry
// without rebuilding the new bone structure. If the lod1 has another bone structure,
// then the bones are mapped to the lod0 ones using controller ids. Matching bones'
// physics info is updated
//////////////////////////////////////////////////////////////////////////
void CModelJoint::UpdateHierarchyPhysics( DynArray<BONE_ENTITY> arrBoneEntities, int nLod )
{

	UnsignedToCryBoneMap mapCtrlId;
	AddHierarchyToControllerIdMap(mapCtrlId);

	uint32 numBoneEntities = arrBoneEntities.size();

	// update physics for each bone entity
	for (uint32 i=0; i<numBoneEntities; i++)
	{
		CModelJoint* pBone = stl::find_in_map(mapCtrlId, arrBoneEntities[i].ControllerID, (CModelJoint*)NULL);
		if (pBone)
		{
			if (nLod==1)
				CopyPhysInfo(pBone->m_PhysInfo[nLod], arrBoneEntities[i].phys);

			int nFlags = 0;
			if (!arrBoneEntities[i].prop[0])
			{
				nFlags = joint_no_gravity|joint_isolated_accelerations;
			}
			else
			{
				if (!CryStringUtils::strnstr(arrBoneEntities[i].prop,"gravity", sizeof(arrBoneEntities[i].prop)))
					nFlags |= joint_no_gravity;
				if (!CryStringUtils::strnstr(arrBoneEntities[i].prop,"active_phys",sizeof(arrBoneEntities[i].prop)))
					nFlags |= joint_isolated_accelerations;
			}
			(pBone->m_PhysInfo[nLod].flags &= ~(joint_no_gravity|joint_isolated_accelerations)) |= nFlags;
		}

	}

}


//////////////////////////////////////////////////////////////////////////
// Updates the given lod level bone physics info from the bones found in
// the given chunk.
// THis is required to update the dead body physics info from the lod1 geometry
// without rebuilding the new bone structure. If the lod1 has another bone structure,
// then the bones are mapped to the lod0 ones using controller ids. Matching bones'
// physics info is updated
void CModelJoint::UpdateHierarchyPhysics (const BONEANIM_CHUNK_DESC* pChunk, unsigned nChunkSize, int nLodLevel)
{
	UnsignedToCryBoneMap mapCtrlId;
	AddHierarchyToControllerIdMap(mapCtrlId);

	// the first bone entity
	const BONE_ENTITY* pBoneEntity = (const BONE_ENTITY*)(pChunk+1);
	// the actual end of the chunk
	const BONE_ENTITY* pBoneEntityEnd = (const BONE_ENTITY*)(((const char*)pChunk)+nChunkSize);

	// if you get this assert, it means the lod 1 file is corrupted
	if (pBoneEntity + pChunk->nBones > pBoneEntityEnd)
	{
		assert (0);
		return;
	}

	// update physics for each bone entity
	for (; pBoneEntity < pBoneEntityEnd; ++pBoneEntity)
	{
		CModelJoint* pBone = stl::find_in_map(mapCtrlId, pBoneEntity->ControllerID, (CModelJoint*)NULL);
		if (pBone)
		{
			//pBone->UpdatePhysics(*pBoneEntity, nLodLevel);

	//		assert (nLod >= 0 && nLod < SIZEOF_ARRAY(m_PhysInfo));
			CopyPhysInfo(pBone->m_PhysInfo[nLodLevel], pBoneEntity->phys);

			int nFlags = 0;
			if (!pBoneEntity->prop[0])
			{
				nFlags = joint_no_gravity|joint_isolated_accelerations;
			}
			else
			{
				if (!CryStringUtils::strnstr(pBoneEntity->prop,"gravity", sizeof(pBoneEntity->prop)))
					nFlags |= joint_no_gravity;

				if (!CryStringUtils::strnstr(pBoneEntity->prop,"active_phys",sizeof(pBoneEntity->prop)))
					nFlags |= joint_isolated_accelerations;
			}

			(pBone->m_PhysInfo[nLodLevel].flags &= ~(joint_no_gravity|joint_isolated_accelerations)) |= nFlags;


		}
	}
}

//////////////////////////////////////////////////////////////////////////
// adds this bone and all its children to the given map controller id-> bone ptr
//////////////////////////////////////////////////////////////////////////
void CModelJoint::AddHierarchyToControllerIdMap (UnsignedToCryBoneMap& mapControllerIdToCryBone)
{
	mapControllerIdToCryBone.insert (UnsignedToCryBoneMap::value_type(m_nJointCRC32, this));

	uint32 numChilds = numChildren();
	for (uint32 i=0; i<numChilds; i++)
		getChild(i)->AddHierarchyToControllerIdMap(mapControllerIdToCryBone);

}

//! Performs post-initialization. This step is requred to initialize the pPhysGeom of the bones
//! After the bone has been loaded but before it is first used. When the bone is first loaded, pPhysGeom
//!   is set to the value equal to the chunk id in the file where the physical geometry (BoneMesh) chunk is kept.
//! After those chunks are loaded, and chunk ids are mapped to the registered physical geometry objects,
//!   call this function to replace pPhysGeom chunk ids with the actual physical geometry object pointers.
//! RETURNS:
//    true if the corresponding physical geometry object has been found
//!	NOTE:
//!	The entries of the map that were used are deleted
bool CModelJoint::PostInitPhysGeom (ChunkIdToPhysGeomMap& mapChunkIdToPhysGeom, int nLodLevel, bool bAllowRopePhys)
{
	phys_geometry*& pPhysGeom = m_PhysInfo[nLodLevel].pPhysGeom;
	ChunkIdToPhysGeomMap::iterator it = mapChunkIdToPhysGeom.find ((INT_PTR)pPhysGeom);
	if (it != mapChunkIdToPhysGeom.end() && (bAllowRopePhys || strnicmp(GetJointName(),"rope",4))) 
	{
		// remap the chunk id to the actual pointer to the geometry
		pPhysGeom = it->second;
		mapChunkIdToPhysGeom.erase (it);
		return true;
	} 
	else
	{
		pPhysGeom = NULL;
		return false;
	}
}

void CModelJoint::PostInitialize()
{
	m_qDefaultRelPhysParent[0].SetIdentity();
	m_qDefaultRelPhysParent[1].SetIdentity();

	if (getParent())
	{
		CModelJoint *pParent,*pPhysParent;
		for(int nLod=0; nLod<2; nLod++)
		{
			for(pParent=pPhysParent=getParent(); pPhysParent && !pPhysParent->m_PhysInfo[nLod].pPhysGeom; pPhysParent=pPhysParent->getParent());
			if (pPhysParent) 
				m_qDefaultRelPhysParent[nLod] = !pParent->m_DefaultAbsPose.q * pPhysParent->m_DefaultAbsPose.q;
			else 
				m_qDefaultRelPhysParent[nLod].SetIdentity();
		}
	}

#if (EXTREME_TEST==1)
	uint32 valid0 = m_qDefaultRelPhysParent[0].IsValid();
	if (valid0==0)
		CryError("CryAnimation: m_qRelPhysParent[0] is invalid in PostInitialize" );

	uint32 valid1 = m_qDefaultRelPhysParent[1].IsValid();
	if (valid1==0)
		CryError("CryAnimation: m_qRelPhysParent[1] is invalid in PostInitialize" );
#endif

}


//---------------------------------------------------------------------------------
//---      for certain IK-operation we need special bone-configurations      ------
//---------------------------------------------------------------------------------
void CModelSkeleton::SetIKJointID()
{
	uint32 numJoints	= m_arrModelJoints.size();
	for (uint32 i=0; i<numJoints; i++)
	{
		const char * BoneName =  m_arrModelJoints[i].GetJointName();
		if (0 == stricmp(BoneName,"Bip01"))
			m_IdxArray[eIM_BipIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 Pelvis"))
			m_IdxArray[eIM_PelvisIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 Spine"))
			m_IdxArray[eIM_Spine0Idx] = i;  
		if (0 == stricmp(BoneName,"Bip01 Spine1"))
			m_IdxArray[eIM_Spine1Idx] = i; 	
		if (0 == stricmp(BoneName,"Bip01 Spine2"))
			m_IdxArray[eIM_Spine2Idx] = i;
		if (0 == stricmp(BoneName,"Bip01 Spine3"))
			m_IdxArray[eIM_Spine3Idx] = i; 	
		if (0 == stricmp(BoneName,"Bip01 Neck"))
			m_IdxArray[eIM_NeckIdx] = i; 
		if (0 == stricmp(BoneName,"Bip01 Head"))
			m_IdxArray[eIM_HeadIdx] = i;
		if (0 == stricmp(BoneName,"eye_left_bone"))
			m_IdxArray[eIM_LeftEyeIdx] = i;
		if (0 == stricmp(BoneName,"eye_right_bone"))
			m_IdxArray[eIM_RightEyeIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 R Clavicle"))
			m_IdxArray[eIM_RClavicleIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 R UpperArm"))
			m_IdxArray[eIM_RUpperArmIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 R ForeArm"))
			m_IdxArray[eIM_RForeArmIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 R Hand"))
			m_IdxArray[eIM_RHandIdx] = i;
		if (0 == stricmp(BoneName,"weapon_bone"))
		{
			m_IdxArray[eIM_WeaponBoneIdx] = i;
			//enable COOL re-rigging for weapon bone
			//m_arrModelJoints[i].m_DefaultRelPose.q.SetRotationZ(-gf_PI/2);
		}


		if (0 == stricmp(BoneName,"Bip01 L Clavicle"))
			m_IdxArray[eIM_LClavicleIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 L UpperArm"))
			m_IdxArray[eIM_LUpperArmIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 L ForeArm"))
			m_IdxArray[eIM_LForeArmIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 L Hand"))
			m_IdxArray[eIM_LHandIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 L Finger2"))
			m_IdxArray[eIM_LHandFinger2Idx] = i;

		//	if (0 == stricmp(BoneName,"alt_weapon_bone01"))
		//		int dsdsds=0;

		if (0 == stricmp(BoneName,"Bip01 L Thigh"))
			m_IdxArray[eIM_LThighIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 L knee"))
			m_IdxArray[eIM_LKneeIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 L knee_end"))
			m_IdxArray[eIM_LKneeEndIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 L Calf"))
			m_IdxArray[eIM_LCalfIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 L Foot"))
			m_IdxArray[eIM_LFootIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 L Heel"))
			m_IdxArray[eIM_LHeelIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 L Toe0"))
			m_IdxArray[eIM_LToe0Idx] = i;
		if (0 == stricmp(BoneName,"Bip01 L Toe0Nub"))
			m_IdxArray[eIM_LToe0NubIdx] = i;

		if (0 == stricmp(BoneName,"Bip01 R Thigh"))
			m_IdxArray[eIM_RThighIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 R knee"))
			m_IdxArray[eIM_RKneeIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 R knee_end"))
			m_IdxArray[eIM_RKneeEndIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 R Calf"))
			m_IdxArray[eIM_RCalfIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 R Foot"))
			m_IdxArray[eIM_RFootIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 R Heel"))
			m_IdxArray[eIM_RHeelIdx] = i;
		if (0 == stricmp(BoneName,"Bip01 R Toe0"))
			m_IdxArray[eIM_RToe0Idx] = i;
		if (0 == stricmp(BoneName,"Bip01 R Toe0Nub"))
			m_IdxArray[eIM_RToe0NubIdx] = i;


		//if both bone-names are in the skeleton, then we assume this is the Hunter
		if (0 == stricmp(BoneName,"root"))
			m_IdxArray[eIM_HunterRootIdx] = i;

		if (0 == stricmp(BoneName,"frontLegLeft01"))
			m_IdxArray[eIM_HunterFrontLegL01Idx] = i;
		if (0 == stricmp(BoneName,"frontLegLeft12"))
			m_IdxArray[eIM_HunterFrontLegL12Idx] = i;

		if (0 == stricmp(BoneName,"frontLegRight01"))
			m_IdxArray[eIM_HunterFrontLegR01Idx] = i;
		if (0 == stricmp(BoneName,"frontLegRight12"))
			m_IdxArray[eIM_HunterFrontLegR12Idx] = i;

		if (0 == stricmp(BoneName,"backLegLeft02"))
			m_IdxArray[eIM_HunterBackLegL01Idx] = i;
		if (0 == stricmp(BoneName,"backLegLeft13"))
			m_IdxArray[eIM_HunterBackLegL13Idx] = i;

		if (0 == stricmp(BoneName,"backLegRight02"))
			m_IdxArray[eIM_HunterBackLegR01Idx] = i;
		if (0 == stricmp(BoneName,"backLegRight13"))
			m_IdxArray[eIM_HunterBackLegR13Idx] =i;
	}


	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------

	int32 icounter,boneid;
	int16 arrIndices[0x0400];

	icounter=0;
	boneid =	m_IdxArray[eIM_LUpperArmIdx];
	for (uint32 i=1; i<numJoints; i++)
	{
		int32 c=i;
		while(c>0)
		{
			if (boneid==c){	arrIndices[icounter++]=i;	break; }
			else c = m_arrModelJoints[c].m_idxParent;
		}
	}
	m_arrLShoulderChilds.resize(icounter);
	for (int32 i=0; i<icounter; i++)
		m_arrLShoulderChilds[i]=arrIndices[i];

//-------------------------------------------------------------

	icounter=0;
	boneid =	m_IdxArray[eIM_RUpperArmIdx];
	for (uint32 i=1; i<numJoints; i++)
	{
		int32 c=i;
		while(c>0)
		{
			if (boneid==c){	arrIndices[icounter++]=i;	break; }
			else c = m_arrModelJoints[c].m_idxParent;
		}
	}
	m_arrRShoulderChilds.resize(icounter);
	for (int32 i=0; i<icounter; i++)
		m_arrRShoulderChilds[i]=arrIndices[i];

	//-------------------------------------------------------------

	icounter=0;
	boneid =	m_IdxArray[eIM_LThighIdx];
	for (uint32 i=1; i<numJoints; i++)
	{
		int32 c=i;
		while(c>0)
		{
			if (boneid==c){	arrIndices[icounter++]=i;	break; }
			else c = m_arrModelJoints[c].m_idxParent;
		}
	}
	m_arrLThighChilds.resize(icounter);
	for (int32 i=0; i<icounter; i++)
		m_arrLThighChilds[i]=arrIndices[i];

//----------------------------------------------------------

	icounter=0;
	boneid =	m_IdxArray[eIM_RThighIdx];
	for (uint32 i=1; i<numJoints; i++)
	{
		int32 c=i;
		while(c>0)
		{
			if (boneid==c){	arrIndices[icounter++]=i;	break; }
			else c = m_arrModelJoints[c].m_idxParent;
		}
	}
	m_arrRThighChilds.resize(icounter);
	for (int32 i=0; i<icounter; i++)
		m_arrRThighChilds[i]=arrIndices[i];

	//----------------------------------------------------
	
	m_arrMirrorJoints.resize(numJoints);
	for (uint32 i=0; i<numJoints; i++)
		m_arrMirrorJoints[i]=-1;

	int32 i=-1;

	i=m_IdxArray[eIM_PelvisIdx];
	if (i>0) m_arrMirrorJoints[i]=i;
	i=m_IdxArray[eIM_Spine0Idx];
	if (i>0) m_arrMirrorJoints[i]=i;
	i=m_IdxArray[eIM_Spine1Idx];
	if (i>0) m_arrMirrorJoints[i]=i;
	i=m_IdxArray[eIM_Spine2Idx];
	if (i>0) m_arrMirrorJoints[i]=i;
	i=m_IdxArray[eIM_Spine3Idx];
	if (i>0) m_arrMirrorJoints[i]=i;
	i=m_IdxArray[eIM_NeckIdx];
	if (i>0) m_arrMirrorJoints[i]=i;
	i=m_IdxArray[eIM_HeadIdx];
	if (i>0) m_arrMirrorJoints[i]=i;


	i=m_IdxArray[eIM_RClavicleIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_LClavicleIdx];
	i=m_IdxArray[eIM_RUpperArmIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_LUpperArmIdx];
	i=m_IdxArray[eIM_RForeArmIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_LForeArmIdx];
	i=m_IdxArray[eIM_RHandIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_LHandIdx];

	i=m_IdxArray[eIM_LClavicleIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_RClavicleIdx];
	i=m_IdxArray[eIM_LUpperArmIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_RUpperArmIdx];
	i=m_IdxArray[eIM_LForeArmIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_RForeArmIdx];
	i=m_IdxArray[eIM_LHandIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_RHandIdx];


	i=m_IdxArray[eIM_RThighIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_LThighIdx];
	i=m_IdxArray[eIM_RCalfIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_LCalfIdx];
	i=m_IdxArray[eIM_RFootIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_LFootIdx];
	i=m_IdxArray[eIM_RToe0Idx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_LToe0Idx];

	i=m_IdxArray[eIM_LThighIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_RThighIdx];
	i=m_IdxArray[eIM_LCalfIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_RCalfIdx];
	i=m_IdxArray[eIM_LFootIdx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_RFootIdx];
	i=m_IdxArray[eIM_LToe0Idx];
	if (i>0) m_arrMirrorJoints[i]=m_IdxArray[eIM_RToe0Idx];


}

bool CModelSkeleton::IsHuman()
{
	bool HasLegs = true;

	if (m_IdxArray[eIM_RHeelIdx] == -1) HasLegs = false;
	else
		if (m_IdxArray[eIM_LHeelIdx] == -1) HasLegs = false;
		else
			if (m_IdxArray[eIM_RToe0NubIdx]	== -1) HasLegs = false;
			else
				if (m_IdxArray[eIM_LToe0NubIdx]	== -1) HasLegs = false;

	return HasLegs;
}
