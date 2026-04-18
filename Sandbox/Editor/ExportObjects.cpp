#include "stdafx.h"
#include "ExportObjects.h"
#include "Objects/SelectionGroup.h"
#include "Objects/BaseObject.h"
#include "Objects/VoxelObject.h"
#include "Brush/SolidBrushObject.h"
#include "Geometry/EdGeometry.h"
#include "IIndexedMesh.h"
#include "OBJExporter1.h"
#include "I3DEngine.h"
#include "Objects/Entity.h"

namespace
{


void AddMeshToModel(COBJExporter & cModel, CBaseObject * obj, IIndexedMesh * pMesh)
{
	if(!pMesh)
		return;
	cModel.StartNewObject(obj->GetName());

	IIndexedMesh::SMeshDesc meshDesc;
	pMesh->GetMesh(meshDesc);

	const Matrix34 &tm = obj->GetWorldTM();

	for(int v=0; v<meshDesc.m_nVertCount; v++)
	{
		CVector3D vec;
		Vec3 tmp = tm.TransformPoint(meshDesc.m_pVerts[v]);
		vec.fX = tmp.x*100;
		vec.fY = tmp.y*100;
		vec.fZ = tmp.z*100;
		cModel.AddVertex(vec);
	}

	for(int v=0; v<meshDesc.m_nCoorCount; v++)
	{
		CTexCoord2D tc;
		tc.fU = meshDesc.m_pTexCoord[v].s;
		tc.fV = meshDesc.m_pTexCoord[v].t;
		cModel.AddTexCoord(tc);
	}

	if (meshDesc.m_nFaceCount == 0 && meshDesc.m_nIndexCount != 0 && meshDesc.m_pIndices != 0)
	{
		int nTris = meshDesc.m_nIndexCount/3;
		for(int f=0; f<nTris; f++)
		{
			CFace face;
			face.iVertexIndices[0]=meshDesc.m_pIndices[f*3+0];
			face.iVertexIndices[1]=meshDesc.m_pIndices[f*3+1];
			face.iVertexIndices[2]=meshDesc.m_pIndices[f*3+2];

			face.iTexCoordIndices[0]=meshDesc.m_pIndices[f*3+0];
			face.iTexCoordIndices[1]=meshDesc.m_pIndices[f*3+1];
			face.iTexCoordIndices[2]=meshDesc.m_pIndices[f*3+2];

			cModel.AddFace(face);
		}
	}
	else
	{
		for(int f=0; f<meshDesc.m_nFaceCount; f++)
		{
			CFace face;
			face.iVertexIndices[0]=meshDesc.m_pFaces[f].v[0];
			face.iVertexIndices[1]=meshDesc.m_pFaces[f].v[1];
			face.iVertexIndices[2]=meshDesc.m_pFaces[f].v[2];

			face.iTexCoordIndices[0]=meshDesc.m_pFaces[f].t[0];
			face.iTexCoordIndices[1]=meshDesc.m_pFaces[f].t[1];
			face.iTexCoordIndices[2]=meshDesc.m_pFaces[f].t[2];

			cModel.AddFace(face);
		}
	}
}


}



bool CExportObjects::Export( const char *pszFileName )
{
	COBJExporter cModel;

	CSelectionGroup *sel = GetIEditor()->GetSelection();
	for(int i=0; i<sel->GetCount(); i++)
	{
		CBaseObject *obj = sel->GetObject(i);
		CEdGeometry * pEdGeom = obj->GetGeometry();
		IIndexedMesh * pMesh=0;

		if(obj->GetType() == OBJTYPE_SOLID)
		{
			CSolidBrushObject *sobj = (CSolidBrushObject*)obj;
			pMesh = GetIEditor()->Get3DEngine()->CreateIndexedMesh();
			// Using not optimized mesh
			sobj->GetBrush()->GenerateIndexMesh(pMesh);
		}

		if (obj->GetType() == OBJTYPE_VOXEL)
		{
			CVoxelObject *pVoxelObj = (CVoxelObject*)obj;
			IRenderMesh *pRenderMesh = pVoxelObj->GetRenderNode()->GetRenderMesh(0);
			
			if (pRenderMesh != NULL)
			{
				pMesh = pRenderMesh->GetIndexedMesh();
			}
		}

		if(!pMesh && pEdGeom)
			pMesh = pEdGeom->GetIndexedMesh();

		if(pMesh)
			AddMeshToModel(cModel, obj, pMesh);

		if(!pMesh && obj->GetType() == OBJTYPE_ENTITY)
		{
			CEntity * pEnt = (CEntity *)obj;
			IEntity * pIEnt = pEnt->GetIEntity();
			if(pIEnt)
			{
				IStatObj *pStatObj = pIEnt->GetStatObj(0);
				if(pStatObj)
				{
					if (pStatObj && pStatObj->GetParentObject())
						pStatObj = pStatObj->GetParentObject();

					if(pStatObj->GetSubObjectCount())
					{
						for(int i = 0; i<pStatObj->GetSubObjectCount(); i++)
						{
							IStatObj::SSubObject * pSubObj = pStatObj->GetSubObject(i);
							if(pSubObj && pSubObj->nType==STATIC_SUB_OBJECT_MESH && pSubObj->pStatObj)
							{
								pMesh = pSubObj->pStatObj->GetIndexedMesh(true);
								AddMeshToModel(cModel, obj, pMesh);
							}
						}
					}
					else
					{
						pMesh = pStatObj->GetIndexedMesh(true);
						AddMeshToModel(cModel, obj, pMesh);
					}
				}
			}
		}

		if(!pMesh && obj->GetType() != OBJTYPE_VOXEL)
		{
			CLogFile::WriteLine("Error while getting the mesh from object!");
			char out[256];
			sprintf(out, "Critical error during exporting! \nObject: %s.", (const char*)obj->GetName() );
			AfxMessageBox(out);
			return false;
		}
	}


	// Write the model into the file
	if (!cModel.WriteOBJ(pszFileName))
	{
		CLogFile::WriteLine("Error while exporting part of the heightmap as OBJ model !");
		AfxMessageBox("Crititcal error during exporting !");
		return false;
	}

	return true;
}