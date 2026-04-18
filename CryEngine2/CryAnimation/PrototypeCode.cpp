////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	
//  Contains:
//	This file contains just prototype-code for testing new features
//	As soon as this stuff is mature enough we will move it into production-tools (in this case the resource-compiler)
//	
//	
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CharacterManager.h"
#include "Model.h"
#include "ModelAnimationSet.h"
#include "ModelMesh.h"
#include "ControllerPQ.h"

const uint32 nAmount=0x200;
const char* g_MultipleFootPlantName[nAmount];
int32 g_MultipleFootPlantGlobalID[nAmount];
int32 g_MultipleFootPlantSegNo[nAmount];
int8 g_MultipleFootPlantDetectFP[nAmount];
SSegments g_MultipleFootPlantSegments[nAmount];

uint32 g_MultipleFootPlantCounter=0;


void CAnimationSet::AnalyseAndModifyAnimations(const string& strPath, CCharacterModel* pModel) 
{
	if (pModel==0)
		return;

	uint32 initialized=0;
	uint32 numGlobalAnims = g_AnimationManager.m_arrGlobalAnimations.size();
	for(uint32 i=0; i<numGlobalAnims; i++)
	{
		if (g_AnimationManager.m_arrGlobalAnimations[i].IsAssetProcessed())
			initialized++;
	}

	//g_pILog->UpdateLoadingScreen("analyse and modify animations for model %s", strPath.c_str() );

	int16 lheel_idx = -1;
	int16 rheel_idx = -1;
	int16 ltoe_idx = -1;
	int16 rtoe_idx = -1;
	int16 ltoenub_idx = -1;
	int16 rtoenub_idx = -1;

	uint32 numBones =  pModel->m_pModelSkeleton->m_arrModelJoints.size();
	for (uint32 i=0; i<numBones; i++)
	{
		const char * BoneName =  pModel->m_pModelSkeleton->m_arrModelJoints[i].GetJointName();
		if (0 == stricmp(BoneName,"Bip01 L Heel"))
			lheel_idx=i;
		if (0 == stricmp(BoneName,"Bip01 L Toe0"))
			ltoe_idx=i;
		if (0 == stricmp(BoneName,"Bip01 L Toe0Nub"))
			ltoenub_idx=i;

		if (0 == stricmp(BoneName,"Bip01 R Heel"))
			rheel_idx=i;
		if (0 == stricmp(BoneName,"Bip01 R Toe0"))
			rtoe_idx=i;
		if (0 == stricmp(BoneName,"Bip01 R Toe0Nub"))
			rtoenub_idx=i;
	}

	uint32 footskel=0;
	if (lheel_idx>=0 && rheel_idx>=0  && ltoe_idx>=0 && rtoe_idx>=0 && ltoenub_idx>=0 && rtoenub_idx>=0)
		footskel=1;

	//---------------------------------------------------------------------------
	uint32 numAnims = m_arrAnimations.size();
	if (footskel)
	{
		//this test is mainly for human-characters
		EvaluteGlobalAnimID();

		//------------------------------------------------------------------------------------
		//---  loop over all HUMAN-animations and evaluate move-speed (meters per second)  ---
		//---  and check if its a cycle animation, and detect foor-plants                  ---
		//------------------------------------------------------------------------------------
		extern std::vector< std::vector<DebugJoint> > g_arrSkeletons;


		uint32 IsAimPose=0;
		for (uint32 i=0; i<numAnims; i++)
		{
			const ModelAnimationHeader* AnimHeader = GetModelAnimationHeader(i);
			if (AnimHeader==0)
				continue;

			uint32 GlobalAnimId = AnimHeader->m_nGlobalAnimId;
			GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimId];
			if (rGAH.IsAimposeUnloaded())
				continue;

			const char* animname= GetNameByAnimID( i );
			IsAimPose = 0;
			for (uint32 n=0; n<0x18; n++)
			{
				int32 n0=animname[0+n]; //'A'
				int32 n1=animname[1+n]; //'i'
				int32 n2=animname[2+n]; //'m'
				int32 n3=animname[3+n]; //'P'
				int32 n4=animname[4+n]; //'o'
				int32 n5=animname[5+n]; //'s'
				int32 n6=animname[6+n]; //'e'
				int32 n7=animname[7+n]; //'s'
				int32 n8=animname[8+n]; //'_'
				if (n0==0) 
					break;

				if (n0=='A' && n1=='i' && n2=='m' && n3=='P' && n4=='o' && n5=='s' && n6=='e' && n7=='s' && n8=='_' )
				{
					IsAimPose=1;

					for (uint32 w=0; w<0x28; w++)
					{
						int32 m0=animname[0+w]; //'_'
						int32 m1=animname[1+w]; //'m'
						int32 m2=animname[2+w]; //'g'
						int32 m3=animname[3+w]; //'_'
						if (m0==0) 
							break;
						if (m0=='_' && m1=='m' && m2=='g' && m3=='_' )
							rGAH.m_fAimDirScale=0.5f;							
					}

					for (uint32 w=0; w<0x28; w++)
					{
						int32 p0=animname[0+w]; //'_'
						int32 p1=animname[1+w]; //'p'
						int32 p2=animname[2+w]; //'i'
						int32 p3=animname[3+w]; //'s'
						int32 p4=animname[4+w]; //'t'
						int32 p5=animname[5+w]; //'o'
						int32 p6=animname[6+w]; //'l'
						int32 p7=animname[7+w]; //'_'
						if (p0==0) 
							break;
						if (p0=='_' && p1=='p' && p2=='i' && p3=='s' && p4=='t' && p5=='o' && p6=='l' && p7=='_' )
							rGAH.m_fAimDirScale=0.0f;							
					}

					for (uint32 w=0; w<0x28; w++)
					{
						int32 r0=animname[0+w]; //'_'
						int32 r1=animname[1+w]; //'r'
						int32 r2=animname[2+w]; //'i'
						int32 r3=animname[3+w]; //'f'
						int32 r4=animname[4+w]; //'l'
						int32 r5=animname[5+w]; //'e'
						int32 r6=animname[6+w]; //'_'
						if (r0==0) 
							break;
						if (r0=='_' && r1=='r' && r2=='i' && r3=='f' && r4=='l' && r5=='e' && r6=='_' )
							rGAH.m_fAimDirScale=0.20f;							
					}

					for (uint32 w=0; w<0x28; w++)
					{
						int32 r0=animname[0+w]; //'_'
						int32 r1=animname[1+w]; //'r'
						int32 r2=animname[2+w]; //'o'
						int32 r3=animname[3+w]; //'c'
						int32 r4=animname[4+w]; //'k'
						int32 r5=animname[5+w]; //'e'
						int32 r6=animname[6+w]; //'t'
						int32 r7=animname[7+w]; //'_'
						if (r0==0) 
							break;
						if (r0=='_' && r1=='r' && r2=='o' && r3=='c' && r4=='k' && r5=='e' && r6=='t' && r7=='_' )
							rGAH.m_fAimDirScale=0.50f;							
					}

				}
			}


			if (IsAimPose)
			{
				CModelSkeleton* pModelSkeleton = pModel->m_pModelSkeleton;
				const CModelJoint* pModelJoint = &pModelSkeleton->m_arrModelJoints[0];

				int32 Spine0Idx			= pModelSkeleton->m_IdxArray[eIM_Spine0Idx];
				int32 Spine1Idx			= pModelSkeleton->m_IdxArray[eIM_Spine1Idx];
				int32 Spine2Idx			= pModelSkeleton->m_IdxArray[eIM_Spine2Idx];
				int32 Spine3Idx			= pModelSkeleton->m_IdxArray[eIM_Spine3Idx];

				int32 NeckIdx				= pModelSkeleton->m_IdxArray[eIM_NeckIdx];
				int32 RClavicleIdx	= pModelSkeleton->m_IdxArray[eIM_RClavicleIdx];
				int32 RUpperArmIdx	= pModelSkeleton->m_IdxArray[eIM_RUpperArmIdx];
				int32 RForeArmIdx		= pModelSkeleton->m_IdxArray[eIM_RForeArmIdx];
				int32 RHandIdx			= pModelSkeleton->m_IdxArray[eIM_RHandIdx];

				int32 WeaponBoneIdx = pModelSkeleton->m_IdxArray[eIM_WeaponBoneIdx];

				int32 HeadIdx				= pModelSkeleton->m_IdxArray[eIM_HeadIdx];
				int32 LClavicleIdx	= pModelSkeleton->m_IdxArray[eIM_LClavicleIdx];
				int32 LUpperArmIdx	= pModelSkeleton->m_IdxArray[eIM_LUpperArmIdx];
				int32 LForeArmIdx		= pModelSkeleton->m_IdxArray[eIM_LForeArmIdx];
				int32 LHandIdx			= pModelSkeleton->m_IdxArray[eIM_LHandIdx];

				if (Spine0Idx<0)
					IsAimPose=0;
				if (Spine1Idx<0)
					IsAimPose=0;
				if (Spine2Idx<0)
					IsAimPose=0;
				if (Spine3Idx<0)
					IsAimPose=0;

				if (NeckIdx<0)
					IsAimPose=0;
				if (RClavicleIdx<0)
					IsAimPose=0;
				if (RUpperArmIdx<0)
					IsAimPose=0;
				if (RForeArmIdx<0)
					IsAimPose=0;
				if (RHandIdx<0)
					IsAimPose=0;

				if (WeaponBoneIdx<0)
					IsAimPose=0;

				if (HeadIdx<0)
					IsAimPose=0;
				if (LClavicleIdx<0)
					IsAimPose=0;
				if (LUpperArmIdx<0)
					IsAimPose=0;
				if (LForeArmIdx<0)
					IsAimPose=0;
				if (LHandIdx<0)
					IsAimPose=0;
			}


			//-------------------------------------------------------


			if (IsAimPose)
			{
				uint32 GlobalAnimId = AnimHeader->m_nGlobalAnimId;
				GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimId];
				rGAH.m_nPlayedCount=1;
				rGAH.m_arrAimIKPoses.resize(AIM_POSES);

				CopyPoses(i, 0,  8, rGAH, pModel );
				CopyPoses(i, 1, 10, rGAH, pModel );
				CopyPoses(i, 2, 12, rGAH, pModel );

				CopyPoses(i, 3, 22, rGAH, pModel );
				CopyPoses(i, 4, 24, rGAH, pModel );
				CopyPoses(i, 5, 26, rGAH, pModel );

				CopyPoses(i, 6, 36, rGAH, pModel );
				CopyPoses(i, 7, 38, rGAH, pModel );
				CopyPoses(i, 8, 40, rGAH, pModel );

				if (Console::GetInst().ca_UnloadAimPoses)
				{
					// we don't need this animation anymore
					g_AnimationManager.UnloadAnimation(GlobalAnimId);
					rGAH.OnAimposeUnloaded();
				}

				Blend_2_Poses(  8, 10,  9, rGAH, 0.5f );
				Blend_2_Poses( 22, 24, 23, rGAH, 0.5f  );
				Blend_2_Poses(  8, 22, 15, rGAH, 0.5f  );
				Blend_2_Poses( 10, 24, 17, rGAH, 0.5f  );

				Blend_2_Poses( 10, 12, 11, rGAH, 0.5f  );
				Blend_2_Poses( 24, 26, 25, rGAH, 0.5f  );
				Blend_2_Poses( 12, 26, 19, rGAH, 0.5f  );

				Blend_2_Poses( 22, 36, 29, rGAH, 0.5f  );
				Blend_2_Poses( 24, 38, 31, rGAH, 0.5f  );
				Blend_2_Poses( 36, 38, 37, rGAH, 0.5f  );

				Blend_2_Poses( 26, 40, 33, rGAH, 0.5f  );
				Blend_2_Poses( 38, 40, 39, rGAH, 0.5f  );


				//blend 4 poses
				//		Blend_2_Poses( 24, 8,  16, rGAH, 0.5f  );
				//		Blend_2_Poses( 24, 12, 18, rGAH, 0.5f  );
				//		Blend_2_Poses( 24, 36, 30, rGAH, 0.5f  );
				//		Blend_2_Poses( 24, 40, 32, rGAH, 0.5f  );
				Blend_4_Poses(  8,10,22,24, 16, rGAH, 0.5f  );
				Blend_4_Poses( 10,24,26,12, 18, rGAH, 0.5f  );
				Blend_4_Poses( 22,24,36,38, 30, rGAH, 0.5f  );
				Blend_4_Poses( 24,26,38,40, 32, rGAH, 0.5f  );


				const f32 fExtScale = 0.4f;

				//extrapolation
				Blend_2_Poses(  8, 16,   0, rGAH, -0.50f*fExtScale); //edge

				Blend_2_Poses(  8, 15,   1, rGAH, -0.90f*fExtScale);
				Blend_2_Poses(  9, 16,   2, rGAH, -1.00f*fExtScale);
				Blend_2_Poses( 10, 17,   3, rGAH, -1.00f*fExtScale);
				Blend_2_Poses( 11, 18,   4, rGAH, -1.00f*fExtScale);
				Blend_2_Poses( 12, 19,   5, rGAH, -0.90f*fExtScale);

				Blend_2_Poses( 12, 18,   6, rGAH, -0.50f*fExtScale);  //edge

				Blend_2_Poses( 12, 11,  13, rGAH, -0.90f*fExtScale);
				Blend_2_Poses( 19, 18,  20, rGAH, -1.00f*fExtScale);
				Blend_2_Poses( 26, 25,  27, rGAH, -1.00f*fExtScale);
				Blend_2_Poses( 33, 32,  34, rGAH, -1.00f*fExtScale);
				Blend_2_Poses( 40, 39,  41, rGAH, -0.90f*fExtScale);

				Blend_2_Poses( 40, 32,  48, rGAH, -0.50f*fExtScale); //edge

				Blend_2_Poses( 40, 33,  47, rGAH, -0.90f*fExtScale);
				Blend_2_Poses( 39, 32,  46, rGAH, -1.00f*fExtScale);
				Blend_2_Poses( 38, 31,  45, rGAH, -1.00f*fExtScale);
				Blend_2_Poses( 37, 30,  44, rGAH, -1.00f*fExtScale);
				Blend_2_Poses( 36, 29,  43, rGAH, -0.90f*fExtScale);

				Blend_2_Poses( 36, 30,  42, rGAH, -0.50f*fExtScale); //edge

				Blend_2_Poses( 36, 37,  35, rGAH, -0.90f*fExtScale);
				Blend_2_Poses( 29, 30,  28, rGAH, -1.00f*fExtScale);
				Blend_2_Poses( 22, 23,  21, rGAH, -1.00f*fExtScale);
				Blend_2_Poses( 15, 16,  14, rGAH, -1.00f*fExtScale);
				Blend_2_Poses(  8,  9,   7, rGAH, -0.90f*fExtScale);

				FindLeftWrist(rGAH, pModel );
				Vec3 lwrist=rGAH.m_AimIKLeftWrist;	
				uint32 ddd=0;	


			}
		}



		//-----------------------------------------------------------------------
		//-----------------------------------------------------------------------
		//---------             patch animations                     ------------
		//-----------------------------------------------------------------------
		//-----------------------------------------------------------------------
		for (uint32 i=0; i<numAnims; i++)
		{
			const char* animname= GetNameByAnimID( i );
			const ModelAnimationHeader* AnimHeader = GetModelAnimationHeader(i);
			if (AnimHeader==0)
				continue;

			uint32 GlobalID = AnimHeader->m_nGlobalAnimId;	
			GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[GlobalID];
			if (rGlobalAnimHeader.IsAssetLMG())
				continue;


			//	if (rGlobalAnimHeader.IsAssetProcessed())
			//		continue;

			int found=-1;
			for (uint32 h=0; h<g_MultipleFootPlantCounter; h++)
			{
				if (g_MultipleFootPlantGlobalID[h]==GlobalID)
				{
					found=h;
					break;
				}
			}
			if (found<0)
				continue;


			//@Ivo; Should work now
			//@Alexey: I had to revert this. It was breaking the idle-steps
			//const CModelJoint* pModelJoint = &pModel->m_pModelSkeleton->m_arrModelJoints[0];
			//IController* pController = g_AnimationManager.m_arrGlobalAnimations[GlobalID].GetControllerByJointID(pModelJoint[0].m_nJointCRC32);//pModelJoint[0].m_arrControllersMJoint[i];
			//if (pController)
			//	SetFootplantBitsAutomatically( g_arrSkeletons, i,GlobalID, lheel_idx,rheel_idx,ltoe_idx,rtoe_idx,ltoenub_idx,rtoenub_idx,g_MultipleFootPlantDetectFP[found]);
			rGlobalAnimHeader.m_FootPlantBits.clear();
			rGlobalAnimHeader.OnAssetProcessed();
		}
	} 
}

void CAnimationSet::CopyPoses(int32 AnimID, uint32 skey,uint32 tkey, GlobalAnimationHeader& rGAH, CCharacterModel* pModel )
{
	const char* animname= GetNameByAnimID( AnimID );

	const ModelAnimationHeader* AnimHeader = GetModelAnimationHeader(AnimID);
	uint32 globalID = AnimHeader->m_nGlobalAnimId;	

	CModelSkeleton* pModelSkeleton = pModel->m_pModelSkeleton;
	const CModelJoint* pModelJoint = &pModelSkeleton->m_arrModelJoints[0];

	int32 Spine0Idx			= pModelSkeleton->m_IdxArray[eIM_Spine0Idx];
	int32 Spine1Idx			= pModelSkeleton->m_IdxArray[eIM_Spine1Idx];
	int32 Spine2Idx			= pModelSkeleton->m_IdxArray[eIM_Spine2Idx];
	int32 Spine3Idx			= pModelSkeleton->m_IdxArray[eIM_Spine3Idx];

	int32 NeckIdx				= pModelSkeleton->m_IdxArray[eIM_NeckIdx];
	int32 RClavicleIdx	= pModelSkeleton->m_IdxArray[eIM_RClavicleIdx];
	int32 RUpperArmIdx	= pModelSkeleton->m_IdxArray[eIM_RUpperArmIdx];
	int32 RForeArmIdx		= pModelSkeleton->m_IdxArray[eIM_RForeArmIdx];
	int32 RHandIdx			= pModelSkeleton->m_IdxArray[eIM_RHandIdx];

	int32 WeaponBoneIdx = pModelSkeleton->m_IdxArray[eIM_WeaponBoneIdx];

	int32 HeadIdx				= pModelSkeleton->m_IdxArray[eIM_HeadIdx];
	int32 LClavicleIdx	= pModelSkeleton->m_IdxArray[eIM_LClavicleIdx];
	int32 LUpperArmIdx	= pModelSkeleton->m_IdxArray[eIM_LUpperArmIdx];
	int32 LForeArmIdx		= pModelSkeleton->m_IdxArray[eIM_LForeArmIdx];
	int32 LHandIdx			= pModelSkeleton->m_IdxArray[eIM_LHandIdx];

	IController* pFAimController00	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[Spine0Idx].m_nJointCRC32);// pModelJoint[Spine0Idx    ].m_arrControllersMJoint[AnimID];
	IController* pFAimController01	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[Spine1Idx].m_nJointCRC32);//pModelJoint[Spine1Idx    ].m_arrControllersMJoint[AnimID];
	IController* pFAimController02	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[Spine2Idx].m_nJointCRC32);//pModelJoint[Spine2Idx    ].m_arrControllersMJoint[AnimID];
	IController* pFAimController03	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[Spine3Idx].m_nJointCRC32);//pModelJoint[Spine3Idx    ].m_arrControllersMJoint[AnimID];

	IController* pFAimController04	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[NeckIdx].m_nJointCRC32);//pModelJoint[NeckIdx      ].m_arrControllersMJoint[AnimID];
	IController* pFAimController05	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[RClavicleIdx].m_nJointCRC32);//pModelJoint[RClavicleIdx ].m_arrControllersMJoint[AnimID];
	IController* pFAimController06	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[RUpperArmIdx].m_nJointCRC32);//pModelJoint[RUpperArmIdx ].m_arrControllersMJoint[AnimID];
	IController* pFAimController07	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[RForeArmIdx].m_nJointCRC32);//pModelJoint[RForeArmIdx  ].m_arrControllersMJoint[AnimID];
	IController* pFAimController08	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[RHandIdx].m_nJointCRC32);//pModelJoint[RHandIdx     ].m_arrControllersMJoint[AnimID];

	IController* pFAimController09	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[WeaponBoneIdx].m_nJointCRC32);//pModelJoint[WeaponBoneIdx].m_arrControllersMJoint[AnimID];

	IController* pFAimController10	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[HeadIdx].m_nJointCRC32);//pModelJoint[HeadIdx      ].m_arrControllersMJoint[AnimID];
	IController* pFAimController11	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[LClavicleIdx].m_nJointCRC32);//pModelJoint[LClavicleIdx ].m_arrControllersMJoint[AnimID];
	IController* pFAimController12	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[LUpperArmIdx].m_nJointCRC32);//pModelJoint[LUpperArmIdx ].m_arrControllersMJoint[AnimID];
	IController* pFAimController13	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[LForeArmIdx].m_nJointCRC32);//pModelJoint[LForeArmIdx  ].m_arrControllersMJoint[AnimID];
	IController* pFAimController14	= g_AnimationManager.m_arrGlobalAnimations[globalID].GetControllerByJointID(pModelJoint[LHandIdx].m_nJointCRC32);//pModelJoint[LHandIdx     ].m_arrControllersMJoint[AnimID];

	if (pFAimController00==0 || pFAimController01==0 || pFAimController02==0 || pFAimController03==0 || pFAimController04==0 || pFAimController05==0 || pFAimController06==0 || pFAimController07==0 || pFAimController08==0 || pFAimController09==0 || pFAimController10==0 || pFAimController11==0 || pFAimController12==0 || pFAimController13==0 || pFAimController14==0)
	{
		g_pILog->UpdateLoadingScreen("Aimpose '%s' has missing controllers. Probably not added to the CBA-file", animname );
	}
	rGAH.m_arrAimIKPoses[tkey].m_Spine0			= GetOrientationByKey( pFAimController00, skey, &pModelJoint[Spine0Idx] );
	rGAH.m_arrAimIKPoses[tkey].m_Spine1			= GetOrientationByKey( pFAimController01, skey, &pModelJoint[Spine1Idx] );
	rGAH.m_arrAimIKPoses[tkey].m_Spine2			= GetOrientationByKey( pFAimController02, skey, &pModelJoint[Spine2Idx] );
	rGAH.m_arrAimIKPoses[tkey].m_Spine3			= GetOrientationByKey( pFAimController03, skey, &pModelJoint[Spine3Idx] );

	rGAH.m_arrAimIKPoses[tkey].m_Neck	= GetOrientationByKey( pFAimController04, skey, &pModelJoint[NeckIdx] );

	rGAH.m_arrAimIKPoses[tkey].m_RClavicle	= GetOrientationByKey( pFAimController05, skey, &pModelJoint[RClavicleIdx] );
	rGAH.m_arrAimIKPoses[tkey].m_RUpperArm	= GetOrientationByKey( pFAimController06, skey, &pModelJoint[RUpperArmIdx] );
	rGAH.m_arrAimIKPoses[tkey].m_RForeArm		= GetOrientationByKey( pFAimController07, skey, &pModelJoint[RForeArmIdx] );
	rGAH.m_arrAimIKPoses[tkey].m_RHand			= GetOrientationByKey( pFAimController08, skey, &pModelJoint[RHandIdx] );

	rGAH.m_arrAimIKPoses[tkey].m_WeaponBone	= GetOrientationByKey( pFAimController09, skey, &pModelJoint[WeaponBoneIdx] );

	rGAH.m_arrAimIKPoses[tkey].m_Head				= GetOrientationByKey( pFAimController10, skey, &pModelJoint[HeadIdx] );

	rGAH.m_arrAimIKPoses[tkey].m_LClavicle	= GetOrientationByKey( pFAimController11, skey, &pModelJoint[LClavicleIdx] );
	rGAH.m_arrAimIKPoses[tkey].m_LUpperArm	= GetOrientationByKey( pFAimController12, skey, &pModelJoint[LUpperArmIdx] );
	rGAH.m_arrAimIKPoses[tkey].m_LForeArm		= GetOrientationByKey( pFAimController13, skey, &pModelJoint[LForeArmIdx] );
	rGAH.m_arrAimIKPoses[tkey].m_LHand			= GetOrientationByKey( pFAimController14, skey, &pModelJoint[LHandIdx] );
}


//--------------------------------------------------------------------------------------------------------

Quat CAnimationSet::GetOrientationByKey( IController* pIController, uint32 skey, const CModelJoint* pModelJoint  )
{
	Quat q;
	if (pIController)
		q = pIController->GetOrientationByKey(skey);
	else
		q = pModelJoint->m_DefaultRelPose.q;

	q.Normalize();
	return q;
}

//------------------------------------------------------------------------------------------------------------------

void CAnimationSet::Blend_2_Poses( uint32 skey0, uint32 skey1, uint32 tkey, GlobalAnimationHeader& rGAH, f32 t  )
{
	rGAH.m_arrAimIKPoses[tkey].m_Spine0			=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_Spine0,		rGAH.m_arrAimIKPoses[skey1].m_Spine0,			t);
	rGAH.m_arrAimIKPoses[tkey].m_Spine1			=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_Spine1,		rGAH.m_arrAimIKPoses[skey1].m_Spine1,			t);
	rGAH.m_arrAimIKPoses[tkey].m_Spine2			=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_Spine2,		rGAH.m_arrAimIKPoses[skey1].m_Spine2,			t);
	rGAH.m_arrAimIKPoses[tkey].m_Spine3			=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_Spine3,		rGAH.m_arrAimIKPoses[skey1].m_Spine3,			t);

	rGAH.m_arrAimIKPoses[tkey].m_Neck				=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_Neck,			rGAH.m_arrAimIKPoses[skey1].m_Neck,				t);
	rGAH.m_arrAimIKPoses[tkey].m_RClavicle	=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_RClavicle,	rGAH.m_arrAimIKPoses[skey1].m_RClavicle,	t);
	rGAH.m_arrAimIKPoses[tkey].m_RUpperArm	=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_RUpperArm,	rGAH.m_arrAimIKPoses[skey1].m_RUpperArm,	t);
	rGAH.m_arrAimIKPoses[tkey].m_RForeArm		=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_RForeArm,	rGAH.m_arrAimIKPoses[skey1].m_RForeArm,		t);
	rGAH.m_arrAimIKPoses[tkey].m_RHand			=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_RHand,			rGAH.m_arrAimIKPoses[skey1].m_RHand,			t);

	rGAH.m_arrAimIKPoses[tkey].m_WeaponBone	=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_WeaponBone,rGAH.m_arrAimIKPoses[skey1].m_WeaponBone,	t);

	rGAH.m_arrAimIKPoses[tkey].m_Head				=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_Head,			rGAH.m_arrAimIKPoses[skey1].m_Head,				t);
	rGAH.m_arrAimIKPoses[tkey].m_LClavicle	=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_LClavicle,	rGAH.m_arrAimIKPoses[skey1].m_LClavicle,	t);
	rGAH.m_arrAimIKPoses[tkey].m_LUpperArm	=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_LUpperArm,	rGAH.m_arrAimIKPoses[skey1].m_LUpperArm,	t);
	rGAH.m_arrAimIKPoses[tkey].m_LForeArm		=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_LForeArm,	rGAH.m_arrAimIKPoses[skey1].m_LForeArm,		t);
	rGAH.m_arrAimIKPoses[tkey].m_LHand			=Quat::CreateNlerp(rGAH.m_arrAimIKPoses[skey0].m_LHand,			rGAH.m_arrAimIKPoses[skey1].m_LHand,			t);
}

void CAnimationSet::Blend_4_Poses( uint32 skey0,uint32 skey1,uint32 skey2,uint32 skey3,   uint32 tkey, GlobalAnimationHeader& rGAH, f32 t  )
{
	rGAH.m_arrAimIKPoses[tkey].m_Spine0			=(rGAH.m_arrAimIKPoses[skey0].m_Spine0		%	rGAH.m_arrAimIKPoses[skey1].m_Spine0		% rGAH.m_arrAimIKPoses[skey2].m_Spine0		%	rGAH.m_arrAimIKPoses[skey3].m_Spine0).GetNormalized();
	rGAH.m_arrAimIKPoses[tkey].m_Spine1			=(rGAH.m_arrAimIKPoses[skey0].m_Spine1		%	rGAH.m_arrAimIKPoses[skey1].m_Spine1		% rGAH.m_arrAimIKPoses[skey2].m_Spine1		%	rGAH.m_arrAimIKPoses[skey3].m_Spine1).GetNormalized();
	rGAH.m_arrAimIKPoses[tkey].m_Spine2			=(rGAH.m_arrAimIKPoses[skey0].m_Spine2		%	rGAH.m_arrAimIKPoses[skey1].m_Spine2		% rGAH.m_arrAimIKPoses[skey2].m_Spine2		%	rGAH.m_arrAimIKPoses[skey3].m_Spine2).GetNormalized();
	rGAH.m_arrAimIKPoses[tkey].m_Spine3			=(rGAH.m_arrAimIKPoses[skey0].m_Spine3		%	rGAH.m_arrAimIKPoses[skey1].m_Spine3		% rGAH.m_arrAimIKPoses[skey2].m_Spine3		%	rGAH.m_arrAimIKPoses[skey3].m_Spine3).GetNormalized();

	rGAH.m_arrAimIKPoses[tkey].m_Neck				=(rGAH.m_arrAimIKPoses[skey0].m_Neck			%	rGAH.m_arrAimIKPoses[skey1].m_Neck			% rGAH.m_arrAimIKPoses[skey2].m_Neck			%	rGAH.m_arrAimIKPoses[skey3].m_Neck).GetNormalized();
	rGAH.m_arrAimIKPoses[tkey].m_RClavicle	=(rGAH.m_arrAimIKPoses[skey0].m_RClavicle	%	rGAH.m_arrAimIKPoses[skey1].m_RClavicle % rGAH.m_arrAimIKPoses[skey2].m_RClavicle	%	rGAH.m_arrAimIKPoses[skey3].m_RClavicle ).GetNormalized();
	rGAH.m_arrAimIKPoses[tkey].m_RUpperArm	=(rGAH.m_arrAimIKPoses[skey0].m_RUpperArm	%	rGAH.m_arrAimIKPoses[skey1].m_RUpperArm % rGAH.m_arrAimIKPoses[skey2].m_RUpperArm	%	rGAH.m_arrAimIKPoses[skey3].m_RUpperArm).GetNormalized();
	rGAH.m_arrAimIKPoses[tkey].m_RForeArm		=(rGAH.m_arrAimIKPoses[skey0].m_RForeArm	%	rGAH.m_arrAimIKPoses[skey1].m_RForeArm	% rGAH.m_arrAimIKPoses[skey2].m_RForeArm	%	rGAH.m_arrAimIKPoses[skey3].m_RForeArm).GetNormalized();
	rGAH.m_arrAimIKPoses[tkey].m_RHand			=(rGAH.m_arrAimIKPoses[skey0].m_RHand			%	rGAH.m_arrAimIKPoses[skey1].m_RHand			% rGAH.m_arrAimIKPoses[skey2].m_RHand			%	rGAH.m_arrAimIKPoses[skey3].m_RHand).GetNormalized();

	rGAH.m_arrAimIKPoses[tkey].m_WeaponBone	=(rGAH.m_arrAimIKPoses[skey0].m_WeaponBone%	rGAH.m_arrAimIKPoses[skey1].m_WeaponBone% rGAH.m_arrAimIKPoses[skey2].m_WeaponBone%rGAH.m_arrAimIKPoses[skey3].m_WeaponBone).GetNormalized();

	rGAH.m_arrAimIKPoses[tkey].m_Head				=(rGAH.m_arrAimIKPoses[skey0].m_Head			%	rGAH.m_arrAimIKPoses[skey1].m_Head			% rGAH.m_arrAimIKPoses[skey2].m_Head			%	rGAH.m_arrAimIKPoses[skey3].m_Head).GetNormalized();
	rGAH.m_arrAimIKPoses[tkey].m_LClavicle	=(rGAH.m_arrAimIKPoses[skey0].m_LClavicle	%	rGAH.m_arrAimIKPoses[skey1].m_LClavicle % rGAH.m_arrAimIKPoses[skey2].m_LClavicle	%	rGAH.m_arrAimIKPoses[skey3].m_LClavicle).GetNormalized();
	rGAH.m_arrAimIKPoses[tkey].m_LUpperArm	=(rGAH.m_arrAimIKPoses[skey0].m_LUpperArm	%	rGAH.m_arrAimIKPoses[skey1].m_LUpperArm % rGAH.m_arrAimIKPoses[skey2].m_LUpperArm	%	rGAH.m_arrAimIKPoses[skey3].m_LUpperArm).GetNormalized();
	rGAH.m_arrAimIKPoses[tkey].m_LForeArm		=(rGAH.m_arrAimIKPoses[skey0].m_LForeArm	%	rGAH.m_arrAimIKPoses[skey1].m_LForeArm	% rGAH.m_arrAimIKPoses[skey2].m_LForeArm	%	rGAH.m_arrAimIKPoses[skey3].m_LForeArm).GetNormalized();
	rGAH.m_arrAimIKPoses[tkey].m_LHand			=(rGAH.m_arrAimIKPoses[skey0].m_LHand			%	rGAH.m_arrAimIKPoses[skey1].m_LHand			% rGAH.m_arrAimIKPoses[skey2].m_LHand			%	rGAH.m_arrAimIKPoses[skey3].m_LHand).GetNormalized();
}

void CAnimationSet::FindLeftWrist(GlobalAnimationHeader& rGAH, CCharacterModel* pModel )
{
	CModelSkeleton* pModelSkeleton = pModel->m_pModelSkeleton;
	const CModelJoint* pModelJoint = &pModelSkeleton->m_arrModelJoints[0];

	int32 Spine0Idx			= pModelSkeleton->m_IdxArray[eIM_Spine0Idx];
	int32 Spine1Idx			= pModelSkeleton->m_IdxArray[eIM_Spine1Idx];
	int32 Spine2Idx			= pModelSkeleton->m_IdxArray[eIM_Spine2Idx];
	int32 Spine3Idx			= pModelSkeleton->m_IdxArray[eIM_Spine3Idx];
	int32 NeckIdx				= pModelSkeleton->m_IdxArray[eIM_NeckIdx];
	int32 HeadIdx				= pModelSkeleton->m_IdxArray[eIM_HeadIdx];

	int32 RClavicleIdx		= pModelSkeleton->m_IdxArray[eIM_RClavicleIdx];
	int32 RUpperArmIdx		= pModelSkeleton->m_IdxArray[eIM_RUpperArmIdx];
	int32 RForeArmIdx			= pModelSkeleton->m_IdxArray[eIM_RForeArmIdx];
	int32 RHandIdx				= pModelSkeleton->m_IdxArray[eIM_RHandIdx];
	int32 WeaponBoneIdx		= pModelSkeleton->m_IdxArray[eIM_WeaponBoneIdx];

	int32 LClavicleIdx		= pModelSkeleton->m_IdxArray[eIM_LClavicleIdx];
	int32 LUpperArmIdx		= pModelSkeleton->m_IdxArray[eIM_LUpperArmIdx];
	int32 LForeArmIdx			= pModelSkeleton->m_IdxArray[eIM_LForeArmIdx];
	int32 LHandIdx				= pModelSkeleton->m_IdxArray[eIM_LHandIdx];
	int32 LHandFinger2Idx	= pModelSkeleton->m_IdxArray[eIM_LHandFinger2Idx];
	

	std::vector<QuatT> relQuatIK;
	std::vector<QuatT> absQuatIK;
	uint32 numJoints = pModelSkeleton->m_arrModelJoints.size();
	relQuatIK.resize(numJoints);
	absQuatIK.resize(numJoints);

	for (uint32 j=0; j<numJoints; j++)
		relQuatIK[j] = pModelSkeleton->m_arrModelJoints[j].m_DefaultRelPose;

	relQuatIK[Spine0Idx].q			= rGAH.m_arrAimIKPoses[24].m_Spine0;
	relQuatIK[Spine1Idx].q			= rGAH.m_arrAimIKPoses[24].m_Spine1;
	relQuatIK[Spine2Idx].q			= rGAH.m_arrAimIKPoses[24].m_Spine2;
	relQuatIK[Spine3Idx].q			= rGAH.m_arrAimIKPoses[24].m_Spine3;
	relQuatIK[NeckIdx].q				= rGAH.m_arrAimIKPoses[24].m_Neck;
	relQuatIK[HeadIdx].q				= rGAH.m_arrAimIKPoses[24].m_Head;

	relQuatIK[RClavicleIdx].q		= rGAH.m_arrAimIKPoses[24].m_RClavicle;
	relQuatIK[RUpperArmIdx].q		= rGAH.m_arrAimIKPoses[24].m_RUpperArm;
	relQuatIK[RForeArmIdx].q		= rGAH.m_arrAimIKPoses[24].m_RForeArm;
	relQuatIK[RHandIdx].q				= rGAH.m_arrAimIKPoses[24].m_RHand;
	relQuatIK[WeaponBoneIdx].q	= rGAH.m_arrAimIKPoses[24].m_WeaponBone;
//	relQuatIK[WeaponBoneIdx].q.SetRotationZ(-gf_PI/2);

	relQuatIK[LClavicleIdx].q		= rGAH.m_arrAimIKPoses[24].m_LClavicle;
	relQuatIK[LUpperArmIdx].q		= rGAH.m_arrAimIKPoses[24].m_LUpperArm;
	relQuatIK[LForeArmIdx].q		= rGAH.m_arrAimIKPoses[24].m_LForeArm;
	relQuatIK[LHandIdx].q				= rGAH.m_arrAimIKPoses[24].m_LHand;

	for (uint32 j=0; j<numJoints; j++)
	{
		absQuatIK[j] = relQuatIK[j];
		int32 p = pModelSkeleton->m_arrModelJoints[j].m_idxParent;
		if (p>=0)	
			absQuatIK[j]	= absQuatIK[p] * absQuatIK[j];
		absQuatIK[j].q.Normalize();
	}

	Vec3 r=absQuatIK[WeaponBoneIdx].t;
	Vec3 l=absQuatIK[LHandFinger2Idx].t;
	rGAH.m_AimIKLeftWrist	=	(l-r) * absQuatIK[WeaponBoneIdx].q;

	uint32 sds=0;
}
















void CAnimationSet::EvaluteGlobalAnimID()
{
	for (uint32 a=0; a<nAmount; a++)
		g_MultipleFootPlantDetectFP[a] = 1; 

	//-------------------------------------------------------------------------------------------
	//---              for this animation we don't need footplants                            ---          
	//-------------------------------------------------------------------------------------------
	uint32 f=0;

	g_MultipleFootPlantName[f]="_#combat_jump_rifle_forward_01";
	g_MultipleFootPlantSegNo[f] = 3;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.294f;
	g_MultipleFootPlantSegments[f].c	=	0.682f;
	g_MultipleFootPlantSegments[f].d	=	1.000f;
	f++;

	g_MultipleFootPlantName[f]="_#combat_jumpSlow_rifle_forward_01";
	g_MultipleFootPlantSegNo[f] = 3;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.273f;
	g_MultipleFootPlantSegments[f].c	=	0.664f;
	g_MultipleFootPlantSegments[f].d	=	1.000f;
	f++;

	g_MultipleFootPlantName[f]="_#combat_jumpSprint_rifle_forward_01";
	g_MultipleFootPlantSegNo[f] = 3;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.280f;
	g_MultipleFootPlantSegments[f].c	=	0.675f;
	g_MultipleFootPlantSegments[f].d	=	1.000f;
	f++;



	g_MultipleFootPlantName[f]="stealth_sprint_pistol_forward_02";
	g_MultipleFootPlantSegNo[f] = 2;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.445f;
	g_MultipleFootPlantSegments[f].c	=	1.000f;
	g_MultipleFootPlantSegments[f].d	=	1.000f;
	f++;

	//-----------------------------------------------------------------------------
	//--------                     Sharp Turn assets               ----------------
	//-----------------------------------------------------------------------------

	g_MultipleFootPlantName[f]="combat_runTurn_rifle_left180_fast_01";
	g_MultipleFootPlantSegNo[f] = 3;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.284f;
	g_MultipleFootPlantSegments[f].c	=	0.693f;
	g_MultipleFootPlantSegments[f].d	=	1.000f;
	f++;

	g_MultipleFootPlantName[f]="combat_runTurn_rifle_left90_fast_01";
	g_MultipleFootPlantSegNo[f] = 3;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.298f;
	g_MultipleFootPlantSegments[f].c	=	0.623f;
	g_MultipleFootPlantSegments[f].d	=	1.000f;
	f++;


	g_MultipleFootPlantName[f]="combat_runTurn_rifle_right180_fast_01";
	g_MultipleFootPlantSegNo[f] = 4;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.250f;
	g_MultipleFootPlantSegments[f].c	=	0.500f;
	g_MultipleFootPlantSegments[f].d	=	0.750f;
	g_MultipleFootPlantSegments[f].e	=	1.000f;
	f++;

	g_MultipleFootPlantName[f]="combat_runTurn_rifle_right90_fast_01";
	g_MultipleFootPlantSegNo[f] = 4;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.250f;
	g_MultipleFootPlantSegments[f].c	=	0.427f;
	g_MultipleFootPlantSegments[f].d	=	0.750f;
	g_MultipleFootPlantSegments[f].e	=	1.000f;
	f++;








//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

	g_MultipleFootPlantName[f]="combat_runStrafeStart_rifle_left_fast_lfoot_01";
	g_MultipleFootPlantSegNo[f] = 2;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.547f;
	f++;
	g_MultipleFootPlantName[f]="combat_runStrafeStart_rifle_right_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 2;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.547f;
	f++;

	g_MultipleFootPlantName[f]="combat_runStart_rifle_back_fast_lfoot_01";
	g_MultipleFootPlantSegNo[f] = 2;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.527f;
	f++;
	g_MultipleFootPlantName[f]="combat_runStart_rifle_back_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 2;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.527f;
	f++;

//------------------------------------------------------------------------

	g_MultipleFootPlantName[f]="combat_runStrafeStart_rifle_left_slow_lfoot_01";
	g_MultipleFootPlantSegNo[f] = 2;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.547f;
	f++;
	g_MultipleFootPlantName[f]="combat_runStrafeStart_rifle_right_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 2;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.547f;
	f++;

	g_MultipleFootPlantName[f]="combat_runStart_rifle_back_slow_lfoot_01";
	g_MultipleFootPlantSegNo[f] = 2;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.527f;
	f++;
	g_MultipleFootPlantName[f]="combat_runStart_rifle_back_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 2;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	g_MultipleFootPlantSegments[f].b	=	0.527f;
	f++;




	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------
	if(1)
	{
		g_MultipleFootPlantName[f]="relaxed_step_nw_back_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="relaxed_step_nw_back_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="relaxed_step_nw_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="relaxed_step_nw_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="relaxed_step_nw_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="relaxed_step_nw_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="relaxed_stepRotate_nw_left_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;
		g_MultipleFootPlantName[f]="relaxed_stepRotate_nw_right_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;

	//-------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="combat_step_mg_back_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_mg_back_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_step_mg_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_mg_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_step_mg_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_mg_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_stepRotate_mg_left_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;
		g_MultipleFootPlantName[f]="combat_stepRotate_mg_right_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;

	//-------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="combat_step_nw_back_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_nw_back_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_step_nw_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_nw_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_step_nw_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_nw_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_stepRotate_nw_left_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;
		g_MultipleFootPlantName[f]="combat_stepRotate_nw_right_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;

		//-----------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="combat_step_pistol_back_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_pistol_back_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_step_pistol_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_pistol_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_step_pistol_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_pistol_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_stepRotate_pistol_left_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;
		g_MultipleFootPlantName[f]="combat_stepRotate_pistol_right_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;

//--------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="combat_step_rifle_back_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_rifle_back_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_step_rifle_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_rifle_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_step_rifle_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="combat_step_rifle_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="combat_stepRotate_rifle_left_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;
		g_MultipleFootPlantName[f]="combat_stepRotate_rifle_right_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;






//-----------------------------------------------------------------
//---     crouch     ----------------------------------------------
//-----------------------------------------------------------------

		g_MultipleFootPlantName[f]="crouch_step_mg_back_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_mg_back_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_step_mg_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_mg_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_step_mg_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_mg_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_stepRotate_mg_left_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;
		g_MultipleFootPlantName[f]="crouch_stepRotate_mg_right_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;

		//-------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="crouch_step_nw_back_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_nw_back_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_step_nw_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_nw_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_step_nw_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_nw_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_stepRotate_nw_left_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;
		g_MultipleFootPlantName[f]="crouch_stepRotate_nw_right_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;

		//-----------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="crouch_step_pistol_back_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_pistol_back_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_step_pistol_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_pistol_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_step_pistol_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_pistol_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_stepRotate_pistol_left_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;
		g_MultipleFootPlantName[f]="crouch_stepRotate_pistol_right_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;

		//--------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="crouch_step_rifle_back_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_rifle_back_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_step_rifle_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_rifle_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_step_rifle_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;
		g_MultipleFootPlantName[f]="crouch_step_rifle_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		f++;

		g_MultipleFootPlantName[f]="crouch_stepRotate_rifle_left_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;
		g_MultipleFootPlantName[f]="crouch_stepRotate_rifle_right_01";
		g_MultipleFootPlantSegNo[f] = 1;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantDetectFP[f]=0;
		f++;


	}














	if (1)
	{

		//----------------------------------------------------------------------------------
		//------               Idle2Move assets  (combat nw)    -------------------------
		//----------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="combat_runStart_nw_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_nw_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_nw_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.505f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_nw_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="combat_runStart_nw_reverse_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.542f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_nw_reverse_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		//----------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="combat_runStart_nw_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.464f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_nw_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_nw_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.460f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_nw_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_nw_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.523f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_nw_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;




		//----------------------------------------------------------------------------------
		//------               Idle2Move assets  (combat mg)    -------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="combat_runStart_mg_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_mg_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_mg_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.505f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_mg_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="combat_runStart_mg_reverse_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.542f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_mg_reverse_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		//----------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="combat_runStart_mg_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.464f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_mg_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		g_MultipleFootPlantName[f]="combat_runStart_mg_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.460f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_mg_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_mg_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.523f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_mg_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;




		//----------------------------------------------------------------------------------
		//------               Idle2Move assets  (combat rifle)    -------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="combat_runStart_rifle_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rifle_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_rifle_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.505f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rifle_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="combat_runStart_rifle_reverse_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.542f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rifle_reverse_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		//----------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="combat_runStart_rifle_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.464f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rifle_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_rifle_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.460f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rifle_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_rifle_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.523f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rifle_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		//----------------------------------------------------------------------------------
		//------               Idle2Move assets  (combat pistol)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="combat_runStart_pistol_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_pistol_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_pistol_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.505f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_pistol_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="combat_runStart_pistol_reverse_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.542f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_pistol_reverse_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		//----------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="combat_runStart_pistol_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.464f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_pistol_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_pistol_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.460f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_pistol_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="combat_runStart_pistol_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.523f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_pistol_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;



		//----------------------------------------------------------------------------------
		//------               Idle2Move assets  (combat rocket)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="combat_runStart_rocket_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rocket_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_rocket_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.505f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rocket_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="combat_runStart_rocket_reverse_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.542f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rocket_reverse_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		//----------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="combat_runStart_rocket_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.464f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rocket_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="combat_runStart_rocket_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.460f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rocket_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="combat_runStart_rocket_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.523f;
		f++;
		g_MultipleFootPlantName[f]="combat_runStart_rocket_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


	}




	if (1)
	{

		//----------------------------------------------------------------------------------
		//------               Idle2Walk assets  (combat nw)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="relaxed_walkStart_nw_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.427f;
		f++;
		g_MultipleFootPlantName[f]="relaxed_walkStart_nw_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.703f; //right on ground 
		f++;

		g_MultipleFootPlantName[f]="relaxed_walkStart_nw_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.453f;
		f++;
		g_MultipleFootPlantName[f]="relaxed_walkStart_nw_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.716f; //right on ground
		f++;

		g_MultipleFootPlantName[f]="relaxed_walkStart_nw_reverse_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.460f;
		f++;
		g_MultipleFootPlantName[f]="relaxed_walkStart_nw_reverse_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.715f; //right on ground
		f++;
		
		


		//----------------------------------------------------------------------------------
		//------               Idle2Walk assets  (combat nw)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="combat_walkStart_nw_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_nw_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.763f; //right on ground 
		f++;

		g_MultipleFootPlantName[f]="combat_walkStart_nw_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.413f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_nw_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.756f; //right on ground
		f++;

		g_MultipleFootPlantName[f]="combat_walkStart_nw_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.500f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_nw_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.675f; //right on ground
		f++;



		//----------------------------------------------------------------------------------
		//------               Idle2Walk assets  (combat mg)        ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="combat_walkStart_mg_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_mg_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.763f; //right on ground 
		f++;

		g_MultipleFootPlantName[f]="combat_walkStart_mg_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.413f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_mg_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.756f; //right on ground
		f++;

		g_MultipleFootPlantName[f]="combat_walkStart_mg_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.500f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_mg_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.675f; //right on ground
		f++;


		//----------------------------------------------------------------------------------
		//------               Idle2Walk assets  (combat rifle)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="combat_walkStart_rifle_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_rifle_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.763f; //right on ground 
		f++;


		g_MultipleFootPlantName[f]="combat_walkStart_rifle_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.413f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_rifle_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.756f; //right on ground
		f++;

		g_MultipleFootPlantName[f]="combat_walkStart_rifle_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.500f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_rifle_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.675f; //right on ground
		f++;



		//----------------------------------------------------------------------------------
		//------               Idle2Walk assets  (combat pistol)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="combat_walkStart_pistol_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_pistol_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.763f; //right on ground 
		f++;


		g_MultipleFootPlantName[f]="combat_walkStart_pistol_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.413f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_pistol_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.756f; //right on ground
		f++;

		g_MultipleFootPlantName[f]="combat_walkStart_pistol_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.500f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_pistol_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.675f; //right on ground
		f++;


		//----------------------------------------------------------------------------------
		//------               Idle2Walk assets  (combat rocket)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="combat_walkStart_rocket_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_rocket_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.763f; //right on ground 
		f++;


		g_MultipleFootPlantName[f]="combat_walkStart_rocket_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.413f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_rocket_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.756f; //right on ground
		f++;

		g_MultipleFootPlantName[f]="combat_walkStart_rocket_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.500f;
		f++;
		g_MultipleFootPlantName[f]="combat_walkStart_rocket_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.675f; //right on ground
		f++;
	}







	if (1)
	{
		//----------------------------------------------------------------------------------
		//------               Idle2Move assets  (stealth nw)    -------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="stealth_runStart_nw_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_nw_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_nw_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.505f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_nw_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="stealth_runStart_nw_reverse_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.542f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_nw_reverse_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		//----------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="stealth_runStart_nw_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.464f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_nw_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_nw_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.460f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_nw_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_nw_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.523f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_nw_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		//----------------------------------------------------------------------------------
		//------               Idle2Move assets  (stealth MG)    -------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="stealth_runStart_mg_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_mg_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_mg_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.505f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_mg_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="stealth_runStart_mg_reverse_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.542f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_mg_reverse_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		//---------------------------------------------------------------------------------


		g_MultipleFootPlantName[f]="stealth_runStart_mg_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.464f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_mg_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_mg_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.460f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_mg_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_mg_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.523f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_mg_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;






		//----------------------------------------------------------------------------------
		//------               Idle2Move assets  (stealth rifle)    -------------------------
		//----------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="stealth_runStart_rifle_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rifle_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_rifle_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.505f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rifle_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="stealth_runStart_rifle_reverse_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.542f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rifle_reverse_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		//----------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="stealth_runStart_rifle_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.464f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rifle_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_rifle_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.460f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rifle_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_rifle_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.523f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rifle_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		//----------------------------------------------------------------------------------
		//------               Idle2Move assets  (stealth pistol)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="stealth_runStart_pistol_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_pistol_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_pistol_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.505f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_pistol_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="stealth_runStart_pistol_reverse_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.542f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_pistol_reverse_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		//----------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="stealth_runStart_pistol_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.464f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_pistol_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_pistol_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.460f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_pistol_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="stealth_runStart_pistol_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.523f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_pistol_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		//----------------------------------------------------------------------------------
		//------               Idle2Move assets  (stealth pistol)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="stealth_runStart_rocket_forward_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rocket_forward_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_rocket_left_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.505f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rocket_right_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="stealth_runStart_rocket_reverse_slow_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.542f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rocket_reverse_slow_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;

		//----------------------------------------------------------------------------------

		g_MultipleFootPlantName[f]="stealth_runStart_rocket_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.464f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rocket_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;


		g_MultipleFootPlantName[f]="stealth_runStart_rocket_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.460f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rocket_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.290f;
		g_MultipleFootPlantSegments[f].c	=	0.790f;
		f++;

		g_MultipleFootPlantName[f]="stealth_runStart_rocket_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.523f;
		f++;
		g_MultipleFootPlantName[f]="stealth_runStart_rocket_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.270f;
		g_MultipleFootPlantSegments[f].c	=	0.770f;
		f++;
	}




	if (1) 
	{

		//----------------------------------------------------------------------------------
		//------               Idle2Walk assets  (stealth nw)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="stealth_walkStart_nw_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_nw_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.763f; //right on ground 
		f++;

		g_MultipleFootPlantName[f]="stealth_walkStart_nw_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.413f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_nw_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.756f; //right on ground
		f++;

		g_MultipleFootPlantName[f]="stealth_walkStart_nw_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.500f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_nw_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.675f; //right on ground
		f++;




		//----------------------------------------------------------------------------------
		//------               Idle2Walk assets  (stealth rifle)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="stealth_walkStart_rifle_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_rifle_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.763f; //right on ground 
		f++;


		g_MultipleFootPlantName[f]="stealth_walkStart_rifle_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.413f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_rifle_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.756f; //right on ground
		f++;

		g_MultipleFootPlantName[f]="stealth_walkStart_rifle_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.500f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_rifle_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.675f; //right on ground
		f++;



		//----------------------------------------------------------------------------------
		//------               Idle2Walk assets  (stealth pistol)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="stealth_walkStart_pistol_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_pistol_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.763f; //right on ground 
		f++;


		g_MultipleFootPlantName[f]="stealth_walkStart_pistol_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.413f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_pistol_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.756f; //right on ground
		f++;

		g_MultipleFootPlantName[f]="stealth_walkStart_pistol_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.500f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_pistol_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.675f; //right on ground
		f++;



		//----------------------------------------------------------------------------------
		//------               Idle2Walk assets  (stealth mg)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="stealth_walkStart_mg_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_mg_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.763f; //right on ground 
		f++;

		g_MultipleFootPlantName[f]="stealth_walkStart_mg_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.413f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_mg_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.756f; //right on ground
		f++;

		g_MultipleFootPlantName[f]="stealth_walkStart_mg_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.500f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_mg_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.675f; //right on ground
		f++;

		//----------------------------------------------------------------------------------
		//------               Idle2Walk assets  (stealth mg)    ------------------------
		//----------------------------------------------------------------------------------
		g_MultipleFootPlantName[f]="stealth_walkStart_rocket_forward_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.457f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_rocket_forward_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.763f; //right on ground 
		f++;

		g_MultipleFootPlantName[f]="stealth_walkStart_rocket_left_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.413f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_rocket_right_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.756f; //right on ground
		f++;

		g_MultipleFootPlantName[f]="stealth_walkStart_rocket_reverse_fast_lfoot_01";
		g_MultipleFootPlantSegNo[f] = 2;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.500f;
		f++;
		g_MultipleFootPlantName[f]="stealth_walkStart_rocket_reverse_fast_rfoot_01";
		g_MultipleFootPlantSegNo[f] = 3;
		g_MultipleFootPlantSegments[f].a	=	0.000f;
		g_MultipleFootPlantSegments[f].b	=	0.250f; //left on ground
		g_MultipleFootPlantSegments[f].c	=	0.675f; //right on ground
		f++;
	}









	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------
	g_MultipleFootPlantName[f]="relaxed_walkStop_nw_forward_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;


	g_MultipleFootPlantName[f]="combat_runStopShort_pistol_forward_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="combat_runStopShort_rifle_forward_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="combat_runStopShort_nw_forward_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="combat_runStopShort_mg_forward_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="combat_runStopShort_rocket_forward_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;


	g_MultipleFootPlantName[f]="combat_walkStop_pistol_forward_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="combat_walkStop_rifle_forward_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="combat_walkStop_nw_forward_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="combat_walkStop_mg_forward_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="combat_walkStop_rocket_forward_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;


	g_MultipleFootPlantName[f]="stealth_runStopShort_nw_forward_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="stealth_runStopShort_pistol_forward_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="stealth_runStopShort_rifle_forward_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="stealth_runStopShort_mg_forward_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="stealth_runStopShort_rocket_forward_fast_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;


	g_MultipleFootPlantName[f]="stealth_walkStop_nw_forward_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="stealth_walkStop_pistol_forward_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="stealth_walkStop_rifle_forward_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="stealth_walkStop_mg_forward_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;
	g_MultipleFootPlantName[f]="stealth_walkStop_rocket_forward_slow_rfoot_01";
	g_MultipleFootPlantSegNo[f] = 1;
	g_MultipleFootPlantSegments[f].a	=	0.000f;
	f++;

	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------








	g_MultipleFootPlantCounter=f;

	for (uint32 i=0; i<g_MultipleFootPlantCounter; i++)
	{
		g_MultipleFootPlantGlobalID[i] = -1; 
		int32 id = GetAnimIDByName(g_MultipleFootPlantName[i]);
		if (id>=0)
		{
			const ModelAnimationHeader* pAnim = GetModelAnimationHeader(id);
			if (pAnim)
			{
				uint32 GlobalAnimId = pAnim->m_nGlobalAnimId;
				g_MultipleFootPlantGlobalID[i]=GlobalAnimId;

				GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimId];
				rGAH.m_Segments = g_MultipleFootPlantSegNo[i];
				rGAH.m_SegmentsTime[0] = g_MultipleFootPlantSegments[i].a;
				rGAH.m_SegmentsTime[1] = g_MultipleFootPlantSegments[i].b;
				rGAH.m_SegmentsTime[2] = g_MultipleFootPlantSegments[i].c;
				rGAH.m_SegmentsTime[3] = g_MultipleFootPlantSegments[i].d;
				rGAH.m_SegmentsTime[4] = g_MultipleFootPlantSegments[i].e;

				rGAH.m_FootPlantVectors.m_LHeelStart=-10000.0f; rGAH.m_FootPlantVectors.m_LHeelEnd=-10000.0f;
				rGAH.m_FootPlantVectors.m_LToe0Start=-10000.0f; rGAH.m_FootPlantVectors.m_LToe0End=-10000.0f;
				rGAH.m_FootPlantVectors.m_RHeelStart=-10000.0f; rGAH.m_FootPlantVectors.m_RHeelEnd=-10000.0f;
				rGAH.m_FootPlantVectors.m_RToe0Start=-10000.0f; rGAH.m_FootPlantVectors.m_RToe0End=-10000.0f;
			}
		}
	}
}



//---------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------
void CAnimationSet::SetFootplantBitsAutomatically( std::vector< std::vector<DebugJoint> >& arrSkeletons, int32 nAnimID,int32 nGlobalAnimID,int32 lHidx,int32 rHidx,int32 lTidx,int32 rTidx,int32 lNidx,int32 rNidx, uint8 detect )
{

	CreateSkeletonArray( arrSkeletons, nAnimID);

#define FLOORDIST (0.01f)
#define SPEED_XY (0.01f)

	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID];

	std::vector<f32> g_arrLHeelVelocity;
	std::vector<f32> g_arrLToe0Velocity;
	std::vector<f32> g_arrLToeNub0Velocity;
	std::vector<f32> g_arrRHeelVelocity;
	std::vector<f32> g_arrRToe0Velocity;
	std::vector<f32> g_arrRToeNub0Velocity;


	uint32 numPoses=arrSkeletons.size();

	if (lHidx>0 && rHidx>0 && lTidx>0 && rTidx>0 && lNidx>0 && rNidx>0)
	{
		//arrFootPlants.resize(numPoses);
		//nGlobalAnimID
		if (rGlobalAnimHeader.m_FootPlantBits.empty())
			rGlobalAnimHeader.m_FootPlantBits.resize(numPoses);

		assert (numPoses == rGlobalAnimHeader.m_FootPlantBits.size());
		for (uint32 i=0; i<numPoses; i++)	
			rGlobalAnimHeader.m_FootPlantBits[i]=0; //disable all of them

		if (detect==0)
			return;


		g_arrLHeelVelocity.resize(numPoses);
		g_arrRHeelVelocity.resize(numPoses);
		g_arrLToe0Velocity.resize(numPoses);
		g_arrRToe0Velocity.resize(numPoses);
		g_arrLToeNub0Velocity.resize(numPoses);
		g_arrRToeNub0Velocity.resize(numPoses);

		for (uint32 s=0; s<numPoses; s++) 
		{
			g_arrLHeelVelocity[s]=1000.0f;
			g_arrRHeelVelocity[s]=1000.0f;

			g_arrLToe0Velocity[s]=1000.0f;
			g_arrRToe0Velocity[s]=1000.0f;

			g_arrLToeNub0Velocity[s]=1000.0f;
			g_arrRToeNub0Velocity[s]=1000.0f;
		}


		Plane plane;
		f32 sloperad = rGlobalAnimHeader.m_fSlope;
		plane.n=Matrix33::CreateRotationX( sloperad ) * Vec3(0,0,1);
		plane.d=0;

		f32 dist=0;
		Vec3 sq; Vec3 zp;
		for (uint32 s=1; s<numPoses; s++) 
		{
			sq= arrSkeletons[s][lHidx].m_AbsoluteQuat.t - arrSkeletons[s-1][lHidx].m_AbsoluteQuat.t; sq.z=0;
			zp=(arrSkeletons[s][lHidx].m_AbsoluteQuat.t + arrSkeletons[s-1][lHidx].m_AbsoluteQuat.t )*0.5f;
			dist=(plane|zp);
			g_arrLHeelVelocity[s]=(dist<FLOORDIST) ? sq.GetLength() : 1000.0f;
			sq= arrSkeletons[s][rHidx].m_AbsoluteQuat.t - arrSkeletons[s-1][rHidx].m_AbsoluteQuat.t; sq.z=0;
			zp=(arrSkeletons[s][rHidx].m_AbsoluteQuat.t + arrSkeletons[s-1][rHidx].m_AbsoluteQuat.t )*0.5f;
			dist=(plane|zp);
			g_arrRHeelVelocity[s]=(dist<FLOORDIST) ? sq.GetLength() : 1000.0f;;

			sq= arrSkeletons[s][lTidx].m_AbsoluteQuat.t - arrSkeletons[s-1][lTidx].m_AbsoluteQuat.t; sq.z=0;
			zp=(arrSkeletons[s][lTidx].m_AbsoluteQuat.t + arrSkeletons[s-1][lTidx].m_AbsoluteQuat.t)*0.5f;
			dist=(plane|zp);
			g_arrLToe0Velocity[s]=(dist<FLOORDIST) ? sq.GetLength() : 1000.0f;
			sq= arrSkeletons[s][rTidx].m_AbsoluteQuat.t - arrSkeletons[s-1][rTidx].m_AbsoluteQuat.t; sq.z=0;
			zp=(arrSkeletons[s][rTidx].m_AbsoluteQuat.t + arrSkeletons[s-1][rTidx].m_AbsoluteQuat.t )*0.5f;
			dist=(plane|zp);
			g_arrRToe0Velocity[s]=(dist<FLOORDIST) ? sq.GetLength() : 1000.0f;

			sq= arrSkeletons[s][lNidx].m_AbsoluteQuat.t - arrSkeletons[s-1][lNidx].m_AbsoluteQuat.t; sq.z=0;
			zp=(arrSkeletons[s][lNidx].m_AbsoluteQuat.t + arrSkeletons[s-1][lNidx].m_AbsoluteQuat.t)*0.5f;
			dist=(plane|zp);
			g_arrLToeNub0Velocity[s]=(dist<FLOORDIST) ? sq.GetLength() : 1000.0f;
			sq= arrSkeletons[s][rNidx].m_AbsoluteQuat.t - arrSkeletons[s-1][rNidx].m_AbsoluteQuat.t; sq.z=0;
			zp=(arrSkeletons[s][rNidx].m_AbsoluteQuat.t + arrSkeletons[s-1][rNidx].m_AbsoluteQuat.t)*0.5f;
			dist=(plane|zp);
			g_arrRToeNub0Velocity[s]=(dist<FLOORDIST) ? sq.GetLength() : 1000.0f;
		}

		g_arrLHeelVelocity[0]=g_arrLHeelVelocity[numPoses-1];
		g_arrRHeelVelocity[0]=g_arrRHeelVelocity[numPoses-1];
		g_arrLToe0Velocity[0]=g_arrLToe0Velocity[numPoses-1];
		g_arrRToe0Velocity[0]=g_arrRToe0Velocity[numPoses-1];
		g_arrLToeNub0Velocity[0]=g_arrLToeNub0Velocity[numPoses-1];
		g_arrRToeNub0Velocity[0]=g_arrRToeNub0Velocity[numPoses-1];

		for (uint32 s=0; s<numPoses; s++) 
		{

			uint8 test = rGlobalAnimHeader.m_FootPlantBits[s];
			rGlobalAnimHeader.m_FootPlantBits[s]=0;

			if (g_arrLHeelVelocity[s]<SPEED_XY)
				rGlobalAnimHeader.m_FootPlantBits[s]|=LHEEL;
			if (g_arrRHeelVelocity[s]<SPEED_XY)
				rGlobalAnimHeader.m_FootPlantBits[s]|=RHEEL;

			if (g_arrLToe0Velocity[s]<SPEED_XY)
				rGlobalAnimHeader.m_FootPlantBits[s]|=LTOE0;
			if (g_arrRToe0Velocity[s]<SPEED_XY)
				rGlobalAnimHeader.m_FootPlantBits[s]|=RTOE0;

			if (g_arrLToeNub0Velocity[s]<SPEED_XY)
				rGlobalAnimHeader.m_FootPlantBits[s]|=LNUB0;
			if (g_arrRToeNub0Velocity[s]<SPEED_XY)
				rGlobalAnimHeader.m_FootPlantBits[s]|=RNUB0;

			//if (test != )
		//	assert(test == rGlobalAnimHeader.m_FootPlantBits[s]);
		}

	}
}








//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
void CAnimationSet::CreateSkeletonArray( std::vector< std::vector<DebugJoint> >& arrSkeletons, uint32 nAnimID )
{
	Diag33 temp;
	const ModelAnimationHeader* pAnim = GetModelAnimationHeader(nAnimID);
	GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[pAnim->m_nGlobalAnimId];

	f32 duration = rGAH.m_fEndSec - rGAH.m_fStartSec;

	f64 PosesPerKey = (1.0/60.0);
	uint32 HowManyPoses = uint32(duration/PosesPerKey);//= HowManyPoses; 
	if (HowManyPoses==0)
		HowManyPoses=1;

	//	rGAH.m_FootPlantBits.resize(HowManyPoses);

	arrSkeletons.resize(HowManyPoses);
	uint32 numJoints = m_pModel->m_pModelSkeleton->m_arrModelJoints.size();
	for(uint32 i=0; i<HowManyPoses; i++)
		arrSkeletons[i].resize(numJoints);

	const CModelJoint* pModelJoint = &m_pModel->m_pModelSkeleton->m_arrModelJoints[0];
	for (uint32 s=0; s<HowManyPoses; s++) 
	{
		//evaluate all controllers for this animation
		f32 newtime = (1.0f/HowManyPoses)*s; 
		for (uint32 j=0; j<numJoints; j++)
		{
			arrSkeletons[s][j].m_RelativeQuat=pModelJoint[j].m_DefaultRelPose;
			IController* pController = g_AnimationManager.m_arrGlobalAnimations[pAnim->m_nGlobalAnimId].GetControllerByJointID(pModelJoint[j].m_nJointCRC32); 
			if (pController)
				pController->GetOP(pAnim->m_nGlobalAnimId, newtime, arrSkeletons[s][j].m_RelativeQuat.q, arrSkeletons[s][j].m_RelativeQuat.t);
		}

		//calculate absolute joints
		arrSkeletons[s][0].m_AbsoluteQuat = arrSkeletons[s][0].m_RelativeQuat;
		for (uint32 i=1; i<numJoints; i++)
		{
			int16 idx = arrSkeletons[s][i].m_idxParent =	m_pModel->m_pModelSkeleton->m_arrModelJoints[i].m_idxParent;
			arrSkeletons[s][i].m_AbsoluteQuat	= arrSkeletons[s][idx].m_AbsoluteQuat * arrSkeletons[s][i].m_RelativeQuat;
			arrSkeletons[s][i].m_AbsoluteQuat.q.Normalize();
		}

	}
}



//--------------------------------------------------------------------------
std::vector<ColorRGB> g_arrDistMap24;

void CAnimationSet::SetFootplantVectors( std::vector< std::vector<DebugJoint> >& arrSkeletons, uint32 nAnimID, SFootPlant& rFootPlants, uint32 nGlobalAnimID )
{

#define lm0 (10)
#define lm1 (11)
#define rm0 (13)
#define rm1 (14)

#define lx0 (16)
#define lx1 (17)
#define rx0 (18)
#define rx1 (19)


	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID];
	int32 poses0 = rGlobalAnimHeader.m_FootPlantBits.size();

	g_arrDistMap24.resize(poses0*20);
	for (int32 i=0; i<(poses0*20); i++)
	{
		g_arrDistMap24[i].r=0;
		g_arrDistMap24[i].g=0;
		g_arrDistMap24[i].b=0;
	}

	int32 hposes=poses0/2;
	for (int32 i=0; i<poses0; i++)
	{

		uint8 FootStep = rGlobalAnimHeader.m_FootPlantBits[i];
		if (FootStep&LHEEL)
			g_arrDistMap24[i+poses0*0].b=0xff;
		if (FootStep&LTOE0)
			g_arrDistMap24[i+poses0*1].r=0xff;
		if (FootStep&LNUB0)
			g_arrDistMap24[i+poses0*2].g=0xff;

		if (FootStep&RHEEL)
			g_arrDistMap24[i+poses0*5].b=0xff;
		if (FootStep&RTOE0)
			g_arrDistMap24[i+poses0*6].r=0xff;
		if (FootStep&RNUB0)
			g_arrDistMap24[i+poses0*7].g=0xff;

		//---------------------------------------------------
		if (i<hposes)
		{
			assert((i+hposes)<poses0);
			if (FootStep&LHEEL)
				g_arrDistMap24[i+poses0*lm0+hposes].b=0xff;
			if (FootStep&LTOE0)
				g_arrDistMap24[i+poses0*lm1+hposes].r=0xff;
			if (FootStep&LNUB0)
				g_arrDistMap24[i+poses0*lm1+hposes].r=0xff;
		} 
		else 
		{
			if (FootStep&LHEEL)
				g_arrDistMap24[i+poses0*lm0-hposes].b=0xff;
			if (FootStep&LTOE0)
				g_arrDistMap24[i+poses0*lm1-hposes].r=0xff;
			if (FootStep&LNUB0)
				g_arrDistMap24[i+poses0*lm1-hposes].r=0xff;
		}

		if (FootStep&RHEEL)
			g_arrDistMap24[i+poses0*rm0].b=0xff;
		if (FootStep&RTOE0)
			g_arrDistMap24[i+poses0*rm1].r=0xff;
		if (FootStep&RNUB0)
			g_arrDistMap24[i+poses0*rm1].r=0xff;
	}		

	f32 r=0.05f;
	//delete area in footsteps
	for (f32 i=0.0f; i<r; i=i+0.001f)
	{
		//start of step
		uint32 idx0=(uint32)(i*(poses0));
		g_arrDistMap24[idx0+poses0*lm0].b=0;
		g_arrDistMap24[idx0+poses0*lm1].r=0;
		g_arrDistMap24[idx0+poses0*rm0].b=0;
		g_arrDistMap24[idx0+poses0*rm1].r=0;

		//end of step
		uint32 idx1=(uint32)( (1.0f-r+i) *(poses0));
		assert(idx1<(uint32)poses0);
		g_arrDistMap24[idx1+poses0*lm0].b=0;
		g_arrDistMap24[idx1+poses0*lm1].r=0;
		g_arrDistMap24[idx1+poses0*rm0].b=0;
		g_arrDistMap24[idx1+poses0*rm1].r=0;
	}

	int16 lhs=-1;
	int16 lhe=-1;
	int16 lts=-1;
	int16 lte=-1;
	int16 rhs=-1;
	int16 rhe=-1;
	int16 rts=-1;
	int16 rte=-1;
	for (int32 i=0,t=poses0-1; i<poses0; i++,t--)
	{
		if (lhs==-1)
			if (g_arrDistMap24[i+poses0*lm0].b==0xff) lhs=i;
		if (lts==-1)
			if (g_arrDistMap24[i+poses0*lm1].r==0xff) lts=i;
		if (lhe==-1)
			if (g_arrDistMap24[t+poses0*lm0].b==0xff) lhe=t;
		if (lte==-1)
			if (g_arrDistMap24[t+poses0*lm1].r==0xff) lte=t;

		if (rhs==-1)
			if (g_arrDistMap24[i+poses0*rm0].b==0xff) rhs=i;
		if (rts==-1)
			if (g_arrDistMap24[i+poses0*rm1].r==0xff) rts=i;
		if (rhe==-1)
			if (g_arrDistMap24[t+poses0*rm0].b==0xff) rhe=t;
		if (rte==-1)
			if (g_arrDistMap24[t+poses0*rm1].r==0xff) rte=t;
	}

	if (lhs!=-1 && lhe!=-1)
		for (int32 i=lhs; i<lhe; i++)	g_arrDistMap24[i+poses0*lm0].b=0xff;
	if (lts!=-1 && lte!=-1)
		for (int32 i=lts; i<lte; i++)	g_arrDistMap24[i+poses0*lm1].r=0xff;

	if (rhs!=-1 && rhe!=-1)
		for (int32 i=rhs; i<rhe; i++)	g_arrDistMap24[i+poses0*rm0].b=0xff;
	if (rts!=-1 && rte!=-1)
		for (int32 i=rts; i<rte; i++)	g_arrDistMap24[i+poses0*rm1].r=0xff;

	//------------------------------------------------------------------------------------

	lhs=-1;
	lhe=-1;
	lts=-1;
	lte=-1;
	rhs=-1;
	rhe=-1;
	rts=-1;
	rte=-1;
	for (int32 i=0,t=poses0-1; i<poses0; i++,t--)
	{
		if (lhs==-1)
			if (g_arrDistMap24[i+poses0*lm0].b==0xff) lhs=i;
		if (lts==-1)
			if (g_arrDistMap24[i+poses0*lm1].r==0xff) lts=i;
		if (lhe==-1)
			if (g_arrDistMap24[t+poses0*lm0].b==0xff) lhe=t;
		if (lte==-1)
			if (g_arrDistMap24[t+poses0*lm1].r==0xff) lte=t;

		if (rhs==-1)
			if (g_arrDistMap24[i+poses0*rm0].b==0xff) rhs=i;
		if (rts==-1)
			if (g_arrDistMap24[i+poses0*rm1].r==0xff) rts=i;
		if (rhe==-1)
			if (g_arrDistMap24[t+poses0*rm0].b==0xff) rhe=t;
		if (rte==-1)
			if (g_arrDistMap24[t+poses0*rm1].r==0xff) rte=t;
	}

	if (lhs!=-1 && lhe!=-1 && rhs!=-1 && rhe!=-1)
	{
		if ( (lhe-hposes)>rhs )
		{
			int32 slh=((lhe-hposes)+rhs)/2;
			g_arrDistMap24[slh+hposes+poses0*lm0].g=0xff;	//left heel start
			g_arrDistMap24[slh+poses0*rm0].g=0x3f;	//right heel end
			for (int32 i=(slh+hposes); i<poses0; i++)
				g_arrDistMap24[i+poses0*lm0].b=0;	//left heel start
			for (int32 i=0; i<slh; i++)
				g_arrDistMap24[i+poses0*rm0].b=0;	//right heel start
		}

		if ( (lhs+hposes)<rhe )
		{
			int32 elh=((lhs+hposes)+rhe)/2;
			g_arrDistMap24[elh-hposes+poses0*lm0].g=0x7f;	//left heel start
			g_arrDistMap24[elh+poses0*rm0].g=0x3f;	//right heel end
			for (int32 i=0; i<(elh-hposes); i++)
				g_arrDistMap24[i+poses0*lm0].b=0;	//left heel start
			for (int32 i=elh; i<poses0; i++)
				g_arrDistMap24[i+poses0*rm0].b=0;	//right heel end
		}
	}

	if (lts!=-1 && lte!=-1 && rts!=-1 && rte!=-1)
	{
		if ( (lte-hposes)>rts )
		{
			int32 slt=((lte-hposes)+rts)/2;
			//g_arrDistMap24[slt+hposes+poses0*lm1].g=0xff;	//right toe start
			//g_arrDistMap24[slt+poses0*rm1].g=0x3f;	//left toe start
			for (int32 i=(slt+hposes); i<poses0; i++)
				g_arrDistMap24[i+poses0*lm1].r=0;	//right toe start
			for (int32 i=0; i<slt; i++)
				g_arrDistMap24[i+poses0*rm1].r=0;	//left toe start
		}

		if ( (lts+hposes)<rte )
		{
			int32 elt=((lts+hposes)+rte)/2;
			//g_arrDistMap24[elt-hposes+poses0*lm1].g=0x7f;	//left toe start
			//g_arrDistMap24[elt+poses0*rm1].g=0x3f;	//right toe end
			for (int32 i=0; i<(elt-hposes); i++)
				g_arrDistMap24[i+poses0*lm1].r=0;	//left toe start
			for (int32 i=elt; i<poses0; i++)
				g_arrDistMap24[i+poses0*rm1].r=0;	//right toe end
		}
	}

	//-----------------------------------------------------------------------------

	for (int32 i=0; i<poses0; i++)
	{
		if (i<hposes)
		{
			g_arrDistMap24[i+poses0*lx0+hposes].b=g_arrDistMap24[i+poses0*lm0].b;	//left toe start
			g_arrDistMap24[i+poses0*lx1+hposes].r=g_arrDistMap24[i+poses0*lm1].r;	//left toe start
		}
		else
		{
			g_arrDistMap24[i+poses0*lx0-hposes].b=g_arrDistMap24[i+poses0*lm0].b;	//left toe start
			g_arrDistMap24[i+poses0*lx1-hposes].r=g_arrDistMap24[i+poses0*lm1].r;	//left toe start
		}

		g_arrDistMap24[i+poses0*rx0].b=g_arrDistMap24[i+poses0*rm0].b;	//left toe start
		g_arrDistMap24[i+poses0*rx1].r=g_arrDistMap24[i+poses0*rm1].r;	//left toe start
	}

	for (int32 i=0; i<poses0; i++)
	{
		uint32 footplant=0;
		if (g_arrDistMap24[i+poses0*lx0].b==0xff)
			footplant|=LHEEL;
		if (g_arrDistMap24[i+poses0*lx1].r==0xff)
			footplant|=LTOE0;
		if (g_arrDistMap24[i+poses0*rx0].b==0xff)
			footplant|=RHEEL;
		if (g_arrDistMap24[i+poses0*rx1].r==0xff)
			footplant|=RTOE0;

//		if (rGlobalAnimHeader.m_FootPlantBits[i]!=footplant)
		rGlobalAnimHeader.m_FootPlantBits[i]=footplant;

	}

	//------------------------------------------------------------------

	lhs=-1;
	lhe=-1;
	lts=-1;
	lte=-1;
	rhs=-1;
	rhe=-1;
	rts=-1;
	rte=-1;
	for (int32 i=0,t=poses0-1; i<poses0; i++,t--)
	{
		if (lhs==-1)
			if (g_arrDistMap24[i+poses0*lm0].b==0xff) lhs=i;
		if (lts==-1)
			if (g_arrDistMap24[i+poses0*lm1].r==0xff) lts=i;
		if (lhe==-1)
			if (g_arrDistMap24[t+poses0*lm0].b==0xff) lhe=t;
		if (lte==-1)
			if (g_arrDistMap24[t+poses0*lm1].r==0xff) lte=t;

		if (rhs==-1)
			if (g_arrDistMap24[i+poses0*rm0].b==0xff) rhs=i;
		if (rts==-1)
			if (g_arrDistMap24[i+poses0*rm1].r==0xff) rts=i;
		if (rhe==-1)
			if (g_arrDistMap24[t+poses0*rm0].b==0xff) rhe=t;
		if (rte==-1)
			if (g_arrDistMap24[t+poses0*rm1].r==0xff) rte=t;
	}


	f32 pose = f32(poses0);


	f32 oldv = rFootPlants.m_LHeelStart;
	f32 newv;

	if (lhs!=-1)
	{
		newv = rFootPlants.m_LHeelStart	=	lhs/pose;
		//assert(fabs(newv - oldv) < 0.000001f);
	}

	oldv = rFootPlants.m_LHeelEnd;
	if (lhe!=-1)
	{
		newv = rFootPlants.m_LHeelEnd		=	lhe/pose;
		//assert(fabs(newv - oldv) < 0.000001f);
	}

	oldv = rFootPlants.m_LToe0Start;
	if (lts!=-1)
	{
		newv = rFootPlants.m_LToe0Start	=	lts/pose;
		//assert(fabs(newv - oldv) < 0.000001f);
	}

	oldv = rFootPlants.m_LToe0End;
	if (lte!=-1)
	{
		newv = rFootPlants.m_LToe0End		=	lte/pose;
	}

	oldv = rFootPlants.m_RHeelStart;
	if (rhs!=-1)
	{
		newv = rFootPlants.m_RHeelStart	=	rhs/pose;;
		//		assert(fabs(newv - oldv) < 0.000001f);
	}

	oldv = rFootPlants.m_RHeelEnd;
	if (rhe!=-1)
	{
		newv = rFootPlants.m_RHeelEnd	=	rhe/pose;;
		//		assert(fabs(newv - oldv) < 0.000001f);
	}

	oldv = rFootPlants.m_RToe0Start;
	if (rts!=-1)
	{
		newv = rFootPlants.m_RToe0Start	=	rts/pose;;
		//		assert(fabs(newv - oldv) < 0.000001f);
	}

	oldv = rFootPlants.m_RToe0End;
	if (rte!=-1)
	{
		newv = rFootPlants.m_RToe0End		=	rte/pose;;
		//		assert(fabs(newv - oldv) < 0.000001f);
	}




}














//--------------------------------------------------------------------------
#define TO_HEX(r,g,b,a) ( (uint32) (((uint8)(r)|((uint16)((uint8)(g))<<8))|(((uint32)(uint8)(b))<<16)) | (((uint32)(uint8)(a))<<24) )

uint32 CAnimationSet::SetFootplantBitsManually( int32 nGlobalAnimID )
{
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID];
	f32 duration = rGlobalAnimHeader.m_fEndSec - rGlobalAnimHeader.m_fStartSec;
	f64 PosesPerKey = (1.0/60.0);
	uint32 HowManyPoses = uint32(duration/PosesPerKey);//= HowManyPoses; 
	if (HowManyPoses==0)
		HowManyPoses=1;


	f32 LTS=-1.0f; f32 LHS=-1.0f;	f32 LTO=-1.0f;	f32 LHO=-1.0f;
	f32 RTS=-1.0f; f32 RHS=-1.0f;	f32 RTO=-1.0f;	f32 RHO=-1.0f;
	uint32 footevents=0;

	uint32 numEvents = rGlobalAnimHeader.m_AnimEvents.size();
	for (uint32 i=0; i<numEvents; i++ )
	{
		const char* AEvent = rGlobalAnimHeader.m_AnimEvents[i].m_strEventName;
		switch( *(uint32*)AEvent )
		{

		case TO_HEX('#','L','T','S'):
			LTS=rGlobalAnimHeader.m_AnimEvents[i].m_time;
			footevents|=0x01;
			break;
		case TO_HEX('#','L','H','S'):
			LHS=rGlobalAnimHeader.m_AnimEvents[i].m_time;
			footevents|=0x02;
			break;
		case TO_HEX('#','L','T','O'):
			LTO=rGlobalAnimHeader.m_AnimEvents[i].m_time;
			footevents|=0x04;
			break;
		case TO_HEX('#','L','H','O'):
			LHO=rGlobalAnimHeader.m_AnimEvents[i].m_time;
			footevents|=0x08;
			break;

		case TO_HEX('#','R','T','S'):
			RTS=rGlobalAnimHeader.m_AnimEvents[i].m_time;
			footevents|=0x10;
			break;
		case TO_HEX('#','R','H','S'):
			RHS=rGlobalAnimHeader.m_AnimEvents[i].m_time;
			footevents|=0x20;
			break;
		case TO_HEX('#','R','T','O'):
			RTO=rGlobalAnimHeader.m_AnimEvents[i].m_time;
			footevents|=0x40;
			break;
		case TO_HEX('#','R','H','O'):
			RHO=rGlobalAnimHeader.m_AnimEvents[i].m_time;
			footevents|=0x80;
			break;

		}
	}

	//-------------------------------------------------------------					

	if (footevents != 0xff)
		return 0;

	rGlobalAnimHeader.m_FootPlantBits.resize(HowManyPoses);
	for (uint32 i=0; i<HowManyPoses; i++)	
		rGlobalAnimHeader.m_FootPlantBits[i]=0;

	SetFootBits(rGlobalAnimHeader,HowManyPoses,LTS,LTO,LTOE0);
	SetFootBits(rGlobalAnimHeader,HowManyPoses,LHS,LHO,LHEEL);

	SetFootBits(rGlobalAnimHeader,HowManyPoses,RTS,RTO,RTOE0);
	SetFootBits(rGlobalAnimHeader,HowManyPoses,RHS,RHO,RHEEL);


	/*	for (uint32 s=0; s<numPoses; s++) 
	{
	if (g_arrLHeelVelocity[s]<SPEED_XY)
	rGlobalAnimHeader.m_FootPlantBits[s]|=LHEEL;
	if (g_arrRHeelVelocity[s]<SPEED_XY)
	rGlobalAnimHeader.m_FootPlantBits[s]|=RHEEL;

	if (g_arrLToe0Velocity[s]<SPEED_XY)
	rGlobalAnimHeader.m_FootPlantBits[s]|=LTOE0;
	if (g_arrRToe0Velocity[s]<SPEED_XY)
	rGlobalAnimHeader.m_FootPlantBits[s]|=RTOE0;

	if (g_arrLToeNub0Velocity[s]<SPEED_XY)
	rGlobalAnimHeader.m_FootPlantBits[s]|=LNUB0;
	if (g_arrRToeNub0Velocity[s]<SPEED_XY)
	rGlobalAnimHeader.m_FootPlantBits[s]|=RNUB0;
	}*/


	return 1;
}


void CAnimationSet::SetFootBits(GlobalAnimationHeader& rGlobalAnimHeader, uint32 HowManyPoses,f32 LTS,f32 LTO, uint32 FootBit)
{
	f32 frequency = 0.5f/HowManyPoses;
	if (LTS<=LTO)
		for (f32 t=LTS; t<LTO; t+=frequency )
		{
			uint32 p = uint32(t*HowManyPoses);
			if (p>=HowManyPoses)
				p=HowManyPoses-1;
			rGlobalAnimHeader.m_FootPlantBits[p] |= FootBit;
		}
	else
	{
		for (f32 t=0; t<LTO; t+=frequency )
		{
			uint32 p = uint32(t*HowManyPoses);
			if (p>=HowManyPoses)
				p=HowManyPoses-1;
			rGlobalAnimHeader.m_FootPlantBits[p] |= FootBit;
		}
		for (f32 t=LTS; t<1.0f; t+=frequency )
		{
			uint32 p = uint32(t*HowManyPoses);
			if (p>=HowManyPoses)
				p=HowManyPoses-1;
			rGlobalAnimHeader.m_FootPlantBits[p] |= FootBit;
		}

	}
}

//--------------------------------------------------------------------------

