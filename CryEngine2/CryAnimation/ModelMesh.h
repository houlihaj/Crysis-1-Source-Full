//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:ModelSkeleton.cpp
//  Implementation of Skeleton class
//
//	History:
//	January 12, 2005: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#ifndef _CRY_SKIN_HDR_
#define _CRY_SKIN_HDR_


#include <CryCompiledFile.h>
#include <CGFContent.h>

struct RVertex {	Vec3 pos;	f32 u,v;	};

class CCharacterModel;
class CModelJoint;
class CSkinInstance;
class CCharInstance;
class CAttachment;
class CMorphing;

struct CloseInfo {
	uint32 FaceNr;
	Vec3 tv0;
	Vec3 tv1;
	Vec3 tv2;
	Vec3 middle;
};

struct BoneID 
{
	/*uint16*/uint8 idx0,idx1,idx2,idx3;
};

struct AttSkinVertex
{
	Vec3 wpos0;			//vertex-position of model.1 
	Vec3 wpos1;			//vertex-position of model.2 
	Vec3 wpos2;			//vertex-position of model.3 
	uint8 boneIDs[4];
	f32 weights[4];
	ColorB color;   //index for blend-array
	void FromExtSkinVertex(const ExtSkinVertex& skin);
};


//////////////////////////////////////////////////////////////////////////
class CMorphTarget : public _i_reference_target_t
{
public:
	struct Vertex
	{
		uint32 nVertexId; // vertex index in the original (mesh) array of vertices
		Vec3 m_MTVertex;      // the target point of the morph target
		float fBalanceLeft;   // Vertex Left balance multiplier.
		float fBalanceRight;   // Vertex Right balance multiplier.
	};

	uint32 m_MeshID;
	// Morph target name.
	string m_name;
	// Morph vertices
	std::vector<Vertex> m_vertices; 
};

//////////////////////////////////////////////////////////////////////////
//                    This is the skinning class:                       //
//       it skins vertices & tangents and applys morph-targets          // 
//////////////////////////////////////////////////////////////////////////
class CModelMesh 
{
public:
	CModelMesh() :m_FrameIdVertexArray(-1), m_FrameIdSkinInit(-1), m_iThreadMeshAccessCounter(0) { }
	~CModelMesh() 
	{
		uint32 i=0;
	}

	CloseInfo FindClosestPointOnMesh( const Vec3& RMWPosition ); 

	void InitSkinningExtSW (CMorphing* pMorphing, f32* pShapeDeform,uint32 nResetMode, int nLOD);

	float ComputeExtent(GeomQuery& geo, EGeomForm eForm);
	void GetRandomPos(CCharInstance* pInstance, RandomPos& ran, GeomQuery& geo, EGeomForm eForm);

	void DrawWireframe( CMorphing* pMorphing, QuatTS* parrNewSkinQuat, f32* pShapeDeform,uint32 nResetMode, int nLOD,const Matrix34& rRenderMat34);
	void DrawWireframeStatic( CMorphing* pMorphing, const Matrix34& m34, int nLOD, uint32 color);

	void DrawTangents (		const Matrix34& rRenderMat34 );
	void DrawBinormals (	const Matrix34& rRenderMat34 );
	void DrawNormals (		const Matrix34& rRenderMat34 );

	size_t SizeOfThis(ICrySizer *pSizer) const;
	void DrawDebugInfo(CCharInstance* pInstance, int nLOD,const Matrix34& rRenderMat34, int DebugMode, IMaterial* & pMaterial,  CRenderObject *pObj, const SRendParams& RendParams);

	int FindMorphTarget(const char* szMorphTargetName);
	void ExportModel();

	Vec3 GetSkinnedExtVertex2( QuatTS* parrNewSkinQuat, uint32 i);
	void LockFullRenderMesh(int lod);
	void UnlockFullRenderMesh(int lod);

	inline ExtSkinVertex GetSkinVertex(int pos)
	{
		if (m_pIndices)
			pos = m_pIndices[pos];
	
		return GetSkinVertexNoInd(pos);
	}
	ExtSkinVertex GetSkinVertexNoInd(int pos);

	void SkinningExtSW(CCharInstance* pInstance, CAttachment* pAttachment, int nLOD);
	void SkinningForDecals(CSkinInstance* pInstance, int bone);
	void GetSingleSkinnedVertex(CCharInstance* pInstance, int nV, Vec3* pVert, Vec3* pNorm, bool bLockUnlock = true);
	uint32 GetVertextCount() const;

private:

	Vec3 GetSkinnedExtNormal(CSkinInstance* pInstance,uint32 i);





	//just for debugging
#if (SKINNING_SW==1)

	void VertexShader_SBS(QuatTS* parrNewSkinQuat, uint32 i); 
	void VertexShader_LBS(QuatTS* parrNewSkinQuat, uint32 i); 

#endif //SKINNING_SW

public:
	//////////////////////////////////////////////////////////////////////////
	// Member variables.
	//////////////////////////////////////////////////////////////////////////

	// For render mesh locks
	int32 m_PosStride;
	int32 m_UVStride;
	int32 m_ColorStride;
	int32 m_SkinStride;
	int32 m_ShapeStride;
	int32 m_TangentStride;
	int32 m_BinormalStride;
	int32 m_Indices;

	unsigned short int * m_pIndices;
	volatile int m_iThreadMeshAccessCounter;
	byte * m_pPositions;
	byte * m_pSkinningInfo;
	byte * m_pMorphingInfo;
	byte * m_pUVs;
	byte * m_pColors;
	byte * m_pTangents;
	byte * m_pBinormals;

	uint32 m_nLOD;
	CCharacterModel* m_pModel;
	Vec4 VertexRegs[8];											//the blend-parameter for shape deformation as they appear in the constat registers

	DynArray<PhysicalProxy> m_arrPhyBoneMeshes;

	// the array of optimized morph targets (only LOD-0 skins)
	// in this array are the source-data for all morph vertices.
	std::vector<_smart_ptr<CMorphTarget> > m_morphTargets;

	
	DynArray<TFace> m_arrIntFaces;						//the triples for each face, internal indexation
	DynArray<IntSkinVertex> m_arrIntVertices;		//internal vertices (just position and links)

	int m_FrameIdVertexArray;						// to avoid skinning a mesh multiple times a frame
	int m_FrameIdSkinInit;							// to avoid skinning a mesh multiple times a frame

	uint32 m_numExtVertices;
	uint32 m_numExtTriangles;

	std::vector<BoneID> m_arrExtBoneIDs;	//this is a list with the global BoneIDs
	std::vector<SPipTangents> m_arrTangents;

	std::vector<AttSkinVertex> m_arrExtVerticesCached;
	//just for debugging
#if (SKINNING_SW==1)
//	std::vector<SMeshSubset> m_arrSubsets;      //subdivision of the vertex-buffer into several render-batches

	//QuatTS m_arrVSQuatL[MAX_BONES_IN_BATCH];	//the bone-matrices per batch as they appear in the constant registers 
	//QuatTS m_arrVSQuatS[MAX_BONES_IN_BATCH];			  //the bone-quaternions  per batch as they appear in the constant registers

	//int m_nTransformedVertices;
	//std::vector<RVertex> m_arrSkinnedDecals;

	std::vector<RVertex> m_arrExtSkinnedStream;
	std::vector<Vec3> m_arrExtMorphStream;			//the second stream to add the morph-targets to the ExtSkinVertices


//	std::vector<uint8> m_arrTransformCache;



#endif //SKINNING_SW
};

#endif
