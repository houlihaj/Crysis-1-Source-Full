#include "StdAfx.h"
#include "VolumeObjectRenderNode.h"
#include "VolumeObjectDataCreate.h"

#include <map>


//////////////////////////////////////////////////////////////////////////
// volume data cache

class CVolumeDataCache
{
public:
	static CVolumeDataCache& Access();

public:
	~CVolumeDataCache();

	void AddItem(const string& name, CVolumeDataItem* pItem);
	void RemoveItem(const string& name);
	CVolumeDataItem* GetItem(const string& name) const;
	size_t size() const;

private:
	typedef std::map<string, CVolumeDataItem*> VolumeDataItemMap;

private:
	CVolumeDataCache();

private:
	VolumeDataItemMap m_cache;
};


CVolumeDataCache& CVolumeDataCache::Access()
{
	static CVolumeDataCache s_inst;
	return s_inst;
}


CVolumeDataCache::CVolumeDataCache()
: m_cache()
{
}


CVolumeDataCache::~CVolumeDataCache()
{
	assert(m_cache.empty());
}


void CVolumeDataCache::AddItem(const string& name, CVolumeDataItem* pItem)
{
	VolumeDataItemMap::iterator it(m_cache.find(name));
	assert(it == m_cache.end());
	if (it == m_cache.end())
		m_cache.insert(VolumeDataItemMap::value_type(name, pItem));
}


void CVolumeDataCache::RemoveItem(const string& name)
{
	VolumeDataItemMap::iterator it(m_cache.find(name));
	assert(it != m_cache.end());
	if (it != m_cache.end())
		m_cache.erase(it);
}


CVolumeDataItem* CVolumeDataCache::GetItem(const string& name) const
{
	VolumeDataItemMap::const_iterator it(m_cache.find(name));
	return it != m_cache.end() ? (*it).second : 0;
}


size_t CVolumeDataCache::size() const
{
	return m_cache.size();
}


//////////////////////////////////////////////////////////////////////////
// volume data item

class CVolumeDataItem
{
public:
	static CVolumeDataItem* Create(const char* filePath, const CREVolumeObject* pVolTexFactory);

public:
	void AddRef();
	void Release();

	bool IsValid() const;
	void AddToCache();

	const SVolumeDataSrcB* GetData() const;
	CREVolumeObject::IVolumeTexture* GetVolumeTexture() const;

	const AABB& GetTightBounds() const;
	float GetScale() const;

private:
	CVolumeDataItem(const char* filePath, const CREVolumeObject* pVolTexFactory);
	~CVolumeDataItem();

private:
	int m_refCount;
	bool m_isValid;
	bool m_isCached;
	AABB m_tightBounds;
	float m_scale;
	string m_volDataFilePath;
	SVolumeDataSrcB* m_pData;
	CREVolumeObject::IVolumeTexture* m_pVolTex;
};


CVolumeDataItem* CVolumeDataItem::Create(const char* filePath, const CREVolumeObject* pVolTexFactory)
{
	return new CVolumeDataItem(filePath, pVolTexFactory);
}


CVolumeDataItem::CVolumeDataItem(const char* filePath, const CREVolumeObject* pVolTexFactory)
: m_refCount(1)
, m_isValid(false)
, m_isCached(false)
, m_tightBounds(Vec3(-1, -1, -1), Vec3(1, 1, 1))
, m_scale(1)
, m_volDataFilePath(filePath)
, m_pData(0)
, m_pVolTex(0)
{
	if (filePath && pVolTexFactory)
	{
		SVolumeDataSrcB* pData = new SVolumeDataSrcB(VOLUME_SIZE, VOLUME_SIZE, VOLUME_SIZE);
		if (pData && pData->m_pData)
		{
			if (CreateVolumeObject(m_volDataFilePath, *pData, m_tightBounds, m_scale))
			{
				m_pVolTex = pVolTexFactory->CreateVolumeTexture();
				if (m_pVolTex)
					m_isValid = m_pVolTex->Create(pData->m_width, pData->m_height, pData->m_depth, pData->m_pData);
			}

			m_pData = new SVolumeDataSrcB(VOLUME_SHADOW_SIZE, VOLUME_SHADOW_SIZE, VOLUME_SHADOW_SIZE);
			if (m_pData && m_pData->m_pData)
				m_isValid &= CreateDownscaledVolumeObject(*pData, *m_pData);
			else
				m_isValid = false;
		}
		SAFE_DELETE(pData);
	}
}


CVolumeDataItem::~CVolumeDataItem()
{
	if (m_isCached)
	{
		assert(CVolumeDataCache::Access().GetItem(m_volDataFilePath) == this);
		CVolumeDataCache::Access().RemoveItem(m_volDataFilePath);
	}

	SAFE_DELETE(m_pData);
	SAFE_RELEASE(m_pVolTex);
}


void CVolumeDataItem::AddRef()
{
	++m_refCount;
}


void CVolumeDataItem::Release()
{
	--m_refCount;
	if (m_refCount <= 0)
		delete this;
}


bool CVolumeDataItem::IsValid() const
{
	return m_isValid;
}


void CVolumeDataItem::AddToCache()
{
	if (m_isValid && !m_isCached)
	{
		CVolumeDataCache::Access().AddItem(m_volDataFilePath, this);
		m_isCached = true;
	}
}


const SVolumeDataSrcB* CVolumeDataItem::GetData() const
{
	return m_pData;
}


CREVolumeObject::IVolumeTexture* CVolumeDataItem::GetVolumeTexture() const
{
	return m_pVolTex;
}


const AABB& CVolumeDataItem::GetTightBounds() const
{
	return m_tightBounds;
}


float CVolumeDataItem::GetScale() const
{
	return m_scale;
}

//////////////////////////////////////////////////////////////////////////
// volume shadow creator

class CVolumeShadowCreator
{
public:
	static CVolumeShadowCreator* Create();

public:
	int AddRef();
	int Release();

	void CalculateShadows(const Vec3& newLightDir, float shadowStrength, const CVolumeDataItem* pVolSrc, CREVolumeObject::IVolumeTexture* pShadDst);

private:
	CVolumeShadowCreator();
	~CVolumeShadowCreator();

private:
	int m_refCount;
	SVolumeDataSrcB* m_pShad;
};


CVolumeShadowCreator* CVolumeShadowCreator::Create()
{
	return new CVolumeShadowCreator;
}


CVolumeShadowCreator::CVolumeShadowCreator()
: m_refCount(1)
, m_pShad(0)
{
}


CVolumeShadowCreator::~CVolumeShadowCreator()
{
}


int CVolumeShadowCreator::AddRef()
{
	++m_refCount;
	return m_refCount;
}


int CVolumeShadowCreator::Release()
{
	--m_refCount;
	int ref(m_refCount);
	if (m_refCount <= 0)
		delete this;
	return ref;
}


void CVolumeShadowCreator::CalculateShadows(const Vec3& newLightDir, float shadowStrength, const CVolumeDataItem* pVolSrc, CREVolumeObject::IVolumeTexture* pShadDst)
{
	const SVolumeDataSrcB* pSrc(pVolSrc->GetData());
	if (!pSrc)
		return;

	if (!m_pShad || m_pShad->m_width != pSrc->m_width || m_pShad->m_height != pSrc->m_height || m_pShad->m_depth != pSrc->m_depth)
	{
		SAFE_DELETE(m_pShad);
		m_pShad = new SVolumeDataSrcB(pSrc->m_width, pSrc->m_height, pSrc->m_depth);
	}

	if (m_pShad)
	{
		//float t = gEnv->pTimer->GetAsyncCurTime();
		CreateVolumeShadow(newLightDir, shadowStrength, *pSrc, *m_pShad);
		//t = gEnv->pTimer->GetAsyncCurTime() - t;
		//gEnv->pLog->Log("CreateVolumeShadow(%.3f, %.3f, %.3f) took %.2fms", newLightDir.x, newLightDir.y, newLightDir.z, t * 1000.0f);
		pShadDst->Update(m_pShad->m_width, m_pShad->m_height, m_pShad->m_depth, m_pShad->m_pData);
	}
}


//////////////////////////////////////////////////////////////////////////
// CVolumeObjectRenderNode implementation

CVolumeShadowCreator* CVolumeObjectRenderNode::ms_pVolShadowCreator(0);
CVolumeObjectRenderNode::VolumeObjectSet CVolumeObjectRenderNode::ms_volumeObjects;

ICVar* CVolumeObjectRenderNode::ms_CV_volobj_stats(0);
int CVolumeObjectRenderNode::ms_volobj_stats(0);


void CVolumeObjectRenderNode::MoveVolumeObjects()
{
	for (VolumeObjectSet::iterator it(ms_volumeObjects.begin()), itEnd(ms_volumeObjects.end()); it != itEnd; ++it)
		(*it)->Move();

	//////////////////////////////////////////////////////////////////////////

	if (ms_volobj_stats)
	{
		CryLogAlways("#VolumeObjects = %d", ms_volumeObjects.size());
		CryLogAlways("#VolumeDataItems = %d", CVolumeDataCache::Access().size());
		ms_volobj_stats = 0;
	}
}


void CVolumeObjectRenderNode::RegisterVolumeObject(CVolumeObjectRenderNode* p)
{
	VolumeObjectSet::iterator it(ms_volumeObjects.find(p));
	assert(it == ms_volumeObjects.end() && "CVolumeObjectRenderNode::RegisterVolumeObject() -- Object already registered!");
	if (it == ms_volumeObjects.end())
		ms_volumeObjects.insert(p);
}


void CVolumeObjectRenderNode::UnregisterVolumeObject(CVolumeObjectRenderNode* p)
{
	VolumeObjectSet::iterator it(ms_volumeObjects.find(p));
	assert(it != ms_volumeObjects.end() && "CVolumeObjectRenderNode::UnregisterVolumeObject() -- Object not registered or previously removed!");
	if (it != ms_volumeObjects.end())
		ms_volumeObjects.erase(it);
}


CVolumeObjectRenderNode::CVolumeObjectRenderNode()
:	m_WSBBox()
, m_pos(0, 0, 0)
, m_origin(0, 0, 0)
, m_matOrig()
, m_mat()
, m_matInv()
, m_lastCachedLightDir(0, 0, 0)
, m_tightBoundsOS()
, m_moveProps()
, m_alpha(1)
, m_scale(1)
, m_shadowStrength(0.4f)
, m_pMaterial(0)
, m_pRE(0)
, m_pRO(0)
, m_pVolDataItem(0)
, m_pVolShadTex(0)
{
	m_matOrig.SetIdentity();
	m_mat.SetIdentity();
	m_matInv.SetIdentity();

	m_WSBBox = AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1));
	m_tightBoundsOS = AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1));

	m_moveProps.m_autoMove = false;
	m_moveProps.m_speed = Vec3(0, 0, 0);
	m_moveProps.m_spaceLoopBox = Vec3(2000.0f, 2000.0f, 2000.0f);
	m_moveProps.m_fadeDistance = 0;

	m_pRE = (CREVolumeObject*) GetRenderer()->EF_CreateRE(eDATA_VolumeObject);
	m_pRO = GetRenderer()->EF_GetObject(false);

	m_pMaterial = GetMatMan()->LoadMaterial("Materials/VolumeData/Default", false);

	if (!ms_pVolShadowCreator)
		ms_pVolShadowCreator = CVolumeShadowCreator::Create();
	else
		ms_pVolShadowCreator->AddRef();

	RegisterVolumeObject(this);

	if (!ms_CV_volobj_stats)
		ms_CV_volobj_stats = gEnv->pConsole->Register("e_volobj_stats", &ms_volobj_stats, 0);
}


CVolumeObjectRenderNode::~CVolumeObjectRenderNode()
{
	if (ms_pVolShadowCreator->Release() <= 0)
		ms_pVolShadowCreator = 0;

	SAFE_RELEASE(m_pVolShadTex);
	SAFE_RELEASE(m_pVolDataItem);

	if (m_pRE)
		m_pRE->Release();
	if (m_pRO)
		m_pRO->RemovePermanent();

	Get3DEngine()->UnRegisterEntity(this);
	Get3DEngine()->FreeRenderNodeState(this);

	UnregisterVolumeObject(this);
}


void CVolumeObjectRenderNode::LoadVolumeData(const char* filePath)
{
	CVolumeDataItem* pNewVolDataItem(CVolumeDataCache::Access().GetItem(filePath));
	if (pNewVolDataItem)
	{
		pNewVolDataItem->AddRef();
		if (m_pVolDataItem)
			m_pVolDataItem->Release();
		m_pVolDataItem = pNewVolDataItem;
		InvalidateLastCachedLightDir();
	}
	else
	{
		pNewVolDataItem = CVolumeDataItem::Create(filePath, m_pRE);
		if (pNewVolDataItem && pNewVolDataItem->IsValid())
		{
			pNewVolDataItem->AddToCache();
			if (m_pVolDataItem)
				m_pVolDataItem->Release();
			m_pVolDataItem = pNewVolDataItem;
			InvalidateLastCachedLightDir();
		}
		else
			SAFE_RELEASE(pNewVolDataItem);
	}

	SetMatrix(m_matOrig);

	if (m_pVolDataItem && m_pRE && !m_pVolShadTex)
	{
		m_pVolShadTex = m_pRE->CreateVolumeTexture();
		if (!m_pVolShadTex->Create(VOLUME_SHADOW_SIZE, VOLUME_SHADOW_SIZE, VOLUME_SHADOW_SIZE, 0))
		{
			SAFE_RELEASE(m_pVolShadTex);
		}
	}
}


void CVolumeObjectRenderNode::SetProperties(const SVolumeObjectProperties& properties)
{
}


void CVolumeObjectRenderNode::SetMovementProperties(const SVolumeObjectMovementProperties& properties)
{
	m_moveProps = properties;
}


void CVolumeObjectRenderNode::SetMatrix(const Matrix34& mat)
{
	m_matOrig = mat;
	float initialScale(m_pVolDataItem ? m_pVolDataItem->GetScale() : 1);
	SetMatrixInternal(mat * Matrix33::CreateScale(Vec3(initialScale)), true);
}


void CVolumeObjectRenderNode::SetMatrixInternal(const Matrix34& mat, bool updateOrigin)
{
	Get3DEngine()->UnRegisterEntity(this);

	if (updateOrigin)
		m_origin = mat.GetTranslation();

	m_pos = mat.GetTranslation();
	m_mat = mat;
	m_matInv = mat.GetInverted();

	m_tightBoundsOS = m_pVolDataItem ? m_pVolDataItem->GetTightBounds() : AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1));
	m_WSBBox.SetTransformedAABB(m_mat, m_tightBoundsOS);

	Vec3 scale(m_mat.GetColumn(0).GetLength(), m_mat.GetColumn(1).GetLength(), m_mat.GetColumn(2).GetLength());
	assert(fabsf(scale.x - scale.y) < 1e-4f && fabsf(scale.x - scale.z) < 1e-4f && fabsf(scale.y - scale.z) < 1e-4f);
	m_scale = max(max(scale.x, scale.y), scale.z);

	Get3DEngine()->RegisterEntity(this);
}


EERType CVolumeObjectRenderNode::GetRenderNodeType()
{
	return eERType_VolumeObject;
}


const char* CVolumeObjectRenderNode::GetEntityClassName() const
{
	return "VolumeObject";
}


const char* CVolumeObjectRenderNode::GetName() const
{
	return "VolumeObject";
}


Vec3 CVolumeObjectRenderNode::GetPos(bool bWorldOnly) const
{
	return m_pos;
}


bool CVolumeObjectRenderNode::IsViewerInsideVolume() const
{
	const CCamera& cam(GetCamera());
	Vec3 camPosOS(m_matInv.TransformPoint(cam.GetPosition()));
	const Vec3& scale(m_tightBoundsOS.max);
	return fabsf(camPosOS.x) < scale.x && fabsf(camPosOS.y) < scale.y && fabsf(camPosOS.z) < scale.z;
}


bool CVolumeObjectRenderNode::NearPlaneIntersectsVolume() const
{
	const CCamera& cam(GetCamera());

	// check if bounding box intersects the near clipping plane
	const Plane* pNearPlane(cam.GetFrustumPlane(FR_PLANE_NEAR));
	Vec3 pntOnNearPlane(cam.GetPosition() - pNearPlane->DistFromPlane(cam.GetPosition()) * pNearPlane->n);
	Vec3 pntOnNearPlaneOS(m_matInv.TransformPoint(pntOnNearPlane));

	Vec3 nearPlaneOS_n(m_matInv.TransformVector(pNearPlane->n)/*.GetNormalized()*/);
	f32 nearPlaneOS_d(-nearPlaneOS_n.Dot(pntOnNearPlaneOS));

	// get extreme lengths
	float t(fabsf(nearPlaneOS_n.x * m_tightBoundsOS.max.x) + fabsf(nearPlaneOS_n.y * m_tightBoundsOS.max.y) + fabsf(nearPlaneOS_n.z * m_tightBoundsOS.max.z));
	//float t(0.0f);
	//if(nearPlaneOS_n.x >= 0) t += -nearPlaneOS_n.x; else t += nearPlaneOS_n.x;
	//if(nearPlaneOS_n.y >= 0) t += -nearPlaneOS_n.y; else t += nearPlaneOS_n.y;
	//if(nearPlaneOS_n.z >= 0) t += -nearPlaneOS_n.z; else t += nearPlaneOS_n.z;

	float t0(t + nearPlaneOS_d);
	float t1(-t + nearPlaneOS_d);

	return t0 * t1 < 0.0f;
}


void CVolumeObjectRenderNode::InvalidateLastCachedLightDir()
{
	m_lastCachedLightDir = Vec3(0, 0, 0);
}


void CVolumeObjectRenderNode::UpdateShadows()
{
	Vec3 newLightDir(m_p3DEngine->GetSunDirNormalized());

	float shadowStrength(GetCVars()->e_volobj_shadow_strength);
	if (ms_pVolShadowCreator)
	{
		Vec3 newLightDirLS(m_matInv.TransformVector(newLightDir).GetNormalized()); // 0.999f = 1.56 deg = ??? minutes of TOD update, TODO: make adjustable
		if (m_shadowStrength != shadowStrength || newLightDirLS.Dot(m_lastCachedLightDir) < 0.999f)
		{
			ms_pVolShadowCreator->CalculateShadows(-newLightDirLS, shadowStrength, m_pVolDataItem, m_pVolShadTex);
			m_lastCachedLightDir = newLightDirLS;
			m_shadowStrength = shadowStrength;
		}
	}
}


Plane CVolumeObjectRenderNode::GetVolumeTraceStartPlane(bool viewerInsideVolume) const
{
	const CCamera& cam(GetCamera());	
	Vec3 vdir(cam.GetViewdir());
	Vec3 vpos(cam.GetPosition());

	if (!viewerInsideVolume)
	{
		const Vec3 bbp[8] = 
		{
			Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.min.y, m_tightBoundsOS.min.z), 
			Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.max.y, m_tightBoundsOS.min.z), 
			Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.max.y, m_tightBoundsOS.min.z), 
			Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.min.y, m_tightBoundsOS.min.z), 
			Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.min.y, m_tightBoundsOS.max.z), 
			Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.max.y, m_tightBoundsOS.max.z), 
			Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.max.y, m_tightBoundsOS.max.z), 
			Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.min.y, m_tightBoundsOS.max.z)
		};

		Plane viewPlane(vdir, -vdir.Dot(vpos));

		Vec3 p(m_mat * bbp[0]);
		float d(viewPlane.DistFromPlane(p));

		for (int i(1); i<8; ++i)
		{
			Vec3 ptmp(m_mat * bbp[i]);
			float dtmp(viewPlane.DistFromPlane(ptmp));

			if (dtmp < d)
			{
				p = ptmp;
				d = dtmp;
			}
		}

		return Plane(vdir, -vdir.Dot(p));
	}
	else
		return Plane(vdir, -vdir.Dot(vpos));
}


float CVolumeObjectRenderNode::GetDistanceToCamera() const
{
	const Vec3 bbp[8] = 
	{
		m_mat * Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.min.y, m_tightBoundsOS.min.z), 
		m_mat * Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.max.y, m_tightBoundsOS.min.z), 
		m_mat * Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.max.y, m_tightBoundsOS.min.z), 
		m_mat * Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.min.y, m_tightBoundsOS.min.z), 
		m_mat * Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.min.y, m_tightBoundsOS.max.z), 
		m_mat * Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.max.y, m_tightBoundsOS.max.z), 
		m_mat * Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.max.y, m_tightBoundsOS.max.z), 
		m_mat * Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.min.y, m_tightBoundsOS.max.z)
	};

	const CCamera& cam(Get3DEngine()->GetCamera());	
	const Plane* pNearPlane(cam.GetFrustumPlane(FR_PLANE_NEAR));
	const Vec3 camPos(cam.GetPosition());

	f32 distSq(0.0f);
	for (int i(0); i<8; ++i)
	{
		float tmpDistSq((bbp[i] - camPos).GetLengthSquared());
		if (tmpDistSq > distSq && pNearPlane->DistFromPlane(bbp[i]) < 0.0f)
			distSq = tmpDistSq;
	}

	return cry_sqrtf(distSq);
}


bool CVolumeObjectRenderNode::Render(const SRendParams& rParam)
{
	// anything to render?
	if (Cry3DEngineBase::m_nRenderStackLevel || !m_pRO || !m_pRE || !m_pMaterial || !m_pVolDataItem || !m_pVolShadTex || !GetCVars()->e_clouds)
		return false;

	// update shadow volume
	UpdateShadows();

	// set basic render object properties
	m_pRO->m_II.m_Matrix = m_mat;
	m_pRO->m_ObjFlags |= FOB_TRANS_MASK;
	m_pRO->m_fSort = 0;
	m_pRO->m_fDistance = GetDistanceToCamera();

	// transform camera into object space
	const CCamera& cam(GetCamera());
	Vec3 viewerPosWS(cam.GetPosition());
	Vec3 viewerPosOS(m_matInv * viewerPosWS);

	// set render element attributes
	bool viewerInsideVolume(IsViewerInsideVolume());
	bool nearPlaneIntersectsVolume(NearPlaneIntersectsVolume());
	m_pRE->m_center = m_pos;
	m_pRE->m_matInv = m_matInv;
	m_pRE->m_eyePosInWS = viewerPosWS;
	m_pRE->m_eyePosInOS = viewerPosOS;
	m_pRE->m_volumeTraceStartPlane = GetVolumeTraceStartPlane(viewerInsideVolume);
	m_pRE->m_renderBoundsOS = m_tightBoundsOS;
	m_pRE->m_viewerInsideVolume = viewerInsideVolume;
	m_pRE->m_nearPlaneIntersectsVolume = nearPlaneIntersectsVolume;
	m_pRE->m_alpha = m_alpha;
	m_pRE->m_scale = m_scale;
	m_pRE->m_pDensVol = m_pVolDataItem->GetVolumeTexture();
	m_pRE->m_pShadVol = m_pVolShadTex;

	float mvd(GetMaxViewDist());
	float d((GetCamera().GetPosition() - m_mat.GetTranslation()).GetLength());
	if (d > 0.9f * mvd)
	{
		float s(clamp_tpl(1.0f - (d - 0.9f * mvd) / (0.1f * mvd), 0.0f, 1.0f));
		m_pRE->m_alpha *= s;
	}	

	// add to renderer
	CRenderObject* pObj(GetRenderer()->EF_GetObject(false, m_pRO->m_Id));
	if (pObj)
	{
		SShaderItem shaderItem(m_pMaterial->GetShaderItem(0));
		int afterWater(GetObjManager()->IsAfterWater(m_pos, GetCamera().GetPosition(), Get3DEngine()->GetWaterLevel()) ? 1 : 0);
		GetRenderer()->EF_AddEf(m_pRE, shaderItem, m_pRO, EFSLIST_TRANSP, afterWater);
	}

#if 0
	IRenderAuxGeom* p = GetRenderer()->GetIRenderAuxGeom();
	if (p)
	{
		const Matrix34& mat = m_mat;
		Vec3 bpp[8] = 
		{
			mat * Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.min.y, m_tightBoundsOS.min.z), 
			mat * Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.min.y, m_tightBoundsOS.min.z), 
			mat * Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.max.y, m_tightBoundsOS.min.z), 
			mat * Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.max.y, m_tightBoundsOS.min.z), 

			mat * Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.min.y, m_tightBoundsOS.max.z), 
			mat * Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.min.y, m_tightBoundsOS.max.z),
			mat * Vec3(m_tightBoundsOS.max.x, m_tightBoundsOS.max.y, m_tightBoundsOS.max.z), 
			mat * Vec3(m_tightBoundsOS.min.x, m_tightBoundsOS.max.y, m_tightBoundsOS.max.z) 
		};

		p->SetRenderFlags(e_Def3DPublicRenderflags);

		p->DrawLine(bpp[0], ColorB(255,0,0,255), bpp[1], ColorB(255,0,0,255));
		p->DrawLine(bpp[1], ColorB(255,0,0,255), bpp[2], ColorB(255,0,0,255));
		p->DrawLine(bpp[2], ColorB(255,0,0,255), bpp[3], ColorB(255,0,0,255));
		p->DrawLine(bpp[3], ColorB(255,0,0,255), bpp[0], ColorB(255,0,0,255));

		p->DrawLine(bpp[4], ColorB(255,0,0,255), bpp[5], ColorB(255,0,0,255));
		p->DrawLine(bpp[5], ColorB(255,0,0,255), bpp[6], ColorB(255,0,0,255));
		p->DrawLine(bpp[6], ColorB(255,0,0,255), bpp[7], ColorB(255,0,0,255));
		p->DrawLine(bpp[7], ColorB(255,0,0,255), bpp[4], ColorB(255,0,0,255));

		p->DrawLine(bpp[0], ColorB(255,0,0,255), bpp[4], ColorB(255,0,0,255));
		p->DrawLine(bpp[1], ColorB(255,0,0,255), bpp[5], ColorB(255,0,0,255));
		p->DrawLine(bpp[2], ColorB(255,0,0,255), bpp[6], ColorB(255,0,0,255));
		p->DrawLine(bpp[3], ColorB(255,0,0,255), bpp[7], ColorB(255,0,0,255));
	}
#endif
	return pObj != 0;
}


IPhysicalEntity* CVolumeObjectRenderNode::GetPhysics() const	
{
	return 0;
}


void CVolumeObjectRenderNode::SetPhysics(IPhysicalEntity*)
{
}


void CVolumeObjectRenderNode::SetMaterial(IMaterial* pMat)
{
	m_pMaterial = pMat;
}


IMaterial* CVolumeObjectRenderNode::GetMaterial(Vec3* pHitPos)
{
	return m_pMaterial;
}


float CVolumeObjectRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_view_dist_min, GetBBox().GetRadius() * GetCVars()->e_view_dist_ratio_detail * GetViewDistRatioNormilized());

	return max(GetCVars()->e_view_dist_min, GetBBox().GetRadius() * GetCVars()->e_view_dist_ratio * GetViewDistRatioNormilized());
}


void CVolumeObjectRenderNode::Precache()
{
}


void CVolumeObjectRenderNode::GetMemoryUsage(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "VolumeTracerNode");
	pSizer->AddObject(this, sizeof(*this));
}


void CVolumeObjectRenderNode::Move()
{
	FUNCTION_PROFILER_3DENGINE;

	m_alpha = 1;

	Vec3 pos(m_mat.GetTranslation());

	ITimer* pTimer(gEnv->pTimer);
	if (m_moveProps.m_autoMove)
	{
		// update position
		float deltaTime = pTimer->GetFrameTime();		

		assert(deltaTime>=0);

		pos += deltaTime * m_moveProps.m_speed;

		// constrain movement to specified loop box
		Vec3 loopBoxMin(m_origin - m_moveProps.m_spaceLoopBox);
		Vec3 loopBoxMax(m_origin + m_moveProps.m_spaceLoopBox);

		if (pos.x < loopBoxMin.x)
			pos.x = loopBoxMax.x;
		if (pos.y < loopBoxMin.y)
			pos.y = loopBoxMax.y;
		if (pos.z < loopBoxMin.z)
			pos.z = loopBoxMax.z;

		if (pos.x > loopBoxMax.x)
			pos.x = loopBoxMin.x;
		if (pos.y > loopBoxMax.y)
			pos.y = loopBoxMin.y;
		if (pos.z > loopBoxMax.z)
			pos.z = loopBoxMin.z;

		// set new position
		Matrix34 mat(m_mat);
		mat.SetTranslation(pos);
		SetMatrixInternal(mat, false);

		// fade out clouds at the borders of the loop box
		if (m_moveProps.m_fadeDistance > 0)
		{
			Vec3 fade(max(m_moveProps.m_spaceLoopBox.x, m_moveProps.m_fadeDistance), 
				max(m_moveProps.m_spaceLoopBox.y, m_moveProps.m_fadeDistance), 
				max(m_moveProps.m_spaceLoopBox.z, m_moveProps.m_fadeDistance));

			fade -= Vec3(fabs(pos.x - m_origin.x), fabs(pos.y - m_origin.y), fabs(pos.z - m_origin.z));

			m_alpha = clamp_tpl(min(min(fade.x, fade.y), fade.z) / m_moveProps.m_fadeDistance, 0.0f, 1.0f);
		}
	}
	else
	{
		if((m_origin - pos).GetLengthSquared() > 1e-4f)
		{
			Matrix34 mat(m_mat);
			mat.SetTranslation(m_origin);
			SetMatrixInternal(mat, false);
		}
	}
}
