////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	10/9/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  interface class to all motions  
/////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CRY_MODEL_ANIMTATION_CONTAINER_HDR_
#define _CRY_MODEL_ANIMTATION_CONTAINER_HDR_

#include "ModelSkeleton.h"
#include "NameCRCHelper.h"
#include "VectorSet.h"
#include "StlUtils.h"

class CModelMesh;
class CCharacterModel;
class CryCGALoader;
class CSkeletonAnim;
class CLoaderDBA;

struct StrafeWeights4;
struct StrafeWeights6;
struct IdleStepWeights6;

struct DebugJoint
{
	DebugJoint() {
		m_idxParent	=	-1;
		m_RelativeQuat.SetIdentity();	
		m_AbsoluteQuat.SetIdentity();
	}

	int16 m_idxParent;					//index of the parent-joint
	QuatT	m_RelativeQuat;		//current relative rotation and position of animation
	QuatT	m_AbsoluteQuat;
};

//! this structure contains info about loaded animations
struct ModelAnimationHeader : public CNameCRCHelper
{

	int32 m_nGlobalAnimId;

	size_t SizeOfThis() const {	return sizeof(ModelAnimationHeader) + m_Name.length(); }

	ModelAnimationHeader() : m_nGlobalAnimId(-1){};
	virtual ~ModelAnimationHeader()	{}

	const char * GetAnimName() const { return GetName(); };

	void SetAnimName(const string& name) { SetNameChar(name.c_str()); };

};


//////////////////////////////////////////////////////////////////////////
// Implementation of ICryAnimationSet, holding the information about animations
// and bones for a single model. Animations also include the subclass of morph targets
//////////////////////////////////////////////////////////////////////////
class CAnimationSet : public IAnimationSet
{
public:
	CAnimationSet();
	~CAnimationSet();

	// gets called when the given animation (by global id) is unloaded.
	// the animation controls need to be unbound to free up the memory
	void Init();
	// prepares to load the specified number of CAFs by reserving the space for the controller pointers
	void prepareLoadCAFs (unsigned nReserveAnimations);
	void selfValidate();
	// tries to load the animation info if isn't present in memory; returns NULL if can't load
	const ModelAnimationHeader* GetModelAnimationHeader(int32 i);

	size_t SizeOfThis() const;

	void PreloadCAF(int nGlobalAnimID);
	// Reload animation from the file
	int ReloadCAF(int nGlobalAnimID);
	int ReloadCAF(const char * szFileName, bool bFullPath = false);

	int UnloadCAF(int nGlobalAnimID);

	// Renew animations information
	int RenewCAF(const char * szFileName, bool bFullPath = false);
	int RenewCAF(int nGlobalAnimID);

	const char* GetFacialAnimationPathByName(const char* szName);
	int GetNumFacialAnimations();
	const char* GetFacialAnimationName(int index);
	const char* GetFacialAnimationPath(int index);

	// when the animinfo is given, it's used to set the values of the global animation as if the animation has already been loaded once -
	// so that the next time the anim info is available and there's no need to load the animation synchronously
	// Returns the global anim id of the file, or -1 if error
	int LoadCAF(const char * szFileName, float fScale, int nAnimID, const char * szAnimName, unsigned nGlobalAnimFlags);
	int LoadLMG(const char * szFileName, float fScale, int nAnimID, const char * szAnimName, unsigned nGlobalAnimFlags);
	int LoadANM(const char * szFileName, float fScale, int nAnimID, const char * szAnimName, unsigned nGlobalAnimFlags, std::vector<CControllerTCB>& m_LoadCurrAnimation, CryCGALoader* pCGA, uint32 unique_model_id );
	bool LoadLocoGroupXML(  GlobalAnimationHeader& rGlobalAnim ); 
	bool LoadParamizedMotionXML(GlobalAnimationHeader& rGlobalAnim);
	// gets called when the given animation (by global id) is loaded
	// the animation controls need to be bound at this point
	void OnAnimationGlobalLoad (int nGlobalAnimId);
	void OnAnimationGlobalUnLoad (int nGlobalAnimId);

//private:
	// This only loops over all local anims and calls the 'per-anim' function.
	// TODO: Maybe it could be removed and the 'per-anim' function called on global load or something instead.
	void ComputeSelectionProperties();
	bool ComputeSelectionProperties(int32 localAnimID);
//public:

	void VerifyLocomotionGroups();

	// pointer to the model
	CCharacterModel* m_pModel;	

	// Animations List

	// fast method
	
	//std::map<uint32,  uint32>	m_

	CNameCRCMap	m_AnimationHashMap;
//	CNameCRCMap	m_FullPathAnimationsHashMap;
	
	typedef std::multimap<uint32, int32> TCRCmap;
	TCRCmap m_FullPathAnimationsMap;

	//struct LocalAnimId
	//{
	//	int nAnimId;
	//	explicit LocalAnimId(int n):nAnimId(n){}
	//};
	struct FacialAnimationEntry
	{
		FacialAnimationEntry(const string& name, const string& path): name(name), path(path) {}
		string name;
		string path;

		operator const char*() const {return name.c_str();}
	};

	typedef VectorSet<FacialAnimationEntry, stl::less_stricmp<FacialAnimationEntry> > FacialAnimationSet;
	FacialAnimationSet m_facialAnimations;


/*
	struct LocalAnimId
	{
		int nAnimId;
		explicit LocalAnimId(int n):nAnimId(n){}
	};*/

	// the index vector: the animations in order of nGlobalAnimId
  //	std::vector<LocalAnimId> m_arrAnimByGlobalId;

	// used to sort the m_arrAnimByGlobalId
	//struct AnimationGlobIdPred
	//{
	//	const std::vector<ModelAnimationHeader>& m_arrAnims;
	//	AnimationGlobIdPred(const std::vector<ModelAnimationHeader>& arrAnims):
	//		m_arrAnims(arrAnims)
	//	{
	//	}

	//	bool operator () (LocalAnimId left, LocalAnimId right)const 
	//	{
	//		return m_arrAnims[left.nAnimId].m_nGlobalAnimId < m_arrAnims[right.nAnimId].m_nGlobalAnimId;
	//	}
	//	bool operator () (int left, LocalAnimId right)const 
	//	{
	//		return left < m_arrAnims[right.nAnimId].m_nGlobalAnimId;
	//	}
	//	bool operator () (LocalAnimId left, int right)const 
	//	{
	//		return m_arrAnims[left.nAnimId].m_nGlobalAnimId < right;
	//	}
	//};

	// the index vector: the animations in order of their local names
//	std::vector<int> m_arrAnimByLocalName;

/*	struct AnimationNamePred
	{
		const std::vector<ModelAnimationHeader>& m_arrAnims;
		AnimationNamePred(const std::vector<ModelAnimationHeader>& arrAnims):
		m_arrAnims(arrAnims) {	}

		bool operator () (int left, int right)const 
		{
			//return stricmp(m_arrAnims[left].m_strAnimName.c_str(),m_arrAnims[right].m_strAnimName.c_str()) < 0;
			return m_arrAnims[left].GetCRC32() == m_arrAnims[right].GetCRC32();
		}
		bool operator () (const char* left, int right)const 
		{
			return stricmp(left,m_arrAnims[right].GetAnimName()) < 0;
		}
		bool operator () (int left, const char* right)const 
		{
			return stricmp(m_arrAnims[left].GetAnimName(),right) < 0;
		}
	};*/


	//---------------------------------------------------------------------
	//---------------------------------------------------------------------
	//---------------------------------------------------------------------

	uint32 numMorphTargets() const; 
	const char* GetNameMorphTarget (int nMorphTargetId);



	// Returns the index of the animation (bone-animations and morphtargets), -1 if there's no such animation
	int GetAnimIDByName (const char* szAnimationName);

	const char* GetNameByAnimID ( int nAnimationId);

	// for internal use only
	uint32 numAnimations() const { return m_arrAnimations.size(); }

	GlobalAnimationHeader* GetGlobalAnimationHeaderFromLocalID(int nAnimationId);
	GlobalAnimationHeader* GetGlobalAnimationHeaderFromLocalName(const char* AnimationName);

	float GetStart (int nAnimationId);
	f32 GetSpeed(int nAnimationId);
	f32 GetSlope(int nAnimationId);
	Vec2 GetMinMaxSpeedAsset_msec(int32 animID );

	uint32 SetLoconames(const GlobalAnimationHeader& rGlobalAnimHeader, int32* nAnimID, const ModelAnimationHeader** pAnim);
	SLocoGroup BuildRealTimeLMG( const ModelAnimationHeader* pAnim0,int nID);
	LMGCapabilities GetLMGPropertiesByName(const char* name, Vec2& vStrafeDirection, f32 fDesiredTurn, f32 fSlope  ); 
	LMGCapabilities GetLMGCapabilities(const SLocoGroup& lmg); 


	f32 GetIWeightForSpeed(int nAnimationId, f32 Speed);
	f32 GetDuration_sec(int nAnimationId);
	uint32 GetAnimationFlags(int nAnimationId);
	uint32 GetBlendSpaceCode(int nAnimationId);
	CryAnimationPath GetAnimationPath(const char* szAnimationName); 
	const QuatT& GetAnimationStartLocation(const char* szAnimationName); 
	virtual const SAnimationSelectionProperties* GetAnimationSelectionProperties(const char* szAnimationName);

	void AnalyseAndModifyAnimations(const string& strPath, CCharacterModel* pModel); 
	void EvaluteGlobalAnimID();

	void CreateSkeletonArray( std::vector< std::vector<DebugJoint> >& arrSkeletons, uint32 nAnimID );
	void SetFootBits(GlobalAnimationHeader& rGlobalAnimHeader, uint32 HowManyPoses, f32 LTS,f32 LTO,uint32 LTOE0);
	uint32 SetFootplantBitsManually( int32 nGlobalAnimID );
	void SetFootplantBitsAutomatically( std::vector< std::vector<DebugJoint> >& arrSkeletons, int32 nAnimID,int32 nGlobalAnimID, int32 lHidx,int32 rHidx,int32 lTidx,int32 rTidx,int32 lNidx,int32 rNidx, uint8 detect  );
	void SetFootplantVectors( std::vector< std::vector<DebugJoint> >& arrSkeletons, uint32 nAnimID, SFootPlant& rFootPlants, uint32 nGlobalAnimID );

	std::vector<std::vector<int> > m_arrStandupAnims;
	std::vector<string> m_arrStandupAnimTypes;

	const char* GetFilePathByName (const char* szAnimationName);
	const char* GetFilePathByID(int nAnimationId);

	int32 GetGlobalIDByName(const char* szAnimationName);
	int32 GetGlobalIDByID(int nAnimationId);
	f32 GetClosestQuatInChannel(const char* szAnimationName,int32 JointID, const Quat& q);
	void AddNullControllers(int nGlobalAnimID);

	void ParameterExtraction( GlobalAnimationHeader& rGlobalAnimHeader );
	void CopyPoses(int32 AnimID, uint32 tkey,uint32 skey, GlobalAnimationHeader& rGAH, CCharacterModel* pModel );
	void Blend_2_Poses( uint32 skey0, uint32 skey1, uint32 tkey, GlobalAnimationHeader& rGAH, f32 t );
	void Blend_4_Poses( uint32 skey0, uint32 skey1, uint32 skey2, uint32 skey3, uint32 tkey, GlobalAnimationHeader& rGAH, f32 t );
	Quat GetOrientationByKey( IController* pIController, uint32 tkey, const CModelJoint* pModelJoint );
	void FindLeftWrist(GlobalAnimationHeader& rGAH, CCharacterModel* pModel );

	IdleStepWeights6 GetIdle2MoveWeights6( Vec2 DesiredDirection  ); 
	IdleStepWeights6 GetIdleStepWeights6(  Vec2 FR,Vec2 RR,Vec2 BR,   Vec2 FL,Vec2 LL,Vec2 BL, Vec2 DesiredDirection  ); 
	StrafeWeights4 GetStrafingWeights4( Vec2 F,Vec2 L,Vec2 B, Vec2 R,Vec2 DesiredDirection ); 
	StrafeWeights6 GetStrafingWeights6( Vec2 F,Vec2 FL,Vec2 L,  Vec2 B,Vec2 BR,Vec2 R, const Vec2& DesiredDirection ); 

	void GetStrafeWeights(const Vec2& F, const Vec2& R, const Vec2& W, float& f, float& r);

	FacialAnimationSet& GetFacialAnimations() {return m_facialAnimations;}

	const char *GetFnPAnimGroupName(int idx) { return (unsigned int)idx<m_arrStandupAnimTypes.size() ? m_arrStandupAnimTypes[idx]:0; }

	// Not safe method. Just direct access to m_arrAnimations
	const ModelAnimationHeader&  GetModelAnimationHeaderRef(int i)
	{
		return m_arrAnimations[i];
	}

	uint32 m_CharEditMode; 

private:

	// No more direct access to vector. No more LocalIDs!!!! This is just vector of registered animations!!!
	// When animation started we need build remap controllers\bones
	std::vector<ModelAnimationHeader> m_arrAnimations;
};



#endif
