//////////////////////////////////////////////////////////////////////
//
// CryEngine Source code
//
// File:Skeleton.cpp
// Implementation of Skeleton class
//
// History:
// January 12, 2005: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#ifndef _SKELETON_POSE_H
#define _SKELETON_POSE_H


#include "ModelAnimationSet.h"

class CCharacterModel;
class CSkinInstance;
class CCharInstance;
class CSkeletonAnim;
class CSkeletonPose;


#define RIGHT_ARM_AIM (10) //the amount of bones we need for the right arm





#define MAX_MOTIONBLUR_FRAMES (2)
#define CA_FADEOUT (0x40000000) 





#define LHEEL (0x01)
#define RHEEL (0x02)
#define LTOE0 (0x04)
#define RTOE0 (0x08)
#define LNUB0 (0x10)
#define RNUB0 (0x20)




struct CPhysicsJoint
{
	CPhysicsJoint() : m_DefaultRelativeQuat(IDENTITY), m_qRelFallPlay(IDENTITY)
	{
		m_qRelPhysParent[0].SetIdentity();
		m_qRelPhysParent[1].SetIdentity();
	}

	QuatT m_DefaultRelativeQuat;		//default relative joint (can be different for every instance)
	Quat m_qRelPhysParent[2];				// default orientation relative to the physicalized parent
	Quat m_qRelFallPlay;
};

struct CCGAJoint
{ 
	CCGAJoint() : m_CGAObjectInstance(0), m_pRNTmpData(0),m_qqqhasPhysics(~0),m_pMaterial(0) {}
	~CCGAJoint() 
	{
		if(m_pRNTmpData)
			GetISystem()->GetI3DEngine()->FreeRNTmpData(&m_pRNTmpData);
		assert(!m_pRNTmpData);
	};

	_smart_ptr<IStatObj> m_CGAObjectInstance;	//Static object controlled by this joint (this can be different for every instance).
	struct CRNTmpData * m_pRNTmpData; // pointer to LOD transition state (allocated in 3dngine for visible objects)
	int m_qqqhasPhysics;						//>=0 if have physics (don't make it int16!!)
	_smart_ptr<IMaterial> m_pMaterial; // custom material override
};


struct ColorRGB{ uint8 r,g,b; };


struct SPostProcess
{
	QuatT m_RelativePose;
	uint32 m_JointIdx;

	SPostProcess(){};

	SPostProcess(int idx,const QuatT& qp)
	{
		m_JointIdx=idx;
		m_RelativePose=qp;
	}
};


struct aux_bone_info {
	quaternionf quat0;
	Vec3 dir0;
	int iBone;
	f32 rlen0;
};

struct aux_phys_data {
	IPhysicalEntity *pPhysEnt;
	Vec3 *pVtx;
	Vec3 *pSubVtx;
	const char *strName;
	aux_bone_info *pauxBoneInfo;
	int nChars;
	int nBones;
	int iBoneTiedTo[2];
	int nSubVtxAlloc;
	bool bPhysical;
	bool bTied0,bTied1;
};



struct SAimDirection 
{
	Vec3 start;
	Vec3 end;
	uint32 evaluated;

	//default Lineseg constructor (without initialisation)
	inline SAimDirection( void ) 
	{
		evaluated=0;
	}
	inline SAimDirection( const Vec3 &s, const Vec3 &e, uint32 eval ) {  start=s; end=e; evaluated=eval; }
	inline void operator () (  const Vec3 &s, const Vec3 &e, uint32 eval ) {  start=s; end=e; evaluated=eval;  }
};


struct SRecoil 
{
	f32 m_fAnimTime;
	f32 m_fDuration;
	f32 m_fStrengh;
	uint32 m_nArms;
	SRecoil()	{	void Init(); }

	void Init()
	{
		m_fAnimTime	=100.0f;
		m_fDuration	=0.0f;
		m_fStrengh	=0.0f;
		m_nArms			=3;
	};
};

struct SAimInfo 
{
	int16	m_numAimPoses;
	int16	m_pad;
	int16	m_nGlobalAimID0;
	int16	m_nGlobalAimID1;
	f32		m_fWeight;
	f32		m_fAnimTime;
};


struct IPol
{
	Vec3 iepv;
	Vec3 ieoutput;
	Vec3 ipv;
	Vec3 output;
	uint32 k0,k1,k2;
	f32 distance;
	uint32 inside;
};


struct SAimIK 
{
	int8	m_UseAimIK;
	int8	m_numActiveAnims;		//active animation in the FIFO queue

	int16 m_nLastValidAimPose;
	SAimInfo m_AimInfo[MAX_EXEC_QUEUE]; 

	Vec3	m_AimIKTarget;
	Vec3	m_AimIKTargetSmooth;
	Vec3	m_AimIKTargetRate;
	f32	  m_AimIKTargetSmoothTime;
	Vec3	m_AimDirection;			//just for debugging

	f32 m_AimIKBlend;
	f32 m_AimIKInfluence;

	uint16 m_AimIKFadeout;
	uint16 m_AimIKFadeoutFlag;
	f32		 m_fAimIKFadeInTime;
	f32		 m_fAimIKFadeOutTime;


	SAimIK() { Init(); }

	void Init()
	{
		m_UseAimIK=0;
		m_numActiveAnims=0;

		m_nLastValidAimPose=-1;
		for (uint32 i=0; i<MAX_EXEC_QUEUE; i++)
		{
			m_AimInfo[i].m_fWeight				= 0; 
			m_AimInfo[i].m_numAimPoses		= 0; 
			m_AimInfo[i].m_nGlobalAimID0	= -1; 
			m_AimInfo[i].m_nGlobalAimID1	= -1;
			m_AimInfo[i].m_fAnimTime			= 0.0f;
		}

		m_AimIKTarget				=	Vec3(ZERO);
		m_AimIKTargetSmooth	=	Vec3(ZERO);
		m_AimIKTargetRate		=	Vec3(ZERO);
		m_AimIKTargetSmoothTime = 0.10f;
		m_AimDirection			=	Vec3(0,1,0);		//just for debugging

		m_AimIKBlend				=0.0f; //percentage value between animation and aim-ik
		m_AimIKInfluence		=0.0f;

		m_AimIKFadeout			=0;
		m_AimIKFadeoutFlag	=1;
		m_fAimIKFadeOutTime = 1.0f/0.6f;  // = 1.0f/TransitionTime
		m_fAimIKFadeInTime = 1.0f/0.3f;  // = 1.0f/TransitionTime
	}
};


#define LOOKIK_BLEND_RATIOS 5
struct SLookIK 
{
	uint8 m_UseLookIK;
	uint8 m_AllowAdditionalTransforms;
	uint16 m_LookIKFadeout;

	Vec3 m_LookDirection;		//just for debugging

	Vec3 m_LookIKTarget;
	Vec3 m_LookIKTargetSmooth;
	Vec3 m_LookIKTargetSmoothRate;

	f32 m_LookIKBlend;
	f32 m_LookIKFOR;

	f32 m_lookIKBlends[LOOKIK_BLEND_RATIOS];
	f32 m_lookIKBlendsSmooth[LOOKIK_BLEND_RATIOS];
	f32 m_lookIKBlendsSmoothRate[LOOKIK_BLEND_RATIOS];

	Quat m_oldSpine1;
	Quat m_oldSpine2;
	Quat m_oldSpine3;
	Quat m_oldNeck;
	Quat m_oldHead;
	Quat m_oldEyeLeft;
	Quat m_oldEyeRight;



	SLookIK() {	Init();	};

	void Init() 
	{
		m_UseLookIK=0;
		m_AllowAdditionalTransforms=1;
		m_LookIKFadeout=0;

		m_LookDirection=Vec3(0,1,0);	//just for debugging
		m_LookIKTarget=Vec3(ZERO);
		m_LookIKTargetSmooth=Vec3(ZERO);
		m_LookIKTargetSmoothRate=Vec3(ZERO);

		m_LookIKBlend	=0.0f;
		m_LookIKFOR		=gf_PI*0.5f;


		m_lookIKBlends[0] = 0.04f;
		m_lookIKBlends[1] = 0.06f;
		m_lookIKBlends[2] = 0.08f;
		m_lookIKBlends[3] = 0.15f;
		m_lookIKBlends[4] = 0.60f;

		m_lookIKBlendsSmooth[0] = 0.04f;
		m_lookIKBlendsSmooth[1] = 0.06f;
		m_lookIKBlendsSmooth[2] = 0.08f;
		m_lookIKBlendsSmooth[3] = 0.15f;
		m_lookIKBlendsSmooth[4] = 0.60f;
		m_lookIKBlendsSmoothRate[0] = 0.04f;
		m_lookIKBlendsSmoothRate[1] = 0.06f;
		m_lookIKBlendsSmoothRate[2] = 0.08f;
		m_lookIKBlendsSmoothRate[3] = 0.15f;
		m_lookIKBlendsSmoothRate[4] = 0.60f;

		m_oldSpine1.SetIdentity();
		m_oldSpine2.SetIdentity();
		m_oldSpine3.SetIdentity();
		m_oldNeck.SetIdentity();
		m_oldHead.SetIdentity();
		m_oldEyeLeft.SetIdentity();
		m_oldEyeRight.SetIdentity();
	};

};



struct SFootAnchor 
{
	uint32 m_UseFootAnchoring;

	uint32 m_LastLegUpdate;
	Vec3	m_vFootSlide;
	Vec3	m_absSkeletonInWorld;

	QuatT m_AbsoluteRoot;
	QuatT m_AbsolutePelvis;
	QuatT m_AbsoluteLThigh;
	QuatT m_AbsoluteLCalf;
	QuatT m_AbsoluteLFoot;
	QuatT m_AbsoluteLHeel;
	QuatT m_AbsoluteLToe0;
	QuatT m_AbsoluteLNub0;
	QuatT m_AbsoluteRThigh;
	QuatT m_AbsoluteRCalf;
	QuatT m_AbsoluteRFoot;
	QuatT m_AbsoluteRHeel;
	QuatT m_AbsoluteRToe0;
	QuatT m_AbsoluteRNub0;


	f32 m_LAnkleIntensity;
	Quat m_LAnkle;
	Quat m_LThig;
	Vec3 m_LastLHeel;
	Vec3 m_LastLToe;


	f32 m_RAnkleIntensity;
	Quat m_RAnkle;
	Quat m_RThig;
	Vec3 m_LastRHeel;
	Vec3 m_LastRToe;


	SFootAnchor() {	Init();	};
	void Init() 
	{
		m_UseFootAnchoring=0;

		m_LastLegUpdate =(uint32)-1;
		m_vFootSlide=Vec3(ZERO);
		m_absSkeletonInWorld=Vec3(ZERO);

		m_AbsoluteRoot.SetIdentity();
		m_AbsolutePelvis.SetIdentity();
		m_AbsoluteLThigh.SetIdentity();
		m_AbsoluteLCalf.SetIdentity();
		m_AbsoluteLFoot.SetIdentity();
		m_AbsoluteLHeel.SetIdentity();
		m_AbsoluteLToe0.SetIdentity();
		m_AbsoluteLNub0.SetIdentity();
		m_AbsoluteRThigh.SetIdentity();
		m_AbsoluteRCalf.SetIdentity();
		m_AbsoluteRFoot.SetIdentity();
		m_AbsoluteRHeel.SetIdentity();
		m_AbsoluteRToe0.SetIdentity();
		m_AbsoluteRNub0.SetIdentity();



		m_LAnkleIntensity = 0;
		m_LAnkle.SetIdentity();
		m_LThig.SetIdentity();
		m_LastLHeel = Vec3(ZERO);
		m_LastLToe	= Vec3(ZERO);

		m_RAnkleIntensity = 0;
		m_RAnkle.SetIdentity();
		m_RThig.SetIdentity();
		m_LastRHeel = Vec3(ZERO);
		m_LastRToe	= Vec3(ZERO);
	};

};






//-------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------
class CSkeletonPose: public ISkeletonPose
{
	friend class CSkinInstance;

public:

	CSkeletonPose()	{	}

	//-----------------------------------------------------------------------------------
	//---- interface functions to access bones ----
	//-----------------------------------------------------------------------------------
	size_t SizeOfThis (ICrySizer * pSizer);

	//! Returns the number of bones; all bone ids are in the range from 0 to this number exclusive; 0th bone is the root
	uint32 GetJointCount() const { return m_AbsolutePose.size(); }
	// finds the bone by its name; returns the index of the bone
	int16 GetJointIDByName (const char* szJointName) const;
	int16 GetParentIDByID (int32 ChildID) const;

	virtual uint32 GetJointCRC32 (int32 nJointID) const;


	// returns the name of bone from bone table, return zero id nId is out of range
	const char* GetJointNameByID(int32 nId) const;

	const QuatT& GetAbsJointByID(int32 id) 
	{ 
		int32 numJoints=(int)m_AbsolutePose.size();
		if(id>=0 && id<numJoints)
			return m_AbsolutePose[id];
		assert(0);
		return g_IdentityQuatT; 
	}
	const QuatT& GetRelJointByID(int32 id) 
	{ 
		int32 numJoints=(int)m_AbsolutePose.size();
		if(id>=0 && id<numJoints)
			return m_RelativePose[id]; 
		assert(!"GetRelJointByID() - Index out of range!");
		return g_IdentityQuatT;
	}

	const QuatT& GetDefaultAbsJointByID(int32 id) 
	{ 
		int32 numJoints=(int)m_AbsolutePose.size();
		if(id>=0 && id<numJoints)
			return m_parrModelJoints[id].m_DefaultAbsPose; 
		assert(!"GetDefaultAbsJointByID() - Index out of range!");
		return g_IdentityQuatT; 
	}
	const QuatT& GetDefaultRelJointByID(int32 id) 
	{ 
		int32 numJoints=(int)m_AbsolutePose.size();
		if(id>=0 && id<numJoints)
			return m_parrModelJoints[id].m_DefaultRelPose; 
		assert(!"GetDefaultRelJointByID() - Index out of range!");
		return g_IdentityQuatT; 
	}




	uint8 GetJointMask(int32 nJointNo, uint8 layer)
	{
		return (m_JointMask[nJointNo] >> layer) & 1;
	}

	void SetJointMask(int32 nJointNo, uint8 layer, uint8 val)
	{
		uint32 mask2 = 1 << layer;
		uint32 mask = (val & 0x1) << layer;
		m_JointMask[nJointNo] = (m_JointMask[nJointNo] & ~mask2) | mask;
	}


	void InitSkeletonPose( CCharInstance* pInstance, CSkeletonAnim* pSkeletonAnim);
	void InitPhysicsSkeleton();
	void InitCGASkeleton();

	void SetDefaultPose();
	void SetDefaultPosePerInstance();

	//enable & disable animation for one specified bone and layer
	bool SetJointMask(const char* szBoneName, uint32 nLayerNo, uint8 nVal)
	{
		if (nLayerNo>=numVIRTUALLAYERS)
			return 0;
		int nBone = GetJointIDByName(szBoneName);
		if(nBone == -1)
			return 0;

		SetJointMask(nBone,nLayerNo, nVal); 
		return 1;
	}

	//enable & disable animation for one full layer
	bool SetLayerMask(uint32 nLayerNo, uint8 nVal)
	{
		if (nLayerNo>=numVIRTUALLAYERS)
			return 0;
		uint32 numBones = m_AbsolutePose.size();
		for (uint32 b=0; b<numBones; b++)
			SetJointMask(b,nLayerNo, nVal);
		return 1;
	}



	//------------------------------------------------------------------------------------


	RMovement RPlayback (const CAnimation& AnimLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo,f32 fScaling );
	RMovement RTransition( CAnimation* parrActiveAnims, uint32 numAnims, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo,f32 fScaling );

	RMovement RPlayback_1 (const CAnimation& AnimLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo,f32 fScaling );
	RMovement RPlayback_2 (const CAnimation& AnimLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo,f32 fScaling );

	RMovement RSingleEvaluation(const SAnimation& rAnim, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling );
	RMovement RSingleEvaluation(const SAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo, f32 fScaling );

	RMovement RSingleEvaluationLMG(const CAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo, f32 fScaling );
	void SpineEvaluation(const SAnimation& rAnim, f32 time_new, int32 seg, RMovement& Movement, QuatT* parrRelQuats, Status4* parrControllerStatus );

	RMovement RTwoWayBlend(uint32 src0,uint32 src1,uint32 dst, f32 r,f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo  );
	void R_BLENDLAYER_MOVE(uint32 src,uint32 dst,f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo  );
	void R_BLENDLAYER_ADD(uint32 src,uint32 dst,f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo  );
	void R_BLENDLAYER_NORM(uint32 dst, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo  );


	void Playback (const CAnimation& AnimLayer, uint32 nLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling  );
	void Transition( CAnimation* parrActiveAnims, uint32 numAnims, uint32 nLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling  );

	void Playback_1 (const CAnimation& AnimLayer, uint32 nLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus,f32 fScaling );
	void Playback_2 (const CAnimation& AnimLayer, uint32 nLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus,f32 fScaling );
	void SingleEvaluation(const SAnimation& rAnim, uint32 nLayer, QuatT* parrRelQuats, Status4* parrControllerStatus,f32 fScaling );
	void SingleEvaluation(const SAnimation& rAnim, uint32 nLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus,f32 fScaling );
	void SingleEvaluationLMG(const CAnimation& rAnim, uint32 nLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus,f32 fScaling );

	void TwoWayBlend(uint32 src0,uint32 src1,uint32 dst, f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus);
	void BLENDLAYER_MOVE(uint32 src,uint32 dst,f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus);
	void BLENDLAYER_ADD(uint32 src,uint32 dst,f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus );
	void BLENDLAYER_NORM(uint32 dst, QuatT* parrRelQuats, Status4* parrControllerStatus );

	void TransModelFast( const GlobalAnimationHeader& rGAH, const CAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling );
	void TransModelSlow( const GlobalAnimationHeader& rGAH, const CAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling );
	void AddLayer( const GlobalAnimationHeader& rGAH, const CAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling, const char* strSpliceAnim );


	void UpdateAbsoluteJointTransforms();
	void SkeletonPostProcess( const QuatT &rPhysLocationNext, const QuatTS &rAnimLocationNext, IAttachment* pIAttachment, float fZoomAdjustedDistanceFromCamera, uint32 OnRender=0 );
	void SkeletonPostProcess2( const QuatT &rPhysLocationNext, const QuatTS &rAnimLocationNext, IAttachment* pIAttachment, uint32 OnRender=0  );
	void KinematicPostProcess(const QuatT &rPhysLocationNext, const QuatTS &rAnimLocationNext);

	SFootPlant BlendFootPlants(const SFootPlant& fp0, const SFootPlant& fp1, f32 t);




	int32 m_nForceSkeletonUpdate;
	void SetForceSkeletonUpdate(int32 i) { m_nForceSkeletonUpdate=i; };

	void UpdateBBox(uint32 update=0);
	void SetSkinningSkeletonMaster(f32 fScaling);

	//-----------------------------------------------------------------------------------


	//------------------------------------------------------------------------
	//----> LookIK
	//------------------------------------------------------------------------
	void SetLookIK(uint32 ik, f32 FOR, const Vec3& LookAtTarget,const f32 *customBlends, bool allowAdditionalTransforms);
	void LookIK( const QuatT& rPhysLocationNext, uint32 LIKFlag );
	void LookIKEyes( const QuatT& rPhysLocationNext );
	void SetLookIKHunter(uint32 ik, f32 FOR, const Vec3& LookAtTarget);
	void LookIKHunter( uint32 LIKFlag, const QuatT& rPhysLocationNew, const QuatTS& rAnimLocationNew  );
	SLookIK m_LookIK;

	//------------------------------------------------------------------------
	//----> AimIK
	//------------------------------------------------------------------------
	void SetAimIKFadeOut(uint32 a) { m_AimIK.m_AimIKFadeoutFlag = a > 0; };
	void SetAimIKFadeOutSpeed(f32 time) {	m_AimIK.m_fAimIKFadeOutTime = 1.0f/time;	};
	void SetAimIKFadeInSpeed(f32 time) {	m_AimIK.m_fAimIKFadeInTime = 1.0f/time;	};
	void SetAimIK(uint32 ik, const Vec3& AimAtTarget);
	void SetAimIKTargetSmoothTime(f32 fSmoothTime) { m_AimIK.m_AimIKTargetSmoothTime=fSmoothTime; };
	uint32 GetAimIKStatus() { return uint32(m_AimIK.m_UseAimIK && (m_AimIK.m_AimIKInfluence>0.9f)); };
	void AimIK( const QuatT& rPhysLocationNext, const QuatTS& rAnimLocationNext );
	IPol CheckTriangles( const QuatT& rPhysLocationNext, uint32 k0,uint32 k1,uint32 k2,ColorB rgb,SAimDirection* ls, const Vec3& localAimTarget, AimIKPose* parrAimIKPoses, f32 fAimDirScale, f32 fIKBlend, int16* parrAimIKBoneIdx );
	SAimDirection EvaluatePose( const QuatT& rPhysLocationNext, uint32 aimpose, AimIKPose* parrAimIKPoses, f32 fAimDirScale, int16* parrAimIKBoneIdx );
	f32 GetAimIKBlend(){ return m_AimIK.m_AimIKInfluence; }
	SAimIK m_AimIK;

	//------------------------------------------------------------------------
	//----> recoil implementation
	//------------------------------------------------------------------------
	void ApplyRecoilAnimation(f32 fDuration, f32 fImpact, uint32 arms=3 );
	void PlayRecoilAnimation( const QuatT& rPhysEntityLocation );
	SRecoil m_Recoil;

	//------------------------------------------------------------------------
	//----> weapon raising
	//------------------------------------------------------------------------
	void SetWeaponRaisedPose(EWeaponRaisedPose pose) {}

	//------------------------------------------------------------------------
	//----> Foot Anchoring
	//------------------------------------------------------------------------
	void SetFootAnchoring(uint32 at){ m_FootAnchor.m_UseFootAnchoring=at > 0; };
	void FootAnchoring( const SFootPlant& BFPlant, uint8 Foot, const QuatTS& rAnimLocationCurr );
	void AbsoluteLegPosition(const QuatTS& rAnimLocationCurr);
	void EaseOut_LAnkle();
	void EaseOut_RAnkle();
	SFootAnchor m_FootAnchor;



	//------------------------------------------------------------------------
	//----> human limb-IK
	//------------------------------------------------------------------------
	uint32 SetCustomArmIK(const Vec3& goal,int32 idx0,int32 idx1,int32 idx2);
	uint32 SetHumanLimbIK(const Vec3& WGoal, uint32 limp);
	uint32 SetLArmIK(const Vec3& goal);
	uint32 SetRArmIK(const Vec3& goal);
	uint32 SetLFootIK(const Vec3& goal2);
	uint32 SetRFootIK(const Vec3& goal2);
	uint32 SetFootGroundAlignmentCCD( uint32 leg, const Plane& GroundPlane);
	uint32 SetFootGroundAlignment(const Vec3& normal2, int32 CalfIdx,int32 FootIdx,int32 HeelIdx,int32 Toe0Idx,int32 ToeNIdx  );
	void EnableFootGroundAlignment(bool enable);
	void TwoBoneIK(int b0,int b1,int b2, const Vec3r& goal);
	void MoveSkeletonVertical( f32 vertical );


	//------------------------------------------------------------------------
	//----> CCD-IK 
	//------------------------------------------------------------------------
	std::vector<int32> m_arrCCDChain;
	void CCDInitIKBuffer(QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK);
	int32* CCDInitIKChain(int32 sCCDJoint,int32 eCCDJoint);
	void CCDRotationSolver(const Vec3& PositionCCD,f32 fThreshold,f32 StepSize,uint32 iTry,const Vec3& normal,QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK,Quat* pQuat=0);
	void CCDTranslationSolver(const Vec3& PositionCCD,QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK);
	void CCDDirConstraint(const Vec3& dir,QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK);
	void CCDQuatConstraint(const Quat* pQuats,QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK);
	void CCDUpdateSkeleton(QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK);
	void CCDLockHunterFoot(int32 idx,QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK);





	void SetPostProcessQuat(int32 idx, const QuatT& qt )
	{
		m_arrPostProcess.push_back( SPostProcess(idx,qt) );
	}
	std::vector<SPostProcess> m_arrPostProcess;


	int (*m_pPostProcessCallback0)(ICharacterInstance*,void*);
	void* m_pPostProcessCallbackData0;
	void SetPostProcessCallback0(int (*func)(ICharacterInstance*,void*), void *pdata)
	{
		m_pPostProcessCallback0 = func;
		m_pPostProcessCallbackData0 = pdata;
	}

	int (*m_pPostProcessCallback1)(ICharacterInstance*,void*);
	void* m_pPostProcessCallbackData1;
	void SetPostProcessCallback1(int (*func)(ICharacterInstance*,void*), void *pdata)
	{
		m_pPostProcessCallback1 = func;
		m_pPostProcessCallbackData1 = pdata;
	}

	int (*m_pPostPhysicsCallback)(ICharacterInstance*,void*);
	void* m_pPostPhysicsCallbackData;
	void SetPostPhysicsCallback(int (*func)(ICharacterInstance*,void*), void *pdata)
	{
		m_pPostPhysicsCallback = func;
		m_pPostPhysicsCallbackData = pdata;
	}



	//-------------------------------------------------------------------------------
	//---         Physics from CSkinInstance                                  ---
	//-------------------------------------------------------------------------------
	IStatObj* GetStatObjOnJoint(int32 nId);
	void SetStatObjOnJoint(int32 nId, IStatObj* pStatObj);
	IPhysicalEntity *GetPhysEntOnJoint(int32 nId) { return m_ppBonePhysics ? m_ppBonePhysics[nId]:0; }
	void SetPhysEntOnJoint(int32 nId, IPhysicalEntity *pPhysEnt);
	int GetPhysIdOnJoint(int32 nId);
	void SetMaterialOnJoint(int32 nId, IMaterial* pMaterial) { assert(nId>=0 && nId<m_arrCGAJoints.size()); m_arrCGAJoints[nId].m_pMaterial = pMaterial; }
	IMaterial* GetMaterialOnJoint(int32 nId) { assert(nId>=0 && nId<m_arrCGAJoints.size()); return m_arrCGAJoints[nId].m_pMaterial; }

	void SetRagdollDefaultPose();
	int getBonePhysChildIndex (int nBoneIndex, int nLod=0);
	int getBonePhysParentIndex (int nBoneIndex, int nLod=0); // same, but finds the first ancestor that has physical geometry
	int getBonePhysParentOrSelfIndex (int nBoneIndex, int nLod=0);

	int TranslatePartIdToDeadBody(int partid);
	float AssessPoseSimilarity(const CSkeletonPose &skelAnim, int iLod);
	void ProcessPhysics(const QuatT& rPhysEntityLocation, f32 fDeltaTimePhys, int nNeff, SCharUpdateFeedback *pCharFeedback=0);

	void Fall();
	void GoLimp();
	void StandUp(const Matrix34 &mtx, bool b3DOF, IPhysicalEntity *&pNewPhysicalEntity, Matrix34 &mtxDelta);

	float Falling() const;
	float Lying() const;
	float StandingUp() const;
	int GetFallingDir();

	bool SetFnPAnimGroup(const char *name);
	bool SetFnPAnimGroup(int idx);

	void FindSpineBones();

	void BuildPhysicalEntity(IPhysicalEntity *pent,f32 mass,int surface_idx,f32 stiffness_scale, int nLod=0,int partid0=0, 
		const Matrix34 &mtxloc=Matrix34(QuatT(IDENTITY)));
	IPhysicalEntity *GetCharacterPhysics(const char *pRootBoneName);
	IPhysicalEntity *GetCharacterPhysics(int iAuxPhys);
	void DestroyCharacterPhysics(int iMode=0);
	IPhysicalEntity* CreateCharacterPhysics(IPhysicalEntity *pHost, f32 mass,int surface_idx,f32 stiffness_scale, int nLod=0, 
		const Matrix34 &mtxloc=Matrix34(QuatT(IDENTITY)));
	int CreateAuxilaryPhysics(IPhysicalEntity *pHost, const Matrix34 &mtx, int nLod=0);
	int CreateAuxilaryPhysics(IPhysicalEntity *pHost, const Matrix34 &mtx, f32 scale,Vec3 offset, int nLod);
	int FillRopeLenArray(float *arr,int i0,int sz);
	void SynchronizeWithPhysicalEntity(IPhysicalEntity *pent,const Vec3& posMaster,const Quat& qMaster);
	void SynchronizeWithPhysicalEntity(IPhysicalEntity *pent, const Vec3& posMaster,const Quat& qMaster, QuatT offset, int iDir=-1 );
	IPhysicalEntity *RelinquishCharacterPhysics(const Matrix34 &mtx, float stiffness=0.0f);
	bool AddImpact(int partid, Vec3 point,Vec3 impact);
	void ResetNonphysicalBoneRotations (int nLOD, f32 fBlend);
	void UnconvertBoneGlobalFromRelativeForm(bool bNonphysicalOnly, int nLod=0, bool bRopeTipsOnly=false);
	void ConvertBoneGlobalToRelativeMatrices();
	//! marks all LODs as needed to be reskinned
	void ForceReskin ();

	IPhysicalEntity* GetCharacterPhysics() const { return m_pCharPhysics; }
	void SetCharacterPhysics(IPhysicalEntity *pent) { 
		m_pCharPhysics=pent; m_bAliveRagdoll = 0;	m_timeStandingUp = -1;
	}

	void DestroyPhysics();
	void SetAuxParams(pe_params* pf);

	void UpdateJointControllers(CAnimation &anim);

	void SetOffset(Vec3 offset) {	m_vOffset = offset; }
	Vec3 GetOffset() { return m_vOffset; }

	CModelJoint* GetModelJointPointer(int nBone) { return &m_parrModelJoints[nBone]; }
	//returns the j-th child of i-th child of the given bone
	int GetModelJointChildIndex (int nBone, int i)
	{
		assert (i >= 0 && i<(int)GetModelJointPointer(nBone)->numChildren());
		return nBone + GetModelJointPointer(nBone)->getFirstChildIndexOffset() + i;
	}
	// given the bone index, (INDEX, NOT ID), returns this bone's parent index
	uint32 getBoneParentIndex (uint32 nBoneIndex);











	void SetFootPlants(int val) { m_FootPlants = val; };
	int GetFootPlants() const { return m_FootPlants; };

	int GetInstanceVisible() const { return m_bFullSkeletonUpdate; };










	//just for debugging
	void DrawBBox( const Matrix34& rRenderMat34 );
	void DrawSkeleton( const Matrix34& rRenderMat34, uint32 shift=0 );
	void DrawDefaultSkeleton( const Matrix34& rRenderMat34 );
	void DrawArrow( const QuatT& location, f32 length, ColorB col );
	f32 SecurityCheck();
	uint32 IsSkeletonValid();
	int32 m_Superimposed;
	void SetSuperimposed(uint32 i){ m_Superimposed=i; };
	void DrawThinSkeletons( const QuatT& m34, const std::vector< std::vector<DebugJoint> >& arrSkeletons, int32 nAnimID,int32 nGlobalAnimID );
	void DrawThinSkeleton( const QuatT& m34, const std::vector<DebugJoint>& arrSkeleton, uint8 FootPlant, ColorB col );
	AABB GetAABB(){ return m_AABB; }

public:
	IPhysicalEntity *m_pCharPhysics;
	IPhysicalEntity *m_pPrevCharHost;
	IPhysicalEntity **m_ppBonePhysics;
	CCharInstance* m_pInstance;
	CModelSkeleton* m_pModelSkeleton;
	CModelJoint* m_parrModelJoints;

	DynArray<CCGAJoint> m_arrCGAJoints;
	DynArray<CPhysicsJoint> m_arrPhysicsJoints;


	std::vector<Status4>	m_arrControllerInfo;
	std::vector<uint16>		m_JointMask; //limited to 16 layers
	std::vector<QuatT>		m_qAdditionalRotPos;
	std::vector<QuatT>		m_RelativePose; 
	std::vector<QuatT>		m_AbsolutePose; 
	std::vector<Vec3>		  m_FaceAnimPosSmooth; 
	std::vector<Vec3>		  m_FaceAnimPosSmoothRate; 









	Vec3 m_vRenderOffset;
	AABB m_AABB;



	Vec3 m_vOffset;
	int m_iSpineBone[3];
	mutable int m_nSpineBones;
	int m_nAuxPhys;
	int m_iSurfaceIdx;
	int m_iFlyDir;
	aux_phys_data m_auxPhys[12];
	int m_b3DOFStandup;
	f32 m_fPhysBlendTime,m_fPhysBlendMaxTime,m_frPhysBlendMaxTime;
	float m_stiffnessScale;
	float m_timeRagdolled,m_timeLying,m_timeNoColl,m_timeStandingUp;
	int m_iFnPSet;
	f32 m_fScale;
	f32 m_fMass;

	f32 m_LFootGroundAlign;
	f32 m_RFootGroundAlign;
	bool m_enableFootGroundAlignment;

	uint32 m_bInstanceVisible : 1;
	uint32 m_bFullSkeletonUpdate : 1;
	uint32 m_bAllNodesValid : 1; //True if this animation was already played once.
	uint32 m_bAllAbsoluteJointsValid : 1;
	uint32 m_bHackForceAbsoluteUpdate : 1;
	uint32 m_bPhysicsRelinquished : 1;
	uint32 m_FootPlants : 1;
	uint32 m_bHasPhysics  : 1, m_bPhysicsAwake : 1, m_bPhysicsWasAwake : 1;
	int m_bAliveRagdoll : 1;
	int m_bLimpRagdoll : 1;
	Matrix34 m_NewTempRenderMat;
	CSkeletonAnim* m_pSkeletonAnim; 

};





#endif // _BONE_H
