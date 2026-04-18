////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjman.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Loading trees, buildings, ragister/unregister entities for rendering
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "CullBuffer.h"
#include "3dEngine.h"
#include "IndexedMesh.h"
#include "WaterVolumes.h"
#include "Brush.h"
#include "LMCompStructures.h"
#include "Vegetation.h"
#include "terrain.h"
#include "ObjectsTree.h"
#include "VoxMan.h"

IStatObj * CObjManager::GetStaticObjectByTypeID(int nTypeID)
{
  if(nTypeID>=0 && nTypeID<m_lstStaticTypes.size())
    return m_lstStaticTypes[nTypeID].pStatObj;

  return 0;
}
/*
void CObjManager::UnregisterCGFFromTypeTables(CStatObj * pStatObj)
{
	for(int i=0; i<m_lstStaticTypes.size(); i++)
	{
		if( m_lstStaticTypes[i].GetStatObj() == pStatObj )
    {
      m_lstStaticTypes[i].pStatObj = NULL;
			memset(&m_lstStaticTypes[i], 0, sizeof(m_lstStaticTypes[i]));
    }
	}
}
*/
void CObjManager::CheckObjectLeaks(bool bDeleteAll)
{
	bool bReport = m_lstLoadedObjects.size() > 0;

	// deleting leaked objects
	if(bReport)
		Warning("CObjManager::CheckObjectLeaks: %d object(s) found in memory", m_lstLoadedObjects.size());

	for (LoadedObjects::iterator it = m_lstLoadedObjects.begin(); it != m_lstLoadedObjects.end(); ++it)
	{
		CStatObj *pStatObj = *it;
		if(bReport && !pStatObj->IsDefaultObject())
		{
			if(!pStatObj->m_szGeomName.empty())
				Warning("Object not deleted: %s / %s", pStatObj->m_szFileName.c_str(), pStatObj->m_szGeomName.c_str());
			else
				Warning("Object not deleted: %s", pStatObj->m_szFileName.c_str());
		}

		if(bDeleteAll)
			delete pStatObj;
	}

  {
    uint32 nObjCount=0;
    GetMatMan()->GetLoadedMaterials(0,nObjCount);

    if(nObjCount>0)
    {
			Warning("CObjManager::CheckMaterialLeaks: %d materials(s) found in memory", nObjCount);

      std::vector<IMaterial *> Materials;

      Materials.resize(nObjCount);

      GetMatMan()->GetLoadedMaterials(&Materials[0], nObjCount);

      for(int i=0; i<nObjCount; i++)
      {
        Warning("Material not deleted: %s", Materials[i]->GetName());
      }
    }
  }

	if(bDeleteAll)
		m_lstLoadedObjects.clear();
}

void CObjManager::UnloadVegetationModels()
{
  //for(int i=0; i<m_lstStaticTypes.size(); i++)
  //{
  //	if( m_lstStaticTypes[i].GetStatObj() )
  //	{
  //		m_lstStaticTypes[i].GetStatObj()->Release();
  //		memset(&m_lstStaticTypes[i], 0, sizeof(m_lstStaticTypes[i]));
  //	}
  //}

  m_lstStaticTypes.clear();
}

void CObjManager::UnloadObjects(bool bDeleteAll)
{
  UnloadVegetationModels();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Create / delete object
//////////////////////////////////////////////////////////////////////////////////////////////////////////

CStatObj * CObjManager::LoadStatObj( const char *__szFileName,const char *_szGeomName,IStatObj::SSubObject **ppSubObject,bool bLoadLater )
{	
  LOADING_TIME_PROFILE_SECTION(GetSystem());

	if (ppSubObject)
		*ppSubObject = NULL;

	if(!strcmp(__szFileName,"NOFILE"))
	{ // make ampty object to be filled from outside
		CStatObj * pObject = new CStatObj();
		m_lstLoadedObjects.insert(pObject);
		return pObject;
	}

  // Normalize file name
	char sFilename[_MAX_PATH];
	strcpy( sFilename,__szFileName );
	std::replace( sFilename,sFilename+strlen(sFilename),'\\','/' ); // To Unix Path

	bool bForceBreakable = false;
	if (_szGeomName && !strcmp(_szGeomName,"#ForceBreakable"))
	{
		bForceBreakable = true;
		_szGeomName = 0;
	}

	// Try to find already loaded object
	CStatObj *pObject = stl::find_in_map( m_nameToObjectMap,CONST_TEMP_STRING(sFilename),NULL );
	if (pObject)
	{
		if(_szGeomName && _szGeomName[0])
		{
			// Return SubObject.
			CStatObj::SSubObject *pSubObject = pObject->FindSubObject( _szGeomName );
			if (!pSubObject || !pSubObject->pStatObj)
				return 0;
			if (pSubObject->pStatObj)
			{
				if (ppSubObject)
					*ppSubObject = pSubObject;
				return (CStatObj*)pSubObject->pStatObj;
			}
		}
		return pObject;
	}

  { // loading screen update
    static float fLastTime = 0;

    float fTime = GetTimer()->GetAsyncCurTime();

    if( fabs(fTime - fLastTime) > 0.5f )
    {
      GetLog()->UpdateLoadingScreen(0);
      fLastTime = fTime;
    }
  }

	// Load new CGF
	pObject = new CStatObj();
  if (!pObject->LoadCGF( sFilename, false, bForceBreakable ))
	{ 
		// object not found
		// if geom name is specified - just return 0
    if(_szGeomName && _szGeomName[0]) 
		{
			delete pObject; 
      return 0;
		}

		delete pObject; 
		
		if(m_pDefaultCGF)
			m_pDefaultCGF->AddRef();
		return m_pDefaultCGF;
  }

  // now try to load lods
	pObject->LoadLowLODs(bLoadLater);

	m_lstLoadedObjects.insert(pObject);
	m_nameToObjectMap[pObject->m_szFileName] = pObject;

	if(_szGeomName && _szGeomName[0])
	{
		// Return SubObject.
		CStatObj::SSubObject *pSubObject = pObject->FindSubObject( _szGeomName );
		if (!pSubObject || !pSubObject->pStatObj)
			return 0;
		if (pSubObject->pStatObj)
		{
			if (ppSubObject)
				*ppSubObject = pSubObject;
			return (CStatObj*)pSubObject->pStatObj;
		}
	}

	return pObject;
}

//////////////////////////////////////////////////////////////////////////
bool CObjManager::InternalDeleteObject( CStatObj *pObject )
{
	assert(pObject);

	if (!m_bLockCGFResources && !IsResourceLocked(pObject->m_szFileName))
	{
		LoadedObjects::iterator it = m_lstLoadedObjects.find(pObject);
		if (it != m_lstLoadedObjects.end())
		{
			m_lstLoadedObjects.erase(it);
			m_nameToObjectMap.erase( pObject->m_szFileName );
		}
		else
		{
			//Warning( "CObjManager::ReleaseObject called on object not loaded in ObjectManager %s",pObject->m_szFileName.c_str() );
			//return false;
		}

		if (!pObject->m_bForInternalUse && pObject->m_szFileName[0])
		{
			if (!pObject->GetCloneSourceObject() && !pObject->GetParentObject())
			{
				CryLog( "Unloading Object: %s  %s Ptr=0x%x",pObject->m_szFileName.c_str(), pObject->m_szGeomName.c_str(), (uint32)(UINT_PTR)pObject );
			}
		}

		delete pObject;
		return true;
	}
	else if (m_bLockCGFResources)
	{
		// Put them to locked stat obj list.
		stl::push_back_unique( m_lockedObjects,pObject );
	}

	return false;
}

CObjManager::CObjManager():
	m_pDefaultCGF (NULL),
	m_decalsToPrecreate()
{
	m_pObjManager = this;
  m_fZoomFactor=1;

  m_REFarTreeSprites = (CREFarTreeSprites*)GetRenderer()->EF_CreateRE(eDATA_FarTreeSprites);
  m_vSkyColor.Set(0,0,0);
	m_fSkyBrightness = 0;
	m_fSunSkyRel = 0;
  m_vSunColor.Set(0,0,0);
	m_fILMul = 1.0f;
	m_fSkyBrightMul = 0.3f;
  m_fSSAOAmount = 1.f;
  
	m_fMaxViewDistanceScale=1.f;
	m_fGSMMaxDistance = 0;

	if( GetRenderer()->GetFeatures() & RFT_OCCLUSIONTEST )
		m_pShaderOcclusionQuery = GetRenderer()->EF_LoadShader("OcclusionTest");
	else
		m_pShaderOcclusionQuery = 0;

	// prepare default object
	m_pDefaultCGF = LoadStatObj("objects/default.cgf");
	if(!m_pDefaultCGF)
	{
		Error("CObjManager::LoadStatObj: Default object not found");
		m_pDefaultCGF = new CStatObj();
	}

	m_pDefaultCGF->m_bDefaultObject = true;
	m_bLockCGFResources = false;
	m_pDefaultCGF->AddRef();

	m_pRMBox = NULL;
	MakeUnitCube();

	m_decalsToPrecreate.reserve( 128 );
	m_fOcclTimeRatio = 1.f;
}

// make unit box for occlusion test
void CObjManager::MakeUnitCube()
{
	struct_VERTEX_FORMAT_P3F arrVerts[8];
	arrVerts[0].xyz = Vec3(0,0,0);
	arrVerts[1].xyz = Vec3(1,0,0);
	arrVerts[2].xyz = Vec3(0,0,1);
	arrVerts[3].xyz = Vec3(1,0,1);
	arrVerts[4].xyz = Vec3(0,1,0);
	arrVerts[5].xyz = Vec3(1,1,0);
	arrVerts[6].xyz = Vec3(0,1,1);
	arrVerts[7].xyz = Vec3(1,1,1);

	//		6-------7
	//	 /		   /|
	//	2-------3	|
	//	|	 			|	|
	//	|	4			| 5
	//	|				|/
	//	0-------1

	ushort arrIndices[36];
	typedef Vec3_tpl<ushort> Vec3us;
	int i=0;
	// front + back
	memcpy(&arrIndices[i], &Vec3us(1,0,2)[0], sizeof(Vec3us)); i+=3;
	memcpy(&arrIndices[i], &Vec3us(2,3,1)[0], sizeof(Vec3us)); i+=3;
	memcpy(&arrIndices[i], &Vec3us(5,6,4)[0], sizeof(Vec3us)); i+=3;
	memcpy(&arrIndices[i], &Vec3us(5,7,6)[0], sizeof(Vec3us)); i+=3;
	
	// left + right
	memcpy(&arrIndices[i], &Vec3us(0,6,2)[0], sizeof(Vec3us)); i+=3;
	memcpy(&arrIndices[i], &Vec3us(0,4,6)[0], sizeof(Vec3us)); i+=3;
	memcpy(&arrIndices[i], &Vec3us(1,3,7)[0], sizeof(Vec3us)); i+=3;
	memcpy(&arrIndices[i], &Vec3us(1,7,5)[0], sizeof(Vec3us)); i+=3;

	// top + bottom
	memcpy(&arrIndices[i], &Vec3us(3,2,6)[0], sizeof(Vec3us)); i+=3;
	memcpy(&arrIndices[i], &Vec3us(6,7,3)[0], sizeof(Vec3us)); i+=3;
	memcpy(&arrIndices[i], &Vec3us(1,4,0)[0], sizeof(Vec3us)); i+=3;
	memcpy(&arrIndices[i], &Vec3us(1,5,4)[0], sizeof(Vec3us)); i+=3;

	if(m_pRMBox)
		GetRenderer()->DeleteRenderMesh(m_pRMBox);
	m_pRMBox = GetRenderer()->CreateRenderMeshInitialized(arrVerts, 
                                                        8, 
                                                        VERTEX_FORMAT_P3F, 
                                                        arrIndices, 
                                                        i, 
                                                        R_PRIMV_TRIANGLES, 
                                                        "OcclusionQueryCube","OcclusionQueryCube", 
                                                        eBT_Static);

	m_pRMBox->SetChunk(NULL, 0, 8, 0, i, 0, true);
}

CObjManager::~CObjManager()
{
	// free default object
	m_pDefaultCGF->Release();
	m_pDefaultCGF=0;

	// free brushes
/*  assert(!m_lstBrushContainer.Count());
	for(int i=0; i<m_lstBrushContainer.Count(); i++)
	{
    if(m_lstBrushContainer[i]->GetEntityStatObj(0))
      ReleaseObject((CStatObj*)m_lstBrushContainer[i]->GetEntityStatObj(0));
		delete m_lstBrushContainer[i];
	}
	m_lstBrushContainer.Reset();
*/
  UnloadObjects(true);
	
	assert(m_lstLoadedObjects.size() == 0);

  m_REFarTreeSprites->Release();
}


// mostly xy size
float CObjManager::GetXYRadius(int type)
{
  if((m_lstStaticTypes.size()<=type || !m_lstStaticTypes[type].pStatObj))
    return 0;

  Vec3 vSize = m_lstStaticTypes[type].pStatObj->GetBoxMax() - m_lstStaticTypes[type].pStatObj->GetBoxMin();
  vSize.z *= 0.5;

  float fRadius = m_lstStaticTypes[type].pStatObj->GetRadius();
  float fXYRadius = vSize.GetLength()*0.5f;

  return fXYRadius;
}

bool CObjManager::GetStaticObjectBBox(int nType, Vec3 & vBoxMin, Vec3 & vBoxMax)
{
  if((m_lstStaticTypes.size()<=nType || !m_lstStaticTypes[nType].pStatObj))
    return 0;

  vBoxMin = m_lstStaticTypes[nType].pStatObj->GetBoxMin();
  vBoxMax = m_lstStaticTypes[nType].pStatObj->GetBoxMax();
  
  return true;
}


void CObjManager::AddDecalToRenderer( IMaterial* pMat,
																			const int nDynLMask,
																			const uint8 sortPrio,
																			Vec3 right,
																			Vec3 up,
																			const UCol& ucResCol,
																			const EParticleBlendType eBlendType,
																			const Vec3& vAmbientColor,
																			Vec3 vPos,
																			const int nAfterWater,
																			CVegetation* pVegetation )
{
	FUNCTION_PROFILER_3DENGINE;

  Vec2 vFinalBending = (pVegetation && pVegetation->m_pRNTmpData) ? pVegetation->m_pRNTmpData->userData.m_vFinalBending : Vec2(0,0);

	if(pVegetation && !!vFinalBending)
	{ // transfer decal into object space
		Matrix34 objMat;
		IStatObj * pEntObject = pVegetation->GetEntityStatObj(0, 0, &objMat);
		assert(pEntObject);
		if(pEntObject)
		{
			objMat.Invert();
			vPos = objMat.TransformPoint(vPos);
			right = objMat.TransformVector(right);
			up = objMat.TransformVector(up);
		}
	}

	// calculate render state
	uint nRenderState=0;
	uchar nRefAlpha = 0;
	switch(eBlendType)
	{
	case ParticleBlendType_AlphaBased:
		nRenderState = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
		break;
	case ParticleBlendType_ColorBased:
		nRenderState = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCCOL;
		break;
	case ParticleBlendType_Additive:
		nRenderState = GS_BLSRC_ONE | GS_BLDST_ONE;
		break;
	}

	// repeated objects are free imedeately in renderer
	CRenderObject* pOb( GetIdentityCRenderObject() );		
  if (!pOb)
    return;

	if (pVegetation && !!vFinalBending)
	{
		pVegetation->GetEntityStatObj(0, 0, &pOb->m_II.m_Matrix);
		pOb->m_ObjFlags	|= FOB_TRANS_MASK;
		CStatObj * pBody = m_lstStaticTypes[pVegetation->m_nObjectTypeID].GetStatObj();
		assert(pBody);
		if(pVegetation && pBody && !!vFinalBending)
			pBody->SetupBending(pOb, vFinalBending);
	}

	// prepare render object
	pOb->m_DynLMMask = nDynLMask;
	pOb->m_nTextureID = -1;
	pOb->m_nTextureID1 = -1;
	//pOb->m_RenderObjectColor = ColorF( (float) ucResCol.bcolor[0] / 255.f, (float) ucResCol.bcolor[1] / 255.f, (float) ucResCol.bcolor[2] / 255.f, (float) ucResCol.bcolor[3] / 255.f );
  pOb->m_fAlpha = (float)ucResCol.bcolor[3] / 255.f;
	pOb->m_II.m_AmbColor = vAmbientColor;
	pOb->m_RState = nRenderState;
	pOb->m_AlphaRef = nRefAlpha;
	pOb->m_fSort = 0; 
	pOb->m_ObjFlags |= FOB_DECAL | FOB_NO_Z_PASS | FOB_INSHADOW;
	//float radius( (right + up ).GetLength() );
	//pOb->m_pShadowCasters = GetShadowFrustumsList( 0, AABB( vPos - Vec3( radius, radius, radius ), vPos + Vec3( radius, radius, radius ) ), 0.0f );
	pOb->m_nRAEId = sortPrio;

	uint32 startVertexIndex( 0 );			
	struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F pVerts[4];
	SPipTangents pTangents[4];
	uint16 pIndices[6];

	// TODO: determine whether this is a decal on opaque or transparent geometry 
	// (put it in the respective renderlist for correct shadowing)
	int32 sortValue( EFSLIST_GENERAL ); 

	// fill general vertex data
	pVerts[0].xyz = (-right-up) + vPos;
	pVerts[0].st[0] = 0; 
	pVerts[0].st[1] = 1;
  pVerts[0].color.dcolor = ~0;

	pVerts[1].xyz = ( right-up) + vPos;
	pVerts[1].st[0] = 1; 
	pVerts[1].st[1] = 1;
  pVerts[1].color.dcolor = ~0;

  pVerts[2].xyz = ( right+up) + vPos;
	pVerts[2].st[0] = 1; 
	pVerts[2].st[1] = 0;
  pVerts[2].color.dcolor = ~0;

  pVerts[3].xyz = (-right+up) + vPos;
	pVerts[3].st[0] = 0; 
	pVerts[3].st[1] = 0;
  pVerts[3].color.dcolor = ~0;

	// prepare tangent space (tangent, binormal) and fill it in
	Vec3 rightUnit( right.GetNormalized() );
	Vec3 upUnit( up.GetNormalized() );
	Vec4sf binormal( tPackF2B( -upUnit.x ), tPackF2B( -upUnit.y ), tPackF2B( -upUnit.z ), tPackF2B( 1 ) );
	Vec4sf tangent( tPackF2B( rightUnit.x ), tPackF2B( rightUnit.y ), tPackF2B( rightUnit.z ), tPackF2B( -1 ) );				
	pTangents[0].Tangent = tangent;
	pTangents[0].Binormal = binormal;
	pTangents[1].Tangent = tangent;
	pTangents[1].Binormal = binormal;
	pTangents[2].Tangent = tangent;
	pTangents[2].Binormal = binormal;
	pTangents[3].Tangent = tangent;
	pTangents[3].Binormal = binormal;

	// fill decals topology (two triangles)
	pIndices[ 0 ] = 0;
	pIndices[ 1 ] = 1;
	pIndices[ 2 ] = 2;	

	pIndices[ 3 ] = 0;
	pIndices[ 4 ] = 2;
	pIndices[ 5 ] = 3;

  GetRenderer()->EF_AddPolygonToScene( pMat->GetShaderItem(), 4, pVerts, pTangents, pOb, pIndices, 6, nAfterWater );
}


void CObjManager::GetMemoryUsage(class ICrySizer * pSizer)
{
	int nSize = sizeof(*this);
	
	nSize += sizeofVector(m_lstFarObjects[0]);
	nSize += sizeofVector(m_lstFarObjects[1]);
/*
  {
    SIZER_COMPONENT_NAME(pSizer, "Loaded StatObj");

	  nSize += m_lstLoadedObjects.size()*sizeof(CStatObj*);
	  for (LoadedObjects::iterator it = m_lstLoadedObjects.begin(); it != m_lstLoadedObjects.end(); ++it)
	  {
		  ((CStatObj*)(*it))->GetMemoryUsage(pSizer);
	  }
  }
*/
  if(PodArray<CStatObj*> * pObjects = CStatObj::GetCounter())
  {
    SIZER_COMPONENT_NAME(pSizer, "StatObj");

    for(int i=0; i<pObjects->Count(); i++)
    {
      CStatObj * pMesh = pObjects->GetAt(i);
      pMesh->GetMemoryUsage(pSizer);
    }
  }

  if(PodArray<CMesh*> * pObjects = CMesh::GetCounter())
  {
    SIZER_COMPONENT_NAME(pSizer, "Mesh");

    for(int i=0; i<pObjects->Count(); i++)
    {
      CMesh * pMesh = pObjects->GetAt(i);
      pSizer->AddObject(pMesh,pMesh->GetMemoryUsage());
    }
  }

  if(PodArray<CVoxelObject*> * pObjects = CVoxelObject::GetCounter())
  {
    SIZER_COMPONENT_NAME(pSizer, "VoxelObject");

    for(int i=0; i<pObjects->Count(); i++)
    {
      CVoxelObject * pMesh = pObjects->GetAt(i);
      pMesh->GetMemoryUsage(pSizer);
    }
  }

	nSize += capacityofArray(m_lstStaticTypes);
	
	pSizer->AddObject((void *)this,nSize);
}

void CObjManager::ReregisterEntitiesInArea(Vec3 vBoxMin, Vec3 vBoxMax)
{
	PodArray<SRNInfo> lstEntitiesInArea;

  AABB vBoxAABB(vBoxMin, vBoxMax);

	if(Get3DEngine()->m_pObjectsTree)
		Get3DEngine()->m_pObjectsTree->MoveObjectsIntoList(&lstEntitiesInArea, &vBoxAABB, true);
  
  if(GetVisAreaManager())
    GetVisAreaManager()->MoveObjectsIntoList(&lstEntitiesInArea, vBoxAABB, true);
  
	int nChanged=0;
	for(int i=0; i<lstEntitiesInArea.Count(); i++)
	{
		IVisArea * pPrevArea = lstEntitiesInArea[i].pNode->GetEntityVisArea();
		Get3DEngine()->UnRegisterEntity(lstEntitiesInArea[i].pNode); 
		Get3DEngine()->RegisterEntity(lstEntitiesInArea[i].pNode);
		if(pPrevArea != lstEntitiesInArea[i].pNode->GetEntityVisArea())
			nChanged++;
	}
}

/*
int CObjManager::CountPhysGeomUsage(CStatObj * pStatObjToFind)
{
  int nRes=0;
	for(int i=0; i<m_lstBrushContainer.Count(); i++)
	{
		CBrush * pBrush = m_lstBrushContainer[i];
		IStatObj * pStatObj = pBrush->GetEntityStatObj(0);
//    assert(((CStatObj*)pStatObj)->m_bStreamable);
		if(pStatObjToFind == pStatObj)
    {
      if(pBrush->GetPhysGeomId(0)>=0 || pBrush->GetPhysGeomId(1)>=0)
        nRes++;
    }
	}

  return nRes;
}*/

void CObjManager::FreeNotUsedCGFs()
{
	//assert(!m_bLockCGFResources);
	m_lockedObjects.clear();

	if (!m_bLockCGFResources)
	{
		//Timur, You MUST use next here, or with erase you invalidating
		LoadedObjects::iterator next;
		for (LoadedObjects::iterator it = m_lstLoadedObjects.begin(); it != m_lstLoadedObjects.end(); it = next)
		{
			next = it; next++;
			CStatObj* p = (CStatObj*)(*it);
			if(p->m_nUsers<=0)
			{
				PrintMessage("Object unloaded: %s  %s",p->m_szFileName.c_str(), p->m_szGeomName.c_str());
				m_lstLoadedObjects.erase(it);
				m_nameToObjectMap.erase(p->m_szFileName);

				delete p;
			}
		}
	}

	ClearStatObjGarbage();
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::GetLoadedStatObjArray( IStatObj** pObjectsArray,int &nCount )
{
	if (!pObjectsArray)
	{
		nCount = m_lstLoadedObjects.size();
		return;
	}

	CObjManager::LoadedObjects::iterator it = m_lstLoadedObjects.begin();
	for (int i = 0; i < nCount && it != m_lstLoadedObjects.end(); i++,it++)
	{
		pObjectsArray[i] = *it;
	}
}

void StatInstGroup::Update(CVars * pCVars, int nGeomDetailScreenRes)
{
	m_dwRndFlags = 0;
	if(bCastShadow)
		m_dwRndFlags |= ERF_CASTSHADOWMAPS;
	if(bPrecShadow)
		m_dwRndFlags |= ERF_CASTSHADOWINTORAMMAP;
	if(bHideability)
		m_dwRndFlags |= ERF_HIDABLE;
	if(bHideabilitySecondary)
		m_dwRndFlags |= ERF_HIDABLE_SECONDARY;
	if(bPickable)
		m_dwRndFlags |= ERF_PICKABLE;
  if(bUseTerrainColor)
    m_dwRndFlags |= ERF_USE_TERRAIN_COLOR;
  if(bAffectedByVoxels)
    m_dwRndFlags |= ERF_AFFECTED_BY_VOXELS;

	uint32 nSpec = (uint32)minConfigSpec;
	if (nSpec != 0)
	{
		m_dwRndFlags &= ~ERF_SPEC_BITS_MASK;
		m_dwRndFlags |= (nSpec << ERF_SPEC_BITS_SHIFT) & ERF_SPEC_BITS_MASK;
	}

	if(GetStatObj())
	{
		m_fSpriteSwitchDistUnScaled = 18.f * GetStatObj()->GetRadiusVert() * 
      max(pCVars->e_vegetation_sprites_distance_custom_ratio_min, fSpriteDistRatio);

		m_fSpriteSwitchDistUnScaled *= pCVars->e_vegetation_sprites_distance_ratio;

    m_fSpriteSwitchDistUnScaled *= max(1.f, (float)nGeomDetailScreenRes / 1024.f);

		if(m_fSpriteSwitchDistUnScaled < pCVars->e_vegetation_sprites_min_distance)
			m_fSpriteSwitchDistUnScaled = pCVars->e_vegetation_sprites_min_distance;
	}
}

bool CObjManager::SphereRenderMeshIntersection(IRenderMesh * pRenderMesh, const Vec3 & vInPos, const float fRadius)
{
  FUNCTION_PROFILER_3DENGINE;

  // get position offset and stride
  int nPosStride = 0;
  byte * pPos  = pRenderMesh->GetStridedPosPtr(nPosStride);

  // get indices
  ushort *pInds = pRenderMesh->GetIndices(0);
  int nInds = pRenderMesh->GetSysIndicesCount();
  assert(nInds%3 == 0);

  // test tris
  PodArray<CRenderChunk> *	pChunks = pRenderMesh->GetChunks();
  for(int nChunkId=0; nChunkId<pChunks->Count(); nChunkId++)
  {
    CRenderChunk * pChunk = pChunks->Get(nChunkId);
    if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
      continue;

    // skip transparent and alpha test
    if(pRenderMesh->GetMaterial())
    {
      const SShaderItem &shaderItem = pRenderMesh->GetMaterial()->GetShaderItem(pChunk->m_nMatID);
      if (!shaderItem.m_pShader || shaderItem.m_pShader->GetFlags2() & EF2_NODRAW)
        continue;
    }

    int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
    for(int i=pChunk->nFirstIndexId; i<nLastIndexId; i+=3)
    {
      assert(pInds[i+0]<pRenderMesh->GetVertCount());
      assert(pInds[i+1]<pRenderMesh->GetVertCount());
      assert(pInds[i+2]<pRenderMesh->GetVertCount());

      // get triangle vertices
      Vec3 & v0 = *((Vec3*)&pPos[nPosStride*pInds[i+0]]);
      Vec3 & v1 = *((Vec3*)&pPos[nPosStride*pInds[i+1]]);
      Vec3 & v2 = *((Vec3*)&pPos[nPosStride*pInds[i+2]]);

      AABB triBox;
      triBox.min = v0;
      triBox.max = v0;
      triBox.Add(v1);
      triBox.Add(v2);

      if(	Overlap::Sphere_AABB(Sphere(vInPos,fRadius), triBox) )
        return true;
    }
  }

  return false;
}

void CObjManager::CullLightsPerTriangle(IRenderMesh * pRenderMesh, uint & nInMask, CRNTmpData * pRNTmpData)
{ // cull lights per triangle for big objects
  int nOutMask = 0;

  PodArray<CDLight> * pSources = Get3DEngine()->GetDynamicLightSources();
  for(int nLight=0; nLight<Get3DEngine()->GetRealLightsNum(); nLight++)
  {
    CDLight * pLight = pSources->Get(nLight);

    if((pLight->m_Id>=0) && (nInMask & (1<<pLight->m_Id)))
    {
      if(pLight->m_Flags & DLF_SUN)
      {
        nOutMask |= (1<<pLight->m_Id);
      }
      else
      {
        CRNTmpData::SRNUserData * pUserData = &pRNTmpData->userData;

        Vec3 vLocalLightPos = pUserData->objMat.GetInverted().TransformPoint(pLight->m_Origin);
        float fMatScale = pUserData->objMat.GetColumn0().GetLength();

        SLightInfo li;
        li.vPos = vLocalLightPos;
        li.fRadius = pLight->m_fRadius/fMatScale;

        int nFind=LIGHTS_CULL_INFO_CACHE_SIZE-1;
        for(; nFind>=0; nFind--)
          if(li == pUserData->lightsCache[nFind])
            break;

        if(nFind>=0)
          li.bAffecting = pUserData->lightsCache[nFind].bAffecting;
        else
        {
          li.bAffecting = CObjManager::SphereRenderMeshIntersection(pRenderMesh, li.vPos, li.fRadius);

          memcpy(&pUserData->lightsCache[0], &pUserData->lightsCache[1],
            sizeof(pUserData->lightsCache)-sizeof(pUserData->lightsCache[0]));

          pUserData->lightsCache[LIGHTS_CULL_INFO_CACHE_SIZE-1] = li;
        }

        if(li.bAffecting)
          nOutMask |= (1<<pLight->m_Id);
      }
    }
  }

  nInMask = nOutMask;
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::CheckForGarbage( CStatObj *pStatObj,bool bAdd )
{
	if (bAdd)
	{
		pStatObj->m_bCheckGarbage = true;
		m_checkForGarbage.push_back(pStatObj);
	}
	else if (pStatObj->m_bCheckGarbage)
	{
		m_checkForGarbage.erase( std::find(m_checkForGarbage.begin(),m_checkForGarbage.end(),pStatObj) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::ClearStatObjGarbage()
{
	for (int i = 0,num = m_checkForGarbage.size(); i < num; i++)
	{
		CStatObj *pStatObj = m_checkForGarbage[i];
		pStatObj->m_bCheckGarbage = false;
		
		// Check if it must be released.
		int nChildRefs = pStatObj->CountChildReferences();
		if (pStatObj->m_nUsers == 0 && nChildRefs == 0)
		{
			// No one needs this stat obj anymore.
			InternalDeleteObject( pStatObj );
		}
	}
	m_checkForGarbage.clear();
}
