#ifndef _3DENGINE_BRUSH_H_
#define _3DENGINE_BRUSH_H_

#include "ObjMan.h"

#if defined(LINUX)
#include "platform.h"
#endif

#include "LMCompStructures.h"
#include "LMData.h"
#include "VSCompStructures.h"

#define MAX_BRUSH_LODS_NUM 3

class CBrush : public IRenderNode, public Cry3DEngineBase
{
	friend class COctreeNode;

public:
	CBrush();
	virtual ~CBrush();

	virtual const char * GetEntityClassName() const;
	virtual Vec3 GetPos(bool bWorldOnly = true) const;
	virtual const char *GetName() const;
	virtual bool HasChanged();
	virtual bool Render(const struct SRendParams & EntDrawParams);
	virtual struct IStatObj * GetEntityStatObj( unsigned int nPartId, unsigned int nSubPartId = 0, Matrix34* pMatrix = NULL, bool bReturnOnlyVisible = false);

	virtual void SetEntityStatObj( unsigned int nSlot, IStatObj * pStatObj, const Matrix34 * pMatrix = NULL );

	virtual void SetLightmap(RenderLMData *pLMData, float *pTexCoords, uint32 iNumTexCoords, int nLod,const int32 SubObjIdx);
	//special call from lightmap serializer/compiler to set occlusion map values
	virtual void SetLightmap(RenderLMData *pLMData, float *pTexCoords, uint32 iNumTexCoords, const unsigned char cucOcclIDCount, /*const std::vector<std::pair<EntityId, EntityId> >& aIDs,*/ const int8 nFirstOcclusionChannel,const int32 SubObjIdx );

	virtual bool HasLightmap(int nLod)
	{
		if (nLod>=MAX_BRUSH_LODS_NUM || m_arrLMData[nLod].m_pLMData == NULL || m_arrLMData[nLod].m_pLMTCBuffer == NULL)
			return nLod==0 && m_SubObjectLightmapData.size()!=0; // return to avoid crash somewhere if in release mode
		return true;
	};

  virtual struct	SLMData* GetLightmapData(int nLod,int SubObject=-1)	{	return SubObject<0?&m_arrLMData[nLod]:&m_SubObjectLightmapData[SubObject];}

	virtual void SetVertexScattering(const SScatteringObjectData& ScatteringObjectData);

	virtual struct IRenderMesh * GetRenderMesh(int nLod);

	virtual IPhysicalEntity* GetPhysics() const ;
	virtual void SetPhysics( IPhysicalEntity* pPhys );
	static bool IsMatrixValid(const Matrix34 & mat);
	void DeleteLMTC();
	virtual void Dephysicalize(bool bKeepIfReferenced=false);
	virtual void Physicalize(bool bInstant=false);

	//! Assign override material to this entity.
	virtual void SetMaterial( IMaterial *pMat );
	virtual IMaterial* GetMaterial(Vec3 * pHitPos = NULL);
	virtual int GetEditorObjectId() const { return m_nEditorObjectId; }
	virtual void SetEditorObjectId(int nEditorObjectId) { m_nEditorObjectId = nEditorObjectId; }
	virtual void CheckPhysicalized();

	virtual float GetMaxViewDist();

  virtual EERType GetRenderNodeType() { return eERType_Brush; }

	void SetStatObj(CStatObj * pStatObj) { m_pStatObj = pStatObj; }

	void SetMatrix( const Matrix34* pMatrix );
	using IRenderNode::SetMatrix;
	void Dematerialize( );
	virtual void GetMemoryUsage(ICrySizer * pSizer);
	static PodArray<SExportedBrushMaterial> m_lstSExportedBrushMaterials;
	virtual void PreloadInstanceResources(Vec3 vPrevPortalPos, float fPrevPortalDistance, float fTime);

	virtual void SetWindBending( Vec2 &pWindBending ) { m_vWindBending = pWindBending; }
	virtual Vec2 GetWindBending() { return m_vWindBending; }
  virtual const AABB GetBBox() const { return m_WSBBox; }
  virtual void SetBBox( const AABB& WSBBox ) { m_WSBBox = WSBBox; }

	ILINE CStatObj* GetStatObj()
	{
		return m_pStatObj;
	}

	//private:
	void DeleteVertexScattering();

	void CalcBBox();
	void UpdatePhysicalMaterials(int bThreadSafe=0);

  void OnRenderNodeBecomeVisible();

  void DoVoxelShape();

	Vec3 m_vPos; 

  Matrix34 m_Matrix;
	IPhysicalEntity * m_pPhysEnt;

	//! Override material.
	_smart_ptr<IMaterial> m_pMaterial;

	SLMData m_arrLMData[MAX_BRUSH_LODS_NUM];
	struct IRenderMesh* m_arrScatterVertexBuffer[MAX_VERTEX_SCATTER_LODS_NUM];
	bool m_bHasVertexScatterBuffer;

	PodArray<SLMData>	m_SubObjectLightmapData;	//needed for RAM-Maps on Subobject of Brushes	
	PodArray<struct IRenderMesh*> m_SubObjectScatterVertexBuffer;
	Vec4 m_VertexScatterTransformZ;

  int m_nEditorObjectId;
  _smart_ptr<CStatObj> m_pStatObj;

	float GetLodForDistance(float fDistance);

  // Wind info 
  Vec2 m_vWindBending;

	bool m_bVehicleOnlyPhysics;

  AABB m_WSBBox;
};

#endif // _3DENGINE_BRUSH_H_
