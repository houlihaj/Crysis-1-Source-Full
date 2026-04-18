//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:voxman.cpp
//  voxel tecnology researh
//
//	History:
//	-:Created by Vladimir Kajalin
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "VoxMan.h"
#include "IndexedMesh.h"
#include "3dEngine.h"
#include "StatObj.h"
#include "terrain_sector.h"
#include "terrain.h"
#include "VisAreas.h"
#include "Brush.h"

CVoxelVolume::CVoxelVolume(float fUnitSize, int nSizeXinUnits, int nSizeYinUnits, int nSizeZinUnits)
{
  m_fUnitSize = fUnitSize;

  m_arrVolume.Allocate(nSizeXinUnits, nSizeYinUnits, nSizeZinUnits);
  m_arrVolumeBackup.Allocate(nSizeXinUnits, nSizeYinUnits, nSizeZinUnits);
  m_arrColors.Allocate(nSizeXinUnits, nSizeYinUnits, nSizeZinUnits);

  for(int x=0; x<m_arrVolumeBackup.m_nSizeX; x++)
    for(int y=0; y<m_arrVolumeBackup.m_nSizeY; y++)
      for(int z=0; z<m_arrVolumeBackup.m_nSizeZ; z++)
        m_arrColors.GetAt(x,y,z) = ColorB(127,127,127,255);

  m_bUpdateRequested = false;
  m_nUpdateRequestedFrameId = 0;
}

CVoxelObject::CVoxelObject(Vec3 vOrigin, float fUnitSize, int nSizeXinUnits, int nSizeYinUnits, int nSizeZinUnits)
{
  if(m_bEditorMode)
  {
    m_pVoxelVolume = new CVoxelVolume(fUnitSize, nSizeXinUnits, nSizeYinUnits, nSizeZinUnits);
    m_pVoxelVolume->m_pSrcArea = this;
  }
  else
    m_pVoxelVolume = NULL;

	memset(m_arrSurfacesPalette,0,sizeof(m_arrSurfacesPalette));
	m_pVoxelMesh = NULL;

	m_pPhysEnt = NULL;
	m_pPhysGeom = NULL;

	m_nFlags = 0;

	m_Matrix.SetIdentity();
	m_Matrix.SetTranslation(vOrigin);
	m_vPos = vOrigin;

	m_fCurrDistance=0;

  m_pObjectName = NULL;

	GetInstCount(eERType_VoxelObject)++;

  GetCounter()->Add(this);
}

void CVoxelObject::ResetRenderMeshs()
{
	if(m_pVoxelMesh)
	{
		m_pVoxelMesh->ResetRenderMeshs();
		delete m_pVoxelMesh;
		m_pVoxelMesh = NULL;
	}
}

CVoxelObject::~CVoxelObject()
{
	ResetRenderMeshs();

	DePhysicalize();

	Get3DEngine()->UnRegisterEntity(this);

  delete m_pVoxelVolume;

	Get3DEngine()->m_lstVoxelObjectsForUpdate.Delete(this);

	GetInstCount(eERType_VoxelObject)--;

  GetCounter()->Delete(this);

  Get3DEngine()->FreeRenderNodeState(this);

  ReleaseMemBlocks();

  SAFE_DELETE(m_pObjectName);
}

void CVoxelVolume::RenderDebug(CVoxelObject ** arrNeighbours)
{  // draw actual voxels
  for(int x=0; x<m_arrVolumeBackup.m_nSizeX; x++)
  for(int y=0; y<m_arrVolumeBackup.m_nSizeY; y++)
  for(int z=0; z<m_arrVolumeBackup.m_nSizeZ; z++)
  {
		Vec3 vOSPos((float)m_fUnitSize*x,(float)m_fUnitSize*y,(float)m_fUnitSize*z);
		Vec3 vVoxPos = m_pSrcArea->m_Matrix.TransformPoint(vOSPos);

		if(GetCamera().GetPosition().GetSquaredDistance(vVoxPos)>64)
			continue;

    if(!GetCamera().IsPointVisible(vVoxPos))
      continue;

		ushort ucVal = m_arrVolumeBackup.GetAt(x,y,z) & ~VOX_MAT_MASK;
		ushort ucSurfId = m_arrVolumeBackup.GetAt(x,y,z) & VOX_MAT_MASK;

    float fOut = ucVal>VOX_ISO_LEVEL;
		float arrColor[] = {fOut,1-fOut,1,1};

    GetRenderer()->DrawLabelEx(vVoxPos, 1, arrColor, false, true, "%d", (int)ucVal);//, (int)ucSurfId);
    ColorF col(arrColor[0],arrColor[1],arrColor[2],1);
    DrawSphere(vVoxPos, 0.05f, col);
  }
}

void CVoxelObject::Compile(CVoxelObject ** pNeighbours)
{
  ResetRenderMeshs();

  if(GetCVars()->e_voxel_make_physics)
    DePhysicalize();

  if(!InitMaterials())
    return;

  PrintMessage("Compiling voxel object %s (%d,%d,%d) ...", 
    m_pObjectName ? m_pObjectName : "", (int)m_vPos.x, (int)m_vPos.y, (int)m_vPos.z);

  memset(m_arrUsedSTypes,0,sizeof(m_arrUsedSTypes));

  CIndexedMesh * pMesh = m_pVoxelVolume->Compile(pNeighbours);
  if(!pMesh)
    return;

  for(ushort ucSurfaceTypeId=0; ucSurfaceTypeId<VOX_MAX_SURF_TYPES_NUM; ucSurfaceTypeId++)
    if(!m_arrUsedSTypes[ucSurfaceTypeId])
      m_arrSurfacesPalette[ucSurfaceTypeId] = NULL;

  delete m_pVoxelMesh; m_pVoxelMesh = NULL;
  m_pVoxelMesh = new CVoxelMesh(&m_arrSurfacesPalette[0]);

  m_pVoxelVolume->SerializeRenderMeshs(this, pMesh);

  delete pMesh;

  m_pVoxelMesh->MakeRenderMeshsFromMemBlocks(this);

  m_WSBBox = m_pVoxelVolume->GetAABB();
}

CIndexedMesh * CVoxelVolume::Compile(CVoxelObject ** pNeighbours)
{
	FUNCTION_PROFILER_3DENGINE;

  LOADING_TIME_PROFILE_SECTION(GetSystem());

  m_bUpdateRequested = false;
  m_nUpdateRequestedFrameId = 0;

	// create list of tris in sector space, material id is also local
	PodArray<TRIANGLE> lstTris;
	GenerateTrianglesFromVoxels(pNeighbours, 0, lstTris);
	if(!lstTris.Count())
		return NULL;

	// compile source indexed mesh from list of triangles
	CIndexedMesh * pIndexedMesh = MakeIndexedMesh(lstTris, pNeighbours);
	if(pIndexedMesh && pIndexedMesh->m_numFaces)
		return pIndexedMesh;
  
  delete pIndexedMesh;
  return NULL;
}

/*
float CVoxelObject::GetVoxelValue(int x, int y, int z, CVoxelObject ** pNeighbours, int nLod)
{
	if(!nLod)
		return GetVoxelValueNoLOD(x, y, z, pNeighbours);

	int nDimHalf = (1<<nLod)/2;
	float fValue = 0, fCounter = 0;
	for(int dx=-nDimHalf;dx<=nDimHalf;dx++)
		for(int dy=-nDimHalf;dy<=nDimHalf;dy++)
			for(int dz=-nDimHalf;dz<=nDimHalf;dz++)
			{
				fValue += GetVoxelValueNoLOD(x+dx, y+dy, z+dz, pNeighbours);
				fCounter++;
			}
	fValue /= fCounter;
	return fValue;
}*/

float CVoxelVolume::GetVoxelValueInterpolated(float x, float y, float z, CVoxelObject ** pNeighbours, int * pMatId, ColorB * pColor, int nScale)
{
#define VOX_EPS VEC_EPSILON

	x /= nScale;
	y /= nScale;
	z /= nScale;

	int nX0 = fastftol_positive(x);
	int nX1 = nX0+1l;
	float fDX = x - nX0;
	assert(fDX>=-VOX_EPS && fDX<=1.f);
	
	int nY0 = fastftol_positive(y);
	int nY1 = nY0+1l;
	float fDY = y - nY0;
	assert(fDY>=-VOX_EPS && fDY<=1.f);

	int nZ0 = fastftol_positive(z);
	int nZ1 = nZ0+1l;
	float fDZ = z - nZ0;
	assert(fDZ>=-VOX_EPS && fDZ<=1.f);

	float arrfValues1D[2];
	float fValue0D;
	int arrnMats1D[2];
	ColorB arrCols1D[2];
	
	for(int nX=0; nX<2; nX++)
	{
		int nMat00, nMat01;
		int nMat10, nMat11;
		ColorB col00, col01;
		ColorB col10, col11;

		float fValue0 = 
			(1.f - fDZ)*GetVoxelValue(nScale*(nX0+nX), nScale*nY0  , nScale*nZ0, pNeighbours, &nMat00, &col00) +
			(			 fDZ)*GetVoxelValue(nScale*(nX0+nX), nScale*nY0  , nScale*nZ1, pNeighbours, &nMat01, &col01);


		float fValue1 = 
			(1.f - fDZ)*GetVoxelValue(nScale*(nX0+nX), nScale*(nY0+1), nScale*nZ0, pNeighbours, &nMat10, &col10) +
			(			 fDZ)*GetVoxelValue(nScale*(nX0+nX), nScale*(nY0+1), nScale*nZ1, pNeighbours, &nMat11, &col11);

		arrfValues1D[nX] = 
			(1.f - fDY)*fValue0 +
			(			 fDY)*fValue1;

		int nMat0 = (fDZ<.5f) ? nMat00 : nMat01;
		int nMat1 = (fDZ<.5f) ? nMat10 : nMat11;
		arrnMats1D[nX] = (fDY<.5f) ? nMat0 : nMat1;

		ColorB col0 = (fDZ<.5f) ? col00 : col01;
		ColorB col1 = (fDZ<.5f) ? col10 : col11;
		arrCols1D[nX] = (fDY<.5f) ? col0 : col1;
	}

	fValue0D = (1.f - fDX)*arrfValues1D[0] +(fDX)*arrfValues1D[1];

	if(pMatId)
		*pMatId = (fDX<.5f) ? arrnMats1D[0] : arrnMats1D[1];

	if(pColor)
		*pColor = (fDX<.5f) ? arrCols1D[0] : arrCols1D[1];

	return fValue0D;
}

bool CVoxelVolume::IsEmpty(CVoxelObject ** pNeighbours)
{
	for(int x=-1; x<=m_arrVolumeBackup.m_nSizeX+1; x++)
		for(int y=-1; y<=m_arrVolumeBackup.m_nSizeY+1; y++)
			for(int z= 0; z<=m_arrVolumeBackup.m_nSizeZ+1; z++)
				if(GetVoxelValue(x,y,z,pNeighbours,NULL,NULL))
					return false;

	return true;
}

ushort CVoxelVolume::GetHeightMapValue(const Vec3 vPos, ushort ucDefaultValue, bool bCheckForHole)
{
	float fTerrZ = Get3DEngine()->GetTerrainElevation(vPos.x,vPos.y);
	float fValue = SATURATE(0.5f - (vPos.z-fTerrZ)/m_fUnitSize/32);	
	ushort ucValue =	ushort(fValue*VOX_MAX_USHORT);
  int x((int)(vPos.x+0.5f));
  int y((int)(vPos.y+0.5f));
	SSurfaceType * pLayer =	GetTerrain()->GetSurfaceTypePtr(x, y);

  if(!pLayer)
  { // hole
    int nGlobalSurfaceTypeId = STYPE_HOLE;
    int nStep = CTerrain::GetHeightMapUnitSize();
    for(int i=-nStep; i<=nStep && (nGlobalSurfaceTypeId == STYPE_HOLE); i+=nStep)
      for(int j=-nStep; j<=nStep && (nGlobalSurfaceTypeId == STYPE_HOLE); j+=nStep)
        nGlobalSurfaceTypeId = GetTerrain()->GetSurfaceTypeID(x+i,y+j);

    if(nGlobalSurfaceTypeId != STYPE_HOLE)
    {
      SSurfaceType * pLayers = GetTerrain()->GetSurfaceTypes();
      pLayer = pLayers[nGlobalSurfaceTypeId].pLayerMat ? &pLayers[nGlobalSurfaceTypeId] : 0;
    }
  }

  int nSurfaceTypeId = pLayer ? GetLocalSurfaceId(pLayer) : 0;

  if(bCheckForHole && !pLayer)
    return 0;

	return (ucValue & ~VOX_MAT_MASK) | (nSurfaceTypeId & VOX_MAT_MASK);
}

void CVoxelVolume::InitVoxelsFromHeightMap(bool bOnlyMaterials)
{
	for(int x=0; x<m_arrVolume.m_nSizeX; x++) 
		for(int y=0; y<m_arrVolume.m_nSizeY; y++) 
			for(int z=0; z<m_arrVolume.m_nSizeZ; z++)
	{

		Vec3 vWSPos = m_pSrcArea->m_Matrix.TransformPoint(Vec3(x,y,z)*m_fUnitSize);

    if(bOnlyMaterials)
      m_arrVolume.GetAt(x,y,z) = (m_arrVolume.GetAt(x,y,z) & ~VOX_MAT_MASK) | (GetHeightMapValue(
				vWSPos, m_arrVolume.GetAt(x,y,z)) & VOX_MAT_MASK);
    else
      m_arrVolume.GetAt(x,y,z) = GetHeightMapValue(vWSPos, m_arrVolume.GetAt(x,y,z));
	}
}

ushort CVoxelVolume::GetVoxelValue(int x, int y, int z, CVoxelObject ** pNeighbours, int * pMatId, ColorB * pColor)
{
	if(pMatId)
		*pMatId = 0;

	if(pColor)
    pColor->r = pColor->g = pColor->b = pColor->a = 0;
	
	ushort ucResult = 0;

	CVoxelObject * pSrcArea = m_pSrcArea;

	bool bOpenBorder = (x >= DEF_VOX_VOLUME_SIZE) || (y >= DEF_VOX_VOLUME_SIZE) || (z >= DEF_VOX_VOLUME_SIZE);

  if(pNeighbours)
  { // find area containing sample point
    int idX, idY, idZ;
    if(x>=m_arrVolumeBackup.m_nSizeX) idX = 2;
    else if(x<1) idX = 0;
    else idX = 1;
    if(y>=m_arrVolumeBackup.m_nSizeY) idY = 2;
    else if(y<1) idY = 0;
    else idY = 1;
    if(z>=m_arrVolumeBackup.m_nSizeZ) idZ = 2;
    else if(z<1) idZ = 0;
    else idZ = 1;
    //assert(pNeighbours[1*9+1*3+1]==this);
    pSrcArea = pNeighbours[idX*9+idY*3+idZ];
  }

  if(!(pSrcArea && m_pSrcArea != pSrcArea))
    bOpenBorder |= (x <= 0) || (y <= 0) || (z <= 0);

	if(pNeighbours && pSrcArea && m_pSrcArea != pSrcArea)
	{ // find area containing sample point
		int idX, idY, idZ;
		if(x>=m_arrVolumeBackup.m_nSizeX) idX = 2;
		else if(x<0) idX = 0;
		else idX = 1;
		if(y>=m_arrVolumeBackup.m_nSizeY) idY = 2;
		else if(y<0) idY = 0;
		else idY = 1;
		if(z>=m_arrVolumeBackup.m_nSizeZ) idZ = 2;
		else if(z<0) idZ = 0;
		else idZ = 1;
		assert(pNeighbours[1*9+1*3+1]==m_pSrcArea);
		pSrcArea = pNeighbours[idX*9+idY*3+idZ];
	}

	if(pSrcArea && m_pSrcArea != pSrcArea)
	{
		if(!pSrcArea->GetEntityVisArea() || (((CVisArea*)pSrcArea->GetEntityVisArea())->m_boxArea.max.z > m_pSrcArea->m_Matrix.TransformPoint(Vec3(x,y,z)*m_fUnitSize).z))
		{
			ucResult = pSrcArea->m_pVoxelVolume->m_arrVolumeBackup.GetAt((unsigned)x%m_arrVolumeBackup.m_nSizeX,(unsigned)y%m_arrVolumeBackup.m_nSizeY,(unsigned)z%m_arrVolumeBackup.m_nSizeZ);
			if(pColor)
				*pColor = pSrcArea->m_pVoxelVolume->m_arrColors.GetAt((unsigned)x%m_arrVolumeBackup.m_nSizeX,(unsigned)y%m_arrVolumeBackup.m_nSizeY,(unsigned)z%m_arrVolumeBackup.m_nSizeZ);
			bOpenBorder = false;
		}
		else
			pSrcArea = NULL;
	}
	else
	{
		ucResult = m_arrVolumeBackup.GetAtClamped(x,y,z);
		if(pColor)
			*pColor = m_arrColors.GetAtClamped(x,y,z);
	}

	if(pMatId)
  {
		*pMatId = ucResult & VOX_MAT_MASK;

	  if(pSrcArea != m_pSrcArea && pSrcArea && ucResult)
	  { // convert mat id into this area mat id
		  SSurfaceType * pLayer = pSrcArea->GetGlobalSurfaceType(*pMatId);
		  *pMatId = pLayer ? GetLocalSurfaceId(pLayer) : 0;
		  assert(m_pSrcArea->m_arrSurfacesPalette[*pMatId]);
	  }

	  if(!m_pSrcArea->m_arrSurfacesPalette[*pMatId] && ucResult)
		  *pMatId=0;
  }

	assert(!pMatId || m_pSrcArea->m_arrSurfacesPalette[*pMatId] || !ucResult);

  if(pSrcArea && !m_pSrcArea->GetEntityVisArea())
    if(m_pSrcArea->m_nFlags&IVOXELOBJECT_FLAG_SNAP_TO_TERRAIN)
      if(!(pSrcArea->m_nFlags&IVOXELOBJECT_FLAG_SNAP_TO_TERRAIN))
        if(x >= (DEF_VOX_VOLUME_SIZE-1) || y >= (DEF_VOX_VOLUME_SIZE-1))
        {
          Vec3 vWSPos = m_pSrcArea->m_Matrix.TransformPoint(Vec3(x,y,z)*m_fUnitSize);
          if(vWSPos.z+1 > GetTerrain()->GetZApr(vWSPos.x,vWSPos.y))
            bOpenBorder = true;
        }

  if( (m_pSrcArea->m_nFlags&IVOXELOBJECT_FLAG_SNAP_TO_TERRAIN) &&
      !m_pSrcArea->GetEntityVisArea() && 
      (m_pSrcArea->m_nFlags&IVOXELOBJECT_FLAG_LINK_TO_TERRAIN || bOpenBorder))
	{ // check terrain value as well
		ushort hmVal = GetHeightMapValue(m_pSrcArea->m_Matrix.TransformPoint(Vec3(x,y,z)*m_fUnitSize), 0, 
      (m_pSrcArea->m_nFlags&IVOXELOBJECT_FLAG_LINK_TO_TERRAIN)==0);

		if(bOpenBorder || (hmVal & ~VOX_MAT_MASK) > (ucResult & ~VOX_MAT_MASK)) // take terrain data here
		{
			if(pMatId)
				*pMatId = hmVal & VOX_MAT_MASK;
			return hmVal | VOX_MAT_MASK;
		}
	}

	return ucResult & ~VOX_MAT_MASK;
}

void CVoxelVolume::FillCellInfo(GRIDCELL & cell, int x, int y, int z, int v, CVoxelObject ** pNeighbours)
{
	cell.val[v] = (float)(GetVoxelValue( x, y, z, pNeighbours, &cell.arrMatId[v], &cell.arrColor[v]) | VOX_MAT_MASK);
	cell.p[v].x = m_fUnitSize*(float)x;
	cell.p[v].y = m_fUnitSize*(float)y;
	cell.p[v].z = m_fUnitSize*(float)z;
}

int CVoxelVolume::GenerateTrianglesForCell(Vec3i & vCell, CVoxelObject ** pNeighbours, int nLod, PodArray<TRIANGLE> & lstTris, int nStep)
{
	int x = vCell.x;
	int y = vCell.y;
	int z = vCell.z;

	GRIDCELL cell;
	int v=0;
	for(int dz=0; dz<=nStep; dz+=nStep)
	{
		int dx=0;
		int dy=0;

		dx=nStep;
		dy=0;
		FillCellInfo(cell,x+dx,y+dy,z+dz,v,pNeighbours);
		v++;

		dx=0;
		dy=0;
		FillCellInfo(cell,x+dx,y+dy,z+dz,v,pNeighbours);
		v++;

		dx=0;
		dy=nStep;
		FillCellInfo(cell,x+dx,y+dy,z+dz,v,pNeighbours);
		v++;

		dx=nStep;
		dy=nStep;
		FillCellInfo(cell,x+dx,y+dy,z+dz,v,pNeighbours);
		v++;
	}

	TRIANGLE triangles[8];

	int nTris = Polygonise( cell, VOX_ISO_LEVEL, triangles );
	assert(nTris<=8);

	bool bMarkForRemove(
		x>=m_arrVolumeBackup.m_nSizeX || x<0 || 
		y>=m_arrVolumeBackup.m_nSizeY || y<0 || 
		z>=m_arrVolumeBackup.m_nSizeZ ||(z<0 && m_pSrcArea->m_vPos.z>0));
	assert(!bMarkForRemove || pNeighbours);

	for(int i=0; i<nTris && i<8; i++)
	{
		if(
			triangles[i].p[0] == triangles[i].p[1] || 
			triangles[i].p[1] == triangles[i].p[2] ||
			triangles[i].p[2] == triangles[i].p[0])
			continue;
		triangles[i].vNormal = (triangles[i].p[1]-triangles[i].p[0]).Cross(triangles[i].p[2]-triangles[i].p[0]);
		triangles[i].vNormal.Normalize();
		triangles[i].bRemove = bMarkForRemove;

		if(!m_pSrcArea->m_arrSurfacesPalette[triangles[i].arrMatId[0]])
			int ttt=0;
		if(!m_pSrcArea->m_arrSurfacesPalette[triangles[i].arrMatId[1]])
			int ttt=0;
		if(!m_pSrcArea->m_arrSurfacesPalette[triangles[i].arrMatId[2]])
			int ttt=0;

		lstTris.Add(triangles[i]);
	}

	return nTris;
}

void CVoxelVolume::GenerateTrianglesFromVoxels(CVoxelObject ** pNeighbours, int nLod, PodArray<TRIANGLE> & lstTris)
{
	FUNCTION_PROFILER_3DENGINE;

	int nStep = 1<<nLod;

  lstTris.Clear();
	
	Vec3i vStart(
		pNeighbours ? -nStep : 0,
		pNeighbours ? -nStep : 0,
		pNeighbours ? -nStep : 0);

	Vec3i vEnd(
		pNeighbours ? m_arrVolumeBackup.m_nSizeX : (m_arrVolumeBackup.m_nSizeX - nStep),
		pNeighbours ? m_arrVolumeBackup.m_nSizeY : (m_arrVolumeBackup.m_nSizeY - nStep),
		pNeighbours ? m_arrVolumeBackup.m_nSizeZ : (m_arrVolumeBackup.m_nSizeZ - nStep));

	if(m_pSrcArea->m_nFlags & IVOXELOBJECT_FLAG_SNAP_TO_TERRAIN && !m_pSrcArea->GetEntityVisArea())
	{ // surface tracking, may fail if geometry is flying in the air
		static PodArray<Vec3i> lstNextCells; 
		lstNextCells.Clear();
		
		static Array3d<bool> arrAdded;
		arrAdded.Allocate(vEnd.x-vStart.x+nStep, vEnd.y-vStart.y+nStep, vEnd.z-vStart.z+nStep);

		for(int x = 0; x < m_arrVolumeBackup.m_nSizeX; x += nStep)
		for(int y = 0; y < m_arrVolumeBackup.m_nSizeY; y += nStep)
		{
			{
				{
					int z = 0;
					lstNextCells.Add(Vec3i(x,y,z));
					arrAdded.GetAt(x-vStart.x,y-vStart.y,z-vStart.z) = true;
				}

				{
					int z = m_arrVolumeBackup.m_nSizeZ-nStep;
					lstNextCells.Add(Vec3i(x,y,z));
					arrAdded.GetAt(x-vStart.x,y-vStart.y,z-vStart.z) = true;
				}
			}

			if( x == 0 || x == (m_arrVolumeBackup.m_nSizeX-nStep) || y == 0 || (y == m_arrVolumeBackup.m_nSizeY-nStep) )
			{
				for(int z = nStep; z < m_arrVolumeBackup.m_nSizeZ; z += nStep)
				{
					lstNextCells.Add(Vec3i(x,y,z));
					arrAdded.GetAt(x-vStart.x,y-vStart.y,z-vStart.z) = true;
				}
			}
		}

		if(lstNextCells.Count())
		{ // surface tracking

			while(lstNextCells.Count())
			{
				Vec3i vThisCell = lstNextCells.Last();
				lstNextCells.DeleteLast();
				if(GenerateTrianglesForCell(vThisCell, pNeighbours, nLod, lstTris, nStep))
				{
					// if triangles were produced - add neighbor cells if was not processed yet
					for(int dx=-nStep; dx<=nStep; dx+=nStep)
					for(int dy=-nStep; dy<=nStep; dy+=nStep)
					for(int dz=-nStep; dz<=nStep; dz+=nStep)
					{
						Vec3i vNewCell(vThisCell.x+dx, vThisCell.y+dy, vThisCell.z+dz);

						vNewCell.CheckMin(vEnd);
						vNewCell.CheckMax(vStart);

						if(arrAdded.GetAt(vNewCell.x-vStart.x,vNewCell.y-vStart.y,vNewCell.z-vStart.z) == 0)
						{
							lstNextCells.Add(vNewCell);
							arrAdded.GetAt(vNewCell.x-vStart.x,vNewCell.y-vStart.y,vNewCell.z-vStart.z) = true;
						}
					}
				}
			}
		}
	}
	else
	{
		for(int x = vStart.x; x <= vEnd.x; x+=nStep)
		for(int y = vStart.y; y <= vEnd.y; y+=nStep)
		for(int z = vStart.z; z <= vEnd.z; z+=nStep)
		{
			Vec3i cell(x, y, z);
			GenerateTrianglesForCell(cell, pNeighbours, nLod, lstTris, nStep);
		}
	}

  PrintMessagePlus(" %d tris, ", lstTris.Count());
}

bool CVoxelObject::DoVoxelShape(EVoxelEditOperation eOperation, Vec3 vPos, float fRadius, 
                                SSurfaceType * pLayer, Vec3 vBaseColor, EVoxelBrushShape eShape, CVoxelObject ** pNeighbours)
{
  if(GetRndFlags()&ERF_HIDDEN || GetRndFlags()&ERF_FROOZEN)
    return false;

  return m_pVoxelVolume->DoVoxelShape(eOperation, vPos, fRadius, pLayer, vBaseColor, eShape, pNeighbours);
}

bool CVoxelVolume::DoVoxelShape(EVoxelEditOperation eOperation, Vec3 vPos, float fRadius, 
  SSurfaceType * pLayer, Vec3 vBaseColor, EVoxelBrushShape eShape, CVoxelObject ** pNeighbours)
{
	if(vPos.x>m_arrVolumeBackup.m_nSizeX*m_fUnitSize+fRadius || vPos.x<-fRadius)
		return false;
	if(vPos.y>m_arrVolumeBackup.m_nSizeY*m_fUnitSize+fRadius || vPos.y<-fRadius)
		return false;
	if(vPos.z>m_arrVolumeBackup.m_nSizeZ*m_fUnitSize+fRadius || vPos.z<-fRadius)
		return false;

	float fCellSizeInv = 1.f/m_fUnitSize;
	
	int x1 = fastftol_positive(max(0.f,											(vPos.x-fRadius)*fCellSizeInv-1));
	int x2 = fastftol_positive(min((float)m_arrVolumeBackup.m_nSizeX,	(vPos.x+fRadius)*fCellSizeInv+1));
	int y1 = fastftol_positive(max(0.f,											(vPos.y-fRadius)*fCellSizeInv-1));
	int y2 = fastftol_positive(min((float)m_arrVolumeBackup.m_nSizeY,	(vPos.y+fRadius)*fCellSizeInv+1));
	int z1 = fastftol_positive(max(0.f,											(vPos.z-fRadius)*fCellSizeInv-1));
	int z2 = fastftol_positive(min((float)m_arrVolumeBackup.m_nSizeZ,	(vPos.z+fRadius)*fCellSizeInv+1));

	fRadius *= fRadius;
	float fSrcValue = (eOperation == eveoCreate) ? VOX_MAX_USHORT : 0;

	// find local surface id and update palette if needed
	int nSurfaceTypeId = ((eOperation==eveoCreate || eOperation==eveoSubstract || eOperation==eveoMaterial) && pLayer) ? GetLocalSurfaceId(pLayer) : -1;

  bool bChanged=0;

  // filling test
  if(eOperation==eveoCreate && GetCVars()->e_voxel_fill_mode)
  {
    int x = fastftol_positive(CLAMP(vPos.x*fCellSizeInv, 0,	31));
    int y = fastftol_positive(CLAMP(vPos.y*fCellSizeInv, 0,	31));
    int z = fastftol_positive(CLAMP(vPos.z*fCellSizeInv, 0,	31));

    static PodArray<Vec3i> arrPoints; arrPoints.Clear();
    arrPoints.Add(Vec3i(x,y,z));

    int nId=0;
    while(nId<arrPoints.Count())
    {
      Vec3i p = arrPoints[nId];

      if(p.x>=0 && p.x<32 && p.y>=0 && p.y<32 && p.z>=0 && p.z<32)
      if( nId==0 || m_arrVolumeBackup.GetAt(p.x,p.y,p.z) < VOX_ISO_LEVEL )
      {
        uint ucNewValue = VOX_MAX_USHORT;

        m_arrVolume.GetAt(p.x,p.y,p.z) = (ucNewValue & ~VOX_MAT_MASK) | (nSurfaceTypeId & VOX_MAT_MASK);

        bChanged = true;

        for(int x=-1; x<=1; x++)
        {
          for(int y=-1; y<=1; y++)
          {
            for(int z=-1; z<=1; z++)
            {
              Vec3i n;
              n.Set(p.x+x, p.y+y, p.z+z);
              if(x!=0 || y!=0 || z!=0)
              if(n.x>=0 && n.x<32 && n.y>=0 && n.y<32 && n.z>=0 && n.z<32)
                if(arrPoints.Find(n)<0)
                  arrPoints.Add(n);
            }
          }
        }
      }

      nId++;
    }

    if(bChanged)
    {
      m_bUpdateRequested = true;
      if(!m_nUpdateRequestedFrameId)
        m_nUpdateRequestedFrameId = GetMainFrameID();
    }

    return true;
  }

	for(int x=x1; x<x2; x++)
	for(int y=y1; y<y2; y++)
	for(int z=z1; z<z2; z++)
	{
		Vec3 vVoxelPos((float)x*m_fUnitSize,(float)y*m_fUnitSize,(float)z*m_fUnitSize);

		float fDist = 0.f;
		if(eShape == evbsSphere)
			fDist = vVoxelPos.GetSquaredDistance(vPos);
		else if(eShape == evbsBox)
		{
			fDist = max(max(fabs(vVoxelPos.x-vPos.x),fabs(vVoxelPos.y-vPos.y)),fabs(vVoxelPos.z-vPos.z));
			fDist*=fDist;
		}

		if(fDist<fRadius)
		{
			fDist = cry_sqrtf(fDist)/cry_sqrtf(fRadius);
			fDist*=fDist;

			Vec3 vPrevColor;
			vPrevColor.x = m_arrColors.GetAt(x,y,z).r;
			vPrevColor.y = m_arrColors.GetAt(x,y,z).g;
			vPrevColor.z = m_arrColors.GetAt(x,y,z).b;

			float fOldValue = float(m_arrVolume.GetAt(x,y,z) & ~VOX_MAT_MASK);
			ushort ucNewValue = 0;
			switch(eOperation)
			{
			case eveoMaterial:
				ucNewValue = m_arrVolume.GetAt(x,y,z) & ~VOX_MAT_MASK; // use old value
				{
					float fDist2 = fDist*fDist;
					Vec3 vNewColor = (vBaseColor*255)*(1.f-fDist2) + vPrevColor*fDist2;
					vNewColor.CheckMin(Vec3(255,255,255));
					m_arrColors.GetAt(x,y,z) = vNewColor;
				}
				break;
			case eveoCopyTerrain: // interpolate between old and new (terrain)
				fSrcValue = GetHeightMapValue(m_pSrcArea->m_Matrix.TransformPoint(Vec3(x,y,z)*m_fUnitSize), 0);
			case eveoCreate:
			case eveoSubstract: // interpolate between old and new based on distance
				ucNewValue = ushort(fSrcValue*(1.f-fDist) + fOldValue*fDist);
				{
					float fDist2 = fDist*fDist;
					Vec3 vNewColor = (vBaseColor*255)*(1.f-fDist2) + vPrevColor*fDist2;
					vNewColor.CheckMin(Vec3(255,255,255));
					m_arrColors.GetAt(x,y,z) = vNewColor;
				}
				break;
			case eveoBlur:
				{
					float fNewValue = 0;
					float fCounter = 0;
					for(int dx=-1; dx<=1; dx++)
					{
						fSrcValue += GetVoxelValue(x,y,z,pNeighbours,NULL,NULL);
						fCounter++;

						for(int dy=-1; dy<=1; dy++)
						for(int dz=-1; dz<=1; dz++)
						{
							int X = dx+x;
							int Y = dy+y;
							int Z = dz+z;

							if(pNeighbours || (X>=0 && X<m_arrVolumeBackup.m_nSizeX && Y>=0 && Y<m_arrVolumeBackup.m_nSizeY && Z>=0 && Z<m_arrVolumeBackup.m_nSizeZ))
							{
								fSrcValue += GetVoxelValue(X,Y,Z,pNeighbours,NULL,NULL);
								fCounter++;
							}
						}
					}

					fSrcValue = (ushort)min((float)VOX_MAX_USHORT,fSrcValue/fCounter);
					
					// interpolate between old and new based on distance
					ucNewValue = ushort(fSrcValue*(1.f-fDist) + fOldValue*fDist);
				}
				break;
			}

			// write result
			if(nSurfaceTypeId>=0)
				m_arrVolume.GetAt(x,y,z) = (ucNewValue & ~VOX_MAT_MASK) | (nSurfaceTypeId & VOX_MAT_MASK);
			else
				m_arrVolume.GetAt(x,y,z) = (ucNewValue & ~VOX_MAT_MASK) | (m_arrVolume.GetAt(x,y,z) & VOX_MAT_MASK);
		}
		
		if(fDist < (fRadius + m_fUnitSize))
			bChanged = true; // update neighbors
	}

  if(bChanged)
  {
    m_bUpdateRequested = true;
    if(!m_nUpdateRequestedFrameId)
      m_nUpdateRequestedFrameId = GetMainFrameID();
  }

	return bChanged;
}

void CVoxelVolume::NormalizeVolume(CVoxelObject ** pNeighbours)
{
  for(int x=0; x<m_arrVolume.m_nSizeX; x++) 
  {
    for(int y=0; y<m_arrVolume.m_nSizeY; y++) 
    {
      for(int z=0; z<m_arrVolume.m_nSizeZ; z++)
      {
        ushort & ucValue = m_arrVolume.GetAt(x,y,z);
        int nMatId = ucValue & VOX_MAT_MASK;

        float fVal = ucValue;
        fVal = VOX_ISO_LEVEL + 2.f*(fVal-VOX_ISO_LEVEL);

        ucValue = CLAMP(fVal, 0, VOX_MAX_USHORT);
        ucValue = ucValue & ~VOX_MAT_MASK;
        ucValue |= nMatId;
      }
    }
  }
}

void CVoxelVolume::SubmitVoxelSpace()
{
	FUNCTION_PROFILER_3DENGINE;
	m_arrVolumeBackup.CopyFrom(m_arrVolume);
}

/*void CVoxelVolume::SmoothVoxelSpace(CVoxelObject ** pNeighbours)
{
	FUNCTION_PROFILER_3DENGINE;

	Array3d<ushort> arrTmpVolume;
	arrTmpVolume.Allocate(m_arrVolume.m_nSizeX,m_arrVolume.m_nSizeY,m_arrVolume.m_nSizeZ);

	for(int x=0; x<m_arrVolume.m_nSizeX; x++)
	for(int y=0; y<m_arrVolume.m_nSizeY; y++)
	for(int z=0; z<m_arrVolume.m_nSizeZ; z++)
	{
		int arrMatUsageTable[VOX_MAX_SURF_TYPES_NUM];
		memset(arrMatUsageTable,0,sizeof(arrMatUsageTable));

		float fRes=0;
		float fCount=0;
		for(int dx=-1; dx<=1; dx++)
		{
			int nMat=0;
			ushort ucVoxelValue = GetVoxelValue(x,y,z,0,&nMat,NULL)&~VOX_MAT_MASK;
			assert(nMat>=0 && nMat<VOX_MAX_SURF_TYPES_NUM);
			fRes += ucVoxelValue;
			arrMatUsageTable[nMat]+=ucVoxelValue;
			fCount++;

			for(int dy=-1; dy<=1; dy++)
			{
				for(int dz=-1; dz<=1; dz++)
				{
					ucVoxelValue = GetVoxelValue(x+dx,y+dy,z+dz,pNeighbours,&nMat,NULL)&~VOX_MAT_MASK;
					assert(nMat>=0 && nMat<VOX_MAX_SURF_TYPES_NUM);
					fRes += ucVoxelValue;
					arrMatUsageTable[nMat]+=ucVoxelValue;
					fCount++;
				}
			}
		}

		assert(fRes/fCount<=VOX_MAX_USHORT && fRes/fCount>=0);

		int nMaxMat=0;
		for(int m=0; m<VOX_MAX_SURF_TYPES_NUM; m++)
			if(arrMatUsageTable[nMaxMat]<arrMatUsageTable[m])
				nMaxMat=m;

		assert(arrMatUsageTable[nMaxMat] || !fRes);

		arrTmpVolume.GetAt(x,y,z) = ushort(fRes/fCount)&~VOX_MAT_MASK | nMaxMat&VOX_MAT_MASK;
	}

	m_arrVolume.CopyFrom(arrTmpVolume);

	if(m_pVoxelMesh)
		m_pVoxelMesh->ResetRenderMeshs();

	DePhysicalize();
}*/
/*
float CVoxelObject::IntersectVoxelSpace( const Vec3 &_vStart, const Vec3 &_vEnd, CVoxelObject ** pNeighbours)
{
	FUNCTION_PROFILER_3DENGINE;

	// transform into height map space
//	vStart.z-= Get3DEngine()->GetTerrainElevation(vStart.x+m_vOrigin.x, vStart.y+m_vOrigin.y)-m_vOrigin.z;
	//vEnd.z	-= Get3DEngine()->GetTerrainElevation(	vEnd.x+m_vOrigin.x,		vEnd.y+m_vOrigin.y)-m_vOrigin.z;

	// transform into voxel space
	Vec3 vStart = _vStart / m_fUnitSize;
	Vec3 vEnd = _vEnd / m_fUnitSize;

	// find step size
	Vec3 vDelta = (vEnd-vStart);
	float fSteps = vDelta.GetLength()/4;
	vDelta /= fSteps;

	float fLight = 256.f*256.f;
	for(int i=0; i<fSteps && vStart.z<DEF_VOX_VOLUME_SIZE && fLight>0; i++)
	{
		vStart += vDelta;
		float fValue = GetVoxelValueInterpolated( vStart.x, vStart.y, vStart.z, pNeighbours, 0, 0, 1 );
		fLight -= fValue;
	}

	fLight = max(0.f, fLight);

	return fLight/256.f/256.f;
}*/

void CVoxelObject::Physicalize(CMesh * pMesh, std::vector<char> * physData)
{
	FUNCTION_PROFILER_3DENGINE;

  DePhysicalize();

	int flags = mesh_multicontact1 | mesh_AABB | mesh_no_vtx_merge | mesh_always_static;
	float tol = 0.05f;
	int nMinTrisPerNode=16, nMaxTrisPerNode=32;

	int arrSurfaceTypesId[VOX_MAX_SURF_TYPES_NUM];
	memset(arrSurfaceTypesId,0,sizeof(arrSurfaceTypesId));

	// make indices
	static PodArray<char> lstMatIndices; lstMatIndices.Clear();
	for (int i=0; i<pMesh->m_nIndexCount; i+=3)
	{
		char nSurfTypeIdLocal0 = pMesh->m_pVertMats[pMesh->m_pIndices[i+0]];
		char nSurfTypeIdLocal1 = pMesh->m_pVertMats[pMesh->m_pIndices[i+1]];
    char nSurfTypeIdLocal2 = pMesh->m_pVertMats[pMesh->m_pIndices[i+2]];

		char nSurfTypeIdLocal = nSurfTypeIdLocal0;
		if(nSurfTypeIdLocal1 == nSurfTypeIdLocal2 && nSurfTypeIdLocal1 != nSurfTypeIdLocal0)
			nSurfTypeIdLocal = nSurfTypeIdLocal1;

		IMaterial::EProjAxis defProjAxis = m_arrSurfacesPalette[nSurfTypeIdLocal]->defProjAxis;
		IMaterial * pProjMat = m_arrSurfacesPalette[nSurfTypeIdLocal]->GetMaterialOfProjection(defProjAxis);
		int nSurfTypeIdGlobal = pProjMat ? pProjMat->GetSurfaceTypeId() : 0;

		assert(nSurfTypeIdLocal>=0 && nSurfTypeIdLocal<VOX_MAX_SURF_TYPES_NUM);
    if(nSurfTypeIdLocal>=0 && nSurfTypeIdLocal<VOX_MAX_SURF_TYPES_NUM)
			arrSurfaceTypesId[nSurfTypeIdLocal] = nSurfTypeIdGlobal;

    lstMatIndices.Add(nSurfTypeIdLocal);
	}

  if(!lstMatIndices.Count())
    return;

  if(physData && physData->size())
  {
    CMemStream stm( &physData->front(), physData->size(), true );
    m_pPhysGeom = GetPhysicalWorld()->GetGeomManager()->LoadPhysGeometry(stm,
      pMesh->m_pPositions,pMesh->m_pIndices,&lstMatIndices[0]);
  }
  else
  {
	  IGeomManager *pGeoman = GetPhysicalWorld()->GetGeomManager();
	  IGeometry * pGeom = pGeoman->CreateMesh((Vec3*)pMesh->m_pPositions, pMesh->m_pIndices, 
		  lstMatIndices.GetElements(), NULL, pMesh->m_nIndexCount/3, flags, 0.05f, 2, 31);
	  m_pPhysGeom = pGeoman->RegisterGeometry(pGeom);
	  pGeom->Release();
  }

	GetPhysicalWorld()->GetGeomManager()->SetGeomMatMapping( m_pPhysGeom, arrSurfaceTypesId, VOX_MAX_SURF_TYPES_NUM );

	assert(!m_pPhysEnt);
	m_pPhysEnt = GetPhysicalWorld()->CreatePhysicalEntity(PE_STATIC,NULL,NULL,PHYS_FOREIGN_ID_STATIC);

	pe_action_remove_all_parts remove_all;
	m_pPhysEnt->Action(&remove_all);

	pe_geomparams params;	  
	m_pPhysEnt->AddGeometry(m_pPhysGeom, &params);

	pe_params_flags par_flags;
	par_flags.flagsOR = pef_never_affect_triggers;
	m_pPhysEnt->SetParams(&par_flags);

	pe_params_pos par_pos;
	par_pos.pMtx3x4 = &m_Matrix;
	m_pPhysEnt->SetParams(&par_pos);

	pe_params_foreign_data par_foreign_data;
	par_foreign_data.pForeignData = (IRenderNode*)this;
	par_foreign_data.iForeignData = PHYS_FOREIGN_ID_STATIC;
	// flag to exclude from AI triangulation
	par_foreign_data.iForeignFlags |= PFF_EXCLUDE_FROM_STATIC;
	m_pPhysEnt->SetParams(&par_foreign_data);
}

void CVoxelObject::DePhysicalize()
{
	if(m_pPhysEnt)
	{
		GetPhysicalWorld()->GetGeomManager()->UnregisterGeometry(m_pPhysGeom);
		m_pPhysEnt->RemoveGeometry(0);
		GetPhysicalWorld()->DestroyPhysicalEntity(m_pPhysEnt);
		m_pPhysEnt = NULL;
	}
}

void CVoxelMesh::CheckUpdateLighting(int nLod, CVoxelObject * pSrcArea)
{
	FUNCTION_PROFILER_3DENGINE;

  int nNewAoRadiusAndScale = GetCVars()->e_voxel_ao_scale * GetCVars()->e_voxel_ao_radius * (pSrcArea->m_nFlags & IVOXELOBJECT_FLAG_COMPUTE_AO);

  if(nNewAoRadiusAndScale == m_arrCurrAoRadiusAndScale[nLod])
    return;

  m_arrCurrAoRadiusAndScale[nLod] = nNewAoRadiusAndScale;
  CVoxelObject * arrNeighbours[3*3*3];
  bool bNeibFound = nNewAoRadiusAndScale ? GetTerrain()->Voxel_FindNeighboursForObject(pSrcArea, arrNeighbours) : false;

	if(nLod>=VOX_MAX_LODS_NUM) nLod=VOX_MAX_LODS_NUM-1;
	if(nLod<0) nLod=0;

	IRenderMesh * pRM = GetRenderMesh(nLod);
	if(!pRM)
		return;

	if(m_arrCurrAoRadiusAndScale[nLod]>0)
		UpdateAmbientOcclusion(arrNeighbours, pSrcArea, nLod);
	else if(pRM)
	{
		// get offsets
		int nColorStride=0;
		uchar * pColor = pRM->GetStridedColorPtr(nColorStride, 0, true);

		for(int i=0, nVertCount = pRM->GetVertCount(); i<nVertCount; i++)
		{
			SMeshColor & uColor = *((SMeshColor*)pColor);
			uColor.a = 255;
			pColor += nColorStride;
		}

		pRM->InvalidateVideoBuffer(1);
	}

	pRM->UnlockStream(VSF_GENERAL);
}

/*void CVoxelObject::GenerateIndicesForQuad(IRenderMesh * pRM, Vec3 vBoxMin, Vec3 vBoxMax, PodArray<ushort> & dstIndices)
{
	dstIndices.Clear();

	int nSrcCount=0;
	ushort * pSrcInds = pRM->GetIndices(&nSrcCount);

	int nPosStride=0;
	byte * pPos = pRM->GetStridedPosPtr(nPosStride);

	for(int i=0; i<(*pRM->GetChunks()).Count(); i++)
	{
		if (!(*pRM->GetChunks())[i].pRE)
			continue;

		CRenderChunk * pMat = &(*pRM->GetChunks())[i];

		for (int j=pMat->nFirstIndexId; j<pMat->nNumIndices+pMat->nFirstIndexId; j+=3)
		{
			int nIndex0 = pSrcInds[j+0];
			int nIndex1 = pSrcInds[j+1];
			int nIndex2 = pSrcInds[j+2];

			assert(nIndex0>=0 && nIndex0<pRM->GetSysVertCount());
			assert(nIndex1>=0 && nIndex1<pRM->GetSysVertCount());
			assert(nIndex2>=0 && nIndex2<pRM->GetSysVertCount());

			Vec3 & vPos0 = *(Vec3*)&pPos[nIndex0*nPosStride];
			Vec3 & vPos1 = *(Vec3*)&pPos[nIndex1*nPosStride];
			Vec3 & vPos2 = *(Vec3*)&pPos[nIndex2*nPosStride];

			if(( vPos0.x>vBoxMin.x && vPos0.y>vBoxMin.y && vPos0.z>vBoxMin.z && vPos0.x<vBoxMax.x && vPos0.y<vBoxMax.y && vPos0.z<vBoxMax.z ) ||
				 ( vPos1.x>vBoxMin.x && vPos1.y>vBoxMin.y && vPos1.z>vBoxMin.z && vPos1.x<vBoxMax.x && vPos1.y<vBoxMax.y && vPos1.z<vBoxMax.z ) ||
				 ( vPos2.x>vBoxMin.x && vPos2.y>vBoxMin.y && vPos2.z>vBoxMin.z && vPos2.x<vBoxMax.x && vPos2.y<vBoxMax.y && vPos2.z<vBoxMax.z ) )
			{
				dstIndices.Add(nIndex0);
				dstIndices.Add(nIndex1);
				dstIndices.Add(nIndex2);
			}
		}
	}
}*/

bool CVoxelObject::InitMaterials()
{
	FUNCTION_PROFILER_3DENGINE;

  bool bMatsFound = false;

	// init surface default mapping
	for(int nMatId=0; nMatId<VOX_MAX_SURF_TYPES_NUM; nMatId++)
		if(m_arrSurfacesPalette[nMatId] && m_arrSurfacesPalette[nMatId]->pLayerMat)
    {
      Get3DEngine()->InitMaterialDefautMappingAxis(m_arrSurfacesPalette[nMatId]->pLayerMat);
      bMatsFound = true;
    }

	return bMatsFound;
}

CIndexedMesh * CVoxelVolume::MakeIndexedMesh(PodArray<TRIANGLE> & lstTris, CVoxelObject ** pNeighbours)
{
	FUNCTION_PROFILER_3DENGINE;

	// make indexed mesh
	CIndexedMesh * pIndexedMesh = new CIndexedMesh();
	pIndexedMesh->m_numFaces = lstTris.Count();
	pIndexedMesh->m_pFaces = (SMeshFace*)calloc(pIndexedMesh->m_numFaces,sizeof(SMeshFace));
	int nAllVertsNum = lstTris.Count()*3;
	for (int i=0; i<pIndexedMesh->m_numFaces; i++)
	{
		pIndexedMesh->m_pFaces[i].v[0] = i*3+0;
		pIndexedMesh->m_pFaces[i].v[1] = i*3+1;
		pIndexedMesh->m_pFaces[i].v[2] = i*3+2;
		pIndexedMesh->m_pFaces[i].t[0] = i*3+0;
		pIndexedMesh->m_pFaces[i].t[1] = i*3+1;
		pIndexedMesh->m_pFaces[i].t[2] = i*3+2;
		pIndexedMesh->m_pFaces[i].nSubset = 0;

		pIndexedMesh->m_pFaces[i].dwFlags = lstTris[i].bRemove ? SFACE_FLAG_REMOVE : 0;

		if(pIndexedMesh->m_pFaces[i].v[0]<nAllVertsNum)
			if(pIndexedMesh->m_pFaces[i].v[1]<nAllVertsNum)
				if(pIndexedMesh->m_pFaces[i].v[2]<nAllVertsNum)
					continue;

		GetConsole()->Exit("CryModelState::GenerateRenderArrays: indices out of range (1): %s", "VoxMan");
	}

	pIndexedMesh->m_pTexCoord = NULL;
	pIndexedMesh->m_nCoorCount = 0;
	pIndexedMesh->m_pNorms = (Vec3 *)calloc(nAllVertsNum,sizeof(Vec3));
	pIndexedMesh->m_pPositions = (Vec3 *)calloc(nAllVertsNum,sizeof(Vec3));
	pIndexedMesh->m_pColor0 = (SMeshColor *)calloc(nAllVertsNum,sizeof(SMeshColor));
	pIndexedMesh->m_pColor1 = (SMeshColor *)calloc(nAllVertsNum,sizeof(SMeshColor));
	pIndexedMesh->m_pVertMats = (int*)calloc(nAllVertsNum,sizeof(int));
	pIndexedMesh->m_numVertices = nAllVertsNum;
	pIndexedMesh->SetBBox( AABB( Vec3(0,0,0),Vec3(100000.f,100000.f,100000.f) ) );

	PodArray<int> lstIndices;
	for (int i=0; i<lstTris.Count(); i++)
	for (int v=0; v<3; v++)
	{
		pIndexedMesh->m_pNorms[i*3+v].x = lstTris[i].vNormal.x;
		pIndexedMesh->m_pNorms[i*3+v].y = lstTris[i].vNormal.y;
		pIndexedMesh->m_pNorms[i*3+v].z = lstTris[i].vNormal.z;

		pIndexedMesh->m_pPositions[i*3+v].x = lstTris[i].p[v].x;// + m_vOrigin.x;
		pIndexedMesh->m_pPositions[i*3+v].y = lstTris[i].p[v].y;// + m_vOrigin.y;
		pIndexedMesh->m_pPositions[i*3+v].z = lstTris[i].p[v].z;// + m_vOrigin.z;

		pIndexedMesh->m_bbox.Add( lstTris[i].p[v] );

		pIndexedMesh->m_pVertMats[i*3+v] = lstTris[i].arrMatId[v];
		assert(pIndexedMesh->m_pVertMats[i*3+v]<VOX_MAX_SURF_TYPES_NUM);

		pIndexedMesh->m_pColor0[i*3+v].r = 255;
		pIndexedMesh->m_pColor0[i*3+v].g = 255;
		pIndexedMesh->m_pColor0[i*3+v].b = 255;
		pIndexedMesh->m_pColor0[i*3+v].a = 255;

		pIndexedMesh->m_pColor1[i*3+v].r = 255;
		pIndexedMesh->m_pColor1[i*3+v].g = 255;
		pIndexedMesh->m_pColor1[i*3+v].b = 255;
		pIndexedMesh->m_pColor1[i*3+v].a = 255;

		pIndexedMesh->m_pColor0[i*3+v].r = lstTris[i].arrColor[v][0];
		pIndexedMesh->m_pColor0[i*3+v].b = lstTris[i].arrColor[v][1];
		pIndexedMesh->m_pColor1[i*3+v].b = lstTris[i].arrColor[v][2];

		lstIndices.Add(i*3+v);
	}

	// remove duplicated vertices and smooth normals
	int nOldNumVerts = pIndexedMesh->m_numVertices;

	float fEpsilon = 0.5f;
	Vec3 vMax(
		m_arrVolumeBackup.m_nSizeX*m_fUnitSize-fEpsilon,
		m_arrVolumeBackup.m_nSizeY*m_fUnitSize-fEpsilon,
		m_arrVolumeBackup.m_nSizeZ*m_fUnitSize-fEpsilon);
	pIndexedMesh->ShareVertices(GetSystem(),
		AABB(Vec3(fEpsilon,fEpsilon,fEpsilon), vMax));

	PrintMessagePlus(" Share verts: (%d->%d),", nOldNumVerts, pIndexedMesh->m_numVertices);

	// remove fake and bad faces
	for(int i=0; i<pIndexedMesh->m_numFaces; i++)
	{
		bool bInvalidTriangle = false;
		if(	pIndexedMesh->m_pFaces[i].v[0] == pIndexedMesh->m_pFaces[i].v[1] || 
				pIndexedMesh->m_pFaces[i].v[1] == pIndexedMesh->m_pFaces[i].v[2] || 
				pIndexedMesh->m_pFaces[i].v[2] == pIndexedMesh->m_pFaces[i].v[0] )
				bInvalidTriangle = true;

		if(pIndexedMesh->m_pFaces[i].dwFlags & SFACE_FLAG_REMOVE || bInvalidTriangle)
		{
			if(i+1 < pIndexedMesh->m_numFaces) // replace this with last
				memcpy(&pIndexedMesh->m_pFaces[i],&pIndexedMesh->m_pFaces[pIndexedMesh->m_numFaces-1],sizeof(pIndexedMesh->m_pFaces[i]));
			i--;
			pIndexedMesh->m_numFaces--;
		}
	}

	assert(pIndexedMesh->m_numFaces < (65000/3));

/*	if(GetCVars()->e_ voxel_lods_num)
	{
		nOldNumVerts = pIndexedMesh->m_numVertices;
		pIndexedMesh->ShareVertices();
		PrintMessagePlus("  SharePass2: (%d->%d)", nOldNumVerts, pIndexedMesh->m_numVertices);
	}*/

	int nStep = CTerrain::GetHeightMapUnitSize();

	Matrix34 matInv = m_pSrcArea->m_Matrix.GetInverted();
//	float fMatScale = m_Matrix.GetColumn0().GetLength();

	memset(m_pSrcArea->m_arrUsedSTypes,0,sizeof(m_pSrcArea->m_arrUsedSTypes));
	for(int i=0; i<pIndexedMesh->m_numVertices; i++)
	{
		// get global surf type id
		uint nSurfIdLocal = pIndexedMesh->m_pVertMats[i];
		assert(nSurfIdLocal<VOX_MAX_SURF_TYPES_NUM);
		m_pSrcArea->m_arrUsedSTypes[nSurfIdLocal]++;
		int nSurfIdGlobal = m_pSrcArea->m_arrSurfacesPalette[nSurfIdLocal]->ucThisSurfaceTypeId;	
		assert(nSurfIdGlobal<MAX_SURFACE_TYPES_COUNT);
/*
		ColorB vBaseColor(255,255,255,255);
		if(m_pVisArea || !(m_nFlags & IVOXELOBJECT_FLAG_LINK_TO_TERRAIN))
		{
			Vec3 vOSPos = pIndexedMesh->m_pPositions[i]/m_fUnitSize + Vec3(.5f,.5f,.5f);
			vBaseColor = m_arrColors.GetAtClamped((int)vOSPos.x,(int)vOSPos.y,(int)vOSPos.z);		
		}
*/
//		vBaseColor = Vec3(0,0,0);

    // 0 pass
//    pIndexedMesh->m_pColor0[i].r = vBaseColor.r;
    pIndexedMesh->m_pColor0[i].g = nSurfIdGlobal;
//    pIndexedMesh->m_pColor0[i].b = vBaseColor.g;
    pIndexedMesh->m_pColor0[i].a = 255; // ambient occlusion


		pIndexedMesh->m_pColor1[i].r = 0;
		pIndexedMesh->m_pColor1[i].g = 0;
//		pIndexedMesh->m_pColor1[i].b = vBaseColor.b;
		pIndexedMesh->m_pColor1[i].a = 0;

#define TAKE_BASE_COLOR_FROM_VERTEX 128

		// set per vertex source of base color
		if(m_pSrcArea->GetEntityVisArea())
			pIndexedMesh->m_pColor0[i].g = nSurfIdGlobal | TAKE_BASE_COLOR_FROM_VERTEX;
		else if(m_pSrcArea->m_nFlags & IVOXELOBJECT_FLAG_SMART_BASE_COLOR)
		{
			Vec3 & vPos = pIndexedMesh->m_pPositions[i];
			Vec3 & vNormal = pIndexedMesh->m_pNorms[i];
			Vec3 vWSPos = m_pSrcArea->m_Matrix.TransformPoint(vPos);

			float fOffset = 1.0f;
			float fTerrainZ  = GetTerrain()->GetZApr(vWSPos.x, vWSPos.y);
			float fTerrainZ0 = GetTerrain()->GetZApr(vWSPos.x+fOffset, vWSPos.y);
			float fTerrainZ1 = GetTerrain()->GetZApr(vWSPos.x, vWSPos.y+fOffset);
			float fTerrainZ2 = GetTerrain()->GetZApr(vWSPos.x-fOffset, vWSPos.y);
			float fTerrainZ3 = GetTerrain()->GetZApr(vWSPos.x, vWSPos.y-fOffset);
			float fMinZ = min(min(fTerrainZ0,fTerrainZ1),min(fTerrainZ2,fTerrainZ3));
			float fMaxZ = max(max(fTerrainZ0,fTerrainZ1),max(fTerrainZ2,fTerrainZ3));
			fMinZ = min(fMinZ, fTerrainZ);
			fMaxZ = max(fMaxZ, fTerrainZ);

			if(vWSPos.z>=(fMinZ-fOffset) && vWSPos.z<=(fMaxZ+fOffset))
				pIndexedMesh->m_pColor0[i].g = nSurfIdGlobal;
			else
				pIndexedMesh->m_pColor0[i].g = nSurfIdGlobal | TAKE_BASE_COLOR_FROM_VERTEX;
		}
		else
			pIndexedMesh->m_pColor0[i].g = nSurfIdGlobal;

		assert((pIndexedMesh->m_pColor0[i].g & 127)<MAX_SURFACE_TYPES_COUNT);

		// snap vertices to height-map
		if(m_pSrcArea->m_nFlags & IVOXELOBJECT_FLAG_SNAP_TO_TERRAIN)
		{
			Vec3 & vPos = pIndexedMesh->m_pPositions[i];
			Vec3 & vNormal = pIndexedMesh->m_pNorms[i];
			Vec3 vWSPos = m_pSrcArea->m_Matrix.TransformPoint(vPos);

			float fOffset = m_pSrcArea->m_Matrix.GetColumn0().GetLength()*0.5;
			float fTerrainZ  = GetTerrain()->GetZApr(vWSPos.x, vWSPos.y);

			if(fabs(vWSPos.z-fTerrainZ)<fOffset)
			{
				vWSPos.z = fTerrainZ;
				vPos.z = matInv.TransformPoint(vWSPos).z;

				// detect border between voxel and terrain and take normal of terrain

				CVoxelObject * pNeibArea = m_pSrcArea;

				int x = (int)(vPos.x/m_fUnitSize);
				int y = (int)(vPos.y/m_fUnitSize);
				int z = (int)(vPos.z/m_fUnitSize);
				bool bBorder = 
					x<=0 || y<=0 || z<=0 || 
					x>=(m_arrVolumeBackup.m_nSizeX) || y>=(m_arrVolumeBackup.m_nSizeY) || z>=(m_arrVolumeBackup.m_nSizeZ);

				if(pNeighbours && bBorder)
				{
					int idX, idY, idZ;
					if(x>=(m_arrVolumeBackup.m_nSizeX)) idX = 2;
					else if(x<=0) idX = 0;
					else idX = 1;
					if(y>=(m_arrVolumeBackup.m_nSizeY)) idY = 2;
					else if(y<=0) idY = 0;
					else idY = 1;
					if(z>=(m_arrVolumeBackup.m_nSizeZ)) idZ = 2;
					else if(z<=0) idZ = 0;
					else idZ = 1;
					assert(pNeighbours[1*9+1*3+1]==m_pSrcArea);
					pNeibArea = pNeighbours[idX*9+idY*3+idZ];
				}

			/*	if( !pNeibArea && m_Matrix.m00 == 1.f)
				{
					vWSPos.x = int(vWSPos.x/nStep)*nStep;
					vWSPos.y = int(vWSPos.y/nStep)*nStep;
					vWSPos.z = GetTerrain()->GetZSafe((int)vWSPos.x, (int)vWSPos.y);

					// calculate surface normal
					float sx;
					if((x+nStep)<CTerrain::GetTerrainSize() && x>=nStep)
						sx = GetTerrain()->GetZSafe(x+nStep,y  ) - GetTerrain()->GetZSafe(x-nStep,y  );
					else
						sx = 0;

					float sy;
					if((y+nStep)<CTerrain::GetTerrainSize() && y>=nStep)
						sy = GetTerrain()->GetZSafe(x  ,y+nStep) - GetTerrain()->GetZSafe(x  ,y-nStep);
					else
						sy = 0;

					vNormal = Vec3(-sx, -sy, 2.f*nStep);
					vNormal.Normalize();

					vPos = matInv.TransformPoint(vWSPos);
				}*/
			}
		}
	}

	if(!pIndexedMesh->m_numFaces)
	{
		delete pIndexedMesh;
		pIndexedMesh = NULL;
	}

	return pIndexedMesh;
}

void CVoxelVolume::SimplifyIndexedMesh(CIndexedMesh * pIndexedMesh, int nLod)
{
	FUNCTION_PROFILER_3DENGINE;

	int nOldTrisNum=pIndexedMesh->m_numFaces;
	PrintMessagePlus(" Simplify to LOD %d ... ", nLod);

	float fEpsilon = 0.5f;
	Vec3 vMax(
		m_arrVolumeBackup.m_nSizeX*m_fUnitSize-fEpsilon,
		m_arrVolumeBackup.m_nSizeY*m_fUnitSize-fEpsilon,
		m_arrVolumeBackup.m_nSizeZ*m_fUnitSize-fEpsilon);
	pIndexedMesh->SimplifyMesh(float(nLod+1)*3.0f, 
		AABB(Vec3(fEpsilon,fEpsilon,fEpsilon), vMax));
	int nRatio = 100*(pIndexedMesh->m_numFaces)/nOldTrisNum;
	PrintMessagePlus("  (%d : %d -> %d ), ", nRatio, nOldTrisNum, pIndexedMesh->m_numFaces); 
}

IRenderMesh * CVoxelObject::GetRenderMesh(int nLod) 
{ 
	return m_pVoxelMesh ? m_pVoxelMesh->GetRenderMesh(nLod) : NULL; 
}
/*
IRenderMesh * CVoxelObject::GetMaterialRenderMesh(int nLod, int nSurfId) 
{ 
	return m_pVoxelMesh ? m_pVoxelMesh->GetMaterialRenderMesh(nLod,nSurfId) : NULL; 
}*/

ushort CVoxelVolume::GetLocalSurfaceId(SSurfaceType * pLayer)
{
	// find local surface id and update palette if needed
	ushort ucSurfaceTypeId = 0;
	for(ucSurfaceTypeId=0; ucSurfaceTypeId<VOX_MAX_SURF_TYPES_NUM; ucSurfaceTypeId++)
	{
		if(m_pSrcArea->m_arrSurfacesPalette[ucSurfaceTypeId] == pLayer)
			break;
		else if(m_pSrcArea->m_arrSurfacesPalette[ucSurfaceTypeId] == NULL)
		{
			m_pSrcArea->m_arrSurfacesPalette[ucSurfaceTypeId] = pLayer;
			break;
		}
	}

	if(ucSurfaceTypeId>=VOX_MAX_SURF_TYPES_NUM)
	{
    SubmitVoxelSpace();
		ucSurfaceTypeId = GetLessUsedSurfaceId();
		m_pSrcArea->m_arrSurfacesPalette[ucSurfaceTypeId] = pLayer;
	}

	return ucSurfaceTypeId;
}

ushort CVoxelVolume::GetLessUsedSurfaceId()
{
	// get materials usage
	int arrnSurfUsage[VOX_MAX_SURF_TYPES_NUM];
	memset(arrnSurfUsage,0,sizeof(arrnSurfUsage));
	for(int i=0; i<VOX_MAX_SURF_TYPES_NUM; i++)
		arrnSurfUsage[i] = 0;

	// do not check neighbours
	for(int x=0; x<m_arrVolumeBackup.m_nSizeX; x++)
	for(int y=0; y<m_arrVolumeBackup.m_nSizeY; y++)
	for(int z=0; z<m_arrVolumeBackup.m_nSizeZ; z++)
	{
		ushort ucValue = m_arrVolumeBackup.GetAt(x,y,z);
		int nMatId = ucValue & VOX_MAT_MASK;
		assert(nMatId>=0 && nMatId<VOX_MAX_SURF_TYPES_NUM);
		arrnSurfUsage[nMatId] ++;
	}

	ushort nResult = 0;
	for(int i=0; i<VOX_MAX_SURF_TYPES_NUM; i++)
	{
		if(arrnSurfUsage[i] < arrnSurfUsage[nResult])
			nResult = i;
	}

	return nResult;
}

SSurfaceType * CVoxelObject::GetGlobalSurfaceType(ushort ucSurfaceTypeId)
{
	assert(this);
	if(m_arrSurfacesPalette[ucSurfaceTypeId])
		return m_arrSurfacesPalette[ucSurfaceTypeId];
	return NULL;
}

bool CVoxelObject::Render(const SRendParams &_rParms)
{
	m_fCurrDistance = _rParms.fDistance;

	if(!GetCVars()->e_voxel || !m_pVoxelMesh)
		return false;

	FUNCTION_PROFILER_3DENGINE;

	SRendParams rParms(_rParms);

	rParms.pMatrix = &m_Matrix;

	rParms.m_pLMData = 0;

  float fRadius = GetBBox().GetRadius();

	int nLod = CObjManager::GetObjectLOD(rParms.fDistance, GetLodRatioNormalized(), fRadius*0.5);

	if(nLod >= min(1+GetCVars()->e_voxel_lods_num,VOX_MAX_LODS_NUM))
		nLod = min(1+GetCVars()->e_voxel_lods_num,VOX_MAX_LODS_NUM) - 1;
	if(nLod<0)
		nLod=0;
	while(nLod && m_pVoxelMesh && !m_pVoxelMesh->GetRenderMesh(nLod))
		nLod--;

  m_pVoxelMesh->CheckUpdateLighting(nLod, this);

	IMaterial * pTerrainEf = NULL;
	GetTerrain()->GetMaterials(pTerrainEf);

	CRenderObject * pBasePassObj = GetIdentityCRenderObject();
  if (!pBasePassObj)
    return false;
	pBasePassObj->m_II.m_Matrix = m_Matrix;
	pBasePassObj->m_ObjFlags |= (rParms.dwFObjFlags & FOB_SELECTED) | FOB_TRANS_MASK;
  pBasePassObj->m_fDistance = rParms.fDistance;

  // cull lights per triangle for big objects
//  if(GetCVars()->e_voxel_cull_lights_per_triangle_min_obj_radius && m_pVoxelMesh->GetRenderMesh(nLod) &&
  //  fRadius > GetCVars()->e_voxel_cull_lights_per_triangle_min_obj_radius)
//    CObjManager::CullLightsPerTriangle(m_pVoxelMesh->GetRenderMesh(nLod), m_Matrix, rParms.nDLightMask, m_lightsCache);

  pBasePassObj->m_pShadowCasters = rParms.pShadowMapCasters;

  pBasePassObj->m_ObjFlags |= FOB_INSHADOW;

	pBasePassObj->m_DynLMMask = GetEntityVisArea() ? 0 : rParms.nDLightMask;
	
  bool bSunMayBeInUse = !GetEntityVisArea() || GetEntityVisArea()->IsConnectedToOutdoor();
	bool bIndoorNoAmboent(!bSunMayBeInUse && GetEntityVisArea() && !rParms.pShadowMapCasters && ((CVisArea*)GetEntityVisArea())->m_vAmbColor.GetLength()<0.1f);
	if(bIndoorNoAmboent)
		pBasePassObj->m_ObjFlags |= FOB_ONLY_Z_PASS;

	pBasePassObj->m_II.m_AmbColor = rParms.AmbientColor;
	if(m_pVoxelMesh->GetRenderMesh(nLod))
	{
		CTerrainNode * pLMNode = NULL;
		if(!GetEntityVisArea())
		{
			if(pLMNode = GetTerrain()->FindMinNodeContainingBox(m_WSBBox))
			{
				if(IsSnappedToTerrainSectors())
				{ // mark terrain sectors having voxel in it
//					assert(pLMNode->m_boxHeigtmap.GetSize().x == 64.f);
					pLMNode->m_bHasLinkedVoxel = true;

					int nSectorSize = GetTerrain()->GetSectorSize();
					CTerrainNode * arrNodes[5];
					arrNodes[0] = pLMNode;
					arrNodes[1] = GetTerrain()->GetSecInfo(pLMNode->m_nOriginX-nSectorSize, pLMNode->m_nOriginY);
					arrNodes[2] = GetTerrain()->GetSecInfo(pLMNode->m_nOriginX+nSectorSize, pLMNode->m_nOriginY);
					arrNodes[3] = GetTerrain()->GetSecInfo(pLMNode->m_nOriginX, pLMNode->m_nOriginY-nSectorSize);
					arrNodes[4] = GetTerrain()->GetSecInfo(pLMNode->m_nOriginX, pLMNode->m_nOriginY+nSectorSize);

					for(int i=0; i<5; i++)
					{
						CTerrainNode * pNode = arrNodes[i];
						while(pNode)
						{
							pNode->m_bMergeNotAllowed = true;
							pNode = pNode->m_pParent;
						}
					}
				}

				static ICVar *pTexturesStreamingIgnore = gEnv->pConsole->GetCVar("r_TexturesStreamingIgnore");

				if (pTexturesStreamingIgnore->GetIVal() == 0)
				{
				pLMNode = pLMNode->GetTexuringSourceNode(0, ett_Diffuse);
        pLMNode->RequestTextures();
				pLMNode->SetupTexturing(false);
				GetTerrain()->ActivateNodeTexture(pLMNode);
			}
		}
		}

		m_pVoxelMesh->GetRenderMesh(nLod)->SetMaterial(pTerrainEf);

		SSectorTextureSet texSet = (GetEntityVisArea() || !pLMNode) ? 
			SSectorTextureSet(GetTerrain()->m_nDefTerrainTexId, GetTerrain()->m_nBlackTexId) 
			: pLMNode->m_nTexSet;

		m_pVoxelMesh->SetupAmbPassMapping(ARR_TEX_OFFSETS_SIZE_DET_MAT, 
			pLMNode ? pLMNode->GetLeafData()->m_arrTexGen[0] : NULL, texSet, GetEntityVisArea()==NULL);
		m_pVoxelMesh->RenderAmbPass(nLod, EFSLIST_GENERAL, rParms.nAfterWater, pBasePassObj, rParms.nTechniqueID, rParms);
	}

	if(GetCVars()->e_detail_materials && !(_rParms.dwFObjFlags&FOB_RENDER_INTO_SHADOWMAP))
		if(m_pVoxelMesh)
			m_pVoxelMesh->RenderLightPasses(nLod, rParms.nDLightMask, this, (pBasePassObj->m_ObjFlags & FOB_INSHADOW)!=0, rParms.fDistance, rParms);

	if(GetCVars()->e_voxel_debug==1 && m_pVoxelVolume)
		m_pVoxelVolume->RenderDebug(NULL);

	return true;
}

void CVoxelObject::GetData(SVoxelChunkVer3 & chunk, Vec3 vOrigin)
{
	memset(&chunk, 0, sizeof(SVoxelChunkVer3));
	chunk.nChunkVersion = 3;
	chunk.vOrigin.x = (int) vOrigin.x;
	chunk.vOrigin.y = (int) vOrigin.y;
	chunk.vOrigin.z = (int) vOrigin.z;
	chunk.vSize.Set(DEF_VOX_VOLUME_SIZE,DEF_VOX_VOLUME_SIZE,DEF_VOX_VOLUME_SIZE);

	for(int m=0; m<VOX_MAX_SURF_TYPES_NUM; m++)
	{
		if(m_arrSurfacesPalette[m])
			strncpy(chunk.m_arrSurfaceNames[m],m_arrSurfacesPalette[m]->szName,sizeof(chunk.m_arrSurfaceNames[m]));
		else
			chunk.m_arrSurfaceNames[m][0]=0;
	}

	chunk.nFlags = m_nFlags;

	m_pVoxelVolume->GetVolume((ushort*)chunk.m_arrVolume, (ColorB*)chunk.m_arrColors);
}

void CVoxelObject::GetData(SVoxelChunkVer4 & chunk, Vec3 vOrigin)
{
  memset(&chunk, 0, sizeof(SVoxelChunkVer4));
  chunk.nChunkVersion = 4;
  chunk.vOrigin.x = (int) vOrigin.x;
  chunk.vOrigin.y = (int) vOrigin.y;
  chunk.vOrigin.z = (int) vOrigin.z;
  chunk.vSize.Set(DEF_VOX_VOLUME_SIZE,DEF_VOX_VOLUME_SIZE,DEF_VOX_VOLUME_SIZE);

  for(int m=0; m<VOX_MAX_SURF_TYPES_NUM; m++)
  {
    if(m_arrSurfacesPalette[m])
      strncpy(chunk.m_arrSurfaceNames[m],m_arrSurfacesPalette[m]->szName,sizeof(chunk.m_arrSurfaceNames[m]));
    else
      chunk.m_arrSurfaceNames[m][0]=0;
  }

  chunk.nFlags = m_nFlags;

//  GetVolume((ushort*)chunk.m_arrVolume, sizeof(chunk.m_arrVolume), nLayer);

//  memcpy(chunk.m_arrColors, m_arrColors.GetElements(), sizeof(chunk.m_arrColors));
}

int CVoxelObject::GetEditorObjectId() const
{
	return m_nEditorObjectId;
}

AABB CVoxelVolume::GetAABB()
{
	Vec3 vMin(0,0,0);
  Vec3 vMax(m_arrVolumeBackup.m_nSizeX*m_fUnitSize,m_arrVolumeBackup.m_nSizeY*m_fUnitSize,m_arrVolumeBackup.m_nSizeZ*m_fUnitSize);
	AABB aabb;
  aabb.SetTransformedAABB(m_pSrcArea->m_Matrix, AABB(vMin,vMax));
  return aabb;
}

void CVoxelObject::SetMatrix( const Matrix34& mat )
{
	Get3DEngine()->UnRegisterEntity(this);

	if(!CBrush::IsMatrixValid(mat))
	{
		Warning( "Error: IRenderNode::SetMatrix: Invalid matrix passed from the editor - ignored, reset to identity: %s", GetName());
		m_Matrix.SetIdentity();
	}
	else
		m_Matrix = mat;

	m_vPos = m_Matrix.GetTranslation();

  m_WSBBox = m_pVoxelVolume->GetAABB();

	Get3DEngine()->RegisterEntity(this);

	if(m_pPhysEnt)
	{
		// Just move physics.
		pe_params_pos par_pos;
		par_pos.pMtx3x4 = &m_Matrix;
		m_pPhysEnt->SetParams(&par_pos);

		// need to exclude from AI triangulation
		pe_params_foreign_data par_foreign_data;
		m_pPhysEnt->GetParams(&par_foreign_data);
		par_foreign_data.iForeignFlags |= PFF_EXCLUDE_FROM_STATIC;
		m_pPhysEnt->SetParams(&par_foreign_data);
	}
}

void CVoxelObject::SetSurfacesInfo( SVoxelChunkVer3 * pChunk )
{
  if(pChunk->nChunkVersion != 3)
    return;

  assert(pChunk->vSize.x == DEF_VOX_VOLUME_SIZE);
  assert(pChunk->vSize.y == DEF_VOX_VOLUME_SIZE);
  assert(pChunk->vSize.z == DEF_VOX_VOLUME_SIZE);

  if(	pChunk->vSize.x != DEF_VOX_VOLUME_SIZE || 
      pChunk->vSize.y != DEF_VOX_VOLUME_SIZE || 
      pChunk->vSize.z != DEF_VOX_VOLUME_SIZE )
    return;

  m_nFlags = pChunk->nFlags;

  for(int m=0; m<VOX_MAX_SURF_TYPES_NUM; m++)
  { // restore palette
    SSurfaceType * pLayers = GetTerrain()->GetSurfaceTypes();
    int nType=0;
    m_arrSurfacesPalette[m] = NULL;
    if(pChunk->m_arrSurfaceNames[m][0])
    {
      for(; nType<MAX_SURFACE_TYPES_COUNT; nType++)
      {
        if(stricmp((char*)pChunk->m_arrSurfaceNames[m],(char*)pLayers[nType].szName)==0)
        {
          m_arrSurfacesPalette[m] = &pLayers[nType];
          break;
        }
      }
    }

    if(nType==MAX_SURFACE_TYPES_COUNT) // use surface type 0
    {
      m_arrSurfacesPalette[m] = &pLayers[0];
      Error("CVoxelObject::SetSurfacesInfo: voxel object [%s] is referencing to undefined terrain surface type: %s", 
        m_pObjectName ? m_pObjectName : "", pChunk->m_arrSurfaceNames[m]);
    }
  }
}

void CVoxelObject::SetData( SVoxelChunkVer3 * pChunk, unsigned char ucChildId )
{
  SetSurfacesInfo(pChunk);

  if(!m_pVoxelVolume)
  {
    m_pVoxelVolume = new CVoxelVolume(DEF_VOX_UNIT_SIZE, DEF_VOX_VOLUME_SIZE, DEF_VOX_VOLUME_SIZE, DEF_VOX_VOLUME_SIZE);
    m_pVoxelVolume->m_pSrcArea = this;
  }

  m_pVoxelVolume->SetVolumeData( pChunk, ucChildId );
  m_pVoxelVolume->SubmitVoxelSpace();
  
  ScheduleRebuild();
}

bool CVoxelVolume::SetVolumeData( SVoxelChunkVer3 * pChunk, unsigned char ucChildId )
{
	if(pChunk->nChunkVersion != 3)
		return false;

	assert(pChunk->vSize.x == DEF_VOX_VOLUME_SIZE);
	assert(pChunk->vSize.y == DEF_VOX_VOLUME_SIZE);
	assert(pChunk->vSize.z == DEF_VOX_VOLUME_SIZE);

	if(	pChunk->vSize.x != DEF_VOX_VOLUME_SIZE || 
			pChunk->vSize.y != DEF_VOX_VOLUME_SIZE || 
			pChunk->vSize.z != DEF_VOX_VOLUME_SIZE )
		return false;

  m_arrVolume.Allocate(       pChunk->vSize.x, pChunk->vSize.y, pChunk->vSize.z);
  m_arrVolumeBackup.Allocate( pChunk->vSize.x, pChunk->vSize.y, pChunk->vSize.z);
  m_arrColors.Allocate(       pChunk->vSize.x, pChunk->vSize.y, pChunk->vSize.z);

	// detect empty voxel chunk volume
	bool bEmptyNode = true;
	for(int x=0; x<DEF_VOX_VOLUME_SIZE; x++)
	for(int y=0; y<DEF_VOX_VOLUME_SIZE; y++)
	for(int z=0; z<DEF_VOX_VOLUME_SIZE; z++)
	{
		if(pChunk->m_arrVolume[x][y][z])
		{
			bEmptyNode = false;
			break;
		}
	}

	m_pSrcArea->m_nFlags = pChunk->nFlags;

	if(bEmptyNode && !(m_pSrcArea->m_nFlags&IVOXELOBJECT_FLAG_LINK_TO_TERRAIN))
  {
    assert(!m_bUpdateRequested);
    CVoxelVolume(m_fUnitSize, DEF_VOX_VOLUME_SIZE, DEF_VOX_VOLUME_SIZE, DEF_VOX_VOLUME_SIZE);
		return true; // empty voxel sector
  }

	assert(DEF_VOX_VOLUME_SIZE == m_arrVolume.m_nSizeX);

	if(ucChildId)
	{ // copy just part of input volume - used for splitting
		int x1 = (ucChildId&1) ? DEF_VOX_VOLUME_SIZE/2 : 0;
		int y1 = (ucChildId&2) ? DEF_VOX_VOLUME_SIZE/2 : 0;
		int z1 = (ucChildId&4) ? DEF_VOX_VOLUME_SIZE/2 : 0;

		for(int x=0; x<DEF_VOX_VOLUME_SIZE; x++)
		for(int y=0; y<DEF_VOX_VOLUME_SIZE; y++)
		for(int z=0; z<DEF_VOX_VOLUME_SIZE; z++)
		{
			int x_read = x1 + x/2;
			int y_read = y1 + y/2;
			int z_read = z1 + z/2;

			m_arrVolume.GetAt(x,y,z) = pChunk->m_arrVolume[x_read][y_read][z_read];
      m_arrColors.GetAt(x,y,z) = pChunk->m_arrColors[x_read][y_read][z_read];
		}

		m_bUpdateRequested = true;
    if(!m_nUpdateRequestedFrameId)
      m_nUpdateRequestedFrameId = GetMainFrameID();
	}
	else
	{
		SetVolume((ushort*)pChunk->m_arrVolume, (ColorB*)pChunk->m_arrColors);	
	}

  ValidateSurfaceTypes();

	return true;
}

void CVoxelVolume::ValidateSurfaceTypes()
{
  // try to find any valid id
  ushort ucFirstGoodType=0;
  for(ucFirstGoodType=0; ucFirstGoodType<VOX_MAX_SURF_TYPES_NUM; ucFirstGoodType++)
    if(m_pSrcArea->m_arrSurfacesPalette[ucFirstGoodType])
      break;

  if(ucFirstGoodType>=VOX_MAX_SURF_TYPES_NUM)
    ucFirstGoodType = 0;

  bool bErrorFound=0;

  for(int x=0; x<DEF_VOX_VOLUME_SIZE; x++)
  {
    for(int y=0; y<DEF_VOX_VOLUME_SIZE; y++)
    {
      for(int z=0; z<DEF_VOX_VOLUME_SIZE; z++)
      {
        ushort usValue = m_arrVolume.GetAt(x,y,z);
        int nMatId = usValue & STYPE_BIT_MASK;
        assert(nMatId>=0 && nMatId<VOX_MAX_SURF_TYPES_NUM);
        if(!m_pSrcArea->m_arrSurfacesPalette[nMatId])
        { // if slot is empty - assign some valid type
          m_arrVolume.GetAt(x,y,z) = m_arrVolume.GetAt(x,y,z) & ~VOX_MAT_MASK;
          m_arrVolume.GetAt(x,y,z) |= ucFirstGoodType & VOX_MAT_MASK;
          bErrorFound = true;
        }
      }
    }
  }

  if(bErrorFound)
  {
    if(m_pSrcArea->m_arrSurfacesPalette[ucFirstGoodType])
    {
      Warning("CVoxelVolume::SetVolumeData: object [%s]:"
        "Voxel volume is referencing to empty surface types palette slot, Surface type is reset to %s", 
        m_pSrcArea->m_pObjectName ? m_pSrcArea->m_pObjectName : "",
        m_pSrcArea->m_arrSurfacesPalette[ucFirstGoodType]->szName);
    }
    else
    {
      Warning("CVoxelVolume::SetVolumeData: object [%s]:"
        "Voxel volume is referencing to empty surface types palette slot, Surface type reset is not possible", 
        m_pSrcArea->m_pObjectName ? m_pSrcArea->m_pObjectName : "");
      assert(!"Bad stuff");
    }
  }
}

IMemoryBlock * CVoxelObject::GetCompiledData()
{
  CMemoryBlock * pMemBlock = new CMemoryBlock;
  SVoxelChunkVer3 chunk;
  GetData(chunk,m_vPos);
  pMemBlock->SetData(&chunk,sizeof(chunk));
  return pMemBlock;
/*
  PrintMessage("Saving compiled voxel object %s ...", m_pObjectName ? m_pObjectName : "");

  // get volume data
  SVoxelChunkVer3 chunk;
	GetData(chunk,m_vPos);

  // tell that mesh data attached
  chunk.nFlags |= IVOXELOBJECT_FLAG_EXIST;

  // gets mesh data
  int nMeshSize = 0;
  byte * pMesh = GetCompiledMeshData(nMeshSize);

  // store both components
  CMemoryBlock * pMemBlock = new CMemoryBlock;
  pMemBlock->Allocate(sizeof(chunk) + nMeshSize);  
  memcpy(pMemBlock->GetData(), &chunk, sizeof(chunk));
  memcpy((byte*)pMemBlock->GetData() + sizeof(chunk), pMesh, nMeshSize);
  
  delete[] pMesh;

	return pMemBlock;*/
}

void CVoxelObject::SetCompiledData(void * pData, int nSize, unsigned char ucChildId)
{
  SVoxelChunkVer3 * pChunk = (SVoxelChunkVer3*)pData;

  if(nSize >= sizeof(SVoxelChunkVer3) && (pChunk->nFlags&IVOXELOBJECT_FLAG_EXIST))
  { // load volume and mesh
    PrintMessage("Loading compiled voxel object %s ...", m_pObjectName ? m_pObjectName : "");

    m_vPos = m_Matrix.GetTranslation();
    SetSurfacesInfo(pChunk);
    m_pVoxelVolume->SetVolumeData(pChunk, ucChildId);
    m_pVoxelVolume->SubmitVoxelSpace();
    if(nSize-sizeof(SVoxelChunkVer3)>0)
      SetCompiledMeshData((byte*)pData+sizeof(SVoxelChunkVer3), nSize-sizeof(SVoxelChunkVer3));
    Get3DEngine()->m_lstVoxelObjectsForUpdate.Delete(this);
  }
  else if(sizeof(SVoxelChunkVer3) == nSize)
  { // load volume and schedule compiling
    m_vPos = m_Matrix.GetTranslation();
    SetSurfacesInfo(pChunk);
    m_pVoxelVolume->SetVolumeData(pChunk, ucChildId);
    m_pVoxelVolume->SubmitVoxelSpace();
    ScheduleRebuild();
  }
  else
    Error("CVoxelObject::SetCompiledData: Unknown voxel data format [%s]", m_pObjectName ? m_pObjectName : "");
}

void CVoxelObject::SetObjectName( const char * pName )
{
  int nLen = strlen(pName)+1;
  SAFE_DELETE(m_pObjectName);
  m_pObjectName = new char[nLen];
  strcpy(m_pObjectName,pName);
}

bool CVoxelObject::ResetTransformation()
{
  m_pVoxelVolume->ResetTransformation();
  ScheduleRebuild();
  return true;
}

bool CVoxelVolume::ResetTransformation()
{
	SubmitVoxelSpace();

	for(int x=0; x<m_arrVolume.m_nSizeX; x++)
	for(int y=0; y<m_arrVolume.m_nSizeY; y++)
	for(int z=0; z<m_arrVolume.m_nSizeZ; z++)
	{
		int A[3]={0,0,0};

		for(int i=0; i<3; i++)
		{
			Vec3 vVec(i==0,i==1,i==2);
			vVec = m_pSrcArea->m_Matrix.TransformVector(vVec); vVec.Normalize();
			vVec = Vec3(int(vVec.x*1.1f),int(vVec.y*1.1f),int(vVec.z*1.1f));

			if(vVec.x==1.f)
				A[i] = x;
			else if(vVec.x==-1.f)
				A[i] = m_arrVolume.m_nSizeX-1-x;

			else if(vVec.y==1.f)
				A[i] = y;
			else if(vVec.y==-1.f)
				A[i] = m_arrVolume.m_nSizeY-1-y;

			else if(vVec.z==1.f)
				A[i] = z;
			else if(vVec.z==-1.f)
				A[i] = m_arrVolume.m_nSizeZ-1-z;

			else
			{ // reset failed
				m_arrVolume.CopyFrom(m_arrVolumeBackup);
				return false;
			}
		}

		m_arrVolume.GetAt(x,y,z) = m_arrVolumeBackup.GetAt(A[0],A[1],A[2]);
	}

	SubmitVoxelSpace();

	return true;
}

void CVoxelObject::ScheduleRebuild()
{
	if(Get3DEngine()->m_lstVoxelObjectsForUpdate.Find(this)<0)
		Get3DEngine()->m_lstVoxelObjectsForUpdate.Add(this);
}

float CVoxelObject::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio_detail*GetViewDistRatioNormilized());

	return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio*GetViewDistRatioNormilized());
}

struct IStatObj * CVoxelObject::GetEntityStatObj( unsigned int nPartId, unsigned int nSubPartId, Matrix34* pMatrix, bool bReturnOnlyVisible )
{
	if(pMatrix)
		*pMatrix = m_Matrix;

	return NULL;
}

extern int GetVecProjId(const Vec3 & vNorm,SSurfaceType * pLayer);

IMaterial *CVoxelObject::GetMaterial(Vec3 * pHitPos)
{
  return m_pVoxelVolume->GetMaterial(pHitPos);
}

IMaterial *CVoxelVolume::GetMaterial(Vec3 * pHitPos)
{ 
	if(pHitPos)
	{
		Vec3 vLocalPos = m_pSrcArea->m_Matrix.GetInverted().TransformPoint(*pHitPos);
		vLocalPos /= m_fUnitSize;
		int nMatId = 0;
		GetVoxelValue((int)vLocalPos.x, (int)vLocalPos.y, (int)vLocalPos.z, NULL, &nMatId, NULL);
		if(SSurfaceType * pLayer = m_pSrcArea->m_arrSurfacesPalette[nMatId])
    {
			IMaterial::EProjAxis projAxes[] = {IMaterial::ePA_XPos,IMaterial::ePA_YPos,IMaterial::ePA_ZPos,IMaterial::ePA_XNeg,IMaterial::ePA_YNeg,IMaterial::ePA_ZNeg};

			int nId = GetVecProjId(pHitPos[1],pLayer);
			return pLayer->GetMaterialOfProjection(projAxes[nId]);
    }
	}

	return NULL; 
}

void CVoxelVolume::SetVolume(ushort * pData, ColorB * pColors) 
{
  m_arrVolume.UpdateFrom(pData, m_arrVolume.GetDataSize());
  m_arrColors.UpdateFrom(pColors, m_arrColors.GetDataSize());

	m_bUpdateRequested = true;
  if(!m_nUpdateRequestedFrameId)
    m_nUpdateRequestedFrameId = GetMainFrameID();
}

void CVoxelVolume::GetVolume(ushort * pData, ColorB * pColors) 
{
  memcpy(pData, m_arrVolume.GetElements(), sizeof(pData[0])*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);
  memcpy(pColors, m_arrColors.GetElements(), sizeof(pColors[0])*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);
}

void CVoxelObject::InterpolateVoxelData()
{
  return m_pVoxelVolume->InterpolateVoxelData();
}

void CVoxelVolume::InterpolateVoxelData()
{
	SubmitVoxelSpace();

	CVoxelObject * arrNeighbours[3*3*3];
	bool bNeibFound = GetTerrain()->Voxel_FindNeighboursForObject(m_pSrcArea, arrNeighbours);

	for(int x=0; x<m_arrVolumeBackup.m_nSizeX; x++)
	for(int y=0; y<m_arrVolumeBackup.m_nSizeY; y++)
	for(int z=0; z<m_arrVolumeBackup.m_nSizeZ; z++)
	{
		int nMatId = 0;
		m_arrVolume.GetAt(x,y,z) = ushort(GetVoxelValueInterpolated(x, y, z, bNeibFound ? arrNeighbours : NULL, &nMatId, NULL, 2));
		m_arrVolume.GetAt(x,y,z) = (m_arrVolume.GetAt(x,y,z) & ~VOX_MAT_MASK) | (nMatId & VOX_MAT_MASK);
	}

	SubmitVoxelSpace();
}

int CVoxelVolume::GetMemoryUsage()
{
  int nSize = sizeof(*this);
  nSize += m_arrVolume.GetDataSize();
  nSize += m_arrVolumeBackup.GetDataSize();
  nSize += m_arrColors.GetDataSize();
  return nSize;
}

void CVoxelObject::GetMemoryUsage(ICrySizer * pSizer)
{ 
	SIZER_COMPONENT_NAME(pSizer, "VoxelObject");

	int nSize = sizeof(*this);

	if(m_pVoxelMesh)
		nSize += m_pVoxelMesh->GetMemoryUsage();

  if(m_pVoxelVolume)
    nSize += m_pVoxelVolume->GetMemoryUsage();

  for(int i=0; i<m_arrMeshesForSerialization.Count(); i++)
    nSize += m_arrMeshesForSerialization[i]->GetSize();

	pSizer->AddObject(this, nSize);
}

void CVoxelObject::SetFlags(int nFlags)
{
  int nFlagsIgnore = IVOXELOBJECT_FLAG_COMPUTE_AO | IVOXELOBJECT_FLAG_COMPILED | IVOXELOBJECT_FLAG_EXIST;
  if((nFlags&~nFlagsIgnore) != (m_nFlags&~nFlagsIgnore))
 		ScheduleRebuild();

	m_nFlags = nFlags;
}

void CVoxelObject::Regenerate()
{
	ScheduleRebuild();
}

void CVoxelObject::CopyHM()
{
  m_pVoxelVolume->CopyHM();
  ScheduleRebuild();
}

void CVoxelVolume::CopyHM()
{
	InitVoxelsFromHeightMap(false);
	SubmitVoxelSpace();
}

bool CVoxelObject::IsSnappedToTerrainSectors()
{
	int nSecSize = GetTerrain()->GetSectorSize();
	return !GetEntityVisArea() &&
		fabs(m_Matrix.m00-1.f)<0.001f &&
		fabs(m_Matrix.m11-1.f)<0.001f &&
		fabs(m_Matrix.m22-1.f)<0.001f &&
		(int(m_WSBBox.min.x)%nSecSize) == 0 &&
		(int(m_WSBBox.min.y)%nSecSize) == 0 &&
		(int(m_WSBBox.max.x)%nSecSize) == 0 &&
		(int(m_WSBBox.max.y)%nSecSize) == 0;
}

int CVoxelObject::GetAmbientOcclusionForPoint(Vec3 vPos, Vec3 vNorm, float & fRes)
{
	vPos = m_Matrix.GetInverted().TransformPoint(vPos);
	return m_pVoxelMesh ? m_pVoxelMesh->GetAmbientOcclusionForPoint(vPos, vNorm, fRes, this) : 0;
}

void CVoxelObject::OnRenderNodeBecomeVisible()
{
  assert(m_pRNTmpData);
  m_pRNTmpData->userData.objMat = m_Matrix;
  m_pRNTmpData->userData.fRadius = GetBBox().GetRadius();
}

void CVoxelObject::ReleaseMemBlocks()
{
  for(int i=0; i<m_arrMeshesForSerialization.Count(); i++)
  {
    m_arrMeshesForSerialization[i]->Release();
    m_arrMeshesForSerialization[i] = NULL;
  }
  m_arrMeshesForSerialization.Clear();
}

int CVoxelObject::GetCompiledMeshDataSize()
{
  int nSize = 0;

  nSize += sizeof(uint32); // count

  for(int i=0; i<m_arrMeshesForSerialization.Count(); i++)
  {
    nSize += sizeof(uint32); // sizes
    nSize += m_arrMeshesForSerialization[i]->GetSize(); // data size
  }

  return nSize;
}

byte * CVoxelObject::GetCompiledMeshData(int & nSize)
{
  nSize = GetCompiledMeshDataSize();
  byte * pData = new byte[nSize];

  byte * nPtr = pData;

  // count
  *(int*)nPtr = m_arrMeshesForSerialization.Count();
  nPtr += sizeof(uint32);

  for(int i=0; i<m_arrMeshesForSerialization.Count(); i++)
  {
    // data size
    *(int*)nPtr = m_arrMeshesForSerialization[i]->GetSize();
    nPtr += sizeof(uint32);

    // data
    memcpy(nPtr, m_arrMeshesForSerialization[i]->GetData(), m_arrMeshesForSerialization[i]->GetSize());
    nPtr += m_arrMeshesForSerialization[i]->GetSize();
  }

  return pData;
}

void CVoxelObject::SetCompiledMeshData(byte * pData, int nDataSize)
{
  LOADING_TIME_PROFILE_SECTION(GetSystem());

  byte * nPtr = pData;

  int nCount = *(int*)nPtr;
  nPtr += sizeof(uint32);

  for(int i=0; i<nCount; i++)
  {
    // data size
    int nSize = *(int*)nPtr;
    nPtr += sizeof(uint32);
    
    // data
    CMemoryBlock * pMB = new CMemoryBlock();
    pMB->SetData(nPtr, nSize);
    m_arrMeshesForSerialization.Add(pMB);
    nPtr += nSize;
  }

  if(!InitMaterials())
    return;

  if(!m_pVoxelMesh)
    m_pVoxelMesh = new CVoxelMesh(&m_arrSurfacesPalette[0]);

  m_pVoxelMesh->MakeRenderMeshsFromMemBlocks(this);
}

bool CVoxelObject::IsEmpty()
{
  CVoxelObject * arrNeighbours[3*3*3];
  bool bNeibFound = GetTerrain()->Voxel_FindNeighboursForObject(this, arrNeighbours);
  return m_pVoxelVolume->IsEmpty(arrNeighbours);
}

void CVoxelObject::MoveContent(const Vec3 &move)
{
	ushort arrVolume[DEF_VOX_VOLUME_SIZE][DEF_VOX_VOLUME_SIZE][DEF_VOX_VOLUME_SIZE];
	ColorB arrColors[DEF_VOX_VOLUME_SIZE][DEF_VOX_VOLUME_SIZE][DEF_VOX_VOLUME_SIZE];

	ushort *sourceVol=m_pVoxelVolume->m_arrVolume.GetElements();
	ColorB *sourceCol=m_pVoxelVolume->m_arrColors.GetElements();

	memset(arrVolume, 0, sizeof(ushort)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);
	memset(arrColors, 0, sizeof(ColorB)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);

	CVoxelObject *arrNeighbours[3*3*3];
  GetTerrain()->Voxel_FindNeighboursForObject(this, arrNeighbours);

	gEnv->pLog->Log("Voxel content moved %f %f %f", move.x, move.y, move.z);

	bool modified=false;
		
	if (move.x > 0.1)
	{
		memcpy(&arrVolume[1][0][0], sourceVol, sizeof(ushort)*(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);
		memcpy(&arrColors[1][0][0], sourceCol, sizeof(ColorB)*(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);

		if (arrNeighbours[1*3+1])
		{
			ushort *sourceVol2=arrNeighbours[1*3+1]->m_pVoxelVolume->m_arrVolume.GetElements();
			ColorB *sourceCol2=arrNeighbours[1*3+1]->m_pVoxelVolume->m_arrColors.GetElements();
			memcpy(arrVolume, &sourceVol2[(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE], sizeof(ushort)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);
			memcpy(arrColors, &sourceCol2[(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE], sizeof(ColorB)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);
		}

		m_pVoxelVolume->SetVolume((ushort *)arrVolume, (ColorB *)arrColors);
		modified=true;
	}
	else if (move.x < -0.1)
	{
		memcpy(arrVolume, &sourceVol[DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE], sizeof(ushort)*(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);
		memcpy(arrColors, &sourceCol[DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE], sizeof(ColorB)*(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);
		
		if (arrNeighbours[2*9+1*3+1])
		{
			ushort *sourceVol2=arrNeighbours[2*9+1*3+1]->m_pVoxelVolume->m_arrVolume.GetElements();
			ColorB *sourceCol2=arrNeighbours[2*9+1*3+1]->m_pVoxelVolume->m_arrColors.GetElements();
			memcpy(&arrVolume[DEF_VOX_VOLUME_SIZE-1][0][0], sourceVol2, sizeof(ushort)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);
			memcpy(&arrColors[DEF_VOX_VOLUME_SIZE-1][0][0], sourceCol2, sizeof(ColorB)*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE);
		}

		m_pVoxelVolume->SetVolume((ushort *)arrVolume, (ColorB *)arrColors);
		modified=true;
	}
	else if (move.y > 0.1)
	{
		for (int xi=0; xi<DEF_VOX_VOLUME_SIZE; xi++)
		{
			memcpy(&arrVolume[xi][1][0], &sourceVol[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE], sizeof(ushort)*(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE);
			memcpy(&arrColors[xi][1][0], &sourceCol[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE], sizeof(ColorB)*(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE);
		}

		if (arrNeighbours[1*9+1])
		{
			ushort *sourceVol2=arrNeighbours[1*9+1]->m_pVoxelVolume->m_arrVolume.GetElements();
			ColorB *sourceCol2=arrNeighbours[1*9+1]->m_pVoxelVolume->m_arrColors.GetElements();
			
			for (int xi=0; xi<DEF_VOX_VOLUME_SIZE; xi++)
			{
				memcpy(&arrVolume[xi][0][0], &sourceVol2[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE], sizeof(ushort)*DEF_VOX_VOLUME_SIZE);
				memcpy(&arrColors[xi][0][0], &sourceCol2[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE], sizeof(ColorB)*DEF_VOX_VOLUME_SIZE);
			}
		}

		m_pVoxelVolume->SetVolume((ushort *)arrVolume, (ColorB *)arrColors);
		modified=true;
	}
	else if (move.y < -0.1)
	{
		for (int xi=0; xi<DEF_VOX_VOLUME_SIZE; xi++)
		{
			memcpy(&arrVolume[xi][0][0], &sourceVol[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+DEF_VOX_VOLUME_SIZE], sizeof(ushort)*(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE);
			memcpy(&arrColors[xi][0][0], &sourceCol[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+DEF_VOX_VOLUME_SIZE], sizeof(ColorB)*(DEF_VOX_VOLUME_SIZE-1)*DEF_VOX_VOLUME_SIZE);
		}

		if (arrNeighbours[1*9+2*3+1])
		{
			ushort *sourceVol2=arrNeighbours[1*9+2*3+1]->m_pVoxelVolume->m_arrVolume.GetElements();
			ColorB *sourceCol2=arrNeighbours[1*9+2*3+1]->m_pVoxelVolume->m_arrColors.GetElements();

			for (int xi=0; xi<DEF_VOX_VOLUME_SIZE; xi++)
			{
				memcpy(&arrVolume[xi][DEF_VOX_VOLUME_SIZE-1][0], &sourceVol2[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE], sizeof(ushort)*DEF_VOX_VOLUME_SIZE);
				memcpy(&arrColors[xi][DEF_VOX_VOLUME_SIZE-1][0], &sourceCol2[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE], sizeof(ColorB)*DEF_VOX_VOLUME_SIZE);
			}
		}

		m_pVoxelVolume->SetVolume((ushort *)arrVolume, (ColorB *)arrColors);
		modified=true;
	}
	else if (move.z > 0.1)
	{
		for (int xi=0; xi<DEF_VOX_VOLUME_SIZE; xi++)
		{
			for (int yi=0; yi<DEF_VOX_VOLUME_SIZE; yi++)
			{
				memcpy(&arrVolume[xi][yi][1], &sourceVol[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+yi*DEF_VOX_VOLUME_SIZE], sizeof(ushort)*(DEF_VOX_VOLUME_SIZE-1));
				memcpy(&arrColors[xi][yi][1], &sourceCol[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+yi*DEF_VOX_VOLUME_SIZE], sizeof(ColorB)*(DEF_VOX_VOLUME_SIZE-1));
			}
		}

		if (arrNeighbours[1*9+1*3])
		{
			ushort *sourceVol2=arrNeighbours[1*9+1*3]->m_pVoxelVolume->m_arrVolume.GetElements();
			ColorB *sourceCol2=arrNeighbours[1*9+1*3]->m_pVoxelVolume->m_arrColors.GetElements();

			for (int xi=0; xi<DEF_VOX_VOLUME_SIZE; xi++)
			{
				for (int yi=0; yi<DEF_VOX_VOLUME_SIZE; yi++)
				{
					arrVolume[xi][yi][0]=sourceVol2[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+yi*DEF_VOX_VOLUME_SIZE+DEF_VOX_VOLUME_SIZE-1];
					arrColors[xi][yi][0]=sourceCol2[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+yi*DEF_VOX_VOLUME_SIZE+DEF_VOX_VOLUME_SIZE-1];
				}
			}
		}
		
		m_pVoxelVolume->SetVolume((ushort *)arrVolume, (ColorB *)arrColors);
		modified=true;
	}
	else if (move.z < -0.1)
	{
		for (int xi=0; xi<DEF_VOX_VOLUME_SIZE; xi++)
		{
			for (int yi=0; yi<DEF_VOX_VOLUME_SIZE; yi++)
			{
				memcpy(&arrVolume[xi][yi][0], &sourceVol[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+yi*DEF_VOX_VOLUME_SIZE+1], sizeof(ushort)*(DEF_VOX_VOLUME_SIZE-1));
				memcpy(&arrColors[xi][yi][0], &sourceCol[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+yi*DEF_VOX_VOLUME_SIZE+1], sizeof(ColorB)*(DEF_VOX_VOLUME_SIZE-1));
			}
		}

		if (arrNeighbours[1*9+1*3+2])
		{
			ushort *sourceVol2=arrNeighbours[1*9+1*3+2]->m_pVoxelVolume->m_arrVolume.GetElements();
			ColorB *sourceCol2=arrNeighbours[1*9+1*3+2]->m_pVoxelVolume->m_arrColors.GetElements();

			for (int xi=0; xi<DEF_VOX_VOLUME_SIZE; xi++)
			{
				for (int yi=0; yi<DEF_VOX_VOLUME_SIZE; yi++)
				{
					arrVolume[xi][yi][DEF_VOX_VOLUME_SIZE-1]=sourceVol2[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+yi*DEF_VOX_VOLUME_SIZE];
					arrColors[xi][yi][DEF_VOX_VOLUME_SIZE-1]=sourceCol2[xi*DEF_VOX_VOLUME_SIZE*DEF_VOX_VOLUME_SIZE+yi*DEF_VOX_VOLUME_SIZE];
				}
			}
		}

		m_pVoxelVolume->SetVolume((ushort *)arrVolume, (ColorB *)arrColors);
		modified=true;
	}

	if (modified)
	{
		m_pVoxelVolume->SubmitVoxelSpace();
		ScheduleRebuild();
	}
}

bool CVoxelObject::IsLocked()
{
	return Get3DEngine()->m_lstVoxelObjectsForUpdate.Find(this) >= 0;
}