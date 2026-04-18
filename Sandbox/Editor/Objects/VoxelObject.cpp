////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   VoxelObject.cpp
//  Version:     v1.00
//  Created:     05/02/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: CVoxelObject implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ObjectManager.h"
#include "..\Viewport.h"
#include "..\VoxelObjectPanel.h"

#include "PanelTreeBrowser.h"

#include "Entity.h"
#include "Geometry\EdMesh.h"
#include "Material\Material.h"
#include "Material\MaterialManager.h"
#include "Settings.h"
#include "CryEditDoc.h"

#include <I3DEngine.h>
#include <IEntitySystem.h>
#include <IEntityRenderState.h>


#include "VoxelObject.h"


//////////////////////////////////////////////////////////////////////////
// CVoxelObject implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CVoxelObject,CBaseObject)

namespace
{
	CVoxelObjectPanel* s_voxelPanel = NULL;
	int s_voxelPanelId = 0;
}


//////////////////////////////////////////////////////////////////////////
CVoxelObject::CVoxelObject()
{
	m_pRenderNode = 0;
	m_pPrevRenderNode = 0;
	m_renderFlags = 0;

	// Init Variables.
	mv_outdoor = false;
	mv_linkToTerrain = false;
	mv_generateLODs = false;
	mv_computeAO = false;
	mv_snapToTerrain = false;
	mv_smartBaseColor = false;
  mv_castShadowMaps = false;
	mv_goodOccluder = false;
//  mv_recvShadowMaps = true;
	mv_castLightmap = true;
	mv_recvLightmap = false;

	mv_ratioLOD = 100;
	mv_ratioViewDist = 100;
	mv_lightmapQuality = 1;
	mv_ratioLOD.SetLimits( 0,255 );
	mv_ratioViewDist.SetLimits( 0,255 );
	mv_lightmapQuality.SetLimits( 0,100 );

	static CString sVarName_OutdoorOnly = "OutdoorOnly";
	static CString sVarName_LinkToTerrain = "LinkToTerrain";
	static CString sVarName_CastShadowMaps = "CastShadowMaps";
	static CString sVarName_RecvShadowMaps = "RecvShadowMaps";
	static CString sVarName_GoodOccluder = "GoodOccluder";
	static CString sVarName_CastLightmap = "CastRAMmap";
	static CString sVarName_LodRatio = "LodRatio";
	static CString sVarName_ViewDistRatio = "ViewDistRatio";
	static CString sVarName_NotTriangulate = "NotTriangulate";
	static CString sVarName_GenerateLODs = "GenerateLODs";
	static CString sVarName_ComputeAO = "ComputeAO";
	static CString sVarName_SnapToTerrain = "SnapToTerrain";
	static CString sVarName_SmartBaseColor = "SmartBaseColor";

	AddVariable( mv_outdoor,sVarName_OutdoorOnly,functor(*this,&CVoxelObject::OnRenderVarChange) );
	AddVariable( mv_castShadowMaps,sVarName_CastShadowMaps,functor(*this,&CVoxelObject::OnRenderVarChange) );
//	AddVariable( mv_recvShadowMaps,sVarName_RecvShadowMaps,functor(*this,&CVoxelObject::OnRenderVarChange) );
	AddVariable( mv_goodOccluder,sVarName_GoodOccluder,functor(*this,&CVoxelObject::OnRenderVarChange) );
	AddVariable( mv_castLightmap,sVarName_CastLightmap,functor(*this,&CVoxelObject::OnRenderVarChange) );
	AddVariable( mv_ratioLOD,sVarName_LodRatio,functor(*this,&CVoxelObject::OnRenderVarChange) );
	AddVariable( mv_ratioViewDist,sVarName_ViewDistRatio,functor(*this,&CVoxelObject::OnRenderVarChange) );
	AddVariable( mv_linkToTerrain,sVarName_LinkToTerrain,functor(*this,&CVoxelObject::OnRenderVarChange) );
	AddVariable( mv_generateLODs,sVarName_GenerateLODs,functor(*this,&CVoxelObject::OnRenderVarChange) );
	AddVariable( mv_computeAO,sVarName_ComputeAO,functor(*this,&CVoxelObject::OnRenderVarChange) );
	AddVariable( mv_snapToTerrain,sVarName_SnapToTerrain,functor(*this,&CVoxelObject::OnRenderVarChange) );
	AddVariable( mv_smartBaseColor,sVarName_SmartBaseColor,functor(*this,&CVoxelObject::OnRenderVarChange) );
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::BeginEditParams( IEditor *ie,int flags )
{
	CBaseObject::BeginEditParams( ie,flags );

	if (!s_voxelPanel)
	{
		s_voxelPanel = new CVoxelObjectPanel;
		s_voxelPanelId = ie->AddRollUpPage( ROLLUP_OBJECTS,_T("Voxel Object"),s_voxelPanel );
	}

	if (s_voxelPanel)
		s_voxelPanel->SetVoxelObject( this );
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::EndEditParams( IEditor *ie )
{
	CBaseObject::EndEditParams( ie );

	if (s_voxelPanel)
	{
		ie->RemoveRollUpPage( ROLLUP_OBJECTS,s_voxelPanelId );
		s_voxelPanel = 0;
		s_voxelPanelId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::BeginEditMultiSelParams( bool bAllOfSameType )
{
	CBaseObject::BeginEditMultiSelParams( bAllOfSameType );
	if (!s_voxelPanel)
	{
		s_voxelPanel = new CVoxelObjectPanel;
		s_voxelPanelId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,_T("Voxel Object"),s_voxelPanel );
	}

	if (s_voxelPanel)
		s_voxelPanel->SetVoxelObject( 0 );
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::EndEditMultiSelParams()
{
	CBaseObject::EndEditMultiSelParams();
	if (s_voxelPanel)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,s_voxelPanelId );
		s_voxelPanel = 0;
		s_voxelPanelId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::OnSizeChange(IVariable *pVar)
{
	InvalidateTM(	TM_POS_CHANGED | TM_ROT_CHANGED | TM_SCL_CHANGED);
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::OnRenderVarChange( IVariable *var )
{
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
bool CVoxelObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	SetColor( RGB(127,127,255) );

	if (IsCreateGameObjects())
	{
		if (prev)
		{
			CVoxelObject *VoxelObj = (CVoxelObject*)prev;
			m_pPrevRenderNode = VoxelObj->m_pRenderNode;
		}
	}

	// Must be after SetVoxel call.
	bool res = CBaseObject::Init( ie,prev,file );
	
	if (prev)
	{
		CVoxelObject *voxelObj = (CVoxelObject*)prev;
		m_bbox = voxelObj->m_bbox;
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CVoxelObject::CreateGameObject()
{
	if (!m_pRenderNode)
	{
		m_pRenderNode = GetIEditor()->Get3DEngine()->CreateRenderNode( eERType_VoxelObject );
		m_pRenderNode->SetEditorObjectId( GetId().Data1 );
    ((IVoxelObject*)m_pRenderNode)->SetObjectName(GetName());

		if(m_pRenderNode && m_pPrevRenderNode)
		{
			IMemoryBlock * pMB = ((IVoxelObject*)m_pPrevRenderNode)->GetCompiledData();
			if(pMB)
			{
				void * pData = pMB->GetData();
				int nSize = pMB->GetSize();
				((IVoxelObject*)m_pRenderNode)->SetCompiledData((uchar*)pData,nSize);
				pMB->Release();
			}
			m_pPrevRenderNode = 0;
		}

		UpdateEngineNode();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CVoxelObject::ResetTransformation()
{
	if(((IVoxelObject*)m_pRenderNode)->ResetTransformation())
	{
		Vec3 pos = GetPos();
		Quat q=GetRotation();
		BBox box;
		GetBoundBox( box );
		Vec3 midbb = Vec3(box.max.x+box.min.x, box.max.y+box.min.y, box.max.z+box.min.z)/2;
		Vec3 of = pos-midbb;
		pos = pos-of;
		of = of*q;
		pos = pos+of;
		SetPos(pos);
		Quat rot;
		rot.SetIdentity();
		SetRotation(rot);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CVoxelObject::Split()
{
	Matrix34 mt = GetWorldTM();
	mt.Scale(Vec3(0.5f, 0.5f, 0.5f));
	IMemoryBlock * pMB = ((IVoxelObject*)m_pRenderNode)->GetCompiledData();

	Vec3 v[]={
		Vec3( 64,   0,   0),
		Vec3(  0,  64,   0),
		Vec3( 64,  64,   0),
		Vec3(  0,   0,  64),
		Vec3( 64,   0,  64),
		Vec3(  0,  64,  64),
		Vec3( 64,  64,  64),
	};

	GetIEditor()->BeginUndo();

	std::vector<IRenderNode*> resultObjects;
	resultObjects.reserve(8);

	for(int i=0; i<7; i++)
	{
		CBaseObject * newObj = GetIEditor()->NewObject("VoxelObject");
		newObj->SetWorldTM(mt);
		newObj->SetPos(mt*v[i]);

		IRenderNode * pRenderNode = ((CVoxelObject*)newObj)->m_pRenderNode;
		if(pRenderNode)
		{
			((IVoxelObject*)pRenderNode)->SetCompiledData((uchar*)pMB->GetData(),pMB->GetSize(), i+1);
			resultObjects.push_back(pRenderNode);
		}
	}
	SetWorldTM(mt);

	if (CUndo::IsRecording())
		//CUndo::Record( new CUndoVoxelObject((IVoxelObject*)m_pRenderNode) );
		CUndo::Record( new CUndoVoxelObject(this) );


	((IVoxelObject*)m_pRenderNode)->SetCompiledData((uchar*)pMB->GetData(),pMB->GetSize(), 8);
	resultObjects.push_back(m_pRenderNode);

	if(pMB)
		pMB->Release();

	// smooth voxel data
	for(int i=0; i<resultObjects.size(); i++)
		((IVoxelObject*)resultObjects[i])->InterpolateVoxelData();

	GetIEditor()->AcceptUndo("Split voxel object");

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::Done()
{
	if (m_pRenderNode)
	{
		GetIEditor()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
		m_pRenderNode = 0;
	}

	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::UpdateEngineNode( bool bOnlyTransform )
{
	if (!m_pRenderNode)
		return;
	Matrix34 tm = GetWorldTM();
	m_pRenderNode->SetMatrix(tm);


	//////////////////////////////////////////////////////////////////////////
	// Set voxel render flags.
	//////////////////////////////////////////////////////////////////////////
	m_renderFlags = 0;
	if (mv_outdoor)
		m_renderFlags |= ERF_OUTDOORONLY;
	//	if (mv_castShadows)
	//	m_renderFlags |= ERF_CASTSHADOWVOLUME;
	//if (mv_selfShadowing)
		//m_renderFlags |= ERF_SELFSHADOW;
	if (mv_castShadowMaps)
		m_renderFlags |= ERF_CASTSHADOWMAPS;
//	if (mv_recvShadowMaps)
	//	m_renderFlags |= ERF_RECVSHADOWMAPS;
	if (mv_goodOccluder)
		m_renderFlags |= ERF_GOOD_OCCLUDER;

	if (mv_castLightmap)
		m_renderFlags |= ERF_CASTSHADOWINTORAMMAP;
	if (mv_recvLightmap)
		m_renderFlags |= ERF_USERAMMAPS;
//	if (mv_hideable)
//		m_renderFlags |= ERF_HIDABLE;
	if (IsSelected())
		m_renderFlags |= ERF_SELECTED;

	if(IsHidden())
		m_renderFlags |= ERF_HIDDEN;

	if(IsFrozen())
		m_renderFlags |= ERF_FROOZEN;

	m_pRenderNode->SetRndFlags( m_renderFlags );

	m_pRenderNode->SetViewDistRatio( mv_ratioViewDist );
	m_pRenderNode->SetLodRatio( mv_ratioLOD );
	m_pRenderNode->SetMinSpec( GetMinSpec() );
	m_pRenderNode->SetMaterialLayers( GetMaterialLayersMask() );
	m_renderFlags = m_pRenderNode->GetRndFlags();

	// Fast exit if only transformation needs to be changed.
	if (bOnlyTransform)
		return;


	if (GetMaterial())
	{
		GetMaterial()->AssignToEntity( m_pRenderNode );
	}
	else
	{
		// Reset all material settings for this node.
		m_pRenderNode->SetMaterial(0);
	}

	if(m_pRenderNode->GetRenderNodeType()==eERType_VoxelObject)
	{
		int nFlags = 0;
		nFlags |= mv_linkToTerrain ? IVOXELOBJECT_FLAG_LINK_TO_TERRAIN : 0;
		nFlags |= mv_generateLODs ? IVOXELOBJECT_FLAG_GENERATE_LODS : 0;
		nFlags |= mv_computeAO ? IVOXELOBJECT_FLAG_COMPUTE_AO : 0;		
		nFlags |= mv_snapToTerrain ? IVOXELOBJECT_FLAG_SNAP_TO_TERRAIN : 0;		
		nFlags |= mv_smartBaseColor ? IVOXELOBJECT_FLAG_SMART_BASE_COLOR : 0;		
		((IVoxelObject*)m_pRenderNode)->SetFlags(nFlags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::SetSelected( bool bSelect )
{
	CBaseObject::SetSelected( bSelect );
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::UpdateVisibility( bool visible )
{
	CBaseObject::UpdateVisibility( visible );
	UpdateEngineNode();
}


//////////////////////////////////////////////////////////////////////////
void CVoxelObject::SetHidden( bool bHidden )
{
	CBaseObject::SetHidden( bHidden );
	UpdateEngineNode();
}


//////////////////////////////////////////////////////////////////////////
void CVoxelObject::SetFrozen( bool bFrozen )
{
	CBaseObject::SetFrozen( bFrozen );
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::InvalidateTM( int nWhyFlags )
{
	CBaseObject::InvalidateTM(nWhyFlags);

	if (nWhyFlags & TM_RESTORE_UNDO) // Can skip updating game object when restoring undo.
		return;

	if (m_pRenderNode)
		UpdateEngineNode(true);
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::GetLocalBounds( BBox &box )
{
	box.min.x = 0;
	box.min.y = 0;
	box.min.z = 0;

	box.max.x = 64;
	box.max.y = 64;
	box.max.z = 64;
}

//////////////////////////////////////////////////////////////////////////
int CVoxelObject::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown || event == eMouseLUp)
	{
		Vec3 pos;
		pos = view->MapViewToCP( point );
		SetPos(pos);

		if (event == eMouseLDown)
			return MOUSECREATE_OK;

		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback( view,event,point,flags );
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::Display( DisplayContext &dc )
{
	const Matrix34 &wtm = GetWorldTM();
	Vec3 wp = wtm.GetTranslation();

	if (IsSelected())
		dc.SetSelectedColor();
	else if (IsFrozen())
		dc.SetFreezeColor();
	else
		dc.SetColor( GetColor() );

	dc.PushMatrix( wtm );
	BBox box;
	GetLocalBounds(box);
	dc.DrawWireBox( box.min,box.max);
	dc.PopMatrix();

	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::ConvertDataToString(char** ppOut, void * pData, int size)
{
	unsigned char * pBuf = (unsigned char *)pData;
	unsigned char * pOut = (unsigned char *)(*ppOut);
	for(int i=0; i<size; i++)
	{
		char idx = 'A' + pBuf[i]/16;
		unsigned char code = 'A' + (pBuf[i] - (pBuf[i]/16)*16);
		assert('A'<=code && code < 'A'+16);
		pOut[i*2] = idx;
		pOut[i*2+1] = code;
	}
	pOut[size*2] = 0;
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::StringToData(void ** ppOut, const char * pStr, int size)
{
	unsigned char * pBuf = (unsigned char *)(*ppOut);
	for(int i=0; i<size; i++)
	{
		char idx = pStr[i*2] - 'A';
		assert( 0<=idx && idx < 16);
		unsigned char code = unsigned char(pStr[i*2+1] - 'A') + idx*16;
		pBuf[i]=code;
	}
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::Serialize( CObjectArchive &ar )
{
	CBaseObject::Serialize( ar );

	int sizeDataBlock = 10000;

	if (ar.bLoading)
	{
		if(!m_pRenderNode)
			CreateGameObject();
		if(m_pRenderNode)
		{
			bool bLoadOld = false;
			CCryEditDoc * pEditDoc = GetIEditor()->GetDocument();
			if(pEditDoc && pEditDoc->GetTmpXmlArch())
			{
				void * pData = NULL;
				int nSize = 0;
				if(pEditDoc->GetTmpXmlArch()->pNamedData->GetDataBlock( GuidUtil::ToString(GetId()), pData, nSize ))
				{
					if(nSize>0)
					{
						((IVoxelObject*)m_pRenderNode)->SetCompiledData((uchar*)pData,nSize);
						bLoadOld = true;
					}
				}
			}
			if(!bLoadOld)
			{
				XmlNodeRef voxelDataParams = ar.node->findChild("VoxelDataParams");
				if(voxelDataParams)
				{
					int numPos;
					int unSize;
					voxelDataParams->getAttr("numNodes", numPos);
					voxelDataParams->getAttr("unSize", unSize);
					char * pStr = new char[numPos*sizeDataBlock+16]; // voxelDataNode->getContent() may return more than sizeDataBlock
					*pStr = 0;
					for(int pos=0; pos < numPos; pos++)
					{
						char name[16];
						sprintf(name, "VoxelData%d", pos);
						XmlNodeRef voxelDataNode = ar.node->findChild(name);
						if(voxelDataNode)
						{
							strcat(pStr, voxelDataNode->getContent());
							// trim spaces
							for(int len = strlen(pStr)-1; len>0 && pStr[len]==32; len--)
								pStr[len]=0;
						}
					}
					if(*pStr!=0)
					{
						int len = strlen(pStr);
						int nSize = len/2;

						CMemoryBlock mb;
						CMemoryBlock mbcomp;
						mb.Allocate(nSize, unSize);

						void * pVoidData = mb.GetBuffer();
						StringToData(&pVoidData, pStr, nSize);

						mb.Uncompress(mbcomp);
						((IVoxelObject*)m_pRenderNode)->SetCompiledData(mbcomp.GetBuffer(),mbcomp.GetSize());
					}
					delete [] pStr;
				}
			}
		}
		UpdateEngineNode();
	}
	else
	{
		if(m_pRenderNode)
		{
			IMemoryBlock * pMB = ((IVoxelObject*)m_pRenderNode)->GetCompiledData();
			/*
			CCryEditDoc * pEditDoc = GetIEditor()->GetDocument();
			if(pMB && pEditDoc && pEditDoc->GetTmpXmlArch())
			{
				if(pMB->GetSize()>0)
					pEditDoc->GetTmpXmlArch()->pNamedData->AddDataBlock( GuidUtil::ToString(GetId()),  pMB->GetData(), pMB->GetSize(), true );
			}
			else
			{
			*/
			CMemoryBlock mb;
			CMemoryBlock mbcomp;
			mb.Attach(pMB->GetData(), pMB->GetSize());
			mb.Compress(mbcomp);
			int len = mbcomp.GetSize()*2+1;
			char * pStr = new char[len];
			ConvertDataToString(&pStr, mbcomp.GetBuffer(), mbcomp.GetSize());
			int pos;
			char prev;
			for(pos=0; pos*sizeDataBlock < len; pos++)
			{
				char name[16];
				sprintf(name, "VoxelData%d", pos);
				XmlNodeRef voxelDataNode = ar.node->newChild(name);
				if(pos*sizeDataBlock < len-sizeDataBlock)
				{
					prev = pStr[pos*sizeDataBlock+sizeDataBlock];
					pStr[pos*sizeDataBlock+sizeDataBlock]=0;
				}
				voxelDataNode->setContent(&pStr[pos*sizeDataBlock]);
				if(pos*sizeDataBlock < len-sizeDataBlock)
					pStr[pos*sizeDataBlock+sizeDataBlock]=prev;
			}
			delete[] pStr;
			XmlNodeRef voxelDataNode = ar.node->newChild("VoxelDataParams");
			voxelDataNode->setAttr("numNodes", pos);
			voxelDataNode->setAttr("unSize", pMB->GetSize());
			//}
			if(pMB)
				pMB->Release();
		}
		ar.node->setAttr( "RndFlags",m_renderFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
IPhysicalEntity* CVoxelObject::GetCollisionEntity() const
{
	if (m_pRenderNode)
		return m_pRenderNode->GetPhysics();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CVoxelObject::HitTest( HitContext &hc )
{
	IPhysicalEntity * pPhEn = GetCollisionEntity();
	if(pPhEn)
	{
		ray_hit hit;
		int flags = rwi_stop_at_pierceable|rwi_ignore_terrain_holes;
		IPhysicalWorld *world = GetIEditor()->GetSystem()->GetIPhysicalWorld();
		//col = world->RayWorldIntersection( hc.raySrc,hc.rayDir,ent_static,flags,&hit,1,&pSkipEnt,1 );
		world->RayWorldIntersection( hc.raySrc,hc.rayDir*2000.0f,ent_static,flags,&hit, 1);
		if (hit.dist > 0 && !hit.bTerrain && hit.pCollider != 0 && hit.pCollider == pPhEn)
			return true;
		return false;
	}
	return true;

	/*
	Vec3 origin = GetWorldPos();

	BBox box;
	GetBoundBox(box);

	float radius = sqrt((box.max.x-box.min.x) * (box.max.x-box.min.x) + (box.max.y-box.min.y) * (box.max.y-box.min.y) + (box.max.z-box.min.z) * (box.max.z-box.min.z))/2;

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross( w );
	float d = w.GetLength();

	if (d < radius + hc.distanceTollerance)
	{
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
	*/
}

void CVoxelObject::Retriangulate( )
{
	if(m_pRenderNode->GetRenderNodeType()==eERType_VoxelObject)
		((IVoxelObject*)m_pRenderNode)->Regenerate();
}

void CVoxelObject::CopyHM( )
{
	if(m_pRenderNode->GetRenderNodeType()!=eERType_VoxelObject)
		return;

	GetIEditor()->BeginUndo();

	if (CUndo::IsRecording())
		CUndo::Record( new CUndoVoxelObject(this) );

	((IVoxelObject*)m_pRenderNode)->CopyHM();

	GetIEditor()->AcceptUndo("Copy heightmap into voxel volume");
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObject::SetMinSpec( uint32 nSpec )
{
	__super::SetMinSpec(nSpec);
	if (m_pRenderNode)
	{
		m_pRenderNode->SetMinSpec( GetMinSpec() );
		m_renderFlags = m_pRenderNode->GetRndFlags();
	}
}
