////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   brush.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <ILMSerializationManager.h>
#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "CullBuffer.h"
#include "3dEngine.h"
#include "IndexedMesh.h"
#include "WaterVolumes.h"
#include "LMCompStructures.h"
#include "Brush.h"
#include "terrain.h"

//////////////////////////////////////////////////////////////////////////
// Brush Export structures.
//////////////////////////////////////////////////////////////////////////
#define BRUSH_FILE_TYPE 1
#define BRUSH_FILE_VERSION 3
#define BRUSH_FILE_SIGNATURE "CRY"

#pragma pack(push,1)
struct SExportedBrushHeader
{
	char signature[3];	// File signature.
	int filetype;				// File type.
	int	version;				// File version.
};

struct SExportedBrushGeom
{
	enum EFlags
	{
		SUPPORT_LIGHTMAP = 0x01,
		NO_PHYSICS = 0x02,
	};
	int size; // Size of this sructure.
	char filename[128];
	int flags; //! @see EFlags
	Vec3 m_minBBox;
	Vec3 m_maxBBox;
};

struct SExportedBrush
{
	int size; // Size of this sructure.
	int id;
	int geometry;
	int material;
	int flags;
	int mergeId;
	Matrix34 matrix;
	uchar ratioLod;
	uchar ratioViewDist;
	uchar reserved1;
	uchar reserved2;
};

#pragma pack(pop)
//////////////////////////////////////////////////////////////////////////

const char * CBrush::GetEntityClassName() const
{
	return "Brush";
}

Vec3 CBrush::GetPos(bool bWorldOnly) const
{
	assert(bWorldOnly);
	return m_vPos;
}

const char *CBrush::GetName() const
{
	if (m_pStatObj)
		return m_pStatObj->GetFilePath();
	return "StatObjNotSet";
}

bool CBrush::HasChanged()
{
	return false;
}

#ifdef WIN64
#pragma warning( push )									//AMD Port
#pragma warning( disable : 4311 )
#endif

bool CBrush::Render(const struct SRendParams & _EntDrawParams)
{
	FUNCTION_PROFILER_3DENGINE;

  if (!m_pStatObj || m_dwRndFlags&ERF_HIDDEN)
    return false;

	if (m_dwRndFlags & ERF_VISIBLE_ONLY_IN_EDITOR)
	{
		// Collision proxy is visible in Editor while in editing mode.
		if (!gEnv->bEditor || gEnv->bEditorGameMode)
		{
			if (GetCVars()->e_debug_draw == 0)
				return true;
		}
	}

	// some parameters will be modified
	SRendParams rParms = _EntDrawParams;

	// enable lightmaps if allowed
	if(m_dwRndFlags&ERF_USERAMMAPS && HasLightmap(0) && GetCVars()->e_ram_maps)
		rParms.m_pLMData	=	GetLightmapData(0);
  else
    rParms.m_pLMData	=	0;

	if (m_bHasVertexScatterBuffer)
	{
		rParms.m_pScatterBuffer = m_arrScatterVertexBuffer;
		rParms.mVertexScatterTransformZ = m_VertexScatterTransformZ;
	}
	else
		rParms.m_pScatterBuffer = NULL;

  if(m_nMaterialLayers)
    rParms.nMaterialLayers = m_nMaterialLayers;

	rParms.pMatrix = &m_Matrix;
	rParms.nMotionBlurAmount = 0;
	rParms.pMaterial = m_pMaterial;
  rParms.ppRNTmpData = &m_pRNTmpData;

  float fRadius = GetBBox().GetRadius();

  if(m_dwRndFlags&ERF_MERGE_RESULT)
  { // merged vegetations
    rParms.dwFObjFlags |= FOB_VEGETATION;
    rParms.dwFObjFlags &= ~FOB_TRANS_MASK;   
    if(m_dwRndFlags&ERF_USE_TERRAIN_COLOR && rParms.pTerrainTexInfo)
    {
      rParms.dwFObjFlags |= FOB_BLEND_WITH_TERRAIN_COLOR;
      Vec3 vTerrainNormal = GetTerrain()->GetTerrainSurfaceNormal(GetBBox().GetCenter(), fRadius);
      uchar ucSunDotTerrain	 = (uint8)(CLAMP((vTerrainNormal.Dot(Get3DEngine()->GetSunDirNormalized()))*255.f, 0, 255));
      rParms.AmbientColor.a *= (1.0f / 255.f * ucSunDotTerrain);
    }
  }

  if( !m_Matrix.m01 && !m_Matrix.m02 && !m_Matrix.m10 && !m_Matrix.m12 && !m_Matrix.m20 && !m_Matrix.m21 )
    rParms.dwFObjFlags &= ~FOB_TRANS_ROTATE;
  else
    rParms.dwFObjFlags |= FOB_TRANS_ROTATE;

  // get statobj for rendering
  CStatObj * pStatObj = GetStatObj();

  // cull lights per triangle for big objects
//  if(GetCVars()->e_brushes_cull_lights_per_triangle_min_obj_radius && pStatObj->GetRenderMesh() &&
  //  fRadius > GetCVars()->e_brushes_cull_lights_per_triangle_min_obj_radius)
//    CObjManager::CullLightsPerTriangle(pStatObj->GetRenderMesh(), m_Matrix, rParms.nDLightMask, m_lightsCache);

	// render
	if(pStatObj)
	{
		if (pStatObj->m_nFlags&STATIC_OBJECT_COMPOUND)
			rParms.m_pLMData	=	(m_SubObjectLightmapData.size()==pStatObj->SubObjectCount())?&m_SubObjectLightmapData[0]:0;

		if (pStatObj->m_nFlags&STATIC_OBJECT_COMPOUND)
		{
			rParms.m_pScatterBuffer = (m_SubObjectScatterVertexBuffer.size() == pStatObj->SubObjectCount()) ? &m_SubObjectScatterVertexBuffer[0] : NULL;
			rParms.mVertexScatterTransformZ = m_VertexScatterTransformZ;
		}

		if(GetCVars()->e_ram_maps && rParms.m_pLMData && rParms.m_pLMData->m_pLMData && rParms.m_pLMData->m_pLMData->GetRAETex())
			rParms.dwFObjFlags |= FOB_RAE_GEOMTERM;
		else
			rParms.dwFObjFlags &= ~FOB_RAE_GEOMTERM;

		pStatObj->Render(rParms);
	}

  if(m_dwRndFlags&ERF_SELECTED && GetCVars()->e_voxel_rasterize_selected_brush)
  {
    DoVoxelShape();

    if(ICVar*	pVarShadowGenGS = GetConsole()->GetCVar("e_voxel_rasterize_selected_brush"))
      pVarShadowGenGS->Set(0);
  }

	return true;
}

#ifdef WIN64
#pragma warning( pop )									//AMD Port
#endif

struct IStatObj * CBrush::GetEntityStatObj( unsigned int nPartId, unsigned int nSubPartId, Matrix34* pMatrix, bool bReturnOnlyVisible )
{
	if(nPartId != 0)
		return 0;

	if(pMatrix)
		*pMatrix = m_Matrix;

	return m_pStatObj;
}

void CBrush::SetMatrix( const Matrix34* pMatrix )
{
	Get3DEngine()->UnRegisterEntity(this);

	//	InvalidateShadow();

	if(pMatrix)
	{
		if(!IsMatrixValid(*pMatrix))
		{
			Warning( "Error: IRenderNode::SetMatrix: Invalid matrix passed from the editor - ignored, reset to identity: %s", GetName());
			m_Matrix.SetIdentity();
		}
		else
			m_Matrix = *pMatrix;

		m_vPos = m_Matrix.GetTranslation();
		CalcBBox();
	}

	Get3DEngine()->RegisterEntity(this);
	if (!m_pPhysEnt)
		Physicalize();
	else
	{
		// Just move physics.
		pe_status_placeholder spc;
		if (m_pPhysEnt->GetStatus(&spc) && !spc.pFullEntity)
		{
			pe_params_bbox pbb;
			pbb.BBox[0]=m_WSBBox.min; pbb.BBox[1]=m_WSBBox.max;
			m_pPhysEnt->SetParams(&pbb);
		} else
		{
			pe_params_pos par_pos;
			par_pos.pMtx3x4 = &m_Matrix;
			m_pPhysEnt->SetParams(&par_pos);
		}

		//////////////////////////////////////////////////////////////////////////
		// Update physical flags.
		//////////////////////////////////////////////////////////////////////////
		pe_params_foreign_data  foreignData;
		m_pPhysEnt->GetParams(&foreignData);
		if (m_dwRndFlags & ERF_HIDABLE)
			foreignData.iForeignFlags |= PFF_HIDABLE;
		else
			foreignData.iForeignFlags &= ~PFF_HIDABLE;
		if (m_dwRndFlags & ERF_HIDABLE_SECONDARY)
			foreignData.iForeignFlags |= PFF_HIDABLE_SECONDARY;
		else
			foreignData.iForeignFlags &= ~PFF_HIDABLE_SECONDARY;
		// flag to exclude from AI triangulation
		if (m_dwRndFlags & ERF_EXCLUDE_FROM_TRIANGULATION)
			foreignData.iForeignFlags |= PFF_EXCLUDE_FROM_STATIC;
		else
			foreignData.iForeignFlags &= ~PFF_EXCLUDE_FROM_STATIC;
		m_pPhysEnt->SetParams(&foreignData);
	}
}

void CBrush::CalcBBox()
{
	m_WSBBox.min = SetMaxBB();
	m_WSBBox.max = SetMinBB();

	if (!m_pStatObj)
		return;

	m_WSBBox.min = m_pStatObj->GetBoxMin();
	m_WSBBox.max = m_pStatObj->GetBoxMax();
	m_WSBBox.SetTransformedAABB(m_Matrix,m_WSBBox);
}

CBrush::CBrush()
{
	m_vPos=m_WSBBox.min=m_WSBBox.max=Vec3(ZERO);
	m_dwRndFlags=0;
	m_Matrix.SetIdentity();
	m_pPhysEnt=0;
	m_Matrix.SetIdentity();
	m_pMaterial = 0;
	m_nEditorObjectId = 0;
	m_pStatObj = NULL;
  m_pMaterial = NULL;
	m_bVehicleOnlyPhysics = false;
	m_bHasVertexScatterBuffer = false;
  m_vWindBending = Vec2(0,0);

	memset(m_arrScatterVertexBuffer, 0, sizeof(m_arrScatterVertexBuffer));

	GetInstCount(eERType_Brush)++;
}

void CBrush::DeleteLMTC()
{
	for(int nLod=0;nLod<MAX_BRUSH_LODS_NUM;nLod++)
		if(m_arrLMData[nLod].m_pLMTCBuffer)
		{
			GetSystem()->GetIRenderer()->DeleteRenderMesh(m_arrLMData[nLod].m_pLMTCBuffer);
			m_arrLMData[nLod].m_pLMTCBuffer = NULL;
		}
		for(int a=0;a<m_SubObjectLightmapData.size();a++)
			if(m_SubObjectLightmapData[a].m_pLMTCBuffer)
			{
				GetSystem()->GetIRenderer()->DeleteRenderMesh(m_SubObjectLightmapData[a].m_pLMTCBuffer);
				m_SubObjectLightmapData[a].m_pLMTCBuffer = NULL;
			}
}

void CBrush::DeleteVertexScattering()
{
	for(int nLod=0; nLod<MAX_BRUSH_LODS_NUM; ++nLod)
	{
		if(m_arrScatterVertexBuffer[nLod])
		{
			GetSystem()->GetIRenderer()->DeleteRenderMesh(m_arrScatterVertexBuffer[nLod]);
			m_arrScatterVertexBuffer[nLod] = NULL;
		}
	}
	for(int a=0; a<m_SubObjectScatterVertexBuffer.size(); ++a)
	{
		if(m_SubObjectScatterVertexBuffer[a])
		{
			GetSystem()->GetIRenderer()->DeleteRenderMesh(m_SubObjectScatterVertexBuffer[a]);
			m_SubObjectScatterVertexBuffer[a] = NULL;
		}
	}
}

CBrush::~CBrush()
{  
	Dephysicalize( );
//	Get3DEngine()->UnRegisterEntity(this);
  Get3DEngine()->FreeRenderNodeState(this);
  
	//m_pLMData = NULL;
/*
	if(GetRndFlags()&ERF_MERGED_NEW && m_nObjectTypeID>=0)
	{
		CStatObj * pBody = (CStatObj*)GetObjManager()->m_lstBrushTypes[m_nObjectTypeID];
		assert(pBody->m_nUsers==1);
		delete pBody;
		GetObjManager()->m_lstBrushTypes[m_nObjectTypeID] = NULL;
		m_nObjectTypeID=-1;
	}
*/
  DeleteLMTC();
	DeleteVertexScattering();
	
  m_pStatObj = NULL;

	if(m_pRNTmpData)
		Get3DEngine()->FreeRNTmpData(&m_pRNTmpData);
	assert(!m_pRNTmpData);

	GetInstCount(eERType_Brush)--;
}

void CBrush::Physicalize(bool bInstant)
{
	float fScaleX = m_Matrix.GetColumn(0).len();
	float fScaleY = m_Matrix.GetColumn(1).len();
	float fScaleZ = m_Matrix.GetColumn(2).len();

	if( fabs(fScaleX - fScaleY)>0.01f || fabs(fScaleX - fScaleZ)>0.01f || !m_pStatObj || !m_pStatObj->IsPhysicsExist() )
	{ // scip non uniform scaled object and object without physics
		
		// Check if we are compound object.
		if (m_pStatObj && !m_pStatObj->IsPhysicsExist() && (m_pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND))
		{
			// Try to physicalize compound object.
		}
		else
		{
			Dephysicalize();
			return;
		}
	}

	/*if (!GetCVars()->e_on_demand_physics || pBody->GetFlags() & STATIC_OBJECT_COMPOUND)
		bInstant = true;

	if (!bInstant)
	{
		pe_status_placeholder spc;
		if (m_pPhysEnt && m_pPhysEnt->GetStatus(&spc) && spc.pFullEntity)
			GetSystem()->GetIPhysicalWorld()->DestroyPhysicalEntity(spc.pFullEntity);

		pe_params_bbox pbb;
		pbb.BBox[0] = m_WSBBox.min;
		pbb.BBox[1] = m_WSBBox.max;
		pe_params_foreign_data pfd;
		pfd.pForeignData = (IRenderNode*)this;
		pfd.iForeignData = PHYS_FOREIGN_ID_STATIC;
		pfd.iForeignFlags = PFF_BRUSH;

		if (!m_pPhysEnt)
			m_pPhysEnt = GetSystem()->GetIPhysicalWorld()->CreatePhysicalPlaceholder(PE_STATIC,&pbb);
		else
			m_pPhysEnt->SetParams(&pbb);
		m_pPhysEnt->SetParams(&pfd);
		return;
	}*/

//  CStatObj * pBody = (CStatObj*)GetObjManager()->m_lstBrushTypes[m_nObjectTypeID];
  m_pStatObj->CheckLoaded();
//  if(!(pBody && (pBody->m_arrPhysGeomInfo[0] || pBody->m_arrPhysGeomInfo[1])))
  //  return;

	//Dephysicalize( );
	// create new
//	pe_params_pos par_pos;
	//pe_status_placeholder spc;
	//int bOnDemandCallback = 0;
	if (!m_pPhysEnt)
	{
		m_pPhysEnt = GetSystem()->GetIPhysicalWorld()->CreatePhysicalEntity(PE_STATIC,NULL,(IRenderNode*)this,PHYS_FOREIGN_ID_STATIC);
		if(!m_pPhysEnt)
			return;
	}
	/*else if (bOnDemandCallback = m_pPhysEnt->GetStatus(&spc) && spc.pFullEntity==0) 
		GetSystem()->GetIPhysicalWorld()->CreatePhysicalEntity(PE_STATIC,5.0f,0,0,0,-1,m_pPhysEnt);*/

	pe_action_remove_all_parts remove_all;
	m_pPhysEnt->Action(&remove_all);//,bOnDemandCallback);

  pe_geomparams params;	  
	if(m_pStatObj->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_DEFAULT])
  {
		if (m_pStatObj->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_NO_COLLIDE])
			params.flags &= ~geom_colltype_ray;
		if (m_bVehicleOnlyPhysics | m_pStatObj->m_bVehicleOnlyPhysics)
				params.flags = geom_colltype6;
		if (GetCVars()->e_obj_quality != CONFIG_LOW_SPEC) 
		{
			params.idmatBreakable = m_pStatObj->m_idmatBreakable;
			if (m_pStatObj->m_bBreakableByGame)
				params.flags |= geom_manually_breakable;
		}	else
			params.idmatBreakable = -1;

		if (GetRndFlags() & ERF_COLLISION_PROXY)
		{
			// Collision proxy only collides with players and vehicles.
			params.flags |= geom_colltype_player | geom_colltype_vehicle;
		}

    m_pPhysEnt->AddGeometry(m_pStatObj->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_DEFAULT], &params);//, -1, bOnDemandCallback);
  }
	params.idmatBreakable = -1;
  if(m_pStatObj->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_NO_COLLIDE])
	{
		params.flags = geom_colltype_ray;
	  m_pPhysEnt->AddGeometry(m_pStatObj->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_NO_COLLIDE], &params);//, -1, bOnDemandCallback);
	}
	if(m_pStatObj->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT])
	{
		params.flags = geom_colltype_obstruct;
		m_pPhysEnt->AddGeometry(m_pStatObj->m_arrPhysGeomInfo[PHYS_GEOM_TYPE_OBSTRUCT], &params);//, -1, bOnDemandCallback);
	}

	if (m_pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
	{
		Matrix34 tm;
		tm.SetIdentity();
		m_pStatObj->PhysicalizeSubobjects(m_pPhysEnt,&tm,0,0);	
	}

  if(m_dwRndFlags & (ERF_HIDABLE|ERF_HIDABLE_SECONDARY|ERF_EXCLUDE_FROM_TRIANGULATION))
  {
    pe_params_foreign_data  foreignData;
    m_pPhysEnt->GetParams(&foreignData);
		if (m_dwRndFlags & ERF_HIDABLE)
			foreignData.iForeignFlags |= PFF_HIDABLE;
		if (m_dwRndFlags & ERF_HIDABLE_SECONDARY)
			foreignData.iForeignFlags |= PFF_HIDABLE_SECONDARY;
		//[PETAR] new flag to exclude from triangulation
		if (m_dwRndFlags & ERF_EXCLUDE_FROM_TRIANGULATION)
			foreignData.iForeignFlags |= PFF_EXCLUDE_FROM_STATIC;
    m_pPhysEnt->SetParams(&foreignData);//,bOnDemandCallback);
  }

	pe_params_flags par_flags;
	par_flags.flagsOR = pef_never_affect_triggers;
	m_pPhysEnt->SetParams(&par_flags);//,bOnDemandCallback);

  pe_params_pos par_pos;
	par_pos.pMtx3x4 = &m_Matrix;
	m_pPhysEnt->SetParams(&par_pos);//,bOnDemandCallback);

	if (m_pMaterial)
		UpdatePhysicalMaterials( );//bOnDemandCallback );
}

//////////////////////////////////////////////////////////////////////////
void CBrush::UpdatePhysicalMaterials(int bThreadSafe)
{
	if (!m_pPhysEnt)
		return;

	if ((GetRndFlags() & ERF_COLLISION_PROXY) && m_pPhysEnt)
	{
		pe_params_part ppart;
		ppart.flagsAND = 0;
		ppart.flagsOR = geom_colltype_player | geom_colltype_vehicle;
		m_pPhysEnt->SetParams( &ppart );
	}
	
	if (m_pMaterial)
	{
		// Assign custom material to physics.
		int surfaceTypesId[MAX_SUB_MATERIALS];
		memset( surfaceTypesId,0,sizeof(surfaceTypesId) );
		int i,numIds = m_pMaterial->FillSurfaceTypeIds(surfaceTypesId);
		ISurfaceTypeManager *pSurfaceTypeManager = Get3DEngine()->GetMaterialManager()->GetSurfaceTypeManager();
		ISurfaceType *pMat;

		for(i=0,m_bVehicleOnlyPhysics=false; i<numIds; i++)
			if ((pMat=pSurfaceTypeManager->GetSurfaceType(surfaceTypesId[i])) &&
						(pMat->GetFlags() & SURFACE_TYPE_VEHICLE_ONLY_COLLISION))
				m_bVehicleOnlyPhysics = true;

		pe_params_part ppart;
		ppart.nMats = numIds;
		ppart.pMatMapping = surfaceTypesId;
		if (m_bVehicleOnlyPhysics)
			ppart.flagsAND = geom_colltype6;
		m_pPhysEnt->SetParams( &ppart, bThreadSafe );
	}	else if (m_bVehicleOnlyPhysics)
	{
		m_bVehicleOnlyPhysics = false;
		if (!m_pStatObj->m_bVehicleOnlyPhysics)
		{
			pe_params_part ppart;
			ppart.flagsOR = geom_colltype_solid|geom_colltype_ray|geom_floats|geom_colltype_explosion;
			m_pPhysEnt->SetParams( &ppart, bThreadSafe );
		}
	}
}

void CBrush::Dephysicalize( bool bKeepIfReferenced )
{
	// delete old physics
	if(m_pPhysEnt && GetSystem()->GetIPhysicalWorld()->DestroyPhysicalEntity(m_pPhysEnt, bKeepIfReferenced*4))
		m_pPhysEnt=0;
}

void CBrush::Dematerialize( )
{
  if(m_pMaterial)
    m_pMaterial = 0;
}

IPhysicalEntity* CBrush::GetPhysics() const
{
	return m_pPhysEnt;
}

//////////////////////////////////////////////////////////////////////////
void CBrush::SetPhysics( IPhysicalEntity* pPhys )
{
	m_pPhysEnt = pPhys;
}

//////////////////////////////////////////////////////////////////////////
bool CBrush::IsMatrixValid(const Matrix34 & mat)
{
	Vec3 vScaleTest = mat.TransformVector(Vec3(0,0,1));
  float fDist = mat.GetTranslation().GetDistance(Vec3(0,0,0));

	if( vScaleTest.GetLength()>1000.f || vScaleTest.GetLength()<0.01f || fDist > 64000 ||
		!_finite(vScaleTest.x) || !_finite(vScaleTest.y) || !_finite(vScaleTest.z) )
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBrush::SetMaterial( IMaterial *pMat )
{
  m_pMaterial = pMat;
	bool bCollisionProxy = false;
	bool bVisibleOnlyInEditor = false;
	if (pMat)
	{
		if (pMat->GetFlags() & MTL_FLAG_COLLISION_PROXY)
			bCollisionProxy = true;
		if (pMat->GetFlags() & MTL_FLAG_VISIBLE_ONLY_IN_EDITOR)
			bVisibleOnlyInEditor = true;
	}
	SetRndFlags( ERF_COLLISION_PROXY,bCollisionProxy );
	SetRndFlags( ERF_VISIBLE_ONLY_IN_EDITOR,bVisibleOnlyInEditor );

	UpdatePhysicalMaterials();

	// register and get brush material id
	m_pMaterial = pMat;

	if (!pMat && m_pStatObj)
		pMat = m_pStatObj->m_pMaterial;

	if (pMat && pMat->IsSubSurfScatterCaster())
    m_dwRndFlags |= ERF_SUBSURFSCATTER;
	else
		m_dwRndFlags &= ~ERF_SUBSURFSCATTER;

	if (pMat && pMat->GetFlags() & MTL_FLAG_VISIBLE_ONLY_IN_EDITOR)
		m_dwRndFlags |= ERF_VISIBLE_ONLY_IN_EDITOR;
	else
		m_dwRndFlags &= ~ERF_VISIBLE_ONLY_IN_EDITOR;

		if(m_pOcNode)
			((COctreeNode*)m_pOcNode)->MarkAsUncompiled();
  }

//////////////////////////////////////////////////////////////////////////
IMaterial* CBrush::GetMaterial(Vec3 * pHitPos)
{
	if (m_pMaterial)
		return m_pMaterial;

	if(m_pStatObj)
		return m_pStatObj->GetMaterial();

	return NULL;
}

void CBrush::CheckPhysicalized()
{
//  if(GetEntityStatObj(0))
  //  ((CStatObj*)GetEntityStatObj(0))->CheckLoaded(true);

  if(!m_pPhysEnt)
    Physicalize();
}

float CBrush::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio_detail*GetViewDistRatioNormilized());

  return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio*GetViewDistRatioNormilized());
}
/*
void CBrush::Serialize(bool bSave, ICryPak * pPak, FILE * f)
{
#if 0

  if(bSave)
  {
    pPak->FWrite(this,sizeof(*this),1,f);

    if(0 && m_pLMTCBuffer)
    {
		*/
  /*    int nPos=0;
      m_pLMTCBuffer->Serialize(nPos,0,true,0,0);
      uchar * pLMLBuffer = new uchar[nPos];
      pPak->FWrite(&nPos,sizeof(nPos),1,f);
      nPos=0;
      m_pLMTCBuffer->Serialize(nPos,pLMLBuffer,true,0,0);*/
/*
      // Make RenderMesh and fill it with texture coordinates
//      m_pLMTCBuffer = GetRenderer()->CreateRenderMeshInitialized(
  //      &vTexCoord3[0], pRenderMesh->GetSysVertCount(), VERTEX_FORMAT_P3F, 
    //    pRenderMesh->GetIndices(0), pRenderMesh->GetIndicesCount(), R_PRIMV_TRIANGLES, "LMapTexCoords", eBT_Static);

      pPak->FWrite(&m_pLMTCBuffer->GetSysVertCount(),sizeof(m_pLMTCBuffer->GetSysVertCount()),1,f);
      byte * pData = (byte *)m_pLMTCBuffer->GetSysVertBuffer()->m_VS[VSF_GENERAL].m_VData;
      assert(m_VertexSize[m_pLMTCBuffer->GetSysVertBuffer()->m_vertexformat] == 12);
      int nSize = m_VertexSize[m_pLMTCBuffer->GetSysVertBuffer()->m_vertexformat]*m_pLMTCBuffer->GetSysVertCount();
      pPak->FWrite(&m_pLMTCBuffer->GetSysVertCount(),sizeof(m_pLMTCBuffer->GetSysVertCount()),1,f);
    }
    else
    {
      int nPos=0;
      pPak->FWrite(&nPos,sizeof(nPos),1,f);
    }
  }
  else
  {
    pPak->FRead(this,sizeof(*this),1,f);

    if(m_nMaterialId>=0)
    { // restore material
      const char * sMtlName = CBrush::m_lstSExportedBrushMaterials[m_nMaterialId].material;
			m_pMaterial = Get3DEngine()->LoadMaterial( sMtlName );
    }
    else
      m_pMaterial = 0;

    int nSize=0;
    pPak->FRead(&nSize,sizeof(nSize),1,f);

    if(nSize)
    {
      assert(0);
      int nVertCount = 0;
      pPak->FRead(&nVertCount,sizeof(nVertCount),1,f);
      byte * pData = new byte[nVertCount*12];
      pPak->FRead(pData,nVertCount*12,1,f);

      // Make RenderMesh and fill it with texture coordinates
      m_pLMTCBuffer = GetRenderer()->CreateRenderMeshInitialized(
        pData, nVertCount, VERTEX_FORMAT_P3F, 
        0, 0, R_PRIMV_TRIANGLES, "LMapTexCoordsStreamed", eBT_Static);

      assert(m_VertexSize[m_pLMTCBuffer->GetSysVertBuffer()->m_vertexformat] == 12);
    }
    else
    {
      m_pLMTCBuffer = 0;
      m_pLMData = NULL;
    }
  }

#endif
}*/

void CBrush::GetMemoryUsage(ICrySizer * pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "Brush");
	pSizer->AddObject(this, sizeof(*this));
}

void CBrush::SetEntityStatObj( unsigned int nSlot, IStatObj * pStatObj, const Matrix34 * pMatrix )
{
  //assert(pStatObj);

	CStatObj *pPrevStatObj = GetStatObj();

  m_pStatObj = (CStatObj*)pStatObj;

  // If object differ we must re-physicalize.
	if (pStatObj != pPrevStatObj)
		if (!pPrevStatObj || !pStatObj || (pStatObj->GetCloneSourceObject()!=pPrevStatObj))
			Physicalize();

  if(pMatrix)
    SetMatrix(pMatrix);

	if(m_pRNTmpData)
		Get3DEngine()->FreeRNTmpData(&m_pRNTmpData);
	assert(!m_pRNTmpData);

	m_nInternalFlags |= UPDATE_DECALS;
}

void CBrush::PreloadInstanceResources(Vec3 vPrevPortalPos, float fPrevPortalDistance, float fTime)
{
	if(!GetEntityStatObj(0))
		return;

	if(GetCVars()->e_stream_cgf)
		((CStatObj*)GetEntityStatObj(0))->StreamCCGF(false);

	if(!GetEntityStatObj(0)->GetRenderMesh())
		return;

	float fDist = fPrevPortalDistance + Vec3(m_vPos).GetDistance(vPrevPortalPos);

	float fMaxViewDist = GetMaxViewDist();
	if(fDist<fMaxViewDist && fDist<GetCamera().GetFarPlane())
		GetEntityStatObj(0)->PreloadResources(fDist,fTime,0);


	for(int nLod=0; nLod<MAX_BRUSH_LODS_NUM; nLod++)
	{
		if(m_arrLMData[nLod].m_pLMData != NULL )
		{
/*			if(m_arrLMData[nLod].m_pLMData->GetColorLerpTex() /*&& m_arrLMData[nLod].m_pLMData->GetDomDirectionTex()* /)
			{
				{
					ITexture * pTexPic = GetRenderer()->EF_GetTextureByID(m_arrLMData[nLod].m_pLMData->GetColorLerpTex());
					if(pTexPic)
						GetRenderer()->EF_PrecacheResource(pTexPic, 0, 1.f, 0);
				}
				/*{
				ITexture * pTexPic = GetRenderer()->EF_GetTextureByID(m_arrLMData[nLod].m_pLMData->GetDomDirectionTex());
				if(pTexPic)
				GetRenderer()->EF_PrecacheResource(pTexPic, 0, 1.f, 0);
				}* /
			}
			if( m_arrLMData[nLod].m_pLMData->GetOcclusionTex() )
			{
				ITexture * pTexPic = GetRenderer()->EF_GetTextureByID(m_arrLMData[nLod].m_pLMData->GetOcclusionTex());
				if(pTexPic)
					GetRenderer()->EF_PrecacheResource(pTexPic, 0, 1.f, 0);
			}*/
			if( m_arrLMData[nLod].m_pLMData->GetRAETex() )
			{
				ITexture * pTexPic = GetRenderer()->EF_GetTextureByID(m_arrLMData[nLod].m_pLMData->GetRAETex());
				if(pTexPic)
					GetRenderer()->EF_PrecacheResource(pTexPic, 0, 1.f, 0);
			}
		}
	}
}

IRenderMesh * CBrush::GetRenderMesh(int nLod)
{ 
	IStatObj * pStatObj = GetStatObj() ? GetStatObj()->GetLodObject(nLod) : NULL; 
	return pStatObj ? pStatObj->GetRenderMesh() : NULL;
}


float CBrush::GetLodForDistance(float fDistance)
{
	assert(m_pStatObj);

	float fLodLevel = CObjManager::GetObjectLOD(fDistance, GetLodRatioNormalized(), GetBBox().GetRadius());

	if(fLodLevel > m_pStatObj->m_nLoadedLodsNum-1)
		fLodLevel = m_pStatObj->m_nLoadedLodsNum-1;

	return fLodLevel;
}

void CBrush::OnRenderNodeBecomeVisible()
{
  assert(m_pRNTmpData);
  m_pRNTmpData->userData.objMat = m_Matrix;
  m_pRNTmpData->userData.fRadius = GetBBox().GetRadius();
}

template <class T, bool bABS> 
int TCompare(const void* v1, const void* v2)
{
  T*p1 = (T*)v1;
  T*p2 = (T*)v2;

  if(bABS)
  {
    if(abs(*p1)>abs(*p2))
      return -1;
    else if(abs(*p1)<abs(*p2))
      return 1;
  }
  else
  {
    if((*p1)>(*p2))
      return -1;
    else if((*p1)<(*p2))
      return 1;
  }

  return 0;
}

void CBrush::DoVoxelShape()
{
  PrintMessage("Voxelizing object ... ");

  int nPaintPoins = 0;

  Matrix34 matInv = m_Matrix.GetInverted();
  float fStep = max(0.01f,GetCVars()->e_voxel_rasterize_selected_brush);
  for(float x=m_WSBBox.min.x; x<=m_WSBBox.max.x; x+=fStep)
  {
    for(float y=m_WSBBox.min.y; y<=m_WSBBox.max.y; y+=fStep)
    {
      PodArray<float> arrSurfaceZ;

      {
        Vec3 vStart = matInv.TransformPoint(Vec3(x,y,m_WSBBox.max.z));
        Vec3 vEnd = matInv.TransformPoint(Vec3(x,y,m_WSBBox.min.z));
        Vec3 vStartCur = vStart;

        bool bHit = true;
        while(bHit) 
        {
          SRayHitInfo hitInfo;
          hitInfo.inRay = Ray(vStartCur, (vEnd-vStart));
          hitInfo.inReferencePoint = vStartCur;

          if(bHit = m_pStatObj->RayIntersection(hitInfo,m_pMaterial))
          {
            vStartCur.z = hitInfo.vHitPos.z - 0.0001f;
            Vec3 vCurWS = m_Matrix.TransformPoint(vStartCur);
            arrSurfaceZ.Add((vCurWS.z+1000.f));
          }
        }
      }

      {
        Vec3 vStart = matInv.TransformPoint(Vec3(x,y,m_WSBBox.min.z));
        Vec3 vEnd = matInv.TransformPoint(Vec3(x,y,m_WSBBox.max.z));
        Vec3 vStartCur = vStart;

        bool bHit = true;
        while(bHit) 
        {
          SRayHitInfo hitInfo;
          hitInfo.bOnlyZWrite = true;
          hitInfo.inRay = Ray(vStartCur, (vEnd-vStart));
          hitInfo.inReferencePoint = vStartCur;

          if(bHit = m_pStatObj->RayIntersection(hitInfo,m_pMaterial))
          {
            vStartCur.z = hitInfo.vHitPos.z + 0.0001f;
            Vec3 vCurWS = m_Matrix.TransformPoint(vStartCur);
            arrSurfaceZ.Add(-(vCurWS.z+1000.f));
          }
        }
      }

      if(arrSurfaceZ.Count()==1 && arrSurfaceZ[0]>0)
        arrSurfaceZ.Add(-(m_WSBBox.min.z+1000.f));

      if(arrSurfaceZ.Count()>=2)
      {
        qsort(arrSurfaceZ.GetElements(), arrSurfaceZ.Count(), sizeof(arrSurfaceZ[0]), TCompare<float,true>);

        for(int i=0; (i+1)<arrSurfaceZ.Count();)
        {
          while(arrSurfaceZ.Count()>i && arrSurfaceZ[i]<0)
            i++;
          if(i>=arrSurfaceZ.Count())
            break;
          float zTop = fabs(arrSurfaceZ[i])-1000.f;

          while(arrSurfaceZ.Count()>i && arrSurfaceZ[i]>0)
            i++;
          if(i>=arrSurfaceZ.Count())
            break;
          float zBot = fabs(arrSurfaceZ[i])-1000.f;

          for(float z=zTop; z>zBot; z-=fStep)
          {
            Get3DEngine()->Voxel_Paint(Vec3(x,y,z),fStep,0,Vec3(1,1,1),eveoCreate,evbsSphere,evetVoxelObjects);
            nPaintPoins++;
          }
        }
      }
    }
  }

  PrintMessagePlus("done (%d pp)", nPaintPoins);
}

void CBrush::SetVertexScattering(const SScatteringObjectData& ScatteringObjectData)
{
	IRenderer* pIRenderer = GetRenderer();
	IStatObj* pIStatObj = GetStatObj();
	if (pIStatObj == NULL)
		return;

	m_VertexScatterTransformZ = ScatteringObjectData.m_VertexScatterTransformZ;

	int subobjectcount = pIStatObj->GetSubObjectCount();

	m_bHasVertexScatterBuffer = false;
	if (pIStatObj->GetRenderMesh())
	{
		for (int nLod=0; nLod<MAX_VERTEX_SCATTER_LODS_NUM; ++nLod)
		{
			if (m_arrScatterVertexBuffer[nLod])
				pIRenderer->DeleteRenderMesh(m_arrScatterVertexBuffer[nLod]);
			SVertexScatterLODLevel* vsll = ScatteringObjectData.m_VertexScatterSubObjects[0].m_VertexScatterLODLevels[nLod];
			if (vsll)
			{
				m_arrScatterVertexBuffer[nLod] = pIRenderer->CreateRenderMeshInitialized(vsll->m_VertexScatterValues, vsll->m_nNumVertices, VERTEX_FORMAT_SCATTER, NULL, 0, R_PRIMV_TRIANGLES, "VertexScatter", pIStatObj->GetFilePath());
				m_bHasVertexScatterBuffer = true;
			}
			else
				m_arrScatterVertexBuffer[nLod] = NULL;
		}
	}
	else
	{
		for (int i=0; i<m_SubObjectScatterVertexBuffer.size(); ++i)
			if (m_SubObjectScatterVertexBuffer[i])
				pIRenderer->DeleteRenderMesh(m_SubObjectScatterVertexBuffer[i]);

		// Tbyte TODO: LOD with submeshes
		m_SubObjectScatterVertexBuffer.resize(ScatteringObjectData.m_nNumSubObjects);
		for (int i=0; i<ScatteringObjectData.m_nNumSubObjects; ++i)
		{
			SVertexScatterLODLevel* vsll = ScatteringObjectData.m_VertexScatterSubObjects[i].m_VertexScatterLODLevels[0];
			if (vsll)
			{
				m_SubObjectScatterVertexBuffer[i] = pIRenderer->CreateRenderMeshInitialized(vsll->m_VertexScatterValues, vsll->m_nNumVertices, VERTEX_FORMAT_SCATTER, NULL, 0, R_PRIMV_TRIANGLES, "VertexScatter", pIStatObj->GetFilePath());
				m_bHasVertexScatterBuffer = true;
			}
			else
				m_SubObjectScatterVertexBuffer[i] = NULL;
		}
	}
}
