

//////////////////////////////////////////////////////////////////////
//
// CryEngine Source code
//
// File:SkeletonAnim.h
// Implementation of Skeleton class
//
// History:
// August 6, 2005: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#ifndef _SKELETON_ANIM_H
#define _SKELETON_ANIM_H

class CSkeletonAnim;
class CSkeletonPose;
class CCharInstance;
struct ModelAnimationHeader;

#define MAX_EXEC_QUEUE (0x8) //max anims in exec-queue

#define numVIRTUALLAYERS (0x10)

#define TMPREG00 (0x00) //for transitions
#define TMPREG01 (0x01) //for transitions
#define TMPREG02 (0x02)	//for transitions
#define TMPREG03 (0x03)	//for transitions
#define MULTILAYER (0x04)	//for multi-layer blending
#define TMPREG05 (0x05) //just for LMGs
#define TMPREG06 (0x06) //just for LMGs
#define TMPSTYLEOLD (0x07)	//just for transitions
#define TMPSTYLENEW (0x08)	//just for transitions
#define FINALREL (0xff)

#define numTMPLAYERS (9)


//----------------------------------------------------------------------------
//--- this structure is used for graphical representation of the skeleton ---
//----------------------------------------------------------------------------
struct StrafeWeights4
{
	f32 f,l,b,r;
	ILINE StrafeWeights4(){};
	ILINE StrafeWeights4( f32 vf, f32 vl, f32 vb, f32 vr ) { f=vf; l=vl; b=vb; r=vr; };
	ILINE void operator () ( f32 vf, f32 vl, f32 vb, f32 vr ) { f=vf; r=vl; l=vb; b=vr; };
	ILINE StrafeWeights4 operator * (f32 k) const { return StrafeWeights4(f*k,l*k,b*k,r*k); }
	ILINE StrafeWeights4 operator + (StrafeWeights4& v1) const { return StrafeWeights4(f+v1.f,l+v1.l,b+v1.b,r+v1.r); }
	ILINE StrafeWeights4& operator /= (f32 k) {	k=1.0f/k; f*=k; l*=k; b*=k; r*=k; return *this;	}
	ILINE StrafeWeights4& operator += (StrafeWeights4& v1) { f+=v1.f; l+=v1.l; b+=v1.b; r+=v1.r; return *this; }
};

struct StrafeWeights6
{
	f32 f,fl,l, b,br,r;
	ILINE StrafeWeights6(){};
	ILINE StrafeWeights6( f32 va, f32 vb, f32 vc, f32 vd, f32 ve, f32 vf ) { f=va; fl=vb; l=vc; b=vd; br=ve; r=vf; };
	ILINE void operator () ( f32 va, f32 vb, f32 vc, f32 vd, f32 ve, f32 vf ) { f=va; fl=vb; l=vc; b=vd; br=ve; r=vf; };
	ILINE StrafeWeights6 operator * (f32 k) const { return StrafeWeights6( f*k,fl*k,l*k,b*k,br*k,r*k ); }
	ILINE StrafeWeights6 operator + (StrafeWeights6& v1) const { return StrafeWeights6(f+v1.f, fl+v1.fl, l+v1.l, b+v1.b, br+v1.br, r+v1.r); }
	ILINE StrafeWeights6& operator /= (f32 k) {	k=1.0f/k; f*=k; fl*=k; l*=k; b*=k; br*=k; r*=k; return *this;	}
	ILINE StrafeWeights6& operator += (StrafeWeights6& v1) { f+=v1.f; fl+=v1.fl; l+=v1.l; b+=v1.b; br+=v1.br; r+=v1.r; return *this; }
};

struct IdleStepWeights6
{
	f32 fr,rr,br, fl,ll,bl;
	ILINE IdleStepWeights6(){};
	ILINE IdleStepWeights6( f32 vfr, f32 vrr, f32 vbr, f32 vfl, f32 vll, f32 vbl ) { fr=vfr; rr=vrr; br=vbr;   fl=vfl; ll=vll; bl=vbl; };
	ILINE void operator() ( f32 vfr, f32 vrr, f32 vbr, f32 vfl, f32 vll, f32 vbl ) { fr=vfr; rr=vrr; br=vbr;   fl=vfl; ll=vll; bl=vbl; };
	ILINE IdleStepWeights6& operator /= (f32 k) {	k=1.0f/k; fr*=k; rr*=k; br*=k;  fl*=k; ll*=k; bl*=k; return *this;	}
	ILINE IdleStepWeights6& operator += (IdleStepWeights6& v1) { fr+=v1.fr; rr+=v1.rr; br+=v1.br;   fl+=v1.fl; ll+=v1.ll; bl+=v1.bl; return *this; }
};

struct LocomotionWeights6
{
	f32 fl,ff,fr,  sl,sf,sr;	
	ILINE LocomotionWeights6(){};
	ILINE LocomotionWeights6( f32 a,f32 b,f32 c,    f32 d,f32 e,f32 f ) {	fl=a; ff=b; fr=c;	 sl=d; sf=e; sr=f; };
	ILINE void operator () (  f32 a,f32 b,f32 c,    f32 d,f32 e,f32 f ) {	fl=a; ff=b; fr=c;  sl=d; sf=e; sr=f; };
	ILINE LocomotionWeights6& operator * (f32 k) { fl*=k; ff*=k; fr*=k; 		sl*=k; sf*=k; sr*=k; return *this; }
	ILINE LocomotionWeights6& operator /= (f32 k) {	k=1.0f/k; fl*=k; ff*=k; fr*=k; 		sl*=k; sf*=k; sr*=k; return *this; }
	ILINE LocomotionWeights6& operator += (LocomotionWeights6& v1) { fl+=v1.fl;	ff+=v1.ff;	fr+=v1.fr;	   sl+=v1.sl;	sf+=v1.sf;	sr+=v1.sr; return *this; }
};

struct LocomotionWeights7
{
	f32 fl,ff,fr,  mf,  sl,sf,sr;	
	ILINE LocomotionWeights7(){};
	ILINE LocomotionWeights7( f32 a,f32 b,f32 c,   f32 d,    f32 e,f32 f,f32 g ) {	fl=a; ff=b; fr=c;	mf=d; sl=e; sf=f; sr=g; };
	ILINE void operator () (  f32 a,f32 b,f32 c,   f32 d,    f32 e,f32 f,f32 g ) {	fl=a; ff=b; fr=c; mf=d;	sl=e; sf=f; sr=g; };
	ILINE LocomotionWeights7& operator * (f32 k) {	fl*=k; ff*=k; fr*=k; 	mf*=k; 	sl*=k; sf*=k; sr*=k; return *this; }
	ILINE LocomotionWeights7& operator /= (f32 k) {	k=1.0f/k; fl*=k; ff*=k; fr*=k; 	mf*=k; 	sl*=k; sf*=k; sr*=k; return *this; }
	ILINE LocomotionWeights7& operator += (LocomotionWeights7& v1) { fl+=v1.fl;	ff+=v1.ff;	fr+=v1.fr;	mf+=v1.mf; sl+=v1.sl;	sf+=v1.sf;	sr+=v1.sr; 	return *this; }
};





// This is used to pass the information about animation that needs to be blended between
// several animations to the bone that calculates the actual position / rotation out of it
struct CLMG
{
	int m_nLMGID;
	int m_nGlobalLMGID;
	uint32 m_numAnims;
	int m_nAnimID[MAX_LMG_ANIMS];
	int m_nGlobalID[MAX_LMG_ANIMS];
	f32 m_fBlendWeight[MAX_LMG_ANIMS];
	int m_nSegmentCounter[MAX_LMG_ANIMS];
};



struct SAnimation
{
	CryCharAnimationParams m_EAnimParams;
	int m_nEAnimID;
	int m_nEGlobalID;
	f32 m_fEBlendWeight;
	f32 m_fETimeNew;				//this is a percentage value between 0-1
	f32 m_fETimeOld;				//this is a percentage value between 0-1
	f32 m_fETimeOldPrev;				//this is a percentage value between 0-1
	int m_nESegmentCounter;	//count the segments in an animation
	SControllerArray m_jointControllers;
	f32 m_fETWDuration;			//time-warped duration 
	uint16 m_numAimPoses;
	int16 m_nAnimEOC; //if 1 then "end of cycle" reached
};

ILINE SAnimation CopyAnim0( const CAnimation& in, int32 i)
{
	uint32 numAimPoses=0;
	numAimPoses+=(in.m_nAnimAimID0>=0);
	numAimPoses+=(in.m_nAnimAimID1>=0);

	SAnimation out;
	out.m_nEAnimID				=	in.m_LMG0.m_nAnimID[i];
	out.m_nEGlobalID			=	in.m_LMG0.m_nGlobalID[i];
	out.m_fEBlendWeight		=	in.m_LMG0.m_fBlendWeight[i];
	out.m_nESegmentCounter=	in.m_LMG0.m_nSegmentCounter[i];
	out.m_jointControllers= in.m_LMG0.m_jointControllers[i];

	out.m_nAnimEOC				=	in.m_bEndOfCycle; //if 1 then "end of cycle" reached

	out.m_fETimeNew				=	in.m_fAnimTime; //this is a percentage value between 0-1
	out.m_fETimeOld				=	in.m_fAnimTimePrev; //in.m_fETimeOld; //this is a percentage value between 0-1
	out.m_fETimeOldPrev		=	in.m_fAnimTimePrev; //this is a percentage value between 0-1
	out.m_fETWDuration	  =	in.m_fCurrentDuration;			//time-warped duration 
	out.m_numAimPoses			=	numAimPoses;
	out.m_EAnimParams			=	in.m_AnimParams;

	//	assert(out.m_fETimeOld==out.m_fETimeOldPrev);

	return out;
}


struct RMovement
{
	SAnimRoot m_relFuturePath[ANIM_FUTURE_PATH_LOOKAHEAD];	//position of the root in the future (up to 1 second in the future)

	SFootPlant m_FootPlant;
	uint32 m_nFootBits;

	f32 m_fUserData[NUM_ANIMATION_USER_DATA_SLOTS];

	f32		m_fRelRotation;
	Vec3	m_vRelTranslation;
	f32		m_fCurrentDuration;
	f32		m_fCurrentSlope;
	f32		m_fCurrentTurn;
	Vec3	m_vCurrentVelocity;

	Quat m_FSpine0RelQuat;
	Quat m_FSpine1RelQuat;
	Quat m_FSpine2RelQuat;
	Quat m_FSpine3RelQuat;
	Quat m_FNeckRelQuat;

	f32 m_fAllowMultilayerAnim;


	RMovement()	{	Init(); }

	void Init()
	{
		m_nFootBits = 0;

		m_fRelRotation=0;
		m_vRelTranslation = Vec3(ZERO);
		m_fCurrentDuration = -1.0;
		m_vCurrentVelocity = Vec3(ZERO);
		m_fCurrentTurn = 0.0f;
		m_fCurrentSlope = 0.0f;

		m_FSpine0RelQuat.SetIdentity();
		m_FSpine1RelQuat.SetIdentity();
		m_FSpine2RelQuat.SetIdentity();
		m_FSpine3RelQuat.SetIdentity();
		m_FNeckRelQuat.SetIdentity();

		for (uint32 i=0; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++)
		{
			m_relFuturePath[i].m_NormalizedTimeAbs=0;					//absolute time in range [0...1]
			m_relFuturePath[i].m_TransRot.SetIdentity();	//relative key-frames of the root in the future
		}

		m_fAllowMultilayerAnim = 1.0f;
		for (int i=0; i<NUM_ANIMATION_USER_DATA_SLOTS; i++)
			m_fUserData[i] = 0.0f;
	};

};



//-------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------
class CSkeletonAnim: public ISkeletonAnim
{
	friend class CSkinInstance;

public:

	CSkeletonAnim()	{}


	size_t SizeOfThis (ICrySizer * pSizer);
	void Serialize(TSerialize ser);


	void SetCharEditMode( uint32 m ); 
	bool GetCharEditMode() { return m_CharEditMode > 0; };

	void SetAnimationDrivenMotion(uint32 ts); 
	void SetMirrorAnimation(uint32 ts); 
	uint32 GetAnimationDrivenMotion() { return m_AnimationDrivenMotion; };

	void SetTrackViewExclusive(uint32 i)
	{
		if (m_TrackViewExclusive && !i)	m_bReinitializeAnimGraph = true;
		m_TrackViewExclusive= i > 0;
	};
	uint32 GetTrackViewStatus() {	return m_TrackViewExclusive;	};


	bool StartAnimation(const char* szAnimName0,const char* szAnimName1, const char* szAim0,const char* szAim1, const struct CryCharAnimationParams& Params);
	bool StopAnimationInLayer(int32 nLayer, f32 BlendOutTime);
	bool StopAnimationsAllLayers();

	uint32 AnimationToQueue( const ModelAnimationHeader* pAnim0,int a0, const ModelAnimationHeader* pAnim1,int a1, const ModelAnimationHeader* pAnimAim0,int aid0,const ModelAnimationHeader* pAnimAim1,int aid1, f32 InterpVal, f32 btime, const CryCharAnimationParams& AnimParams);
	void ClearFIFOLayer( int32 nLayer );
	void UnloadAnimationAssets(std::vector<CAnimation>& arrAFIFO, int num);
	int GetNumAnimsInFIFO(uint32 nLayer) { return m_arrLayer_AFIFO[nLayer].size(); };
	bool RemoveAnimFromFIFO(uint32 nLayer, uint32 num);
	CAnimation& GetAnimFromFIFO(uint32 nLayer, uint32 num )
	{
		uint32 numAnimsInFifo = m_arrLayer_AFIFO[nLayer].size();
		if (num>=numAnimsInFifo)
			return g_DefaultAnim;
		return m_arrLayer_AFIFO[nLayer][num];
	}
	void ManualSeekAnimationInFIFO(uint32 nLayer, uint32 num, float time, bool advance);

	void SetIWeight(uint32 nLayer, f32 bw);
	f32 GetIWeight(uint32 nLayer);

	//! Set the current time of the given layer, in seconds
	void SetLayerTime (uint32 nLayer, float fTimeSeconds);
	//! Return the current time of the given layer, in seconds
	float GetLayerTime (uint32 nLayer) const;

	// sets the animation speed scale for layers
	void SetLayerUpdateMultiplier(int32 nLayer, f32 fSpeed);
	f32 GetAnimationSpeedLayer (uint32 nLayer)
	{
		if (nLayer<numVIRTUALLAYERS)
			return m_arrLayerSpeedMultiplier[nLayer];
		return 1.0f;
	}

	ILINE uint8 GetActiveLayer(uint8 layer)
	{
		return (m_ActiveLayer >> layer) & 1;
	}

	ILINE void SetActiveLayer(uint8 layer, uint8 val)
	{
		uint32 mask2 = 1 << layer;
		uint32 mask = (val & 0x1) << layer;
		m_ActiveLayer = (m_ActiveLayer & ~mask2) | mask;
	}






	virtual void SetDesiredLocalLocation(const QuatT& desiredLocalLocation, float deltaTime, float frameTime, float turnSpeedMultiplier);
	virtual void SetDesiredMotionParam(EMotionParamID id, float value, float deltaTime, bool initOnly = false);
	virtual float GetDesiredMotionParam(EMotionParamID id) const;
	virtual void SetBlendSpaceOverride(EMotionParamID id, float value, bool enable); // Enable/Disable direct override of blendspace weights, used from CharacterEditor.




	void SetDesiredMotionParamsFromDesiredLocalLocation(float frameTime);
	void SetClampedMotionParam(SLocoGroup& lmg, EMotionParamID id, float newValue, float deltaTime, bool initOnly);

	void UpdateMotionParamDescs(SLocoGroup& lmg, float transitionBlendWeight);
	void ConvertLMGCapsToMotionParamDesc(EMotionParamID id, MotionParamDesc& desc, const LMGCapabilities& caps);

	void UpdateMotionParamBlendSpacesForActiveMotions(float deltaTime);
	void UpdateMotionBlendSpace(SLocoGroup& lmg, float deltaTime, float transitionBlendWeight);
	void UpdateMotionParamBlendSpace(SLocoGroup& lmg, EMotionParamID id); // Update blendspace for the given parameter
	void CalculateMotionParamBlendSpace(MotionParam& param, float undefined); // Helper function to UpdateMotionParamBlendSpace

	// Converts new blendspace into old blendspace (and updates/interpolates strafe vector)
	void UpdateOldMotionBlendSpace(SLocoGroup& lmg, float frameTime, float transitionBlendWeight);

	// Converts new blendspace into old blendspace (and updates/interpolates strafe vector)
	void UpdateOldMotionBlendSpace(SLocoGroup& lmg, float frameTime);

	void ProcessAnimations(  const QuatT& rPhysLocationCurr, const QuatTS& rAnimCharLocationCurr, uint32 OnRender );
	uint32 BlendManager (f32 deltatime, std::vector<CAnimation>& arrLayer, uint32 nLayer );
	uint32 BlendManagerDebug ( std::vector<CAnimation>& arrLayer, uint32 nLayer );
	void LayerBlendManager( f32 fDeltaTime, uint32 nLayer );
	void UpdateAndPushIntoExecBuffer( std::vector<CAnimation>& arrAFIFO, uint32 nLayer, uint32 NumAnimsInQueue, uint32 AnimNo );


	void WeightComputation(SLocoGroup& lmg);
	void WeightComputation_STF2( const GlobalAnimationHeader& rGlobalAnimHeader, const SLocoGroup& lmg, uint32 offset, struct BlendWeights_STF2& st2 );
	void WeightComputation_S2( const GlobalAnimationHeader& rGlobalAnimHeader, const SLocoGroup& lmg, uint32 offset, struct BlendWeights_S2& s2 );
	void WeightComputation_ST2( const GlobalAnimationHeader& rGAH, const SLocoGroup& lmg, uint32 offset, struct BlendWeights_ST2& st2 );
	void WeightComputation_STX( const GlobalAnimationHeader& rGAH, const SLocoGroup& lmg, uint32 offset, struct BlendWeights_STX& stx );
	static Vec3 WeightComputation3(const Vec2& P, const Vec2 &v0, const Vec2 &v1, const Vec2 &v2);
	static Vec4 WeightComputation4(const Vec2& P, const Vec2 &p0, const Vec2 &p1, const Vec2 &p2, const Vec2 &p3);
	StrafeWeights6 WeightCalculation6( const Vec2& P, const Vec2 &F, const Vec2 &FL, const Vec2 &L, const Vec2 &B, const Vec2 &BR, const Vec2 &R ); 
	LocomotionWeights6 WeightCalculationLoco6( Vec2& P,Vec2 &FL,Vec2 &FM,Vec2 &FR, Vec2 &BL,Vec2 &BM,Vec2 &BR); 
	LocomotionWeights7 WeightCalculationLoco7( Vec2& P,Vec2 &FL,Vec2 &FM,Vec2 &FR,Vec2 &MM,Vec2 &BL,Vec2 &BM,Vec2 &BR); 
	f32 GetTimewarpedDuration(SLocoGroup& lmg);



	QuatT m_desiredLocalLocation;
	float m_desiredArrivalDeltaTime;
	float m_desiredTurnSpeedMultiplier; // Makes players turn/catchup faster, being more responsive to quick turns.
	f32 m_fDesiredTurnSpeedOffset; 
	f32 m_fDesiredTurnSpeedScale;
	f32 m_fDesiredTurnSpeedBlend;
	// Enable/Disable direct override of blendspace weight values, primarily used from CharacterEditor.
	float m_desiredMotionParam[eMotionParamID_COUNT];
	bool m_CharEditBlendSpaceOverrideEnabled[eMotionParamID_COUNT];
	float m_CharEditBlendSpaceOverride[eMotionParamID_COUNT];

	std::vector<CAnimation> m_arrLayer_AFIFO[numVIRTUALLAYERS]; //for every layer we can have an execution queue
	f32 m_arrLayerSpeedMultiplier[numVIRTUALLAYERS];
	f32 m_arrLayerBlending[numVIRTUALLAYERS];
	f32 m_arrLayerBlendingTime[numVIRTUALLAYERS];

	Vec2 m_vAbsDesiredMoveDirectionSmooth;
	Vec2 m_vAbsDesiredMoveDirectionSmoothRate;


	int (*m_pEventCallback)(ICharacterInstance*,void*);
	void* m_pEventCallbackData;
	void SetEventCallback(int (*func)(ICharacterInstance*,void*), void *pdata)
	{
		m_pEventCallback = func;
		m_pEventCallbackData = pdata;
	}
	AnimEventInstance GetLastAnimEvent() { return m_LastAnimEvent; };

	void AnimCallback ( std::vector<CAnimation>& arrAFIFO, uint32 AnimNo, uint32 AnimQueueLen);


	int (*m_pPreProcessCallback)(ICharacterInstance*,void*);
	void* m_pPreProcessCallbackData;
	void SetPreProcessCallback(int (*func)(ICharacterInstance*,void*), void *pdata)
	{
		m_pPreProcessCallback = func;
		m_pPreProcessCallbackData = pdata;
	}
	AnimEventInstance m_LastAnimEvent;

	uint32 m_IsAnimPlaying;  
	uint16 m_AnimationDrivenMotion;
	uint16 m_MirrorAnimation;
	uint16 m_ActiveLayer;
	uint16 m_ShowDebugText;

	f32 GetFootPlantStatus();
	void SetDebugging( uint32 debugFlags );

	uint32 m_FuturePath : 1;
	uint32 m_CharEditMode: 1; //this is a flag register
	uint32 m_TrackViewExclusive : 1;
	uint32 m_bReinitializeAnimGraph : 1;

	void SetFuturePathAnalyser(uint32 a) { m_FuturePath=a > 0; };
	void GetFuturePath( SAnimRoot* pRelFuturePath );
	AnimTransRotParams GetBlendedAnimTransRot(f32 DeltaTime, uint32 clamp);
	AnimTransRotParams GetTransRot(f32 fDeltaTime, const Vec3* parrAbsFuturePathPos, const Quat* parrAbsFuturePathRot);

	Vec3 GetCurrentVelocity() {	return m_BlendedRoot.m_vCurrentVelocity;	};
	Vec3 GetCurrentAimDirection();
	Vec3 GetCurrentLookDirection();
	f32 GetCurrentSlope() { return m_BlendedRoot.m_fCurrentSlope; };
	Vec3 GetRelTranslation();
	QuatT GetRelMovement();
	Vec3 GetRelFootSlide();
	f32 GetRelRotationZ();
	f32 GetUserData(int i){ return m_BlendedRoot.m_fUserData[i]; }
	void InitSkeletonAnim( CCharInstance* pInstance, CSkeletonPose* pSkeletonPose );
	void ProcessAnimationUpdate();
	void ProcessForwardKinematics( const QuatT& rPhysLocationCurr, const QuatTS& rAnimCharLocationCurr );

	RMovement m_BlendedRoot;
	CSkeletonPose* m_pSkeletonPose; 
	CCharInstance* m_pInstance;
};





#endif // _JOINT_H
