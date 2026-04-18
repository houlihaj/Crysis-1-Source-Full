////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   PickObjectTool.cpp
//  Version:     v1.00
//  Created:     18/12/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MaterialPickTool.h"
#include "Viewport.h"
#include "CryEditDoc.h"
#include "SurfaceType.h"
#include "Material\MaterialManager.h"

#include <IPhysics.h>
#include <I3Dengine.h>
#include <IEntitySystem.h>
#include <ICryAnimation.h>
#include <IEntityRenderState.h>

#define RENDER_MESH_TEST_DISTANCE 0.2f

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CMaterialPickTool,CEditTool)

static IClassDesc* s_ToolClass = NULL;

//////////////////////////////////////////////////////////////////////////
CMaterialPickTool::CMaterialPickTool()
{
	m_pClassDesc = s_ToolClass;
	m_statusText = _T("Left Click To Pick Material");

	m_hitNormal.Set(0,0,0);
	m_hitPos.Set(0,0,0);
}

//////////////////////////////////////////////////////////////////////////
CMaterialPickTool::~CMaterialPickTool()
{
	SetMaterial(0);
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialPickTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseLDown)
	{
		if (m_pMaterial)
		{
			CMaterial *pMtl = GetIEditor()->GetMaterialManager()->FromIMaterial(m_pMaterial);
			if (pMtl)
			{
				GetIEditor()->OpenDataBaseLibrary( EDB_TYPE_MATERIAL,pMtl );
				Abort();
				return true;
			}
		}
	}
	else if (event == eMouseMove)
	{
		return OnMouseMove( view,flags,point );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialPickTool::Display( DisplayContext &dc )
{
	CPoint mousePoint;
	::GetCursorPos( &mousePoint );

	dc.view->ScreenToClient( &mousePoint );

	Vec3 wp = dc.view->ViewToWorld( mousePoint );

	if (m_pMaterial)
	{
		float color[4] = {1,1,1,1};
		dc.renderer->Draw2dLabel( mousePoint.x+12,mousePoint.y+8,1.2f,color,false,"%s",(const char*)m_displayString );
		//m_renderer->DrawLabelEx( hit.pt+Vec3(0.2f,0,-0.2f),1.2f,color,true,false,"%s",pSurfaceType->GetName() );
	}

	//dc.SetColor( ColorB(255,0,0,255) );
	//if (!m_hitPos.IsZero())
		//dc.DrawPoint( m_hitPos,3 );

	float fScreenScale = dc.view->GetScreenScaleFactor(m_hitPos) * 0.06f;

	dc.DepthTestOff();
	dc.SetColor( ColorB(0,0,255,255) );
	if (!m_hitNormal.IsZero())
	{
		dc.DrawLine( m_hitPos,m_hitPos + m_hitNormal*fScreenScale );

		Vec3 raySrc,rayDir;
		dc.view->ViewToWorldRay( mousePoint,raySrc,rayDir );

		Matrix34 tm;

		Vec3 zAxis = m_hitNormal;
		Vec3 xAxis = rayDir.Cross(zAxis);
		if (!xAxis.IsZero())
		{
			xAxis.Normalize();
			Vec3 yAxis = xAxis.Cross(zAxis).GetNormalized();
			tm.SetFromVectors( xAxis,yAxis,zAxis,m_hitPos );

			dc.PushMatrix( tm );
			dc.DrawCircle( Vec3(0,0,0),0.5f*fScreenScale );
			dc.PopMatrix();
		}
	}
	dc.DepthTestOn();

	if (m_pMaterial)
	{
		CInputLightMaterial lm = m_prevLightMaterial;
		COLORREF col = dc.GetSelectedColor();

		lm.m_Diffuse.r = GetRValue(col)/255.0f + 0.1f;
		lm.m_Diffuse.g = GetGValue(col)/255.0f + 0.1f;
		lm.m_Diffuse.b = GetBValue(col)/255.0f + 0.1f;

		lm.m_Emission.r = GetRValue(col)/255.0f;
		lm.m_Emission.g = GetGValue(col)/255.0f;
		lm.m_Emission.b = GetBValue(col)/255.0f;
		lm.m_Emission.a = 1.0f;


		const SShaderItem &shaderItem = m_pMaterial->GetShaderItem();
		if (shaderItem.m_pShaderResources)
		{
			//shaderItem.m_pShaderResources->SetInputLM( lm );
			ColorF & dCol = shaderItem.m_pShaderResources->GetDiffuseColor();
			ColorF & eCol = shaderItem.m_pShaderResources->GetEmissiveColor();
			dCol = lm.m_Diffuse;
			eCol = lm.m_Emission;
			shaderItem.m_pShaderResources->UpdateConstants(shaderItem.m_pShader);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialPickTool::OnMouseMove( CViewport *view,UINT nFlags,CPoint point )
{
	view->SetCurrentCursor( STD_CURSOR_HIT );

	m_hitPos.Set(0,0,0);
	m_hitNormal.Set(0,0,0);
	
	Vec3 raySrc,rayDir;
	view->ViewToWorldRay( point,raySrc,rayDir );
	raySrc = raySrc + rayDir*1.0f;
	rayDir = rayDir*1000.0f;

	int objTypes = ent_all;

	// Include obstruct geometry.
	int flags = (geom_colltype_ray|geom_colltype14)<<rwi_colltype_bit | rwi_colltype_any | rwi_ignore_terrain_holes | rwi_stop_at_pierceable;

	m_surfaceId = 0;
	ray_hit hit;
	hit.pCollider = 0;
	int col = gEnv->pPhysicalWorld->RayWorldIntersection( raySrc,rayDir,objTypes,flags,&hit,1 );
	if (col > 0)
	{
		m_surfaceId = hit.surface_idx;
		m_hitNormal = hit.n;
		m_hitPos = hit.pt;
		if (hit.bTerrain)
		{
			int nTerrainSurfaceType = hit.idmatOrg;
			if (nTerrainSurfaceType >= 0 && nTerrainSurfaceType < GetIEditor()->GetDocument()->GetSurfaceTypeCount())
			{
				CSurfaceType *pSurfaceType = GetIEditor()->GetDocument()->GetSurfaceTypePtr(nTerrainSurfaceType);
				CString mtlName = pSurfaceType ? pSurfaceType->GetMaterial() : "";
				if (!mtlName.IsEmpty())
				{
					SetMaterial( GetIEditor()->Get3DEngine()->GetMaterialManager()->FindMaterial( mtlName ) );
					return true;
				}
			}
		}
		else
		{
			// Not terrain.
			int nHitMatID = hit.idmatOrg;
			if (hit.pCollider)
			{
				int type = hit.pCollider->GetiForeignData();
				switch (type)
				{
				case PHYS_FOREIGN_ID_STATIC:
					{
						IRenderNode *pRenderNode = (IRenderNode*)hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
						if (pRenderNode)
						{
							Matrix34 worldTM;
							IStatObj * pStatObj = pRenderNode->GetEntityStatObj( 0, 0, &worldTM );
							if(pStatObj)
							{
								Matrix33 worldRot = Matrix33(worldTM);
								// transform decal into object space 
								Matrix34 worldTM_Inverted = worldTM.GetInverted();
								// put hit normal into the object space
								Vec3 vRayDir = rayDir.GetNormalized()*worldRot;
								// put hit position into the object space
								Vec3 vHitPos = worldTM_Inverted.TransformPoint(raySrc);
								Vec3 vLineP1 = vHitPos - vRayDir*RENDER_MESH_TEST_DISTANCE;
								SRayHitInfo hitInfo;
								memset(&hitInfo,0,sizeof(hitInfo));
								hitInfo.inReferencePoint = vHitPos;
								hitInfo.inRay.origin = vLineP1;
								hitInfo.inRay.direction = vRayDir;
								hitInfo.bInFirstHit = false;
								hitInfo.bUseCache = true;

								if(pStatObj->RayIntersection(hitInfo, 0))
									nHitMatID = hitInfo.nHitMatID;
							}

              Vec3 arrHitPosAndSurfaceNormal[2];
              arrHitPosAndSurfaceNormal[0] = m_hitPos;
              arrHitPosAndSurfaceNormal[1] = m_hitNormal;

							SetMaterial( pRenderNode->GetMaterial(arrHitPosAndSurfaceNormal),nHitMatID );
							return true;
						}
					}
					break;
				case PHYS_FOREIGN_ID_ENTITY:
					{
						IEntity *pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider);
						if (pEntity)
						{
							IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
							if (pRenderProxy)
							{
								IRenderNode * pRenderNode = pRenderProxy->GetRenderNode();
								if(pRenderNode)
								{
									Matrix34 worldTM;
									IStatObj * pStatObj = pRenderNode->GetEntityStatObj( 0, 0, &worldTM );
									if(pStatObj)
									{
										Matrix33 worldRot = Matrix33(worldTM);
										// transform decal into object space 
										Matrix34 worldTM_Inverted = worldTM.GetInverted();
										// put hit normal into the object space
										Vec3 vRayDir = rayDir.GetNormalized()*worldRot;
										// put hit position into the object space
										Vec3 vHitPos = worldTM_Inverted.TransformPoint(raySrc);
										Vec3 vLineP1 = vHitPos - vRayDir*RENDER_MESH_TEST_DISTANCE;
										SRayHitInfo hitInfo;
										memset(&hitInfo,0,sizeof(hitInfo));
										hitInfo.inReferencePoint = vHitPos;
										hitInfo.inRay.origin = vLineP1;
										hitInfo.inRay.direction = vRayDir;
										hitInfo.bInFirstHit = false;
										hitInfo.bUseCache = true;

										if(pStatObj->RayIntersection(hitInfo, 0))
											nHitMatID = hitInfo.nHitMatID;
									}
								}

								if (pEntity->GetMaterial())
								{
									SetMaterial( pEntity->GetMaterial(),nHitMatID );
									return true;
								}
								else
								{
									for (int i = 0; i < pEntity->GetSlotCount(); i++)
									{
										// If rendered slot.
										if (pEntity->GetSlotFlags(i) & ENTITY_SLOT_RENDER)
										{
											SEntitySlotInfo si;
											if (pEntity->GetSlotInfo( i,si ))
											{
												if (si.pMaterial)
												{
													SetMaterial( pEntity->GetMaterial(),nHitMatID );
													return true;
												}
												if (si.pStatObj)
												{
													SetMaterial( si.pStatObj->GetMaterial(),nHitMatID );
													return true;
												}
												if (si.pCharacter)
												{
													SetMaterial( si.pCharacter->GetMaterial(),nHitMatID );
													return true;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	SetMaterial( 0 );
	return true;
}

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CMaterialPickTool_ClassDesc : public CRefCountClassDesc
{
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }
	virtual REFGUID ClassID() {
		// {FD20F6F2-7B87-4349-A5D4-7533538E357F}
		static const GUID guid = { 0xfd20f6f2, 0x7b87, 0x4349, { 0xa5, 0xd4, 0x75, 0x33, 0x53, 0x8e, 0x35, 0x7f } };
		return guid;
	}
	virtual const char* ClassName() { return "EditTool.PickMaterial"; };
	virtual const char* Category() { return "Material"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CMaterialPickTool); }
};

//////////////////////////////////////////////////////////////////////////
void CMaterialPickTool::RegisterTool( CRegistrationContext &rc )
{
	rc.pClassFactory->RegisterClass( s_ToolClass = new CMaterialPickTool_ClassDesc );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialPickTool::SetMaterial( IMaterial *pMaterial,int nSubMtlIt )
{
	if (nSubMtlIt >= 0 && pMaterial)
	{
		pMaterial = pMaterial->GetSafeSubMtl(nSubMtlIt);
	}

	if (pMaterial == m_pMaterial)
		return;

	if (m_pMaterial && (!(m_pMaterial->GetFlags() & MTL_FLAG_NODRAW)))
	{
		const SShaderItem &shaderItem = m_pMaterial->GetShaderItem();
		if (shaderItem.m_pShaderResources)
		{
			// Restore old light material pointer.
			//shaderItem.m_pShaderResources->SetInputLM( m_prevLightMaterial );
			ColorF & dCol = shaderItem.m_pShaderResources->GetDiffuseColor();
			ColorF & eCol = shaderItem.m_pShaderResources->GetEmissiveColor();
			dCol = m_prevLightMaterial.m_Diffuse;
			eCol = m_prevLightMaterial.m_Emission;
			shaderItem.m_pShaderResources->UpdateConstants(shaderItem.m_pShader);
		}
	}

	m_pMaterial = pMaterial;

	m_displayString = "";
	if (pMaterial)
	{
		if (!(pMaterial->GetFlags() & MTL_FLAG_NODRAW))
		{
			const SShaderItem &shaderItem = pMaterial->GetShaderItem();
			if (shaderItem.m_pShaderResources)
			{
				//shaderItem.m_pShaderResources->ToInputLM( m_prevLightMaterial );
				ColorF & dCol = shaderItem.m_pShaderResources->GetDiffuseColor();
				ColorF & eCol = shaderItem.m_pShaderResources->GetEmissiveColor();
				m_prevLightMaterial.m_Diffuse = dCol;
				m_prevLightMaterial.m_Emission = eCol;
			}
		}

		CString sfTypeName = GetIEditor()->Get3DEngine()->GetMaterialManager()->GetSurfaceType( m_surfaceId,"Material Pick Tool" )->GetName();
		CString sfType;
		sfType.Format( "(%d: %s)",m_surfaceId,(const char*)sfTypeName );

		m_displayString = pMaterial->GetName();
		m_displayString += "\n";
		m_displayString += CString(pMaterial->GetSurfaceType()->GetName()) + " " + sfType;
	}
}
