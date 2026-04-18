////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   objmanWaterVolumes.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Loading water volumes, prepare water geometry
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
/*
#include "WaterVolumes.h"
#include "VisAreas.h"
#include "3dEngine.h"

// Shader params ids.
enum {
	SHP_WATER_FLOW_POS
};

CWaterVolumeManager::CWaterVolumeManager( )
{
	// Add Water Flow Pos shader parameter.
	SShaderParam pr;
	pr.m_Type = eType_FLOAT;
	pr.m_Value.m_Float = 0;
	strcpy( pr.m_Name, "WaterFlowPos" );
	m_shaderParams.Reserve(1);
	m_shaderParams.AddElem(pr);
}

void CWaterVolumeManager::LoadWaterVolumesFromXML(XmlNodeRef pDoc)
{
	// reset old data
	for(int i=0; i<m_lstWaterVolumes.Count(); i++)
	{
		m_lstWaterVolumes[i]->m_lstPoints.Reset();
		GetRenderer()->DeleteRenderMesh(m_lstWaterVolumes[i]->m_pRenderMesh);
		m_lstWaterVolumes[i]->m_pRenderMesh=0;
		delete m_lstWaterVolumes[i];
	}
	m_lstWaterVolumes.Reset();

	// fill list of volumes of shape points
	XmlNodeRef pObjectsNode = pDoc->findChild("Objects");
	if (pObjectsNode)
	{
		for (int i = 0; i < pObjectsNode->getChildCount(); i++)
		{
			XmlNodeRef pNode = pObjectsNode->getChild(i);
			if (pNode->isTag("Object") && strstr(pNode->getAttr("Type"),"WaterVolume"))
			{
				CWaterVolume * pNewVolume = new CWaterVolume( GetRenderer() );
				m_lstWaterVolumes.Add(pNewVolume);
				pNewVolume->m_vBoxMax=SetMinBB();
				pNewVolume->m_vBoxMin=SetMaxBB();

				const char * pName = pNode->getAttr("Name");
				const char * pMatName = pNode->getAttr("Material");

				if (pMatName)
				{
					IMaterial *pMat = GetSystem()->GetI3DEngine()->GetMaterialManager()->LoadMaterial( pMatName );
					pMat->AddRef();
					if (pMat)
						pNewVolume->SetMaterial( pMat );
				}
				
				// set tessellation
				float fTriMinSize = 64.f; pNode->getAttr("TriangleMinSize",fTriMinSize);
				float fTriMaxSize = 64.f; pNode->getAttr("TriangleMaxSize",fTriMaxSize);
				pNewVolume->SetTriSizeLimits(fTriMinSize, fTriMaxSize);

				// get flow speed and AffectToVolFog
				if(pNode->getAttr("WaterSpeed",pNewVolume->m_fFlowSpeed) && pNode->getAttr("AffectToVolFog",pNewVolume->m_bAffectToVolFog) && pName)
				{ // load vertices
					pNewVolume->SetName(pName);

					pNode->getAttr("Height",pNewVolume->m_fHeight);

					XmlNodeRef pPointsNode = pNode->findChild("Points");
					if(pPointsNode)
					for(int i=0;  i<pPointsNode->getChildCount(); i++)
					{
						XmlNodeRef pPointNode = pPointsNode->getChild(i);
						Vec3 vPos;
						if (pPointNode!=NULL && pPointNode->isTag("Point") && pPointNode->getAttr("Pos", vPos))
						{
							pNewVolume->m_lstPoints.Add(vPos);
							pNewVolume->m_vBoxMax.CheckMax(vPos);
							pNewVolume->m_vBoxMin.CheckMin(vPos);
						}
					}
					
					if(	(pNewVolume->m_lstPoints.Last()).GetDistance(pNewVolume->m_lstPoints[0])>0.1f )
						pNewVolume->m_lstPoints.Add(Vec3(pNewVolume->m_lstPoints[0]));

					pNewVolume->UpdateVisArea();
				}
			}
		}
	}
}

void MidVert(const struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F & v1, const struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F & v2, struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F & vRes)
{
  vRes.xyz = 0.5f*v1.xyz + 0.5f*v2.xyz;

  vRes.color.bcolor[0] = uchar(0.5f*v1.color.bcolor[0] + 0.5f*v2.color.bcolor[0]);
  vRes.color.bcolor[1] = uchar(0.5f*v1.color.bcolor[1] + 0.5f*v2.color.bcolor[1]);
  vRes.color.bcolor[2] = uchar(0.5f*v1.color.bcolor[2] + 0.5f*v2.color.bcolor[2]);
  vRes.color.bcolor[3] = uchar(0.5f*v1.color.bcolor[3] + 0.5f*v2.color.bcolor[3]);

  vRes.st[0] = 0.5f*v1.st[0] + 0.5f*v2.st[0];
  vRes.st[1] = 0.5f*v1.st[1] + 0.5f*v2.st[1];
}

bool CWaterVolume::TesselateFace(PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> & lstVerts, PodArray<ushort> & lstIndices, int nFacePos, PodArray<Vec3> & lstDirections)
{
  int n0 = lstIndices[nFacePos+0];
  int n1 = lstIndices[nFacePos+1];
  int n2 = lstIndices[nFacePos+2];

  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F & v0 = lstVerts[n0];
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F & v1 = lstVerts[n1];
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F & v2 = lstVerts[n2];

  // get edge lengts
  float fDist01 = v0.xyz.GetDistance(v1.xyz);
  float fDist12 = v1.xyz.GetDistance(v2.xyz);
  float fDist20 = v2.xyz.GetDistance(v0.xyz);

  float fMaxDist = max(max(fDist01,fDist12), fDist20);
	float fCameraDist=200;
	
	if(fMaxDist == fDist01)
		fCameraDist = GetCamera().GetPosition().GetSquaredDistance((v0.xyz+v1.xyz)*0.5f);
	else if(fMaxDist == fDist12)
		fCameraDist = GetCamera().GetPosition().GetSquaredDistance((v1.xyz+v2.xyz)*0.5f);
	else if(fMaxDist == fDist20)
		fCameraDist = GetCamera().GetPosition().GetSquaredDistance((v2.xyz+v0.xyz)*0.5f);
	else
		assert(0);

	if(fMaxDist<m_fTriMaxSize)
	{
		if(fMaxDist<fCameraDist/25 && m_fTriMinSize<8.f)
			return false;

		if(fMaxDist<m_fTriMinSize)
			return false;
	}

  // delete old face
//	lstIndices.Delete(nFacePos,3);
	lstIndices.DeleteFastUnsorted(nFacePos,3);

  int nNewIndex = lstVerts.Count();

  // tesselate longest
  if(fDist01>fDist12 && fDist01>fDist20)
  { // 01
		struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F v01;
    MidVert(v0, v1, v01);

    lstVerts.Add(v01);
    lstDirections.Add((lstDirections[n0]+lstDirections[n1]).GetNormalized());

    lstIndices.Add(n0);
    lstIndices.Add(nNewIndex);
    lstIndices.Add(n2);

    lstIndices.Add(n2);
    lstIndices.Add(nNewIndex);
    lstIndices.Add(n1);
  }
  else
  if(fDist12>fDist01 && fDist12>fDist20)
  { // 12
    struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F v12;
		MidVert(v1, v2, v12);

    lstVerts.Add(v12);
    lstDirections.Add((lstDirections[n1]+lstDirections[n2]).GetNormalized());

    lstIndices.Add(n1);
    lstIndices.Add(nNewIndex);
    lstIndices.Add(n0);

    lstIndices.Add(n0);
    lstIndices.Add(nNewIndex);
    lstIndices.Add(n2);
  }
  else
  if(fDist20>fDist12 && fDist20>fDist01)
  { // 20
    struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F v20;
		MidVert(v2, v0, v20);

    lstVerts.Add(v20);
    lstDirections.Add((lstDirections[n2]+lstDirections[n0]).GetNormalized());

    lstIndices.Add(n2);
    lstIndices.Add(nNewIndex);
    lstIndices.Add(n1);

    lstIndices.Add(n1);
    lstIndices.Add(nNewIndex);
    lstIndices.Add(n0);
  }

  return true;
}

void CWaterVolume::TesselateStrip(PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> & lstVerts, PodArray<ushort> & lstIndices, PodArray<Vec3> & lstDirections)
{
	FUNCTION_PROFILER_3DENGINE;

	lstIndices.Clear();

  for(int i=0; i<lstVerts.Count()-2; i++)
  {
    if(i&1)
    {
      lstIndices.Add(i+2);
      lstIndices.Add(i+1);
      lstIndices.Add(i+0);
    }
    else
    {
      lstIndices.Add(i+0);
      lstIndices.Add(i+1);
      lstIndices.Add(i+2);
    }
  }
                    
  for(int i=0; i<lstIndices.Count() && i<10000; i+=3)
    if(TesselateFace(lstVerts, lstIndices, i, lstDirections))
      i-=3;
}

void CWaterVolumeManager::InitWaterVolumes()
{
  for(int i=0; i<m_lstWaterVolumes.Count(); i++)
    m_lstWaterVolumes[i]->CheckForUpdate(false);
}

inline int unmapVtx(int i,int n)
{
	int bOdd = -(i&1);
	return (n-2 & bOdd)+(i>>1^bOdd)-bOdd;
}

void CWaterVolume::CheckForUpdate(bool bMakeLowestLod)
{
	FUNCTION_PROFILER_3DENGINE;

	Vec3 vCenter = (m_vBoxMin + m_vBoxMax) * 0.5f;
	float fRadius = (m_vBoxMin - vCenter).GetLength();

	if( m_fTriMinSize != m_fPrevTriMinSize || m_fTriMaxSize != m_fPrevTriMaxSize ||
		((m_vCurrentCamPos.GetDistance(GetCamera().GetPosition())>2 && (m_fTriMinSize<m_fTriMaxSize))))
	{ // force to rebuild water volume geometry
		m_vCurrentCamPos = GetCamera().GetPosition();
		GetRenderer()->DeleteRenderMesh(m_pRenderMesh);
		m_pRenderMesh=0;
	}

	if(!m_pRenderMesh )
	if(	m_pMaterial )
	if(	m_lstPoints.Count() > 3 )
	if(	m_lstPoints.Last().GetDistance(m_lstPoints[0]) < 1.f )
	{
		m_fPrevTriMaxSize = m_fTriMaxSize;
		m_fPrevTriMinSize = m_fTriMinSize;

		m_vCurrentCamPos = GetCamera().GetPosition();
    // make verts strip list
		struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F tmp;
		PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> lstVertices; 
    m_lstDirections.Clear();

		Vec3 vNormal = (m_lstPoints[1]-m_lstPoints[0]).Cross(m_lstPoints[2]-m_lstPoints[0]);

		for(int p=0; p<m_lstPoints.Count()/2; p++)
		{
			int p2 = m_lstPoints.Count() - p - 2;

			if(p>=p2)
				break;

			// first
			tmp.xyz = m_lstPoints[p]+m_vWaterLevelOffset;
      
      //tmp.normal = Vec3(0.f, 0.f, 1.f);

			tmp.st[0] = (float)p;
			tmp.st[1] = 0.f;
			tmp.color.dcolor = ~0;

			lstVertices.Add(tmp);
			
      // calc water move direction in this point
      Vec3 vDir1 = (p+1>=p2) ? (m_lstPoints[p] - m_lstPoints[p-1]) : (m_lstPoints[p+1] - m_lstPoints[p]);
			m_lstDirections.Add(vDir1.GetNormalized());

			// second
			tmp.xyz = m_lstPoints[p2]+m_vWaterLevelOffset;
			tmp.st[0] = (float)p;
			tmp.st[1] = 1.f;

			lstVertices.Add(tmp);

      Vec3 vDir2 = (p+1>=p2) ? (m_lstPoints[p2] - m_lstPoints[p2+1]) : (m_lstPoints[p2-1] - m_lstPoints[p2]);
			m_lstDirections.Add( vDir2.GetNormalized() );
		}

		if (!m_pPhysArea)
		{
			int i,j,npt=m_lstPoints.Count(),*pidx=new int[(lstVertices.Count()-2)*3];
			Vec3 *pFlows=new Vec3[m_lstPoints.Count()];
			for(i=0; i<lstVertices.Count()-2; i++) 
			{
				pidx[i*3+0]=unmapVtx(i+(i&1)*2,npt); pidx[i*3+1]=unmapVtx(i+1,npt); pidx[i*3+2]=unmapVtx(i+2-(i&1)*2,npt);
				if ((m_lstPoints[pidx[i*3+1]]-m_lstPoints[pidx[i*3]] ^ m_lstPoints[pidx[i*3+2]]-m_lstPoints[pidx[i*3]]).z<0) {
					j=pidx[i*3]; pidx[i*3]=pidx[i*3+2]; pidx[i*3+2]=j;
				}
			}
			for(i=0; i<m_lstPoints.Count() && i<m_lstDirections.Count(); i++)
				pFlows[unmapVtx(i,npt)] = m_lstDirections[i]*m_fFlowSpeed;

			if (m_pPhysArea = GetPhysicalWorld()->AddArea(&m_lstPoints[0],npt,min(0.0f,m_fHeight),max(0.0f,m_fHeight)+10.0f,
					Vec3(ZERO),Quat(),1.0f, Vec3(0,0,1), pidx,lstVertices.Count()-2,pFlows))
			{
				pe_status_pos sp;
				pe_params_buoyancy pb;
				m_pPhysArea->GetStatus(&sp);
				pb.waterPlane.n = sp.q*Vec3(0,0,1);
				pb.waterPlane.origin = m_lstPoints[0]+pb.waterPlane.n*max(0.0f,m_fHeight);
				//pb.waterFlow = sp.q*Vec3(m_fFlowSpeed,0,0);
				m_pPhysArea->SetParams(&pb);
			}
			delete []pFlows; delete []pidx;
		}

    // Tesselate to alllow nice reflections
		PodArray<ushort> lstIndices;
    TesselateStrip(lstVertices, lstIndices, m_lstDirections);

//      for(int i=0; i<lstVertices.Count(); i++)
//      GetRenderer()->DrawLabel(Vec3(lstVertices[i].x,lstVertices[i].y,lstVertices[i].z),4,"%d", i);

		m_pRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
			lstVertices.GetElements(), lstVertices.Count(), VERTEX_FORMAT_P3F_COL4UB_TEX2F, 
			lstIndices.GetElements(), lstIndices.Count(), R_PRIMV_TRIANGLES,
			"WaterVolume",GetName(), eBT_Static);

		m_pRenderMesh->SetChunk(m_pMaterial,
			0,lstVertices.Count(), 0,lstIndices.Count());

		m_pRenderMesh->GetChunks()->Get(0)->m_vCenter = vCenter;
		m_pRenderMesh->GetChunks()->Get(0)->m_fRadius = fRadius;

		m_pRenderMesh->SetBBox(m_vBoxMin, m_vBoxMax);
	}
}

void CWaterVolumeManager::RenderWaterVolumes(bool bOutdoorVisible)
{
	if(!GetCVars()->e_water_volumes || m_nRenderStackLevel)
		return;

	FUNCTION_PROFILER_3DENGINE;
	for(int i=0; i<m_lstWaterVolumes.Count(); i++)
	{
		CWaterVolume * pWaterVolumes = m_lstWaterVolumes[i];

		// check vis
		if(!GetCamera().IsAABBVisible_F( AABB(pWaterVolumes->m_vBoxMin,pWaterVolumes->m_vBoxMax)))
			continue;

    if(GetFrameID()%32==0) // hack
      pWaterVolumes->UpdateVisArea();

		if(m_lstWaterVolumes[i]->m_lstVisAreas.Count())
		{ // water in indoors
			if(!m_lstWaterVolumes[i]->IsWaterVolumeAreasVisible())
				continue; // water area not visible
		}
		else if(!bOutdoorVisible)
			continue; // water area (outdoor) not visible

    // create geometry if not ready
    m_lstWaterVolumes[i]->CheckForUpdate(false);

		if(!m_lstWaterVolumes[i]->m_pRenderMesh)
			continue;

    m_lstWaterVolumes[i]->m_nLastRndFrame = GetFrameID();

		// draw debug
		if(GetCVars()->e_water_volumes==2)
		{
			int nPosStride=0;
			byte * pPos = (byte *)m_lstWaterVolumes[i]->m_pRenderMesh->GetStridedPosPtr(nPosStride);

			for(int p=0; p<m_lstWaterVolumes[i]->m_pRenderMesh->GetSysVertCount(); p++)
			{
				Vec3 vPos = *(Vec3*)&pPos[p*nPosStride];
				GetRenderer()->SetMaterialColor(p==0, p!=0, 0.0f, 1);
				DrawBBox(vPos-Vec3(0.2f,0.2f,0.2f), vPos+Vec3(0.2f,0.2f,0.2f));
				GetRenderer()->DrawLabel(vPos,2,"%d",p);
				DrawLine(vPos+Vec3(0,0,0.1f), vPos+m_lstWaterVolumes[i]->m_lstDirections[p]+Vec3(0,0,0.1f));
			}

			GetRenderer()->SetMaterialColor(1, 1, 0, 1);
			DrawBBox(pWaterVolumes->m_vBoxMin,pWaterVolumes->m_vBoxMax);
		}
		else if(GetCVars()->e_bboxes)
			DrawBBox(pWaterVolumes->m_vBoxMin,pWaterVolumes->m_vBoxMax);

		// draw volume geometry
		CRenderObject * pObject = GetRenderer()->EF_GetObject(true);

		// Assign water flow pos shader param to dynamic object.
		m_shaderParams[SHP_WATER_FLOW_POS].m_Value.m_Float = -m_lstWaterVolumes[i]->m_fFlowSpeed * GetCurTimeSec();
		pObject->m_ShaderParams = &m_shaderParams;

		pObject->m_Matrix.SetIdentity();
		
		// object should have some translation to simplify reflections processing in the renderer
		pObject->m_Matrix.SetTranslation(Vec3(0,0,0.025f));
		pObject->m_ObjFlags |= FOB_TRANS_TRANSLATE;

		uint nDynMask = 0;
		if(m_lstWaterVolumes[i]->m_lstVisAreas.Count())
		{
			nDynMask = (uint)-1;

			// remove sun
			for(int nId=0; nId<32; nId++) 
			{
				if(nDynMask & (1<<nId))
				{
					CDLight * pLight = (CDLight*)GetRenderer()->EF_Query(EFQ_LightSource, nId);
					if(pLight && pLight->m_Flags & DLF_SUN)
					{
						nDynMask = nDynMask & ~(1<<nId);
						break;
					}
				}
			}

			float fRadius = (m_lstWaterVolumes[i]->m_vBoxMax-m_lstWaterVolumes[i]->m_vBoxMin).len()*0.5f;
			Vec3 vCenter = (m_lstWaterVolumes[i]->m_vBoxMin+m_lstWaterVolumes[i]->m_vBoxMax)*0.5f;
			((C3DEngine*)Get3DEngine())->CheckDistancesToLightSources(nDynMask,vCenter,fRadius);
		}
		else
			nDynMask=0;

    pObject->m_fAlpha = 0.99f;
		m_lstWaterVolumes[i]->m_pRenderMesh->AddRenderElements(pObject, m_lstWaterVolumes[i]->m_pMaterial);

    m_lstWaterVolumes[i]->m_pRenderMesh->SetBBox(GetCamera().GetPosition(), GetCamera().GetPosition());
	}

  // release old
	for(int i=0; i<m_lstWaterVolumes.Count(); i++)
	{
		CWaterVolume * pWaterVolumes = m_lstWaterVolumes[i];
    if(pWaterVolumes->m_pRenderMesh && m_lstWaterVolumes[i]->m_nLastRndFrame < GetFrameID()-100)
    {
      m_lstWaterVolumes[i]->CheckForUpdate(true);
	//	  GetRenderer()->DeleteRenderMesh(pWaterVolumes->m_pRenderMesh);
	  //  pWaterVolumes->m_pRenderMesh=0;
    }
  }
}

float CWaterVolumeManager::GetWaterVolumeLevelFor2DPoint(const Vec3 & vPos, Vec3 * pvFlowDir)
{
	float fResLevel = WATER_LEVEL_UNKNOWN;

	// check all volumes bboxes
	for(int i=0; i<m_lstWaterVolumes.Count(); i++)
	{ 
		CWaterVolume * pWaterVolumes = m_lstWaterVolumes[i];
		Vec3 vBoxMin2d = pWaterVolumes->m_vBoxMin;
		Vec3 vBoxMax2d = pWaterVolumes->m_vBoxMax;
		vBoxMin2d.z=WATER_LEVEL_UNKNOWN;
		vBoxMax2d.z=1024;

		if(!pWaterVolumes->m_pRenderMesh)
			pWaterVolumes->CheckForUpdate(true);

		AABB aabb;
		aabb.min=vBoxMin2d;
		aabb.max=vBoxMax2d;
		if(pWaterVolumes->m_pRenderMesh && Overlap::Point_AABB(vPos,aabb))
		{ // if inside bbox
			int nInds = 0;
      ushort *pInds = pWaterVolumes->m_pRenderMesh->GetIndices(&nInds);
			int nPosStride=0;
			const byte * pPos = pWaterVolumes->m_pRenderMesh->GetStridedPosPtr(nPosStride);
			for(int i=0; (i+2)<nInds; i+=3)
			{	// test all triangles of water surface strip
				Vec3 v0 = *(Vec3*)&pPos[nPosStride*pInds[i+0]];
				Vec3 v1 = *(Vec3*)&pPos[nPosStride*pInds[i+1]];
				Vec3 v2 = *(Vec3*)&pPos[nPosStride*pInds[i+2]];
				v0.z = v1.z = v2.z = 0; // make triangle 2d
				Vec3 vPos2d(vPos.x,vPos.y,0);
				if(Overlap::PointInTriangle( vPos2d, v0,v1,v2,Vec3(0,0,1.f)))
				{ // triangle found
					Plane plane;
					plane.SetPlane( // calc plane using real vertices
						*(Vec3*)&pPos[nPosStride*pInds[i+0]],
						*(Vec3*)&pPos[nPosStride*pInds[i+2]],
						*(Vec3*)&pPos[nPosStride*pInds[i+1]]);
					float fDist = plane.DistFromPlane(vPos);
					float fDot = plane.n.Dot(Vec3(0,0,1.f));
					if(fDot>0) 
						fDist = -fDist;

					// check bottom bounds
					if(pWaterVolumes->m_fHeight && fDist > -pWaterVolumes->m_fHeight)
						continue;
						
					if(GetCVars()->e_water_volumes==2)
					{
						PrintMessage("CameraLevel=%.2f, WaterVolumeLevel=%.2f %d", 
							GetCamera().GetPosition().z, fDist, GetFrameID());
						Vec3 vPos = vPos2d+Vec3(0,0,fDist);
//						DrawBBox(vPos-Vec3(0.05f,0.05f,0.05f),vPos+Vec3(0.05f,0.05f,0.05f),DPRIM_SOLID_SPHERE);

					}

					if(pvFlowDir)
					{
						// todo: calculate the barycentric coordinates of the triangle

						float fDist0 = vPos2d.GetDistance(v0);
						float fDist1 = vPos2d.GetDistance(v1);
						float fDist2 = vPos2d.GetDistance(v2);
						float fSumm = fDist0 + fDist1 + fDist2;
						fDist0 /= fSumm;
						fDist1 /= fSumm;
						fDist2 /= fSumm;
						
						*pvFlowDir =	pWaterVolumes->m_lstDirections[pInds[i+0]]*(1.f-fDist0)+
													pWaterVolumes->m_lstDirections[pInds[i+1]]*(1.f-fDist1)+
													pWaterVolumes->m_lstDirections[pInds[i+2]]*(1.f-fDist2);

						if(GetCVars()->e_water_volumes==2)
							DrawLine(vPos, vPos+*pvFlowDir);

            *pvFlowDir *= pWaterVolumes->m_fFlowSpeed;
					}

					if( (vPos.z+fDist) > fResLevel )
						fResLevel = (vPos.z+fDist);
				}
			}
		}
	}

	return fResLevel;
}

IWaterVolume * CWaterVolumeManager::CreateWaterVolume()
{
	CWaterVolume * pNewVolume = new CWaterVolume( GetRenderer() );
	m_lstWaterVolumes.Add(pNewVolume);
	return pNewVolume;
}

void CWaterVolumeManager::DeleteWaterVolume(IWaterVolume * pWaterVolume)
{
	GetRenderer()->DeleteRenderMesh(((CWaterVolume*)pWaterVolume)->m_pRenderMesh);
	((CWaterVolume*)pWaterVolume)->m_pRenderMesh=0;
	if (((CWaterVolume*)pWaterVolume)->m_pPhysArea)
		GetPhysicalWorld()->RemoveArea(((CWaterVolume*)pWaterVolume)->m_pPhysArea);
	delete pWaterVolume;
	m_lstWaterVolumes.Delete((CWaterVolume*)pWaterVolume);
}

void CWaterVolumeManager::UpdateWaterVolumeVisAreas()
{
	for(int i=0; i<m_lstWaterVolumes.Count(); i++)
		m_lstWaterVolumes[i]->UpdateVisArea();
}

void CWaterVolume::UpdatePoints(const Vec3 * pPoints, int nCount, float fHeight)
{
	m_lstPoints.PreAllocate(nCount+1,nCount);

	m_fHeight = fHeight;
	
	if(nCount)
	{
		memcpy(&m_lstPoints[0], pPoints, sizeof(Vec3)*nCount);
		if(	m_lstPoints.Last().GetDistance(m_lstPoints[0])>0.1f )
			m_lstPoints.Add(m_lstPoints[0]); // loop
	}

	// update bbox
	m_vBoxMax = SetMinBB();
	m_vBoxMin = SetMaxBB();

	for(int i=0; i<nCount; i++)
	{
		m_vBoxMax.CheckMax(pPoints[i]);
		m_vBoxMin.CheckMin(pPoints[i]);
	}

	// remake RenderMesh
	m_pRenderer->DeleteRenderMesh(m_pRenderMesh);
	m_pRenderMesh=0;

	if (m_pPhysArea)
		GetPhysicalWorld()->RemoveArea(m_pPhysArea);
	m_pPhysArea = 0;

	UpdateVisArea();
}

CWaterVolumeManager::~CWaterVolumeManager()
{
	for(int i=0; i<m_lstWaterVolumes.Count(); i++)
	{ 
		CWaterVolume * pWaterVolume = m_lstWaterVolumes[i];
		GetRenderer()->DeleteRenderMesh(pWaterVolume->m_pRenderMesh);
		pWaterVolume->m_pRenderMesh=0;
		if (pWaterVolume->m_pPhysArea)
			GetPhysicalWorld()->RemoveArea(pWaterVolume->m_pPhysArea);
		delete pWaterVolume;
	}
  m_lstWaterVolumes.Reset();
}

void CWaterVolume::UpdateVisArea()
{
	m_lstVisAreas.Clear();

	if(!m_lstPoints.Count())
		return;
	
	// scan water volume bbox for vis areas
	const float dx = (m_vBoxMax.x - m_vBoxMin.x)/int(m_vBoxMax.x - m_vBoxMin.x) + 0.1f;
	const float dy = (m_vBoxMax.y - m_vBoxMin.y)/int(m_vBoxMax.y - m_vBoxMin.y) + 0.1f;
//	const float dz = (m_vBoxMax.z - m_vBoxMin.z)/int(m_vBoxMax.z - m_vBoxMin.z) + 0.1f;

	float z = (m_vBoxMin.z+m_vBoxMax.z)*0.5f;
	for(float x = m_vBoxMin.x; x<=m_vBoxMax.x; x+=dx*5)
		for(float y = m_vBoxMin.y; y<=m_vBoxMax.y; y+=dy*5)
//			for(float z = m_vBoxMin.z; z<=m_vBoxMax.z; z+=dz)
	{
		CVisArea * pVisArea = (CVisArea *)Get3DEngine()->GetVisAreaFromPos(Vec3(x,y,z));
		if(pVisArea && m_lstVisAreas.Find(pVisArea)<0)
		{
			m_lstVisAreas.Add(pVisArea);
			if(m_bAffectToVolFog)
				UpdateVisAreaFogVolumeLevel(pVisArea);
		}
	}
}

void CWaterVolume::UpdateVisAreaFogVolumeLevel(CVisArea*pVisArea)
{
}

//////////////////////////////////////////////////////////////////////////
void CWaterVolume::SetMaterial( IMaterial *pMat )
{
	m_pMaterial = pMat;
}

IMaterial * CWaterVolume::GetMaterial()
{
  return m_pMaterial;
}

//////////////////////////////////////////////////////////////////////////
IWaterVolume * CWaterVolumeManager::FindWaterVolumeByName(const char * szName)
{
	for(int i=0; i<m_lstWaterVolumes.Count(); i++)
	{ 
		CWaterVolume * pWaterVolume = m_lstWaterVolumes[i];
		if(!stricmp(pWaterVolume->m_szName, szName))
			return pWaterVolume;
	}

	return 0;
}

void CWaterVolume::SetPositionOffset(const Vec3 & vNewOffset)
{
	m_vWaterLevelOffset = vNewOffset;

	m_vBoxMax = SetMinBB();
	m_vBoxMin = SetMaxBB();

	for(int p=0; p<m_lstPoints.Count(); p++)
	{
		Vec3 vPos = m_lstPoints[p] + m_vWaterLevelOffset;
		m_vBoxMax.CheckMax(vPos);
		m_vBoxMin.CheckMin(vPos);
	}

	m_vBoxMax.z += 0.01f;
	m_vBoxMin.z -= 0.01f;

	GetRenderer()->DeleteRenderMesh(m_pRenderMesh);
	m_pRenderMesh=0;
	if (m_pPhysArea)
		GetPhysicalWorld()->RemoveArea(m_pPhysArea);
	m_pPhysArea = 0;

	UpdateVisArea();
}

bool CWaterVolume::IsWaterVolumeAreasVisible()
{
	assert(m_lstVisAreas.Count());

	// water in indoors
	int v;
	for(v=0; v<m_lstVisAreas.Count(); v++)
		if(abs(((CVisArea*)m_lstVisAreas[v])->m_nRndFrameId - GetFrameID())<2)
			break; // water area is visible
	
	return v<m_lstVisAreas.Count();
}

void CWaterVolume::SetTriSizeLimits(float fTriMinSize, float fTriMaxSize)
{ 
	m_fTriMinSize = max(0.25f,min(fTriMinSize, 64.0f));
	m_fTriMaxSize = max(0.25f,min(fTriMaxSize, 64.0f));
	// remake RenderMesh
	m_pRenderer->DeleteRenderMesh(m_pRenderMesh);
	m_pRenderMesh=0;
}*/