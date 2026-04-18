#include "StdAfx.h"
#include "AutoCubeMapRenderNode.h"
#include "VisAreas.h"

//////////////////////////////////////////////////////////////////////////
// Helper class to cache all visible AutoCubeMapRenderNodes for current frame

class CAutoCubeMapCache
{
public:
	static CAutoCubeMapCache& GetInstance();

public:
	void Add(CAutoCubeMapRenderNode* pEntry);
	CAutoCubeMapRenderNode* GetClosest(const Vec3& p);

private:
	CAutoCubeMapCache();
	~CAutoCubeMapCache();

private:
	uint32 m_curFrameID;
	std::vector<CAutoCubeMapRenderNode*> m_cache;
};


CAutoCubeMapCache& CAutoCubeMapCache::GetInstance()
{
	static CAutoCubeMapCache s_instance;
	return s_instance;
}


CAutoCubeMapCache::CAutoCubeMapCache()
: m_curFrameID(0)
, m_cache()
{
	m_cache.reserve(32);
}


CAutoCubeMapCache::~CAutoCubeMapCache()
{
}


void CAutoCubeMapCache::Add(CAutoCubeMapRenderNode* pEntry)
{
	FUNCTION_PROFILER_FAST(C3DEngine::GetSystem(), PROFILE_3DENGINE, C3DEngine::m_bProfilerEnabled);
	
	// check frame ID and purge cache if rendering of new frame started
	uint32 frameID(gEnv->pRenderer->GetFrameID());
	if (frameID != m_curFrameID)
	{
		m_cache.resize(0);
		m_curFrameID = frameID;
	}
//#ifdef _DEBUG
//	for (size_t i(0); i<m_cache.size(); ++i)
//	{
//		assert(pEntry != m_cache[i]);
//	}
//#endif
	m_cache.push_back(pEntry);
}


CAutoCubeMapRenderNode* CAutoCubeMapCache::GetClosest(const Vec3& p)
{
	FUNCTION_PROFILER_FAST(C3DEngine::GetSystem(), PROFILE_3DENGINE, C3DEngine::m_bProfilerEnabled);

	float minDistSq(FLT_MAX);
	CAutoCubeMapRenderNode* pRes(0);

	for (size_t i(0); i<m_cache.size(); ++i)
	{
		CAutoCubeMapRenderNode* pCurACM(m_cache[i]);
		const SAutoCubeMapProperties& curAcmProp(pCurACM->GetProperties());

		if (Overlap::Point_OBB(p, curAcmProp.m_obb.c, curAcmProp.m_obb))
		{
			float curDistSq((p-curAcmProp.m_refPos).GetLengthSquared());
			if (!pRes || minDistSq > curDistSq)
			{
				pRes = pCurACM;
				minDistSq = curDistSq;
			}
		}
	}
	
	return pRes;
}


//////////////////////////////////////////////////////////////////////////
// auto cube map related implementation of I3DEngine interface

IAutoCubeMapRenderNode* C3DEngine::GetClosestAutoCubeMap(const Vec3& p)
{
	return CAutoCubeMapCache::GetInstance().GetClosest(p);
}


//////////////////////////////////////////////////////////////////////////
// CAutoCubeMapRenderNode implementation

uint32 CAutoCubeMapRenderNode::ms_acmGenID(0);


CAutoCubeMapRenderNode::CAutoCubeMapRenderNode()
: m_id(ms_acmGenID)
, m_properties()
, m_pPrivateData(0)
{
	++ms_acmGenID;
}


CAutoCubeMapRenderNode::~CAutoCubeMapRenderNode()
{
	if (m_pOcNode)
		Get3DEngine()->UnRegisterEntity(this);
  Get3DEngine()->FreeRenderNodeState(this);
}


void CAutoCubeMapRenderNode::UpdateBoundingInfo()
{
	Matrix34 mat(m_properties.m_obb.m33);
	mat.SetColumn(3, m_properties.m_obb.c);

	m_WSBBox.SetTransformedAABB(mat, AABB(-m_properties.m_obb.h, m_properties.m_obb.h));
}


void CAutoCubeMapRenderNode::SetProperties(const SAutoCubeMapProperties& properties)
{
	m_properties = properties;					
	UpdateBoundingInfo();
}


const SAutoCubeMapProperties& CAutoCubeMapRenderNode::GetProperties() const
{
	return m_properties;
}


uint32 CAutoCubeMapRenderNode::GetID() const
{
	return m_id;
}


void CAutoCubeMapRenderNode::SetPrivateData(const void* pData)
{
	m_pPrivateData = pData;
}


const void* CAutoCubeMapRenderNode::GetPrivateData() const
{
	return m_pPrivateData;
}


void CAutoCubeMapRenderNode::SetMatrix(const Matrix34& mat)
{
	Get3DEngine()->UnRegisterEntity(this);	

	m_properties.m_obb.c = mat.GetTranslation();
	m_properties.m_obb.m33 = Matrix33(mat);
	if (!m_properties.m_obb.m33.IsOrthonormal())
		m_properties.m_obb.m33.OrthonormalizeFast();
	
	UpdateBoundingInfo();	
	
	Get3DEngine()->RegisterEntity(this);
}


EERType CAutoCubeMapRenderNode::GetRenderNodeType() 
{ 
	return eERType_AutoCubeMap; 
};


const char* CAutoCubeMapRenderNode::GetEntityClassName() const 
{ 
	return "AutoCubeMap"; 
};


const char* CAutoCubeMapRenderNode::GetName() const 
{ 
	return "AutoCubeMap"; 
};


Vec3 CAutoCubeMapRenderNode::GetPos(bool bWorldOnly) const 
{ 
	return m_properties.m_obb.c; 
};


bool CAutoCubeMapRenderNode::Render(const SRendParams& rParam)
{
	CAutoCubeMapCache::GetInstance().Add(this);
	return true;
}


IPhysicalEntity* CAutoCubeMapRenderNode::GetPhysics() const
{
	return 0;
}


void CAutoCubeMapRenderNode::SetPhysics(IPhysicalEntity*)
{
}


void CAutoCubeMapRenderNode::SetMaterial(IMaterial* pMat)
{
}


IMaterial* CAutoCubeMapRenderNode::GetMaterial(Vec3 * pHitPos)
{
	return 0;
}


float CAutoCubeMapRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_view_dist_min, GetBBox().GetRadius() * GetCVars()->e_view_dist_ratio_detail * GetViewDistRatioNormilized());

	return max(GetCVars()->e_view_dist_min, GetBBox().GetRadius() * GetCVars()->e_view_dist_ratio * GetViewDistRatioNormilized());
}


void CAutoCubeMapRenderNode::Precache()
{
}


void CAutoCubeMapRenderNode::GetMemoryUsage(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "AutoCubeMap");
	size_t size(sizeof(*this));
	pSizer->AddObject(this, size);
}
