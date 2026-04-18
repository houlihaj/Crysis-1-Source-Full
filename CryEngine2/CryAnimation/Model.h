////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	28/09/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  stores the loaded model and animations
/////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CRY_MODEL_HEADER_
#define _CRY_MODEL_HEADER_

#include "SkeletonAnim.h"
#include "SkeletonPose.h"
#include "ModelAnimationSet.h"	//embedded
#include "ModelSkeleton.h"      //embedded
#include "ModelMesh.h"					//embedded
#include "StringUtils.h"

//----------------------------------------------------------------------
// CModel class
//----------------------------------------------------------------------

class CFacialModel;
class CModelMesh;
class CharacterManager;
class CCharInstance;
struct MorphTargets;

struct CCFAnimGeomInfo;
struct CCFBoneGeometry;
struct CCFBGBone;
struct CCFMorphTargetSet;
struct CCFCharLightDesc;

#define ENABLE_BONE_BBOXES (0)
#define MAX_JOINT_AMOUNT (255)


//struct MeshCollisionInfo
//{
//	AABB m_aABB;
//	OBB m_OBB;
//	Vec3 m_Pos;
//	std::vector<short int> m_arrIndexes;
//	int m_iBoneId;
//
//	MeshCollisionInfo()
//	{
//		// This didn't help much.
//		// The BBs are reset to opposite infinites, 
//		// but never clamped/grown by any member points.
//		m_aABB.min.zero();
//		m_aABB.max.zero();
//		m_OBB.m33.SetIdentity();
//		m_OBB.h.zero();
//		m_OBB.c.zero();
//		m_Pos.zero();
//	}
//};

////////////////////////////////////////////////////////////////////////////////
// This class contain data which can be shared between different several models of same type.
// It loads and contains all geometry and materials.
// Also it contains default model state.
class CCharacterModel : public ICharacterModel//, public _i_reference_target_t
{

public:  
	//////////////////////////////////////////////////////////////////////////
	// ICharacterModel implementation.
	//////////////////////////////////////////////////////////////////////////

	virtual void AddRef()	
	{
		++m_nRefCounter;
	}
	virtual void Release()
	{
		if (--m_nRefCounter <= 0)
			delete this;
	}

	int NumRefs()	const
	{
		return m_nRefCounter;
	}


	virtual uint32 GetNumInstances() { return NumInstances();	}
	virtual uint32 GetNumLods();
	virtual IRenderMesh* GetRenderMesh( int nLod );

	virtual uint32 GetTextureMemoryUsage( ICrySizer *pSizer=0 );
	virtual uint32 GetMeshMemoryUsage( ICrySizer *pSizer=0 );
	uint32 SizeOfThis(ICrySizer *pSizer=0 );

	virtual ICharacterInstance * GetInstance(uint32 num)
	{
		if(num > NumInstances())
			return 0;

		std::set<ICharacterInstance*>::iterator it = m_SetInstances.begin();
		advance(it, num);

		return (ICharacterInstance*)*it;
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////////////////
	// ----------------------------- GAME INTERFACE FUNCTIONS ----------------------------- //
	//////////////////////////////////////////////////////////////////////////////////////////
	CCharacterModel(const string& strFileName, CharacterManager* pManager, uint32 type);
	virtual ~CCharacterModel();


	// Returns the interface for animations applicable to this model
	IAnimationSet* GetAnimationSet() {	return &m_AnimationSet;	}

	// Called on a model after it was completly loaded.
	void PostLoad();

	//--------------------------------------------------------------------------------
	//------      Members of ModelJoint      -----------------------------------------
	//--------------------------------------------------------------------------------

	// This is the bone hierarchy. All the bones of the hierarchy are present in this array
//	std::vector<CModelJoint> m_arrModelJoints;
	//CModelSkeleton m_ModelSkeleton;


	//! Performs post-initialization. This step is requred to initialize the pPhysGeom of the bones
	//! After the bone has been loaded but before it is first used. When the bone is first loaded, pPhysGeom
	//! is set to the value equal to the chunk id in the file where the physical geometry (BoneMesh) chunk is kept.
	//! After those chunks are loaded, and chunk ids are mapped to the registered physical geometry objects,
	//! call this function to replace pPhysGeom chunk ids with the actual physical geometry object pointers.
	//!	NOTE:
	//!	The entries of the map that were used are deleted upon exit

	typedef CModelJoint::ChunkIdToPhysGeomMap ChunkIdToPhysGeomMap;
	bool PostInitBonePhysGeom (ChunkIdToPhysGeomMap& mapChunkIdToPhysGeom, int nLodLevel)
	{
		// map each bone,
		// and set the hasphysics to 1 if at least one bone has recognized and mapped its physical geometry
		bool bHasPhysics = false;
		bool bAllowRopePhys = strstr(m_strFilePath,"alien")!=0 || strstr(m_strFilePath,"Alien")!=0;
		uint32 numJoints=m_pModelSkeleton->m_arrModelJoints.size();
		for (uint32 i=0; i<numJoints; ++i)
		{
			if (m_pModelSkeleton->m_arrModelJoints[i].PostInitPhysGeom (mapChunkIdToPhysGeom, nLodLevel, bAllowRopePhys))
				// yes, this model has the physics
				bHasPhysics = true;
		}

		return bHasPhysics;
	}
	void UpdatePhysBonePrimitives(DynArray<BONE_ENTITY> arrBoneEntities, int iLOD);

	void onBonePhysicsChanged()
	{
		uint32 numJoints=m_pModelSkeleton->m_arrModelJoints.size();
		for (uint32 i=0; i<numJoints; ++i)
			m_pModelSkeleton->m_arrModelJoints[i].PostInitialize();
	}

	// updates the physics info of the given lod from the given bone animation descriptor
	void UpdateRootBonePhysics (const BONEANIM_CHUNK_DESC* pChunk, unsigned nChunkSize, int nLodLevel)
	{
		uint32 numJoints=m_pModelSkeleton->m_arrModelJoints.size();
		if (numJoints)
			m_pModelSkeleton->m_arrModelJoints[0].UpdateHierarchyPhysics (pChunk, nChunkSize, nLodLevel);
	}

	// scales the skeleton (its initial pose)
	void scaleBones (float fScale)
	{
		uint32 numJoints=m_pModelSkeleton->m_arrModelJoints.size();
		for (uint32 i=0; i<numJoints; i++)
			m_pModelSkeleton->m_arrModelJoints[i].scale (fScale);
	}


	//--------------------------------------------------------------------------------
	//------                  Instance Management        -----------------------------
	//--------------------------------------------------------------------------------
	// the set of all child objects created; used for the final clean up

	void RegisterInstance (ICharacterInstance*);
	void UnregisterInstance (ICharacterInstance*);

	// destroys all characters. This may (and should) lead to destruction and self-deregistration of this body
	void CleanupInstances();
	// returns true if the instance is registered in this body
	bool DoesInstanceExist (ICharacterInstance* pInstance)	{
		return m_SetInstances.find (pInstance) != m_SetInstances.end();
	}
	uint32 NumInstances() {	return m_SetInstances.size();	}

	//--------------------------------------------------------------------------------
	//------      Members of ModelMesh       -----------------------------------------
	//--------------------------------------------------------------------------------
	// the character file name, empty string means no geometry was loaded (e.g. because of an error)
	const string m_strFilePath;
	const string& GetFilePath() const {	return m_strFilePath;	}
	const char* GetModelFilePath() const {	return m_strFilePath.c_str(); }
	const char* GetNameCStr() const {	return CryStringUtils::FindFileNameInPath(m_strFilePath.c_str());	}

	const char* GetModelAnimEventDatabaseCStr() { return m_strAnimEventFilePath.c_str(); }
	void SetModelAnimEventDatabase(const string& sAnimEventDatabasePath) 
	{
		m_strAnimEventFilePath = sAnimEventDatabasePath;
	}


	void AddModelTracksDatabase(const string& sAnimTracksDatabasePath);

	uint32 GetModelMeshCount()
	{
		return m_arrModelMeshes.size();
	}
	// returns the geometry of the given lod (0 is the highest detail lod)
	CModelMesh* GetModelMesh(uint32 nLOD)
	{
		uint32 numMeshes = m_arrModelMeshes.size();
		if (numMeshes<=nLOD) 
			return 0;
		return &m_arrModelMeshes[nLOD];
	}
	// returns the geometry of the given lod (0 is the highest detail lod)
	const CModelMesh* GetModelMesh(uint32 nLOD) const
	{
		uint32 numMeshes = m_arrModelMeshes.size();
		if (numMeshes<=nLOD) 
			return 0;
		return &m_arrModelMeshes[nLOD];
	}

	//--------------------------------------------------------------------------------
	//------      Render Mesh                -----------------------------------------
	//--------------------------------------------------------------------------------

	// this is the array that's returned from the RenderMesh
	PodArray<CRenderChunk>* getRenderMeshMaterials()
	{
		if (m_pRenderMeshs[m_nBaseLOD])
			return m_pRenderMeshs[m_nBaseLOD]->GetChunks();
		else
			return NULL;
	}


	// fills the iGameID for each material, using its name to enumerate the physical materials in 3D Engine
	void fillMaterialGameId();
	// offset of the model LCS - by this value, the whole model is offset
	enum {g_nMaxMaterialCount = 128};
	void SetMaterial( IMaterial *pMaterial ) {	m_pMaterial = pMaterial; 	};
	IMaterial* GetMaterial();

	const Vec3& getModelOffset()const;
	CMorphTarget* getMorphSkin (unsigned nLOD, int nMorphTargetId);

	//////////////////////////////////////////////////////////////////////////
	CFacialModel* GetFacialModel() { return m_pFacialModel;	}




	public:

		AABB m_ModelAABB; //AABB of the model in default pose
		_smart_ptr<IRenderMesh> m_pRenderMeshs[g_nMaxGeomLodLevels];

		Vec3 m_vModelOffset;

		string m_strAnimEventFilePath;
		CCharInstance *m_pKinCharInstance; // helper instance: used to test animations on it
		uint32 m_IsProcessed;
		CharacterManager* m_pManager;
		uint32 m_ObjectType;
		CAnimationSet m_AnimationSet;
		int m_nDefaultGameID;
		int m_nBaseLOD;
		std::vector<string> m_arrTracksDBFilePath;
		_smart_ptr<IStatObj> pCGA_Object;
		std::vector<CModelMesh> m_arrModelMeshes;// the set of geometries for each lod level
		std::set<ICharacterInstance*> m_SetInstances;
		_smart_ptr<CModelSkeleton> m_pModelSkeleton;
		DynArray<MeshCollisionInfo> m_arrCollisions;


		char m_bHasPhysics2;

	private:
	
		// Default material for this model.
	_smart_ptr<IMaterial> m_pMaterial;
	CFacialModel* m_pFacialModel;
	int m_nRefCounter;
};


TYPEDEF_AUTOPTR(CCharacterModel);
typedef std::set<CCharacterModel_AutoPtr> CCharacterModel_AutoSet;


#endif // _CryModel_H_
