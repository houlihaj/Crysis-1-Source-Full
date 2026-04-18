//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:SkinInstance.h
//  Declaration of SkinInstance class
//
//	History:
//	June 25, 2007: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////
#ifndef _CHAR_INSTANCE_HEADER_
#define _CHAR_INSTANCE_HEADER_

struct CryEngineDecalInfo;
struct CryParticleSpawnInfo;

extern f32 g_YLine;

#include <ICryAnimation.h>
#include <CGFContent.h>

#include "Model.h"				//embedded
#include "CryModEffMorph.h"			//embedded

#include "SkeletonAnim.h"						//embedded
#include "SkeletonPose.h"						//embedded
#include "Morphing.h"						//embedded
#include "SkinInstance.h"
#include "SkeletonEffectManager.h"


//////////////////////////////////////////////////////////////////////

struct AnimData;
class CryCharManager;
class CFacialInstance;
class CModelMesh;
class CCamera;


///////////////////////////////////////////////////////////////////////////////////////////////
// Implementation of ISkinInstance interface, the main interface in the Animation module
///////////////////////////////////////////////////////////////////////////////////////////////
class CCharInstance : public CSkinInstance
{
	friend class CAttachmentManager;
	friend class CAttachment;
	friend class CModelMesh;

public:

	CCharInstance (const string &strFileName, CCharacterModel* pModel);
	~CCharInstance();

	void Release();

	virtual AABB GetAABB(); 
	virtual ISkeletonAnim* GetISkeletonAnim() {	return &m_SkeletonAnim;	}
	virtual ISkeletonPose* GetISkeletonPose() {	return &m_SkeletonPose; }

	// calculates the mask ANDed with the frame id that's used to determine whether to skin the character on this frame or not.
	virtual void SkeletonPreProcess(const QuatT &rPhysLocationCurr, const QuatTS &rAnimLocationCurr, const CCamera& rCamera, uint32 OnRender ); // processes animations and recalc bones

	virtual void SkeletonPostProcess( const QuatT &rPhysLocationNext, const QuatTS &rAnimLocationNext, IAttachment* pIAttachment, float fZoomAdjustedDistanceFromCamera, uint32 OnRender ) 
	{
		m_SkeletonPose.SkeletonPostProcess( rPhysLocationNext, rAnimLocationNext, pIAttachment, fZoomAdjustedDistanceFromCamera, OnRender );
		ISkeletonAnim* pSkeletonAnim = GetISkeletonAnim();
		ISkeletonPose* pSkeletonPose = GetISkeletonPose();
		if (pSkeletonAnim && pSkeletonPose)
			m_skeletonEffectManager.Update(pSkeletonAnim, pSkeletonPose, Matrix34(rPhysLocationNext));
	};
	uint32 GetUpdateFrequencyMask(const Vec3& vPos, const CCamera& rCamera );

	bool m_bEnableStartAnimation;
	void EnableStartAnimation (bool bEnable)
	{
		m_bEnableStartAnimation = bEnable;
	}

	uint32 m_ResetMode;
	virtual uint32 GetResetMode() { return m_ResetMode; };
	virtual void SetResetMode(uint32 rm) { m_ResetMode = rm > 0; };

	f32 m_fDeltaTime;
	f32 m_fOriginalDeltaTime;

	uint32 m_HideMaster : 1;
	virtual void HideMaster(uint32 h) { m_HideMaster=h > 0; };

	CSkeletonAnim m_SkeletonAnim;
	CSkeletonPose m_SkeletonPose;

	uint32 GetPhysicsRelinquished() { return m_SkeletonPose.m_bPhysicsRelinquished;};
	uint32 IsCharacterVisible(){ return m_SkeletonPose.m_bFullSkeletonUpdate; };

	virtual bool IsAnimationGraphValid()
	{
		if (m_SkeletonAnim.m_bReinitializeAnimGraph)
		{
			m_SkeletonAnim.m_bReinitializeAnimGraph = false;
			return false;
		}
		return true;
	}

	void UpdatePhysicsCGA( f32 fScale, const QuatTS& rAnimLocationNext );
	virtual void OnDetach();
	virtual float ComputeExtent(GeomQuery& geo, EGeomForm eForm);
	virtual void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm);
	virtual void SetAlternativeTargetForGetRandomPos(ICharacterInstance *target);

	CCharacterModel *m_pAlternativeModelForGetRandomPos;

	virtual void Serialize(TSerialize ser);
	size_t SizeOfThis (ICrySizer * pSizer);

	void Render(const struct SRendParams &rParams, const QuatTS& Offset, Matrix34 *pFinalPhysLocation=0);
	void RenderCGA(const struct SRendParams& rParams, const Matrix34& RotTransMatrix34, const Matrix34& RotTransPrevMatrix34 );
	void RenderCHR(const struct SRendParams& rParams, const Matrix34& RotTransMatrix34, const Matrix34& RotTransPrevMatrix34 );
	uint32 GetSkeletonPose( int nLod, const Matrix34& RenderMat34, QuatTS*& pBoneQuatsL, QuatTS*& pBoneQuatsS, QuatTS*& pMBBoneQuatsL, QuatTS*& pMBBoneQuatsS, Vec4 shapeDeformationData[], uint32 &DoWeNeedMorphtargets, uint8*& pRemapTable);
	void CreateDecal(CryEngineDecalInfo& Decal);


	void SetFPWeapon(f32 fp) { m_fFPWeapon=fp; }
	f32 m_fFPWeapon;
	Vec3 m_vDifSmooth;
	Vec3 m_vDifSmoothRate;
	uint32 m_nInstanceNumber;

	// This is the scale factor that affects the animation speed of the character.
	// All the animations are played with the constant real-time speed multiplied by this factor.
	// So, 0 means still animations (stuck at some frame), 1 - normal, 2 - twice as fast, 0.5 - twice slower than normal.
	f32 m_fAnimSpeedScale;
	void SetAnimationSpeed(f32 speed) { m_fAnimSpeedScale = speed; }
	f32 GetAnimationSpeed() { return m_fAnimSpeedScale; }


	QuatTS m_RenderCharLocation;
	QuatT m_PhysEntityLocation;
	uint32 m_LastUpdateFrameID_Pre;
	uint32 m_LastUpdateFrameID_Post;
	uint32 m_LastUpdateFrameID_PreType;
	uint32 m_LastUpdateFrameID_PostType;
	const CCamera*	m_pCamera;

	void MotionBlurTest(int nLOD,const Matrix34& mat );
	void UpdatePreviousFrameBones(int nLOD);
  bool MotionBlurMotionCheck( uint nObjFlags );
	std::vector<QuatTS> m_arrMBSkinQuat;	//bone quaternions for skinning (entire skeleton)
	std::vector<QuatTS> m_arrNewSkinQuat[MAX_MOTIONBLUR_FRAMES];	//bone quaternions for skinning (entire skeleton)

	void DrawWireframeStatic( const Matrix34& m34, int nLOD, uint32 color)
	{
		CModelMesh* pModelMesh = m_pModel->GetModelMesh(nLOD);
		if (pModelMesh)
			pModelMesh->DrawWireframeStatic(&this->m_Morphing,m34,nLOD,color);
	}

	uint32 m_HadUpdate;
  bool   m_bUpdateMotionBlurSkinnning;
  bool   m_bPrevFrameFPWeapon;
	char   m_iActiveFrame;
	char   m_iBackFrame;

	f32 m_arrShapeDeformValues[8];
	f32* GetShapeDeformArray() { return m_arrShapeDeformValues; }

	void SpawnSkeletonEffect(int animID, const char* animName, const char* effectName, const char* boneName, const Vec3& offset, const Vec3& dir, const Matrix34& entityTM)
	{
		ISkeletonPose* pSkeletonPose = GetISkeletonPose();
		if (pSkeletonPose)
			m_skeletonEffectManager.SpawnEffect(pSkeletonPose, animID, animName, effectName, boneName, offset, dir, entityTM);
	}

	void KillAllSkeletonEffects()
	{
		m_skeletonEffectManager.KillAllEffects();
	}

	CSkeletonEffectManager m_skeletonEffectManager;

	CryCharInstanceRenderParams rp;
	void SetFlags(int nFlags) { rp.m_nFlags=nFlags; }
	int  GetFlags() { return rp.m_nFlags; }
	void SetColor(f32 fR, f32 fG, f32 fB, f32 fA);

protected:
	CCharInstance (){};
};


#endif

