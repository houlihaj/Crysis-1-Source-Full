////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   CloudRenderNode.h
//  Version:     v1.00
//  Created:     15/2/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CloudRenderNode.h"
#include "CloudsManager.h"
#include "VisAreas.h"
#include "ObjMan.h"

//////////////////////////////////////////////////////////////////////////
CCloudRenderNode::CCloudRenderNode()
{
	m_bounds.min = Vec3(-1,-1,-1);
	m_bounds.max = Vec3(1,1,1);
	m_fScale = 1.0f;
	m_offsetedMatrix.SetIdentity();
	m_matrix.SetIdentity();
	m_vOffset.Set(0,0,0);
  m_alpha = 1.f;

	m_pCloudRenderElement = (CREBaseCloud*)GetRenderer()->EF_CreateRE( eDATA_Cloud );
	m_pRenderObject = GetRenderer()->EF_GetObject(false);

	GetCloudsManager()->AddCloudRenderNode(this);

	m_origin = Vec3(0, 0, 0);
	m_moveProps.m_autoMove = false;
	m_moveProps.m_speed = Vec3(0, 0, 0);
	m_moveProps.m_spaceLoopBox = Vec3(2000.0f, 2000.0f, 2000.0f);
	m_moveProps.m_fadeDistance = 0;
}

//////////////////////////////////////////////////////////////////////////
CCloudRenderNode::~CCloudRenderNode()
{
	GetCloudsManager()->RemoveCloudRenderNode(this);
	m_pRenderObject->RemovePermanent();
	m_pCloudRenderElement->Release();

	Get3DEngine()->UnRegisterEntity( this );
  Get3DEngine()->FreeRenderNodeState(this);
}

//////////////////////////////////////////////////////////////////////////
float CCloudRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio_detail*GetViewDistRatioNormilized());

	return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio*GetViewDistRatioNormilized());
}

//////////////////////////////////////////////////////////////////////////
bool CCloudRenderNode::LoadCloudFromXml( XmlNodeRef root )
{
	m_pCloudDesc = new SCloudDescription;
	GetCloudsManager()->ParseCloudFromXml( root,m_pCloudDesc );

	SetCloudDesc( m_pCloudDesc );
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CCloudRenderNode::LoadCloud( const char *sCloudFilename )
{
	m_bounds.min = Vec3(-1,-1,-1);
	m_bounds.max = Vec3(1,1,1);

	SetCloudDesc( GetCloudsManager()->LoadCloud( sCloudFilename ) );

	return m_pCloudDesc != 0;
}

//////////////////////////////////////////////////////////////////////////
void CCloudRenderNode::SetMovementProperties(const SCloudMovementProperties& properties)
{
	m_moveProps = properties;
}

//////////////////////////////////////////////////////////////////////////
void CCloudRenderNode::SetCloudDesc( SCloudDescription *pCloud )
{
	m_pCloudDesc = pCloud;
	if (m_pCloudDesc != NULL && m_pCloudDesc->m_particles.size() > 0)
	{
		m_vOffset = m_pCloudDesc->m_offset;
		m_bounds.min = m_pCloudDesc->m_bounds.min - m_pCloudDesc->m_offset;
		m_bounds.max = m_pCloudDesc->m_bounds.max - m_pCloudDesc->m_offset;
		if (m_pCloudDesc->m_pMaterial)
			m_pMaterial = m_pCloudDesc->m_pMaterial;
		m_pCloudRenderElement->SetParticles( &m_pCloudDesc->m_particles[0],m_pCloudDesc->m_particles.size() );

		m_WSBBox.SetTransformedAABB( m_matrix,m_bounds );
		m_fScale = m_matrix.GetColumn(0).GetLength();
		// Offset matrix by the cloud bounds offset.
		m_offsetedMatrix = m_matrix * Matrix34::CreateTranslationMat(-m_vOffset);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCloudRenderNode::SetMatrix( const Matrix34& mat )
{
	SetMatrixInternal(mat, true);
}

//////////////////////////////////////////////////////////////////////////
void CCloudRenderNode::SetMatrixInternal(const Matrix34& mat, bool updateOrigin)
{
  m_dwRndFlags |= ERF_OUTDOORONLY;

	if (updateOrigin)
		m_origin = mat.GetTranslation();

	m_matrix = mat;
	m_pos = m_matrix.GetTranslation();
//	m_WSBBox.SetTransformedAABB( m_matrix,m_bounds );
	m_fScale = m_matrix.GetColumn(0).GetLength();
	m_WSBBox.SetTransformedAABB( Matrix34::CreateTranslationMat(m_pos),AABB(m_bounds.min*m_fScale,m_bounds.max*m_fScale) );

	// Offset matrix by the cloud bounds offset.
//	m_offsetedMatrix = m_matrix * Matrix34::CreateTranslationMat(-m_vOffset);
	m_offsetedMatrix = Matrix34::CreateTranslationMat(m_pos-m_vOffset*m_fScale);
	m_offsetedMatrix.Scale(Vec3(m_fScale,m_fScale,m_fScale));
	Get3DEngine()->RegisterEntity( this );
}

//////////////////////////////////////////////////////////////////////////
void CCloudRenderNode::MoveCloud()
{
	FUNCTION_PROFILER_3DENGINE;
	
	Vec3 pos(m_matrix.GetTranslation());

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
		Matrix34 mat(m_matrix);
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
			Matrix34 mat(m_matrix);
			mat.SetTranslation(m_origin);
			SetMatrixInternal(mat, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CCloudRenderNode::Render( const SRendParams &rParams )
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_pRenderObject == NULL || m_pMaterial == NULL	|| !GetCVars()->e_clouds)
		return false;

	SShaderItem shaderItem;
	if (rParams.pMaterial)
		shaderItem = rParams.pMaterial->GetShaderItem(0);
	else
		shaderItem = m_pMaterial->GetShaderItem(0);

	m_pRenderObject->m_II.m_Matrix = m_offsetedMatrix;
	m_pRenderObject->m_ObjFlags |= FOB_TRANS_MASK;
	m_pRenderObject->m_fTempVars[0] = m_fScale;
	m_pRenderObject->m_fSort = 0;
	m_pRenderObject->m_fDistance = rParams.fDistance;
	int nAfterWater = CObjManager::IsAfterWater(m_offsetedMatrix.GetTranslation(),GetCamera().GetPosition(),Get3DEngine()->GetWaterLevel()) ? 1 : 0;
	m_pRenderObject->m_II.m_AmbColor = rParams.AmbientColor;
	m_pRenderObject->m_fAlpha = rParams.fAlpha * m_alpha;

	float mvd(GetMaxViewDist());
	float d((GetCamera().GetPosition() - m_offsetedMatrix.GetTranslation()).GetLength());
	if (d > 0.9f * mvd)
	{
		float s(clamp_tpl(1.0f - (d - 0.9f * mvd) / (0.1f * mvd), 0.0f, 1.0f));
		m_pRenderObject->m_fAlpha *= s;
	}

	CRenderObject *pObj = GetRenderer()->EF_GetObject(false, m_pRenderObject->m_Id);
  if (!pObj)
    return false;
  GetRenderer()->EF_AddEf(m_pCloudRenderElement, shaderItem, m_pRenderObject, EFSLIST_TRANSP, nAfterWater);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CCloudRenderNode::CheckIntersection(const Vec3& p1, const Vec3& p2)
{
	if(p1==p2)
		return false;
	if(m_pCloudDesc && m_pCloudDesc->m_pCloudTree)
	{
		Vec3 outp;
		if(Intersect::Lineseg_AABB(Lineseg(p1, p2), m_WSBBox, outp))
		{
			Matrix34 pInv = m_offsetedMatrix.GetInverted();
			return m_pCloudDesc->m_pCloudTree->CheckIntersection(pInv * p1, pInv * p2);
		}
	}
	return false;
}
