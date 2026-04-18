#include "StdAfx.h"
#include "DistanceCloudRenderNode.h"
#include "VisAreas.h"
#include "ObjMan.h"
#include "terrain.h"


CDistanceCloudRenderNode::CDistanceCloudRenderNode()
:	m_pos(0, 0, 0)
, m_sizeX(1)
, m_sizeY(1)
, m_rotationZ(0)
, m_pMaterial(NULL)
{	
}


CDistanceCloudRenderNode::~CDistanceCloudRenderNode()
{
	Get3DEngine()->UnRegisterEntity(this);
  Get3DEngine()->FreeRenderNodeState(this);
}


SDistanceCloudProperties CDistanceCloudRenderNode::GetProperties() const
{
	SDistanceCloudProperties properties;

	properties.m_sizeX = m_sizeX;
	properties.m_sizeY = m_sizeY;
	properties.m_rotationZ = m_rotationZ;
	properties.m_pos = m_pos;
	properties.m_pMaterialName = 0; // query materialID instead!

	return properties;
}


void CDistanceCloudRenderNode::SetProperties( const SDistanceCloudProperties& properties )
{
	// register material
	m_pMaterial = GetMatMan()->LoadMaterial(properties.m_pMaterialName, false);

	// copy distance cloud properties 
	m_sizeX = properties.m_sizeX;
	m_sizeY = properties.m_sizeY;
	m_rotationZ = properties.m_rotationZ;
	m_pos = properties.m_pos;
}


void CDistanceCloudRenderNode::SetMatrix( const Matrix34& mat )
{
	Get3DEngine()->UnRegisterEntity(this);

	m_pos = mat.GetTranslation();

	m_WSBBox.SetTransformedAABB(mat, AABB(-Vec3(1, 1, 1e-4f), Vec3(1, 1, 1e-4f)));

	Get3DEngine()->RegisterEntity(this);
}


EERType CDistanceCloudRenderNode::GetRenderNodeType()
{
	return eERType_DistanceCloud; 
}


const char* CDistanceCloudRenderNode::GetEntityClassName() const
{
	return "DistanceCloud";
}


const char* CDistanceCloudRenderNode::GetName() const
{
	return "DistanceCloud";
}


Vec3 CDistanceCloudRenderNode::GetPos( bool bWorldOnly ) const
{
	return m_pos;
}


bool CDistanceCloudRenderNode::Render( const SRendParams& rParam )
{
	IMaterial* pMaterial(GetMaterial());

	if (!GetCVars()->e_clouds || !pMaterial)
		return false;

	CRenderObject* pOb(gEnv->pRenderer->EF_GetObject(true));		
  if (!pOb)
    return false;

	// prepare render object
	pOb->m_DynLMMask = 0;
	pOb->m_nTextureID = -1;
	pOb->m_nTextureID1 = -1;
	pOb->m_II.m_AmbColor = ColorF(1, 1, 1, 1);
	pOb->m_RState = 0;
	pOb->m_AlphaRef = 0;
	pOb->m_fSort = 0; 
	pOb->m_ObjFlags = 0 /*FOB_DECAL*/;

	// fill general vertex data
	f32 sinZ(0), cosZ(1);
	cry_sincosf(DEG2RAD(m_rotationZ), &sinZ, &cosZ);
	Vec3 right(m_sizeX * cosZ, m_sizeY * sinZ, 0);
	Vec3 up(-m_sizeX * sinZ, m_sizeY * cosZ, 0);

	struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F pVerts[4];
	pVerts[0].xyz = (-right-up) + m_pos;
	pVerts[0].st[0] = 0; 
	pVerts[0].st[1] = 1;
	pVerts[0].color.dcolor = ~0;

	pVerts[1].xyz = ( right-up) + m_pos;
	pVerts[1].st[0] = 1; 
	pVerts[1].st[1] = 1;
	pVerts[1].color.dcolor = ~0;

	pVerts[2].xyz = ( right+up) + m_pos;
	pVerts[2].st[0] = 1; 
	pVerts[2].st[1] = 0;
	pVerts[2].color.dcolor = ~0;

	pVerts[3].xyz = (-right+up) + m_pos;
	pVerts[3].st[0] = 0; 
	pVerts[3].st[1] = 0;
	pVerts[3].color.dcolor = ~0;

	// prepare tangent space (tangent, binormal) and fill it in
	Vec3 rightUnit(cosZ, sinZ, 0);
	Vec3 upUnit(-sinZ, cosZ, 0);
	Vec4sf binormal(tPackF2B(-upUnit.x), tPackF2B(-upUnit.y), tPackF2B(-upUnit.z), tPackF2B(1));
	Vec4sf tangent(tPackF2B(rightUnit.x), tPackF2B(rightUnit.y), tPackF2B(rightUnit.z), tPackF2B(1));

	SPipTangents pTangents[4];
	pTangents[0].Tangent = tangent;
	pTangents[0].Binormal = binormal;
	pTangents[1].Tangent = tangent;
	pTangents[1].Binormal = binormal;
	pTangents[2].Tangent = tangent;
	pTangents[2].Binormal = binormal;
	pTangents[3].Tangent = tangent;
	pTangents[3].Binormal = binormal;

	// prepare indices
	uint16 pIndices[6];
	pIndices[0] = 0;
	pIndices[1] = 1;
	pIndices[2] = 2;

	pIndices[3] = 0;
	pIndices[4] = 2;
	pIndices[5] = 3;

	int afterWater(GetObjManager()->IsAfterWater(m_pos, GetCamera().GetPosition(), Get3DEngine()->GetWaterLevel()) ? 1 : 0);
	GetRenderer()->EF_AddPolygonToScene(pMaterial->GetShaderItem(), 4, pVerts, pTangents, pOb, pIndices, 6, afterWater);

	return true;
}



IPhysicalEntity* CDistanceCloudRenderNode::GetPhysics() const
{
	return 0;
}


void CDistanceCloudRenderNode::SetPhysics( IPhysicalEntity* )
{
}


void CDistanceCloudRenderNode::SetMaterial( IMaterial* pMat )
{
	m_pMaterial = pMat;
}


IMaterial* CDistanceCloudRenderNode::GetMaterial( Vec3 * pHitPos )
{
	return m_pMaterial;
}


float CDistanceCloudRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio_detail*GetViewDistRatioNormilized());

	return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio * GetViewDistRatioNormilized());
}


void CDistanceCloudRenderNode::Precache()
{
}


void CDistanceCloudRenderNode::GetMemoryUsage( ICrySizer* pSizer )
{
	SIZER_COMPONENT_NAME(pSizer, "DistanceCloudNode");
	pSizer->AddObject(this, sizeof(*this));
}
