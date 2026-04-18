////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	10/9/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  default-model joint of the skeleton   
/////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CRY_MODELJOINT
#define _CRY_MODELJOINT

// this class contains the information that's common to all instances of bones
// in the given model: like bone name, and misc. shared properties
class CModelJoint : public CNameCRCHelper
{
public:

	CModelJoint() 
	{
		m_nJointCRC32	=	0;
		m_CGAObject		=	0;
		m_NodeID			= ~0;
		m_numChildren =	0x55aa55aa;

		m_idx					=	-1;  //index-counter of this joint 
		m_idxParent		=	-1;	 //index of  parent-joint	
		m_numVLinks				= 0;    //how many vertices are affected by this joint
		m_numLevels		= 0;

		m_PhysInfo[0].pPhysGeom = 0;
		m_PhysInfo[1].pPhysGeom = 0;
		m_DefaultRelPose.SetIdentity(); 
		m_DefaultAbsPose.SetIdentity(); 
	}

	~CModelJoint ();


	const char * GetJointName() const { return GetName(); };
//	const char * GetJointNameString() const { return GetName(); };

	void SetJointName(const char* szName) 
	{
		SetNameChar(szName); 

		static const char *g_arrLimbNames[4] = { "L UpperArm","R UpperArm","L Thigh","R Thigh" };
		m_nLimbId = -1;
		for (int j=0; j < SIZEOF_ARRAY(g_arrLimbNames) ;j++)
		{
			if (strstr(szName,g_arrLimbNames[j]))
			{
				m_nLimbId = j;
				break;
			}
	}
	}


	uint32 numChildren ()const {return m_numChildren;}
	int getFirstChildIndexOffset() const {return m_nOffsetChildren;}


	bool qhasParent() const 
	{
		return m_idxParent != -1;
	}
	int getParentIndexOffset() const 
	{
		if (m_idxParent<0)
			return 0;
		int32 offset = m_idxParent-m_idx;		
		return offset;
	}

	CModelJoint* getParent () 
	{
		if (m_idxParent<0)
			return 0;

		int32 offset = m_idxParent-m_idx;		
		assert(offset);
		return (this+offset);
	}


	int getLimbId () const {return m_nLimbId;}

	CryBonePhysics& getPhysInfo (int nLod) {return m_PhysInfo[nLod];}

	// updates this bone physics, from the given entity descriptor, and of the given lod
	void UpdatePhysics (const BONE_ENTITY& entity, int nLod);

	void setPhysics (int nLod, const CryBonePhysics& BonePhysics)
	{
		assert (nLod >= 0 && nLod < sizeof(m_PhysInfo)/sizeof(m_PhysInfo[0]));
		m_PhysInfo[nLod] = BonePhysics;
	}

	// the physics for the given LOD is not available
	void resetPhysics (int nLod)
	{
		assert (nLod >= 0 && nLod < sizeof(m_PhysInfo)/sizeof(m_PhysInfo[0]));
		memset (&m_PhysInfo[nLod], 0, sizeof(m_PhysInfo[nLod]));
	}
	const CryBonePhysics& getPhysics (int nLod) const
	{
		assert (nLod >= 0 && nLod < sizeof(m_PhysInfo)/sizeof(m_PhysInfo[0]));
		return m_PhysInfo[nLod];
	}

	// scales the bone with the given multiplier
	void scale (f32 fScale);

	//! Performs post-initialization. This step is requred to initialize the pPhysGeom of the bones
	//! After the bone has been loaded but before it is first used. When the bone is first loaded, pPhysGeom
	//! is set to the value equal to the chunk id in the file where the physical geometry (BoneMesh) chunk is kept.
	//! After those chunks are loaded, and chunk ids are mapped to the registered physical geometry objects,
	//! call this function to replace pPhysGeom chunk ids with the actual physical geometry object pointers.
	//!	NOTE:
	//!	The entries of the map that were used are deleted
	typedef std::map<INT_PTR, struct phys_geometry*> ChunkIdToPhysGeomMap;


	CModelJoint* getChild (unsigned i) {assert(i < numChildren()); return this + m_nOffsetChildren + i;}
	const CModelJoint* getChild (unsigned i) const {assert(i < numChildren()); return this + m_nOffsetChildren + i;}

	// binds this bone to a controller from the specified animation, using the controller manager
	//class IController* BindController (GlobalAnimationHeader& GlobalAnim, unsigned nAnimID);
	// unbinds the bone from the given animation's controller
	//void UnbindController (unsigned nAnimID);

	size_t SizeOfThis()const;

	void PostInitialize();
	Quat &getqRelPhysParent(int nLod) { return m_qDefaultRelPhysParent[nLod]; }

	// updates the given lod level bone physics info from the bones found in the given chunk
	void UpdateHierarchyPhysics (const BONEANIM_CHUNK_DESC* pChunk, unsigned nChunkSize, int nLodLevel);
	//void UpdateHierarchyPhysics( std::vector<BONE_ENTITY> arrBoneEntities );
	void UpdateHierarchyPhysics( DynArray<BONE_ENTITY> arrBoneEntities, int nLod );

	typedef std::map<unsigned, CModelJoint*> UnsignedToCryBoneMap;
	// adds this bone and all its children to the given map controller id-> bone ptr
	void AddHierarchyToControllerIdMap (UnsignedToCryBoneMap& mapControllerIdToCryBone);

	//! Performs post-initialization. This step is requred to initialize the pPhysGeom of the bones
	//! After the bone has been loaded but before it is first used. When the bone is first loaded, pPhysGeom
	//! is set to the value equal to the chunk id in the file where the physical geometry (BoneMesh) chunk is kept.
	//! After those chunks are loaded, and chunk ids are mapped to the registered physical geometry objects,
	//! call this function to replace pPhysGeom chunk ids with the actual physical geometry object pointers.
	//!	NOTE:
	//!	The entries of the map that were used are deleted
//	typedef std::map<int, struct phys_geometry*> ChunkIdToPhysGeomMap;
	bool PostInitPhysGeom ( ChunkIdToPhysGeomMap& mapChunkIdToPhysGeom, int nLodLevel, bool bAllowRopePhys);

	//--------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------

	bool operator == (const CModelJoint& joint) const
	{
		return m_nJointCRC32 == joint.m_nJointCRC32 && m_nOffsetChildren == joint.m_nOffsetChildren && 
			m_numChildren == joint.m_numChildren && m_idx == joint.m_idx && m_DefaultAbsPose.IsEquivalent(joint.m_DefaultAbsPose) &&
			m_DefaultRelPose.IsEquivalent(joint.m_DefaultRelPose);
	}

	bool operator != (const CModelJoint& joint) const
	{
		return !(operator == (joint));
	};

	_smart_ptr<IStatObj> m_CGAObject;			//Static object controlled by this joint.

	int32 m_nOffsetChildren;		// this is 0 if there are no children


	// The whole hierarchy of bones is kept in one big array that belongs to the ModelState
	// Each bone that has children has its own range of bone objects in that array,
	// and this points to the beginning of that range and defines the number of bones.
	uint32	m_NodeID;  //CGA-node
	uint32	m_numChildren;
	
	int16	m_idx;
	int16		m_idxParent;	//index of parent-joint. if the idx==-1 then this joint is the root. Usually this values are > 0
	uint16	m_numVLinks;
	uint16	m_numLevels;


	QuatT m_DefaultAbsPose;		//initial-pose quaternion Bone2World (aka absolute quaternion)
	QuatT m_DefaultAbsPoseInverse;		//initial-pose quaternion Bone2World (aka absolute quaternion)
	QuatT m_DefaultRelPose;		//initial-pose relative quaternion 

	uint32 m_nJointCRC32; //unique ID of bone (generated from bone name in the max)

	//physics info for different lods
	// lod 0 is the physics of alive body, 
	// lod 1 is the physics of a dead body
	CryBonePhysics m_PhysInfo[2]; 
	f32 m_fMass;
	int	m_nLimbId; // set by model state class
	Quat m_qDefaultRelPhysParent[2];

};

#endif
