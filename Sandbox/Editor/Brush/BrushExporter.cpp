////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   brushexporter.cpp
//  Version:     v1.00
//  Created:     15/12/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BrushExporter.h"
#include "CryEditDoc.h"

#include "Objects\BrushObject.h"
#include "Brush\SolidBrushObject.h"
#include "Objects\Group.h"
#include "Objects\ObjectManager.h"
#include "Material\Material.h"
#include "Brush.h"
#include "CryArray.h"
#include "Util\Pakfile.h"
#include "IChunkFile.h"

#include <I3DEngine.h>

#define BRUSH_SUB_FOLDER "Brush"
#define BRUSH_FILE "brush.lst"

//////////////////////////////////////////////////////////////////////////
void CBrushExporter::SaveMesh()
{
	/*
	std::vector<CBrushObject*> objects;
	m_indoor->GetObjects( objects );
	for (int i = 0; i < objects.size(); i++)
	{
		SaveObject( objects[i] );
	}
	*/
}

int CBrushExporter::AddChunk( const CHUNK_HEADER &hdr,CCryMemFile &file )
{
	int fsize = file.GetLength();
	return m_file.AddChunk( hdr,file.Detach(),fsize );
}

//////////////////////////////////////////////////////////////////////////
void CBrushExporter::SaveObject( CBrushObject *obj )
{
	/*
	SBrush *brush = obj->GetBrush();
	IStatObj *geom = 	brush->GetIndoorGeom();
	if (!geom)
		return;

	IRenderMesh *buf = geom->GetRenderMesh();
	int numVerts = buf->GetSysVertCount();
	int posStride,normalStride,uvStride;
	unsigned char *verts = buf->GetPosPtr( posStride );
	unsigned char *normals = buf->GetNormalPtr( normalStride );
	unsigned char *uvs = buf->GetUVPtr( uvStride );

  int numTris;
	ushort *pInds = buf->GetIndices(&numTris);
  numTris /= 3;

	
	MESH_CHUNK_DESC chunk;
	chunk.HasBoneInfo	= false;
	chunk.HasVertexCol = false;
	chunk.nFaces			= numTris;
	chunk.nTVerts			= numVerts;
	chunk.nVerts			= numVerts;
	chunk.VertAnimID	= 0;

	CCryMemFile memfile;
	CArchive ar( &memfile, CArchive::store );

	ar.Write( &chunk,sizeof(chunk) );

	std::vector<CryVertex> arrVerts;
	std::vector<CryFace> arrFaces;
	std::vector<CryUV> arrUVs;
	std::vector<CryTexFace> arrTexFaces;

	int i;

	// fill the vertices in
	arrVerts.resize(numVerts);
	arrUVs.resize(numVerts);
	for(i=0;i<numVerts;i++)
	{
		CryVertex& vert = arrVerts[i];
		vert.p = *((Vec3*)(verts));
		vert.n = *((Vec3*)(normals));
		arrUVs[i] = *((CryUV*)(uvs));

		normals += normalStride;
		verts += posStride;
		uvs += uvStride;
	}

	// fill faces.
	arrFaces.resize(numTris);
	arrTexFaces.resize(numTris);
	for (i = 0; i < numTris; i++) 
	{
		CryFace& mf = arrFaces[i];
		CryTexFace& texf = arrTexFaces[i];
		mf.v0	= pInds[i*3];
		mf.v1	= pInds[i*3+1];
		mf.v2	= pInds[i*3+2];
		mf.SmGroup = 0;
		mf.MatID = -1;

		texf.t0 = mf.v0;
		texf.t1 = mf.v1;
		texf.t2 = mf.v2;
	}

	// Save verts.
	ar.Write( &arrVerts[0], sizeof(arrVerts[0]) * arrVerts.size() );
	// Save faces.
	ar.Write( &arrFaces[0], sizeof(arrFaces[0]) * arrFaces.size() );

	// Save UVs
	ar.Write( &arrUVs[0], sizeof(arrUVs[0]) * arrUVs.size() );
	// Save tex faces.
	ar.Write( &arrTexFaces[0], sizeof(arrTexFaces[0]) * arrTexFaces.size() );
	ar.Close();
	
	int meshChunkId = AddChunk( chunk.chdr,memfile );

	//////////////////////////////////////////////////////////////////////////
	// Add node.
	//////////////////////////////////////////////////////////////////////////
	NODE_CHUNK_DESC ncd;
	ZeroStruct(ncd);
	ncd.MatID	= 0;
	ncd.nChildren	= 0;
	ncd.ObjectID	= meshChunkId;
	ncd.ParentID	= 0;

	ncd.IsGroupHead = false;
	ncd.IsGroupMember = false;
	ncd.rot.SetIdentity();

//	ncd.tm.SetIdentity();
	ncd.tm[0][0]=1;	ncd.tm[0][1]=0;	ncd.tm[0][2]=0;	ncd.tm[0][3]=0; 
	ncd.tm[1][0]=0;	ncd.tm[1][1]=1;	ncd.tm[1][2]=0;	ncd.tm[1][3]=0; 
	ncd.tm[2][0]=0;	ncd.tm[2][1]=0;	ncd.tm[2][2]=1;	ncd.tm[2][3]=0; 
	ncd.tm[3][0]=0;	ncd.tm[3][1]=0;	ncd.tm[3][2]=0;	ncd.tm[3][3]=1; 

	strcpy( ncd.name,"$0_sector_outside" );
	m_file.AddChunk( ncd.chdr,&ncd,sizeof(ncd) );
	*/
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CBrushExporter::ExportBrushes( const CString &path,const CString &levelName,CPakFile &pakFile )
{
	CLogFile::WriteLine( "Exporting Brushes...");

	int i;

	pakFile.RemoveDir( Path::Make( path,BRUSH_SUB_FOLDER ) );

	CString filename = Path::Make( path,BRUSH_FILE );
	// Export first brushes geometries.
	//if (!CFileUtil::OverwriteFile( filename ))
		//return;

	// Delete the old one.
	//DeleteFile( filename );
	CCryMemFile file;

	//////////////////////////////////////////////////////////////////////////
	// Clear export data.
	//////////////////////////////////////////////////////////////////////////
	std::vector<CBaseObject*> objects;
	GetIEditor()->GetObjectManager()->GetObjects( objects );

	m_levelName = levelName;

	for (i = 0; i < objects.size(); i++)
	{
		if (objects[i]->GetType() == OBJTYPE_BRUSH)
		{
			// Cast to brush.
			CBrushObject *obj = (CBrushObject*)objects[i];
			ExportBrush( path,obj );
		}
		else if (objects[i]->GetType() == OBJTYPE_SOLID)
		{
			// Cast to solid.
			CSolidBrushObject *obj = (CSolidBrushObject*)objects[i];
			ExportSolid( path,obj,pakFile );
		}
	}

	if (m_geoms.empty() || m_brushes.empty())
	{
		// Nothing to export.
		pakFile.RemoveFile( filename );
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	// Export to file.
	//////////////////////////////////////////////////////////////////////////
	/*
	CFile file;
	if (!file.Open( filename,CFile::modeCreate|CFile::modeWrite ))
	{
		Error( "Unable to open indoor geometry brush.lst file %s for writing",(const char*)filename );
		return;
	}
	*/

	//////////////////////////////////////////////////////////////////////////
	// Write brush file header.
	//////////////////////////////////////////////////////////////////////////
	SExportedBrushHeader header;
	ZeroStruct( header );
	memcpy( header.signature,BRUSH_FILE_SIGNATURE,3 );
	header.filetype = BRUSH_FILE_TYPE;
	header.version = BRUSH_FILE_VERSION;
	file.Write( &header,sizeof(header) );

	//////////////////////////////////////////////////////////////////////////
	// Write materials.
	//////////////////////////////////////////////////////////////////////////
	int numMtls = m_materials.size();
	file.Write( &numMtls,sizeof(numMtls) );
	for (i = 0; i < numMtls; i++)
	{
		// write geometry description.
		file.Write( &m_materials[i],sizeof(m_materials[i]) );
	}

	//////////////////////////////////////////////////////////////////////////
	// Write geometries.
	//////////////////////////////////////////////////////////////////////////
	// Write number of brush geometries.
	int numGeoms = m_geoms.size();
	file.Write( &numGeoms,sizeof(numGeoms) );
	for (i = 0; i < m_geoms.size(); i++)
	{
		// write geometry description.
		file.Write( &m_geoms[i],sizeof(m_geoms[i]) );
	}

	//////////////////////////////////////////////////////////////////////////
	// Write brush instances.
	//////////////////////////////////////////////////////////////////////////
	// write number of brushes.
	int numBrushes = m_brushes.size();
	file.Write( &numBrushes,sizeof(numBrushes) );
	for (i = 0; i < m_brushes.size(); i++)
	{
		// write geometry description.
		file.Write( &m_brushes[i],sizeof(m_brushes[i]) );
	}

	pakFile.UpdateFile( filename,file );

	CLogFile::WriteString("Done.");
}

//////////////////////////////////////////////////////////////////////////
void CBrushExporter::ExportBrush( const CString &path,CBrushObject *brushObject )
{
	IStatObj *prefab = brushObject->GetIStatObj();
	if (!prefab)
	{
		return;
	}
	CString geomName = prefab->GetFilePath();
	int geomIndex = stl::find_in_map( m_geomIndexMap,geomName,-1 );
	if (geomIndex < 0)
	{
		// add new geometry.
		SExportedBrushGeom geom;
		ZeroStruct( geom );
		geom.size = sizeof(geom);
		strcpy( geom.filename,geomName );
		geom.flags = 0;
		geom.m_minBBox = prefab->GetBoxMin();
		geom.m_maxBBox = prefab->GetBoxMax();
		m_geoms.push_back( geom );
		geomIndex = m_geoms.size()-1;
		m_geomIndexMap[geomName] = geomIndex;
	}
	CMaterial *pMtl =	brushObject->GetMaterial();
	int mtlIndex = -1;
	if (pMtl)
	{
		mtlIndex = stl::find_in_map( m_mtlMap,pMtl,-1 );
		if (mtlIndex < 0)
		{
			SExportedBrushMaterial mtl;
			mtl.size = sizeof(mtl);
			strncpy( mtl.material,pMtl->GetFullName(),sizeof(mtl.material) );
			m_materials.push_back(mtl);
			mtlIndex = m_materials.size()-1;
			m_mtlMap[pMtl] = mtlIndex;
		}
	}

	SExportedBrushGeom *pBrushGeom = &m_geoms[geomIndex];
	if (pBrushGeom)
	{
		if (brushObject->IsRecieveLightmap())
			pBrushGeom->flags |= SExportedBrushGeom::SUPPORT_LIGHTMAP;
		
		IStatObj *pBrushStatObj = brushObject->GetIStatObj();
		if (pBrushStatObj)
		{
			if (pBrushStatObj->GetPhysGeom(0) == NULL && pBrushStatObj->GetPhysGeom(1) == NULL)
			{
				pBrushGeom->flags |= SExportedBrushGeom::NO_PHYSICS;
			}
		}
	}

	SExportedBrush expbrush;
	expbrush.size = sizeof(expbrush);
	ZeroStruct( expbrush );
	CGroup *pGroup = brushObject->GetGroup();
	if (pGroup)
	{
		expbrush.mergeId = pGroup->GetGeomMergeId();
	}
	expbrush.id = brushObject->GetId().Data1;
	expbrush.matrix = brushObject->GetWorldTM();
	expbrush.geometry = geomIndex;
	expbrush.flags = brushObject->GetRenderFlags();
	expbrush.material = mtlIndex;
	expbrush.ratioLod = brushObject->GetRatioLod();
	expbrush.ratioViewDist = brushObject->GetRatioViewDist();
	m_brushes.push_back( expbrush );
}

//////////////////////////////////////////////////////////////////////////
void CBrushExporter::ExportSolid( const CString &path,CSolidBrushObject *pSolid,CPakFile &pakFile )
{
	SBrush *pBrush = pSolid->GetBrush();
	if (!pBrush)
		return;
	IStatObj *pStatObj = pBrush->GetIStatObj();
	if (!pStatObj)
		return;

	CString guidStr = GuidUtil::ToString(pSolid->GetId());
	CString sRealGeomFileName = Path::AddBackslash(path) + BRUSH_SUB_FOLDER + "\\" + pSolid->GetName() + "_" + guidStr + "." + CRY_GEOMETRY_FILE_EXT;
	CString sGeomFileName = Path::MakeGamePath(sRealGeomFileName);
	
	// Export Static Object to the CGF file inside level.pak
	{
		IChunkFile *pChunkFile = NULL;
		if (pStatObj->SaveToCGF( sGeomFileName,&pChunkFile ))
		{
			void *pMemFile = NULL;
			int nFileSize = 0;
			pChunkFile->WriteToMemory( &pMemFile,&nFileSize );
			pakFile.UpdateFile( sGeomFileName,pMemFile,nFileSize,true,ICryArchive::LEVEL_FASTER );
			pChunkFile->Release();
		}
	}

	// add new geometry.
	SExportedBrushGeom geom;
	ZeroStruct( geom );
	geom.size = sizeof(geom);
	strcpy( geom.filename,sGeomFileName );
	geom.flags = 0;
	geom.m_minBBox = pStatObj->GetBoxMin();
	geom.m_maxBBox = pStatObj->GetBoxMax();
	m_geoms.push_back( geom );
	int geomIndex = m_geoms.size()-1;
	//m_geomIndexMap[geomName] = geomIndex;

	CMaterial *pMtl =	pSolid->GetMaterial();
	int mtlIndex = -1;
	if (pMtl)
	{
		mtlIndex = stl::find_in_map( m_mtlMap,pMtl,-1 );
		if (mtlIndex < 0)
		{
			SExportedBrushMaterial mtl;
			mtl.size = sizeof(mtl);
			strncpy( mtl.material,pMtl->GetFullName(),sizeof(mtl.material) );
			m_materials.push_back(mtl);
			mtlIndex = m_materials.size()-1;
			m_mtlMap[pMtl] = mtlIndex;
		}
	}

	SExportedBrushGeom *pBrushGeom = &m_geoms[geomIndex];
	if (pBrushGeom)
	{
		//if (pSolid->IsRecieveLightmap())
			//pBrushGeom->flags |= SExportedBrushGeom::SUPPORT_LIGHTMAP;

		if (pStatObj)
		{
			if (pStatObj->GetPhysGeom(0) == NULL && pStatObj->GetPhysGeom(1) == NULL)
			{
				pBrushGeom->flags |= SExportedBrushGeom::NO_PHYSICS;
			}
		}
	}

	SExportedBrush expbrush;
	expbrush.size = sizeof(expbrush);
	ZeroStruct( expbrush );
	expbrush.id = pSolid->GetId().Data1;
	expbrush.matrix = pSolid->GetWorldTM();
	expbrush.geometry = geomIndex;
	expbrush.flags = pSolid->GetRenderFlags();
	expbrush.material = mtlIndex;
	expbrush.ratioLod = 100;
	expbrush.ratioViewDist = 100;
	//expbrush.ratioLod = pSolid->GetRatioLod();
	//expbrush.ratioViewDist = pSolid->GetRatioViewDist();
	m_brushes.push_back( expbrush );
}
