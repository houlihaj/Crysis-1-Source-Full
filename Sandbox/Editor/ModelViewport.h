////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   ModelViewport.h
//  Version:     v1.00
//  Created:     8/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ModelViewport_h__
#define __ModelViewport_h__

#if _MSC_VER > 1000
#pragma once
#endif


#include "RenderViewport.h"
#include "Material\Material.h"
#include <ICryAnimation.h>
#include <IInput.h>
#include <ISound.h>

struct IPhysicalEntity;
struct CryCharAnimationParams;
struct ISkeletonAnim;
class CAnimationSet;
struct IVec	{    Vec3 ntoe;Vec3 toe;   Vec3 nheel;Vec3 heel;   };
struct AimPoses	
{ 
	const char* n0; 
	const char* n1; 
	AimPoses(const char* a0,const char* a1 )
	{
		n0=a0; 
		n1=a1; 
	}
};

struct SAnimationBlendingParams
{
	SAnimationBlendingParams()
	{
		m_yawAngle = 0;
		m_speed = 0;
		m_strafeParam = 0;
		m_fUpDownParam = 0;
	}
	f32 m_yawAngle; 
	f32 m_speed; 
	f32 m_strafeParam; 
	f32 m_fUpDownParam;
};



struct SMotionParams
{
	f32	 m_fCatchUpSpeed;
};

/////////////////////////////////////////////////////////////////////////////
// TODO: Merge this code with copied code from CryAction
class CAnimatedCharacterEffectManager
{
public:
	CAnimatedCharacterEffectManager();
	~CAnimatedCharacterEffectManager();
	
	void SetSkeleton(ISkeletonAnim* pSkeletonAnim,ISkeletonPose* pSkeletonPose);
	void Update();
	void SpawnEffect(int animID, const char* animName, const char* effectName, const char* boneName, const Vec3& offset, const Vec3& dir);
	void KillAllEffects();
	void Render(SRendParams& params);

private:
	struct EffectEntry
	{
		EffectEntry(_smart_ptr<IParticleEffect> pEffect, _smart_ptr<IParticleEmitter> pEmitter, int boneID, const Vec3& offset, const Vec3& dir, int animID);
		~EffectEntry();

		_smart_ptr<IParticleEffect> pEffect;
		_smart_ptr<IParticleEmitter> pEmitter;
		int boneID;
		Vec3 offset;
		Vec3 dir;
		int animID;
	};

	void GetEffectTM(Matrix34& tm, int boneID, const Vec3& offset, const Vec3& dir);
	bool IsPlayingAnimation(int animID);
	bool IsPlayingEffect(const char* effectName);

	ISkeletonAnim* m_pSkeleton2;
	ISkeletonPose* m_pSkeletonPose;
	DynArray<EffectEntry> m_effects;
};

/////////////////////////////////////////////////////////////////////////////
// CModelViewport window
//: public IRendererEventListener, public IInputEventListener, public IHardwareMouse, public ISystemEventListener
class CModelViewport : public CRenderViewport, public IInputEventListener
{
	DECLARE_DYNCREATE(CModelViewport)

	// Construction
public:
	CModelViewport();
	virtual ~CModelViewport();

	virtual EViewportType GetType() const { return ET_ViewportModel; }
	virtual void SetType( EViewportType type ) { assert(type == ET_ViewportModel); };

	virtual void LoadObject( const CString &obj,float scale );

	virtual void OnActivate();
	virtual void OnDeactivate();

	virtual bool CanDrop( CPoint point,IDataBaseItem *pItem );
	virtual void Drop( CPoint point,IDataBaseItem *pItem );

	void AttachObjectToBone( const CString &model,const CString &bone );
	void AttachObjectToFace( const CString &model );

	void StopAnimationInLayer(int nLayer);

	// Callbacks.
	void OnShowShaders( IVariable *var );
	void OnShowNormals( IVariable *var );
	void OnShowTangents( IVariable *var );

	void OnShowPortals( IVariable *var );
	void OnShowShadowVolumes( IVariable *var );
	void OnShowTextureUsage( IVariable *var );
	void OnShowAllTextures( IVariable *var );
	void OnShowPhysics( IVariable *var );
	void OnShowOcclusion( IVariable *var );

	void OnLightColor( IVariable *var );
	void OnDisableVisibility( IVariable *var );

	void OnSubmeshSetChanged();
	ICharacterInstance* GetCharacterBase(){ return m_pCharacterBase; }
	ICharacterInstance* GetCharacterAnim(){ return m_pCharacterAnim; }

	IStatObj* GetStaticObject(){ return m_object; }

	void GetOnDisableVisibility( IVariable *var );
	
	class CharPanel_Animation * GetModelPanelA() const { return m_pCharPanel_Animation; }
	void SetModelPanelA(class CharPanel_Animation * ptr) { m_pCharPanel_Animation=ptr; }

	class CharPanel_Preset * GetCharPanelPreset() const { return m_pCharPanel_Preset; }
	void SetCharPanelPreset(class CharPanel_Preset * ptr) { m_pCharPanel_Preset=ptr; }

	class CharPanel_BAttach* GetModelPanelW() const { return m_pCharPanel_BAttach; }
	void SetModelPanelW(class CharPanel_BAttach* ptr) { m_pCharPanel_BAttach=ptr; }

	const CVarObject* GetVarObject() const { return &m_vars; }

	virtual void Update();
	void ExternalPostProcessing(ICharacterInstance* pInstance);


	void ExternalPostProcessing_PlayControl(ICharacterInstance* pInstance);


	void AnimEventProcessing(ICharacterInstance* pInstance);

	void PreProcessCallback(ICharacterInstance* pInstance);
	void LocomotionCallback(ICharacterInstance* pInstance);

	Plane RayCast(const QuatT& rPhysLocation, f32 AnimHeight, const Vec3& MiddlePos, const Vec3& obbpos, const OBB& obb, Vec3& Intersection  );
	uint32 UseHumanLimbIK(ICharacterInstance* pInstance, uint32 i);
	uint32 UseGroundAlignTest(ICharacterInstance* pInstance);
	uint32 UseFootIKNew(ICharacterInstance* pInstance);

	uint32 UseCCDIK(ICharacterInstance* pInstance);

	// Set current material to render object.
	void SetCustomMaterial( CMaterial *pMaterial );
	// Get custom material that object is rendered with.
	CMaterial* GetCustomMaterial() { return m_pCurrentMaterial; };

	ICharacterManager* GetAnimationSystem() { return m_pAnimationSystem; };

	// Get material the object is actually rendered with.
	CMaterial* GetMaterial();

	void ReleaseObject();

	Vec3 m_GridOrigin;
	_smart_ptr<ICharacterInstance> m_pCharacterBase;
	_smart_ptr<ICharacterInstance> m_pCharacterAnim;
	void UpdateAnimationList();

	class CharPanel_Animation* m_pCharPanel_Animation;
	class CharPanel_Preset* m_pCharPanel_Preset;

	void SetPaused(bool bPaused);
	bool GetPaused() {return m_bPaused;}

	virtual void qqStartAnimation (const char*szName);

	const CString& GetLoadedFileName() const { return m_loadedFile; }

protected:
	void LoadStaticObject( const CString &file );

	// Called to render stuff.
	virtual void OnRender();

	void DrawGrid( const Quat& tmRotation, const Vec3& MotionTranslation,const Vec3& FootSlide, const Matrix33& rGridRot);
	void DrawHeightField();

	void DrawModel();
	void DrawSkyBox();
	void Physicalize();
	void DrawCharacter( ICharacterInstance* pInstance, SRendParams rp );

	void AnimPreview_UnitTest( ICharacterInstance* pInstance, IAnimationSet* pIAnimationSet, SRendParams rp  );
	void PlayerControl_UnitTest( ICharacterInstance* pInstance, IAnimationSet* pIAnimationSet, SRendParams rp  );
	void PathFollowing_UnitTest( ICharacterInstance* pInstance, IAnimationSet* pIAnimationSet, SRendParams rp  );
	void Idle2Move_UnitTest( ICharacterInstance* pInstance, IAnimationSet* pIAnimationSet, SRendParams rp  );
	void IdleStep_UnitTest( ICharacterInstance* pInstance, IAnimationSet* pIAnimationSet, SRendParams rp );

	void DrawCoordSystem( const QuatT& q, f32 length );
	void DrawArrow( const QuatT& q, f32 length, ColorB col );

	void SetConsoleVar( const char *var,int value );
	virtual void SetCharacterUIInfo();

	AimPoses AttachAimPoses( const char* szName );


	f32 PlayerControlHuman();
	SMotionParams EntityFollowing(uint32& ypos);
	void PathFollowing(const SMotionParams& mp, f32 fDesiredSpeed );
	void SetLocomotionValuesIvo(const SMotionParams& mp );

	f32 PathCreation( f32 fDesiredSpeed );

	f32 Idle2Move( IAnimationSet* pIAnimationSet );
	f32 IdleStep( IAnimationSet* pIAnimationSet );


//	SAnimationBlendingParams HumanBlending(  const Vec3& absPlayerPos, const Vec3& DesiredBodyDirection, const Vec3& DesiredMoveDirection);
	SAnimationBlendingParams HumanBlending( const Vec3& absPlayerPos, const Vec3& DesiredBodyDirection,f32 DBDSmoothTime, const Vec3& DesiredMoveDirection,f32 DMDSmoothTime, f32 Rad2Turn);
	SAnimationBlendingParams HumanBlendingPF( const Vec3& absPlayerPos, const SMotionParams& mp);
	void StateMachine(uint32 style, f32 YawAngle );

	void StandingAnims(uint32 NewStance, f32 LeftRightParam, const CryCharAnimationParams& Params, ISkeletonAnim* pISkeletonAnim);
	void StandRotateRight(uint32 NewStance, f32 LeftRightParam, const CryCharAnimationParams& Params, ISkeletonAnim* pISkeletonAnim);
	void StandRotateLeft(uint32 NewStance, f32 LeftRightParam, const CryCharAnimationParams& Params, ISkeletonAnim* pISkeletonAnim);
	
	void WalkingForward(uint32 style, const CryCharAnimationParams& Params, ISkeletonAnim* pISkeletonAnim);
	void RunningForward(uint32 style, const CryCharAnimationParams& Params, ISkeletonAnim* pISkeletonAnim);
	void WalkingBackward(uint32 style, const CryCharAnimationParams& Params, ISkeletonAnim* pISkeletonAnim);
	void RunningBackward(uint32 style, const CryCharAnimationParams& Params, ISkeletonAnim* pISkeletonAnim);
	
	void StartAnimation( CryCharAnimationParams& Params, f32 Interpolation );

	//f32 GetAverageFrameTime(f32 sec, f32 LastAverageFrameTime); 
	//f32 GetAverageYawRadiant(f32 sec, f32 aft, f32 NewAngle ); 
	//f32 GetAveragePitchRadiant(f32 sec, f32 aft, f32 NewAngle ); 


	struct BBox	{ OBB obb; Vec3 pos; ColorB col;	};
	std::vector<BBox>  m_arrBBoxes;

	IVec CheckFootIntersection(const Vec3& Final_Heel,const Vec3& Final_ToeN);

	void OnEditorNotifyEvent( EEditorNotifyEvent event )
	{
		if ( event != eNotify_OnBeginGameMode )
			CRenderViewport::OnEditorNotifyEvent( event );
	}


	//virtual bool OnInputEvent( const SInputEvent &event ) = 0;
	bool OnInputEvent(const SInputEvent &rInputEvent);

	Vec2 m_LTHUMB;
	Vec2 m_RTHUMB;

	IStatObj* m_object;
	IStatObj *m_weaponModel;
	// this is the character to attach, instead of weaponModel
	_smart_ptr<ICharacterInstance> m_attachedCharacter;
	CString m_attachBone;

	AABB m_AABB;

	// Camera control.
	float m_camRadius;
  Vec3 m_CamPos;
	
	// True to show grid.
	bool m_bGrid;
	bool m_bBase;

	class CharPanel_BAttach* m_pCharPanel_BAttach;

	int m_rollupIndex;
	int m_rollupIndex2;

	ICharacterManager *m_pAnimationSystem;

	CString m_loadedFile;
	std::vector<CDLight> m_VPLights;
	
	f32 m_LightRotationRadiant;

	CRESky* m_pRESky;
	ICVar* m_pSkyboxName;
  IShader *m_pSkyBoxShader;
	_smart_ptr<CMaterial> m_pCurrentMaterial;

	// Sound
	ListenerID				m_ListenerID;
	std::vector<tSoundID>	m_SoundIDs;

	//---------------------------------------------------
	//---    debug options                            ---
	//---------------------------------------------------
	CVariable<bool> mv_wireframe;
	CVariable<bool> mv_showGrid;
	CVariable<bool> mv_showBase;

	CVariable<bool> mv_showTangents;
	CVariable<bool> mv_showBinormals;
	CVariable<bool> mv_showNormals;

	CVariable<bool> mv_showSkeleton;
	CVariable<bool> mv_showJointNames;
	CVariable<bool> mv_showJointsValues;
	CVariable<bool> mv_showSuperimposed;
	CVariable<bool> mv_showRootUpdate;
	CVariable<bool> mv_showMotionCaps;
	CVariable<bool> mv_showStartLocation;
	CVariable<bool> mv_showFuturePath;
	CVariable<bool> mv_showCoordinates;
	CVariable<bool> mv_showBodyMoveDir;
	CVariable<bool> mv_printDebugText;

	CVariable<bool> mv_useKeyStrafe;
	CVariable<bool> mv_useNaturalSpeed;

	CVariable<bool> mv_showShaders;
	CVariable<bool> mv_showSkyBox;

	CVariable<bool> mv_showTextureUsage;
	CVariable<bool> mv_showAllTextures;

	CVariable<bool> mv_lighting;
	CVariable<bool> mv_animateLights;

	CVariable<bool> mv_disableLod;

	CVariable<Vec3> mv_backgroundColor;
	CVariable<Vec3> mv_objectAmbientColor;

	CVariable<Vec3> mv_lightDiffuseColor0;
	CVariable<Vec3> mv_lightDiffuseColor1;
	CVariable<Vec3> mv_lightDiffuseColor2;

	CVariable<bool> mv_disableVisibility;
	CVariable<float> mv_fov;
	CVariable<bool> mv_showPhysics;
	CVariable<bool> mv_showPhysicsTetriders;
	CVariable<bool> mv_showOcclusion;

	CVariableArray mv_advancedTable;

	CVarObject m_vars;

	IPhysicalEntity *m_pPhysicalEntity;

	CAnimatedCharacterEffectManager m_effectManager;

	bool m_bPaused;

	// Generated message map functions
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnAnimBack();
	afx_msg void OnAnimFastBack();
	afx_msg void OnAnimFastForward();
	afx_msg void OnAnimFront();
	afx_msg void OnAnimPlay();
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnDestroy();

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // __ModelViewport_h__
