//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File: AnimationManager.h
//  Implementation of Animation Manager.h
//
//	History:
//	January 12, 2005: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#ifndef _CRYTEK_CONTROLLER_MANAGER_HEADER_
#define _CRYTEK_CONTROLLER_MANAGER_HEADER_

#include "Controller.h"
#include "ControllerPQLog.h"
//#include "ControllerPQ.h"
#include "ControllerTCB.h"

//#include "ParamizedMotion/ParamizedMotion.h"

#include "NameCRCHelper.h"
#include <queue>
#include <functional>


class CAnimationSet;
class CryCGALoader;
class CAnimationManager;
class CAnimationSet;

#define AIM_POSES (49)


struct SFootPlant
{
	f32 m_LHeelStart,m_LHeelEnd;
	f32 m_LToe0Start,m_LToe0End;
	f32 m_RHeelStart,m_RHeelEnd;
	f32 m_RToe0Start,m_RToe0End;

	SFootPlant()
	{
		m_LHeelStart=-10000.0f; m_LHeelEnd=-10000.0f;
		m_LToe0Start=-10000.0f; m_LToe0End=-10000.0f;
		m_RHeelStart=-10000.0f; m_RHeelEnd=-10000.0f;
		m_RToe0Start=-10000.0f; m_RToe0End=-10000.0f;
	};

};

struct SSegments
{
	f32 a,b,c,d,e,f;

	ILINE SSegments()
	{
		a=1.0f;
		b=1.0f;
		c=1.0f;
		d=1.0f;
		e=1.0f;
	};
	ILINE SSegments( f32 va, f32 vb, f32 vc, f32 vd, f32 ve, f32 vf ) { a=va; b=vb; c=vc; d=vd; e=ve; f=vf; };
	ILINE void operator () ( f32 va, f32 vb, f32 vc, f32 vd, f32 ve, f32 vf ) { a=va; b=vb; c=vc; d=vd; e=ve; f=vf; };
};

ILINE SFootPlant operator * (const SFootPlant& fp, f32 t) 
{ 
	SFootPlant rfp;
	rfp.m_LHeelStart=fp.m_LHeelStart*t;	rfp.m_LHeelEnd=fp.m_LHeelEnd*t;
	rfp.m_LToe0Start=fp.m_LToe0Start*t;	rfp.m_LToe0End=fp.m_LToe0End*t;
	rfp.m_RHeelStart=fp.m_RHeelStart*t;	rfp.m_RHeelEnd=fp.m_RHeelEnd*t;
	rfp.m_RToe0Start=fp.m_RToe0Start*t;	rfp.m_RToe0End=fp.m_RToe0End*t;
	return rfp;
}

//vector self-addition
ILINE void operator += (SFootPlant& v0, const SFootPlant& v1) 
{
	v0.m_LHeelStart+=v1.m_LHeelStart;	v0.m_LHeelEnd+=v1.m_LHeelEnd;
	v0.m_LToe0Start+=v1.m_LToe0Start;	v0.m_LToe0End+=v1.m_LToe0End;
	v0.m_RHeelStart+=v1.m_RHeelStart;	v0.m_RHeelEnd+=v1.m_RHeelEnd;
	v0.m_RToe0Start+=v1.m_RToe0Start;	v0.m_RToe0End+=v1.m_RToe0End;
}

struct AnimEvents
{
	f32 m_time;							//percentage value
	string m_strEventName;
	string m_strCustomParameter;
	string m_strBoneName;
	Vec3 m_vOffset;
	Vec3 m_vDir;
};

struct BSAnimation
{
	Vec3 m_Position;
	string m_strAnimName;
};



struct AimIKPose
{
	Quat m_Spine0;
	Quat m_Spine1;
	Quat m_Spine2;
	Quat m_Spine3;
	Quat m_Neck;
	Quat m_RClavicle;
	Quat m_RUpperArm;
	Quat m_RForeArm;
	Quat m_RHand;
	Quat m_WeaponBone;

	Quat m_Head;
	Quat m_LClavicle;
	Quat m_LUpperArm;
	Quat m_LForeArm;
	Quat m_LHand;

	ILINE AimIKPose operator * (f32 k) 
	{ 
		AimIKPose ap;
		ap.m_Spine0			=	m_Spine0		*k;
		ap.m_Spine1			=	m_Spine1		*k;
		ap.m_Spine2			=	m_Spine2		*k;
		ap.m_Spine3			=	m_Spine3		*k;
		ap.m_Neck				=	m_Neck			*k;
		ap.m_RClavicle	=	m_RClavicle	*k;
		ap.m_RUpperArm	=	m_RUpperArm	*k;
		ap.m_RForeArm		=	m_RForeArm	*k;
		ap.m_RHand			=	m_RHand			*k;
		ap.m_WeaponBone	=	m_WeaponBone*k;

		ap.m_Head				=	m_Head			*k;
		ap.m_LClavicle	=	m_LClavicle	*k;
		ap.m_LUpperArm	=	m_LUpperArm	*k;
		ap.m_LForeArm		=	m_LForeArm	*k;
		ap.m_LHand			=	m_LHand			*k;
		return ap; 
	}

	ILINE void SetNLerp(const AimIKPose& q,const AimIKPose& p, f32 t) 
	{ 
		m_Spine0.SetNlerp(q.m_Spine0,p.m_Spine0,t);//			=	(m_Spine0			% p.m_Spine0).GetNormalized();
		m_Spine1.SetNlerp(q.m_Spine1,p.m_Spine1,t);//			=	(m_Spine1			% ap.m_Spine1).GetNormalized();
		m_Spine2.SetNlerp(q.m_Spine2,p.m_Spine2,t);//			=	(m_Spine2			% ap.m_Spine2).GetNormalized();
		m_Spine3.SetNlerp(q.m_Spine3,p.m_Spine3,t);//			=	(m_Spine3			% ap.m_Spine3).GetNormalized();
		m_Neck.SetNlerp(q.m_Neck,p.m_Neck,t);//				=	(m_Neck				% ap.m_Neck).GetNormalized();
		m_RClavicle.SetNlerp(q.m_RClavicle,p.m_RClavicle,t);//	=	(m_RClavicle	% ap.m_RClavicle).GetNormalized();
		m_RUpperArm.SetNlerp(q.m_RUpperArm,p.m_RUpperArm,t);//	=	(m_RUpperArm	% ap.m_RUpperArm).GetNormalized();
		m_RForeArm.SetNlerp(q.m_RForeArm,p.m_RForeArm,t);//		=	(m_RForeArm		% ap.m_RForeArm).GetNormalized();
		m_RHand.SetNlerp(q.m_RHand,p.m_RHand,t);//			=	(m_RHand			% ap.m_RHand).GetNormalized();
		m_WeaponBone.SetNlerp(q.m_WeaponBone,p.m_WeaponBone,t);//	=	(m_WeaponBone% ap.m_WeaponBone).GetNormalized();

		m_Head.SetNlerp(q.m_Head,p.m_Head,t);//				=	(m_Head				% ap.m_Head).GetNormalized();
		m_LClavicle.SetNlerp(q.m_LClavicle,p.m_LClavicle,t);//	=	(m_LClavicle	% ap.m_LClavicle).GetNormalized();
		m_LUpperArm.SetNlerp(q.m_LUpperArm,p.m_LUpperArm,t);//	=	(m_LUpperArm	% ap.m_LUpperArm).GetNormalized();
		m_LForeArm.SetNlerp(q.m_LForeArm,p.m_LForeArm,t);//		=	(m_LForeArm		% ap.m_LForeArm).GetNormalized();
		m_LHand.SetNlerp(q.m_LHand,p.m_LHand,t);//			=	(m_LHand			% ap.m_LHand).GetNormalized();
	}

	ILINE void operator %= (const AimIKPose& ap) 
	{ 
		m_Spine0		%=	ap.m_Spine0;
		m_Spine1		%=	ap.m_Spine1;
		m_Spine2		%=	ap.m_Spine2;
		m_Spine3		%=	ap.m_Spine3;
		m_Neck			%=	ap.m_Neck;
		m_RClavicle	%=	ap.m_RClavicle;
		m_RUpperArm	%=	ap.m_RUpperArm;
		m_RForeArm	%=	ap.m_RForeArm;
		m_RHand			%=	ap.m_RHand;
		m_WeaponBone%=	ap.m_WeaponBone;

		m_Head			%=	ap.m_Head;
		m_LClavicle	%=	ap.m_LClavicle;
		m_LUpperArm	%=	ap.m_LUpperArm;
		m_LForeArm	%=	ap.m_LForeArm;
		m_LHand			%=	ap.m_LHand;
	}

	ILINE void Normalize() 
	{ 
		m_Spine0.Normalize();
		m_Spine1.Normalize();
		m_Spine2.Normalize();
		m_Spine3.Normalize();
		m_Neck.Normalize();
		m_RClavicle.Normalize();
		m_RUpperArm.Normalize();
		m_RForeArm.Normalize();
		m_RHand.Normalize();
		m_WeaponBone.Normalize();

		m_Head.Normalize();
		m_LClavicle.Normalize();
		m_LUpperArm.Normalize();
		m_LForeArm.Normalize();
		m_LHand.Normalize();
	}

};




#include <PoolAllocator.h>
extern stl::PoolAllocatorNoMT<sizeof(SAnimationSelectionProperties)> *g_Alloc_AnimSelectProps;

struct GlobalAnimationHeader;

//bool GAHCompare(const GlobalAnimationHeader * a, const GlobalAnimationHeader * b);
//template<>
struct GAHless /*: public binary_function <GlobalAnimationHeader, GlobalAnimationHeader, bool> */
{
	inline bool operator()(const GlobalAnimationHeader* _Left, const GlobalAnimationHeader* _Right) const;
};

typedef std::vector<GlobalAnimationHeader *> TGAHqueue;
//////////////////////////////////////////////////////////////////////////
// this is the animation information on the module level (not on the per-model level)
// it doesn't know the name of the animation (which is model-specific), but does know the file name
// Implements some services for bone binding and ref counting
struct GlobalAnimationHeader : public CNameCRCHelper
{
	friend class CAnimationManager;
	friend class CAnimationSet;

	GlobalAnimationHeader ()
	{
		m_nRefCount				= 0;
		m_nPlayCount	 	  = 0;
		m_nFlags					= 0;
		m_fSecsPerTick		= SECONDS_PER_TICK;
		m_nTicksPerFrame	= TICKS_PER_FRAME;

		m_nStartKey				= -1;
		m_nEndKey					= -1;
		m_fStartSec				= -1;		// Start time in seconds.
		m_fEndSec					= -1;		// End time in seconds.

		m_AimIKLeftWrist	= Vec3(ZERO);
		m_fAimDirScale		= 0.25f;
		m_fScale					=  0.01f;

		m_fTotalDuration	= -1.0f;				//asset-features
		m_fDistance				=	-1.0f;				//asset-features
		m_fSpeed					=	-1.0f;				//asset-features
		m_fSlope				  =	0.0f;					//asset-features
		m_fTurnSpeed 			=	0.0f;					//asset-features
		m_fAssetTurn 			=	0.0f;					//asset-features
		m_vVelocity				= Vec3(0,0,0);	//asset-features
		m_StartLocation.SetIdentity();		//asset-features

		m_Segments				=	1;						//asset-features 
		m_SegmentsTime[0] = 0.0f;					//asset-features
		m_SegmentsTime[1] = 1.0f;					//asset-features
		m_SegmentsTime[2] = 1.0f;					//asset-features
		m_SegmentsTime[3] = 1.0f;					//asset-features
		m_SegmentsTime[4] = 1.0f;					//asset-features

		m_nBlendCodeLMG		=	0;
		m_nPlayedCount = 0;
		m_nSelectionCapsCode		=	0;
		m_vCache = 0;
		m_nPriority = 0;

		m_pSelectionProperties = NULL;

		m_bFromDatabase		= false;
		//	m_pPM = NULL;
		OnAssetCreated();
	}

	void AllocateAnimSelectProps()
	{
		assert(m_pSelectionProperties == NULL);
		m_pSelectionProperties = (SAnimationSelectionProperties*)g_Alloc_AnimSelectProps->Allocate();
		m_pSelectionProperties->init();
	}

	virtual ~GlobalAnimationHeader();

	const char * GetPathName() const { return GetName(); };
	int GetCRCPathName() { return m_CRC32Name; }
	//	const char * GetPathNameString() const { return GetNameString(); };
	ILINE f32 GetSegmentDuration(uint32 segment ) 
	{ 
		int32 seg = m_Segments-1;
		assert(segment<=m_Segments);
		f32 t0	=	m_SegmentsTime[segment+0];
		f32 t1	=	m_SegmentsTime[segment+1];
		f32 t		=	t1-t0;
		return m_fTotalDuration*t; 
	};

	ILINE f32 GetSegmentDuration2(uint32 segment ) 
	{ 
		assert(segment>0);
		f32 t0	=	0;//m_SegmentsTime[segment-1];
		f32 t1	=	m_SegmentsTime[segment+0];
		f32 t		=	t1-t0;
		return t; 
	};

	void SetPathName(const string& name) { /*SetNameChar(name.c_str())*/; SetName(name); };


	ILINE uint32 IsAssetLoaded() const {return m_nFlags&CA_ASSET_LOADED;}
	ILINE void OnAssetLoaded() {m_nFlags |= CA_ASSET_LOADED;}

	ILINE uint32 IsAimposeUnloaded() const {return m_nFlags&CA_AIMPOSE_UNLOADED;}
	ILINE void OnAimposeUnloaded() {m_nFlags |= CA_AIMPOSE_UNLOADED;}

	ILINE void ClearAssetLoaded() {m_nFlags &= ~CA_ASSET_LOADED;}

	ILINE uint32 IsAssetCreated() const {return m_nFlags&CA_ASSET_CREATED;}
	ILINE void OnAssetCreated() {m_nFlags |= CA_ASSET_CREATED;}

	ILINE uint32 IsAssetCycle() const {return m_nFlags&CA_ASSET_CYCLE;}
	ILINE void OnAssetCycle() {m_nFlags |= CA_ASSET_CYCLE;}

	ILINE uint32 IsAssetLMG() const {return m_nFlags&CA_ASSET_LMG;}
	ILINE void OnAssetLMG() {m_nFlags |= CA_ASSET_LMG;}

	ILINE uint32 IsAssetLMGValid()const { return m_nFlags&CA_ASSET_LMG_VALID; }
	ILINE void OnAssetLMGValid() { m_nFlags |= CA_ASSET_LMG_VALID; }

	ILINE uint32 IsAssetProcessed()const { return m_nFlags&CA_ASSET_PROCESSED; }
	ILINE void OnAssetProcessed() { m_nFlags |= CA_ASSET_PROCESSED; }

	ILINE uint32 IsAssetRequested()const { return m_nFlags&CA_ASSET_REQUESTED; }
	ILINE void OnAssetRequested() { m_nFlags |= CA_ASSET_REQUESTED; }

	ILINE uint32 IsAssetOnDemand()const { return m_nFlags&CA_ASSET_ONDEMAND; }
	ILINE void OnAssetOnDemand() { m_nFlags |= CA_ASSET_ONDEMAND; }

	void AddRef()
	{
		++m_nRefCount;
	}

	void Release()
	{
		if (!--m_nRefCount)
		{

#ifdef _DEBUG
			for (IController_AutoArray::iterator it = m_arrController.begin(); it!= m_arrController.end(); ++it)
				if (m_bFromDatabase)
					assert ((*it)->NumRefs()==2); // only this object references the controllers now
				else
					assert ((*it)->NumRefs()==1); // only this object references the controllers now
#endif
			m_arrController.clear();		// nobody uses the controllers; clean them up. 
			m_arrBSAnimations.clear();	// nobody uses the blend-spaces; clean them up.
			m_FootPlantBits.clear();			// nobody uses the foot-plants; clean them up.
		}
	}

#ifdef _DEBUG
	// returns the maximum reference counter from all controllers. 1 means that nobody but this animation
	// structure refers to them
	int MaxControllerRefCount()
	{
		if (m_arrController.empty())
			return 0;
		int nMax = m_arrController[0]->NumRefs();
		for (IController_AutoArray::iterator it = m_arrController.begin()+1; it!= m_arrController.end(); ++it)
			if((*it)->NumRefs() > nMax)
				nMax = (*it)->NumRefs();
		return nMax;
	}
#endif
	size_t SizeOfThis() const;

	//---------------------------------------------------------------
	IController* GetControllerByJointID(uint32 nControllerID);

	//void MakePM(int32 localAnimID, int32 globalAnimID);
	//CParamizedMotion* GetPM() { return m_pPM; }

	size_t GetControllersCount() {
		return m_arrController.size();
	}

	void ClearControllers() {
		m_arrController.clear();
	}


	void StartAnimation();
	void StopAnimation();


private:
	// unpack animation in temporary memory
	bool UnpackAnimation();
	// 
	void ClearCache();
	static void PutInCache(GlobalAnimationHeader* ptr);

public:

	SAnimationSelectionProperties* m_pSelectionProperties;

	SFootPlant m_FootPlantVectors;
	DynArray<BSAnimation> m_arrBSAnimations;
	DynArray<string> m_strSpliceAnim;
	string m_strLayerAnim;
	string m_strOldStyle;
	string m_strNewStyle;
	DynArray<AimIKPose> m_arrAimIKPoses; //if this animation contains aim-poses, we store them here
	DynArray<uint8> m_FootPlantBits;
	DynArray<AnimEvents> m_AnimEvents;
	QuatT m_StartLocation;     //asset-feature: the original location of the animation in world-space
	Vec3  m_vVelocity;         //asset-feature: the velocity vector for this asset
	// controllers comprising the animation; within the animation, they're sorted by ids

	// the number of referrers to this global animation record (doesn't matter if the controllers are currently loaded)
	int m_nRefCount;
	int m_nPlayCount;  //when we play this animation we increase the counter

	uint32 m_nPlayedCount;
	uint32 m_nPriority;

	Vec3 m_AimIKLeftWrist;
	f32 m_fAimDirScale;

	// the parameter from the initial load animation, used for reloadin
	f32 m_fScale; 
	// the flags (see the enum in the top of the declaration)
	uint32 m_nFlags;

	// timing data, retrieved from the timing_chunk_desc
	int32 m_nTicksPerFrame;
	f32 m_fSecsPerTick;
	int32 m_nStartKey;
	int32 m_nEndKey;
	f32 m_fStartSec;					//Start time in seconds.
	f32 m_fEndSec;						//End time in seconds.
	f32 m_fTotalDuration;			//asset-feature: total duration in seconds.
	f32 m_fDistance;					//asset-feature: the absolute distance this objects is moving
	f32 m_fSpeed;							//asset-feature: speed (meters in second)
	f32 m_fSlope;							//asset-feature: uphill-downhill measured in degrees 
	f32 m_fTurnSpeed;					//asset-feature: turning speed per second 
	f32 m_fAssetTurn;					//asset-feature: radiant between first and last frame 
	uint32 m_Segments;				//asset-feature: amount of segements 
	f32 m_SegmentsTime[5];    //asset-feature: normalized-time for each segment
	uint32 m_nBlendCodeLMG;
	uint32 m_nSelectionCapsCode;

	// loaded from database
	char m_bFromDatabase : 1;
	//	char m_bStreamed : 1;
	// streaming feature
	//	bool m_bLoaded;


private:
	//	CParamizedMotion* m_pPM;
	IController_AutoArray m_arrController;
	IController_AutoArray m_arrControllerCache;
	Quat * m_vCache;

	static TGAHqueue m_arrGAHQueue;

};


inline bool GAHless::operator()(const GlobalAnimationHeader* _Left, const GlobalAnimationHeader* _Right) const
{
	return _Left->m_nPriority > _Right->m_nPriority;
}


inline bool GAHCompare(const GlobalAnimationHeader* _Left, const GlobalAnimationHeader* _Right) 
{
	return _Left->m_nPriority > _Right->m_nPriority;
}

enum eLoadingOptions {
	eLoadFullData,
	eLoadOnlyInfo
};

class CLoaderDBA;
class CCommonSkinningInfo;
/////////////////////////////////////////////////////////////////////////////////////////////////////
// class CAnimationManager 
// Responsible for creation of multiple animations, subsequently bind to the character bones
// There is one instance of this class per a game.
/////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimationManager : public IAnimEvents
{
public:

	CNameCRCMap		m_AnimationMap;


	//! Returns the number of installed anim-events for this asset
	int32 GetAnimEventsCount(int nGlobalID ) const; 
	int32 GetAnimEventsCount(const char* pFilePath );
	int GetGlobalAnimID(const char* pFilePath);
	void AddAnimEvent(int nGlobalID, const char* pName, const char* pParameter, const char* pBone, float fTime, const Vec3& vOffset, const Vec3& vDir);
	void DeleteAllEventsForAnimation(int nGlobalID);



	// finds the animation by name. Returns -1 if no animation was found. Returns the animation ID if it was found
	int GetGlobalIDbyFilePath(const char * sAnimFileName);
	// create header with the specified name. If the header already exists, it return the index
	int CreateGlobalMotionHeader (const string& strFileName);
	// returns the structure describing the animation data, given the global anim id
	GlobalAnimationHeader& GetGlobalAnimationHeader (int nAnimID);
	// return GlobalAnimationHeader from CRC and dummy if nor found
	GlobalAnimationHeader& GlobalAnimationHeaderFromCRC(uint32);
	uint32 GlobalIDFromCRC(uint32 crc);

	// loads existing animation record, returns false on error

	bool LoadAnimation(int nGlobalAnimId, eLoadingOptions flags);
	bool LoadAnimationFromDBA(int nAnimId, GlobalAnimationHeader& Anim, bool bShowError);
	bool LoadAnimationFromFile(int nGlobalAnimId, GlobalAnimationHeader& Anim, bool bShowError, eLoadingOptions flags);
	bool LoadAnimationPQLogNull(int nGlobalAnimId, GlobalAnimationHeader& Anim);
	bool LoadAnimationTCB(int nGlobalAnimId, std::vector<CControllerTCB>& m_LoadCurrAnimation, CryCGALoader* pCGA, uint32 unique_model_id  );


	void UnloadAnimation(int nGLobalAnimID);
	// returns the total number of animations hold in memory (for statistics)
	uint32 NumGlobalAnimations() { return m_arrGlobalAnimations.size();	}

	// notifies the controller manager that this client doesn't use the given animation any more.
	// these calls must be balanced with AnimationAddRef() calls
	void AnimationRelease (int nGlobalAnimId, CAnimationSet* pClient);

	// puts the size of the whole subsystem into this sizer object, classified, according to the flags set in the sizer
	void GetSize(class ICrySizer* pSizer);

	void Register (CAnimationSet* pClient)
	{
		m_arrClients.insert (std::lower_bound(m_arrClients.begin(), m_arrClients.end(), pClient), pClient);
	}
	void Unregister (CAnimationSet* pClient)
	{
		std::vector<CAnimationSet*>::iterator it = std::lower_bound(m_arrClients.begin(), m_arrClients.end(), pClient);
		if(it != m_arrClients.end() && *it == pClient)
			m_arrClients.erase (it);
		else
			assert (0); // the unregistered client tries to unregister
	}

	bool LoadDBA(std::vector<string>& name, std::vector< CLoaderDBA* > * loaders = 0);
	void RemoveDBA( const string& file);
	const CCommonSkinningInfo * GetSkinningInfo(const char * filename) const;
	void RemoveSkinningInfo(const char * filename);

	void Clear();
	void Reset();

	void ClearChache();
	void ClearStatistics();

	size_t GetGlobalAnimCount() {
		return m_arrGlobalAnimations.size();
	}

	bool GetGlobalAnimStatistics(size_t num, SAnimationStatistics&);


private:
	CLoaderDBA * GetLoaderDBA( const string& file);
	void RemoveLoaderDBA( const string& file);


public:

	// array of global animations
	std::vector<GlobalAnimationHeader> m_arrGlobalAnimations;


	void selfValidate();

	// the sorted list of pointers to the clients that use the services of this manager
	std::vector<CAnimationSet*> m_arrClients;

	size_t m_nCachedSizeofThis;





	CAnimationManager(const CAnimationManager&);
	void operator=(const CAnimationManager&);

	CAnimationManager()
	{ 
		g_Alloc_AnimSelectProps = new stl::PoolAllocatorNoMT<sizeof(SAnimationSelectionProperties)>();

		// we reserve the place for future animations. The best performance will be
		// archived when this number is >= actual number of animations that can be used, and not much greater
		m_arrGlobalAnimations.reserve(4000/*0x800*/); //contains all animation we use in the game
		m_nCachedSizeofThis = 0;
	}

	~CAnimationManager()
	{
		Clear();
		delete g_Alloc_AnimSelectProps;
	}

private:
	typedef std::vector<CLoaderDBA *> TLoadersArray;
	TLoadersArray m_arrLoaders;

};													 

#endif
