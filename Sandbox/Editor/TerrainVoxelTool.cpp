////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TerrainVoxelTool.cpp
//  Version:     v1.00
//  Created:     17/11/2004 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain Voxel Painter Tool implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "INITGUID.H"

#include <I3DEngine.h>
#include <IPhysics.h>
#include <IRenderAuxGeom.h>


#include "TerrainPanel.h"
#include "Viewport.h"
#include "TerrainVoxelPanel.h"
#include "Heightmap.h"
#include "Objects\DisplayContext.h"
#include "Objects\ObjectManager.h"
#include "Objects\VoxelObject.h"
#include "TerrainVoxelTool.h"
#include "ITransformManipulator.h"

#include "CryEditDoc.h"
#include "SurfaceType.h"




class CUndoTerrainVoxelTool : public IUndoObject
{
public:
	CUndoTerrainVoxelTool( CTerrainVoxelTool *pTool )
	{
		m_tool = pTool;
		m_Undo = *(m_tool->GetAlignPlaneTM());
	}
protected:
	virtual int GetSize() { return sizeof(*this); }
	virtual const char* GetDescription() { return ""; };

	virtual void Undo( bool bUndo )
	{
		if (bUndo)
		{
			m_Redo = *(m_tool->GetAlignPlaneTM());
		}
		*(m_tool->GetAlignPlaneTM()) = m_Undo;
		m_tool->UpdateTransformManipulator();
	}
	virtual void Redo()
	{
		*(m_tool->GetAlignPlaneTM()) = m_Redo;
		m_tool->UpdateTransformManipulator();
	}
private:
	CTerrainVoxelTool * m_tool;
	Matrix34 m_Redo;
	Matrix34 m_Undo;
};


//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTerrainVoxelTool,CEditTool)

CTerrainVoxelBrush CTerrainVoxelTool::m_brush[eVoxelBrushTypeLast];
VoxelBrushType			CTerrainVoxelTool::m_currentBrushType = eVoxelBrushCreate;
VoxelBrushType			CTerrainVoxelTool::m_prevBrushType = eVoxelBrushTypeLast;
VoxelBrushShape			CTerrainVoxelTool::m_currentBrushShape = eVoxelBrushShapeSphere;

VoxelPaintParams		CTerrainVoxelTool::m_vpParams;


static IClassDesc *s_pClassTool = 0;

bool CTerrainVoxelTool::m_bIsPositionSetuped;
Matrix34 CTerrainVoxelTool::m_AlignPlaneTM;
float CTerrainVoxelTool::m_PlaneSize=100;



//////////////////////////////////////////////////////////////////////////
CTerrainVoxelTool::CTerrainVoxelTool()
{
	m_voxObjs.empty();

	m_pClassDesc = s_pClassTool;

	SetStatusText( _T("Terrain Voxel Painter") );
	m_panelId = 0;
	m_panel = 0;

	m_bSmoothOverride = false;
	m_bQueryHeightMode = false;
	m_bPaintingMode = false;
	m_bLowerTerrain = false;

	m_brush[eVoxelBrushSubtract].type = eVoxelBrushSubtract;
	m_brush[eVoxelBrushCreate].type = eVoxelBrushCreate;
	m_brush[eVoxelBrushMaterial].type = eVoxelBrushMaterial;
	m_brush[eVoxelBrushBlur].type = eVoxelBrushBlur;
	m_brush[eVoxelBrushCopyTerrain].type = eVoxelBrushCopyTerrain;

	m_brush[eVoxelBrushSubtract].shape = eVoxelBrushShapeSphere;
	m_brush[eVoxelBrushCreate].shape = eVoxelBrushShapeSphere;
	m_brush[eVoxelBrushMaterial].shape = eVoxelBrushShapeSphere;
	m_brush[eVoxelBrushBlur].shape = eVoxelBrushShapeSphere;
	m_brush[eVoxelBrushCopyTerrain].shape = eVoxelBrushShapeSphere;

	m_pBrush = &m_brush[m_currentBrushType];
	
	m_pointerPos(0,0,0);
	
	//m_bIsPositionSetuped = false;
	m_bIsSetupPosition = false;
	m_IsPlaneAlign = false;
	//m_PlaneSize = 100.0f;
	
	GetIEditor()->ClearSelection();

	m_AlignPlaneTM.SetIdentity();

	m_hPaintCursor = AfxGetApp()->LoadCursor( IDC_HAND_INTERNAL );
}

//////////////////////////////////////////////////////////////////////////
CTerrainVoxelTool::~CTerrainVoxelTool()
{
	m_pointerPos(0,0,0);

	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->CancelUndo();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::BeginEditParams( IEditor *ie,int flags )
{
	m_ie = ie;
	if (!m_panelId)
	{
		m_panel = new CTerrainVoxelPanel(this,AfxGetMainWnd());
		m_panelId = m_ie->AddRollUpPage( ROLLUP_TERRAIN,"Terrain Voxel Painter",m_panel );
		AfxGetMainWnd()->SetFocus();

		m_panel->SetBrush(*m_pBrush, m_vpParams.m_voxelType);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::EndEditParams()
{
	if (m_panelId)
	{
		m_ie->RemoveRollUpPage(ROLLUP_TERRAIN,m_panelId);
		m_panel = 0;
	}
	SetupPosition(false);
	PlaneAlign(false);
}


					/*
					HitContext hitInfo;
					if (view->HitTest( point,hitInfo ))
						if(hitInfo.object->GetClassDesc()->GetObjectType()==OBJTYPE_VOXEL)
							m_pCurObject = (CVoxelObject*) hitInfo.object;
				
					if(m_pCurObject)
						CUndo::Record( new CUndoVoxelObject(m_pCurObject, hit.pt,fRadius) );
						*/


//////////////////////////////////////////////////////////////////////////
bool CTerrainVoxelTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	EVoxelEditTarget vt = evetVoxelObjects;

	switch(event)
	{
	case eMouseLDown:
		{
			if(!m_bIsSetupPosition)
			{
				m_voxObjs.resize(0);

				if(vt == evetVoxelObjects)
				{
					GetIEditor()->BeginUndo();
				}
			}
		}
		break;
	case eMouseLUp:
		if(!m_bIsSetupPosition)
		{
			if(vt == evetVoxelObjects)
				GetIEditor()->AcceptUndo("Voxel object undo");
			view->ReleaseMouse();
		}
		if(m_bIsSetupPosition)
		{
			m_bIsPositionSetuped = true;
			GetIEditor()->AcceptUndo( "Move Plane" );
		}
		break;
	}

	Vec3 raySrc,rayDir;
	GetIEditor()->GetActiveView()->ViewToWorldRay(point,raySrc,rayDir); rayDir.Normalize();

	float fRadius = m_brush[m_currentBrushType].radius;

	Vec3 pos;
	bool bIsComputedPos = false;

	if(m_IsPlaneAlign)
	{
		Vec3 p1 = m_AlignPlaneTM * Vec3( 0,0,0 );
		Vec3 p2 = m_AlignPlaneTM * Vec3( 1,0,0 );
		Vec3 p3 = m_AlignPlaneTM * Vec3( 0,1,0 );

		Plane plane = Plane::CreatePlane(p1,p2,p3);

		Vec3 output;
		if(Intersect::Ray_Plane( Ray(raySrc, rayDir), plane, output))
		{
			pos = output;
			bIsComputedPos = true;

			if(m_brush[m_currentBrushType].depth>0)
			{
				Vec3 norm = m_AlignPlaneTM * Vec3( 0, 0, 1) - m_AlignPlaneTM * Vec3( 0, 0, 0); 
				norm.Normalize();
				float dot = norm.Dot(-rayDir);
				Vec3 newpos = pos + (-rayDir)*(fRadius/dot*m_brush[m_currentBrushType].depth/100.0f);
				if((newpos-pos).len2()<256*fRadius*fRadius)
					pos = newpos;
			}
		}
	}
	else  // World Align (terain, voxels, objects)
	{
		int objTypes = ent_static|ent_terrain;
		int nFlags = rwi_stop_at_pierceable|rwi_ignore_terrain_holes;
		ray_hit hit;
		if(gEnv->pPhysicalWorld->RayWorldIntersection( raySrc,rayDir*1000.0f,objTypes,nFlags,&hit,1 ))
		{
			pos = hit.pt;
			if(m_bIsSetupPosition && !m_bIsPositionSetuped)
			{
				ITransformManipulator *pManipulator = GetIEditor()->ShowTransformManipulator(true);
				m_AlignPlaneTM.SetIdentity();
				m_AlignPlaneTM.SetTranslation( pos );
				pManipulator->SetTransformation( COORDS_LOCAL, m_AlignPlaneTM );
			}

			if(!m_bIsSetupPosition)
			{
				if(m_brush[m_currentBrushType].depth>0)
				{
					Vec3 axis = Vec3(1, 0, 0);
					if(fabs(rayDir.Cross(axis).len2())<0.1f)
						axis = Vec3(0, 1, 0);

					Vec3 rayTan = rayDir.Cross(axis);   rayTan.Normalize();
					Vec3 rayBin = rayDir.Cross(rayTan); rayBin.Normalize();

					Vec3 raySrc1 = raySrc + 0.5f * rayTan;
					Vec3 raySrc2 = raySrc + 0.5f * rayBin;
					
					Vec3 pos1, pos2;
					if(gEnv->pPhysicalWorld->RayWorldIntersection( raySrc1,rayDir*1000.0f,objTypes,nFlags,&hit,1 ))
					{
						pos1 = hit.pt;
						if(gEnv->pPhysicalWorld->RayWorldIntersection( raySrc2,rayDir*1000.0f,objTypes,nFlags,&hit,1))
						{
							pos2 = hit.pt;
							Vec3 norm = (pos2-pos).Cross(pos1-pos); norm.Normalize();
							float dot = norm.Dot(-rayDir);
							Vec3 newpos = pos + (-rayDir)*(fRadius/dot*m_brush[m_currentBrushType].depth/100.0f);
							if((newpos-pos).len2()<16*fRadius*fRadius + fRadius*0.2f)
								pos = newpos;
						}
					}
				}
				bIsComputedPos = true;
			}
		}
	}


	if(bIsComputedPos)
	{
		m_pointerPos = pos;
		if(flags&MK_LBUTTON)
		{
			int nSurfaceTypeID = -1;
			if(strlen(m_vpParams.m_SurfType)>0)
			{
				nSurfaceTypeID = GetIEditor()->GetDocument()->FindSurfaceType( m_vpParams.m_SurfType );

				EVoxelEditOperation eOperation;
				if(m_brush[m_currentBrushType].type==eVoxelBrushSubtract)
					eOperation = eveoSubstract;
				else if(m_brush[m_currentBrushType].type==eVoxelBrushCreate)
					eOperation = eveoCreate;
				else if(m_brush[m_currentBrushType].type==eVoxelBrushMaterial)
					eOperation = eveoMaterial;
				else if(m_brush[m_currentBrushType].type==eVoxelBrushBlur)
					eOperation = eveoBlur;
				else if(m_brush[m_currentBrushType].type==eVoxelBrushCopyTerrain)
					eOperation = eveoCopyTerrain;
				else
					return true;

				EVoxelBrushShape eShape;
				if(m_brush[m_currentBrushType].shape==eVoxelBrushShapeSphere)
					eShape = evbsSphere;
				else //if(m_brush[m_currentBrushType].shape==eVoxelBrushShapeBox)
					eShape = evbsBox;


				//if (CUndo::IsRecording())
					//CUndo::Record( new CUndoVoxelModify(pos,fRadius, vt == evetVoxelTerrain) );

				if(nSurfaceTypeID>=0 || eOperation!=eveoCreate || eOperation!=eveoMaterial)
				{
					if(vt == evetVoxelObjects)
					{
						IMemoryBlock * pMem = gEnv->p3DEngine->Voxel_GetObjects(pos, fRadius, 
						(eOperation==eveoCreate || eOperation==eveoMaterial) ? nSurfaceTypeID : -1, 
						eOperation, eShape, vt);

						IVoxelObject ** pData = (IVoxelObject **) pMem->GetData();
						int size = pMem->GetSize()/4;

						for(int i=0; i<size; i++)
						{
							bool find = false;
							for(std::vector<IVoxelObject*>::iterator it=m_voxObjs.begin(); it!=m_voxObjs.end(); it++)
							{
								if(pData[i] == *it)
								{
									find = true;
									break;
								}
							}
							if(!find)
							{
								m_voxObjs.push_back(pData[i]);
								if (CUndo::IsRecording())
								{
									CBaseObjectsArray objects;
									GetIEditor()->GetObjectManager()->GetObjects( objects );
									for (int j=0; j < objects.size(); j++)
									{  
										CBaseObject *obj = objects[j];
										if(obj->GetClassDesc()->GetObjectType()==OBJTYPE_VOXEL)
										{
											CVoxelObject * voxObj = (CVoxelObject *)obj;
											if(voxObj->GetRenderNode()==pData[i])
											{
												CUndo::Record( new CUndoVoxelObject( voxObj ));
												break;
											}
										}
									}
								}
							}
						}
						pMem->Release();
					}

					ColorF cfColor = ColorF((unsigned int) m_vpParams.m_Color );

					gEnv->p3DEngine->Voxel_Paint(pos, fRadius, 
					(eOperation==eveoCreate || eOperation==eveoSubstract || eOperation==eveoMaterial) ? nSurfaceTypeID : -1, Vec3(cfColor.r,cfColor.g,cfColor.b),
					eOperation, eShape, vt);
				}
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::Display( DisplayContext &dc )
{
	if(m_bIsSetupPosition || m_IsPlaneAlign)
	{
		dc.SetColor( RGB(64,0,0),0.4f);
		Vec3 p1 = m_AlignPlaneTM * Vec3(-m_PlaneSize/2,-m_PlaneSize/2, 0);
		Vec3 p2 = m_AlignPlaneTM * Vec3( m_PlaneSize/2,-m_PlaneSize/2, 0);
		Vec3 p3 = m_AlignPlaneTM * Vec3( m_PlaneSize/2, m_PlaneSize/2, 0);
		Vec3 p4 = m_AlignPlaneTM * Vec3(-m_PlaneSize/2, m_PlaneSize/2, 0);
		dc.DrawQuad(p1, p2, p3, p4);

		for(int i=-10; i<=10; i++)
		{
			if(i==-10 || i==10)
			{
				if(m_bIsSetupPosition)
					dc.SetColor(dc.GetSelectedColor());
				else
					dc.SetColor( RGB(255,255,128), 1);
			}
			else
				dc.SetColor( RGB(0,64,0), 1);
			Vec3 pos1 = m_AlignPlaneTM * Vec3(m_PlaneSize*i/10/2,-m_PlaneSize/2, 0);
			Vec3 pos2 = m_AlignPlaneTM * Vec3(m_PlaneSize*i/10/2, m_PlaneSize/2, 0);
			dc.DrawLine(pos1, pos2);
			pos1 = m_AlignPlaneTM * Vec3(-m_PlaneSize/2, m_PlaneSize*i/10/2, 0);
			pos2 = m_AlignPlaneTM * Vec3( m_PlaneSize/2, m_PlaneSize*i/10/2, 0);
			dc.DrawLine(pos1, pos2);
		}
	}

	if(!m_bIsSetupPosition)
	{
		ColorB color(150,200,255,100);
		float radius = m_brush[m_currentBrushType].radius;

		if(m_brush[m_currentBrushType].shape==eVoxelBrushShapeSphere)
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_pointerPos,radius,color);
		else if(m_brush[m_currentBrushType].shape==eVoxelBrushShapeBox)
		{
			AABB aabbBox(m_pointerPos-Vec3(radius,radius,radius),m_pointerPos+Vec3(radius,radius,radius));
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB(aabbBox,true,color,eBBD_Faceted);
		}
	}


	/*
	if (dc.flags & DISPLAY_2D)
		return;

	if (m_pBrush->type != eVoxelBrushSmooth)
	{
		dc.SetColor( 0.5f,1,0.5f,1 );
		dc.DrawTerrainCircle( m_pointerPos,m_pBrush->radiusInside,0.2f );
	}
	dc.SetColor( 0,1,0,1 );
	dc.DrawTerrainCircle( m_pointerPos,m_pBrush->radius,0.2f );
	if (m_pointerPos.z < m_pBrush->height)
	{
		if (m_pBrush->type != eVoxelBrushSmooth)
		{
			Vec3 p = m_pointerPos;
			p.z = m_pBrush->height;
			dc.SetColor( 1,1,0,1 );
			if (m_pBrush->type == eVoxelBrushFlatten)
				dc.DrawTerrainCircle( p,m_pBrush->radius,m_pBrush->height-m_pointerPos.z );
			dc.DrawLine( m_pointerPos,p );
		}
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainVoxelTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	bool bProcessed = false;
	if (nChar == VK_ADD)
	{
		m_pBrush->radius += 1;
		bProcessed = true;
	}
	if (nChar == VK_SUBTRACT)
	{
		if (m_pBrush->radius > 1)
			m_pBrush->radius -= 1;
		bProcessed = true;
	}
	if (nChar == VK_SHIFT)
	{
		if(m_prevBrushType == eVoxelBrushTypeLast)
			m_prevBrushType	=	m_currentBrushType;
		SetActiveBrushType(eVoxelBrushBlur);
		bProcessed = true;
	}
	if (nChar == VK_CONTROL)
	{
		if(m_prevBrushType == eVoxelBrushTypeLast)
			m_prevBrushType	=	m_currentBrushType;
		SetActiveBrushType(eVoxelBrushSubtract);
		bProcessed = true;
	}
	if (bProcessed)
	{
		UpdateUI();
	}
	return bProcessed;
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainVoxelTool::OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	bool bProcessed = false;
	if (nChar == VK_SHIFT)
	{
		if(m_prevBrushType != eVoxelBrushTypeLast)
			SetActiveBrushType(m_prevBrushType);
		m_prevBrushType	= eVoxelBrushTypeLast;
		bProcessed = true;
	}
	if (nChar == VK_CONTROL)
	{
		if(m_prevBrushType != eVoxelBrushTypeLast)
			SetActiveBrushType(m_prevBrushType);
		m_prevBrushType	= eVoxelBrushTypeLast;
		bProcessed = true;
	}
	if (bProcessed)
	{
		UpdateUI();
	}
	return bProcessed;
}


//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::UpdateUI()
{
	if (m_panel)
	{
		m_panel->SetBrush(*m_pBrush, m_vpParams.m_voxelType);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::Paint()
{
	/*
	CHeightmap *heightmap = GetIEditor()->GetHeightmap();
	int unitSize = heightmap->GetUnitSize();

	//dc.renderer->SetMaterialColor( 1,1,0,1 );
	int tx = RoundFloatToInt(m_pointerPos.y / unitSize);
	int ty = RoundFloatToInt(m_pointerPos.x / unitSize);

	float fInsideRadius = (m_pBrush->radiusInside / unitSize);
	int tsize = (m_pBrush->radius / unitSize);
	if (tsize == 0)
		tsize = 1;
	int tsize2 = tsize*2;
	int x1 = tx - tsize;
	int y1 = ty - tsize;

	if (m_pBrush->type == eVoxelBrushFlatten && !m_bSmoothOverride)
		heightmap->DrawSpot2( tx,ty,tsize,fInsideRadius,m_pBrush->height,m_pBrush->hardness,m_pBrush->bNoise,m_pBrush->noiseFreq/10.0f,m_pBrush->noiseScale/1000.0f );
	if (m_pBrush->type == eVoxelBrushRiseLower && !m_bSmoothOverride)
	{
		float h = m_pBrush->height;
		if (m_bLowerTerrain)
			h = -h;
		heightmap->RiseLowerSpot( tx,ty,tsize,fInsideRadius,h,m_pBrush->hardness,m_pBrush->bNoise,m_pBrush->noiseFreq/10.0f,m_pBrush->noiseScale/1000.0f );
	}
	else if (m_pBrush->type == eVoxelBrushSmooth || m_bSmoothOverride)
		heightmap->SmoothSpot( tx,ty,tsize,m_pBrush->height,m_pBrush->hardness );//,m_pBrush->noiseFreq/10.0f,m_pBrush->noiseScale/10.0f );

	heightmap->UpdateEngineTerrain( x1,y1,tsize2,tsize2,true,false );

	if (m_pBrush->bRepositionObjects)
	{
		BBox box;
		box.min = m_pointerPos - Vec3(m_pBrush->radius,m_pBrush->radius,0);
		box.max = m_pointerPos + Vec3(m_pBrush->radius,m_pBrush->radius,0);
		box.min.z -= 10000;
		box.max.z += 10000;
		// Make sure objects preserve height.
		GetIEditor()->GetObjectManager()->SendEvent( EVENT_KEEP_HEIGHT,box );
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainVoxelTool::OnSetCursor( CViewport *vp )
{
	if (m_bQueryHeightMode)
	{
		//SetCursor( m_hPickCursor );
		//return true;
	}
	/*
	if (m_bPaintingMode)
	{
		SetCursor( m_hPaintCursor );
		return true;
	}
	*/
	//if ((m_pBrush->type == eVoxelBrushFlatten || m_pBrush->type == eVoxelBrushRiseLower) && !m_bSmoothOverride)
	{
		//SetCursor( m_hFlattenCursor );
		//return true;
	}
	//else if (m_pBrush->type == eVoxelBrushSmooth || m_bSmoothOverride)
	{
		//SetCursor( m_hSmoothCursor );
		//return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::SetActiveBrushType( VoxelBrushType type  )
{
	m_currentBrushType = type;
	m_pBrush = &m_brush[m_currentBrushType];
	if (m_panel)
	{
		CTerrainVoxelBrush brush;
		GetBrush( brush );
		m_panel->SetBrush( brush, m_vpParams.m_voxelType );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::SetActiveBrushShape( VoxelBrushShape shape  )
{
	m_currentBrushShape = shape;
	m_pBrush = &m_brush[m_currentBrushType];
	m_pBrush->shape = shape;
	if (m_panel)
	{
		CTerrainVoxelBrush brush;
		GetBrush( brush );
		brush.shape = shape;
		m_panel->SetBrush( brush, m_vpParams.m_voxelType );
	}
}


//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::SetActiveVoxelObjectType( int type )
{
	m_vpParams.m_voxelType = type;
}



//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::Command_Activate()
{
	/*
	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CTerrainVoxelTool)))
	{
		// Already active.
		return;
	}
	pTool = new CTerrainVoxelTool;
	GetIEditor()->SetEditTool( pTool );
	GetIEditor()->SelectRollUpBar( ROLLUP_TERRAIN );
	AfxGetMainWnd()->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
	*/
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::Command_Flatten()
{
	/*
	Command_Activate();
	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CTerrainVoxelTool)))
	{
		CTerrainVoxelTool *pModTool = (CTerrainVoxelTool*)pTool;
		pModTool->SetActiveBrushType(eVoxelBrushFlatten);
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::Command_Smooth()
{
	/*
	Command_Activate();
	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CTerrainVoxelTool)))
	{
		CTerrainVoxelTool *pModTool = (CTerrainVoxelTool*)pTool;
		pModTool->SetActiveBrushType(eVoxelBrushSmooth);
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::Command_RiseLower()
{
	/*
	Command_Activate();
	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CTerrainVoxelTool)))
	{
		CTerrainVoxelTool *pModTool = (CTerrainVoxelTool*)pTool;
		pModTool->SetActiveBrushType(eVoxelBrushRiseLower);
	}
	*/
}


//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CTerrainVoxelTool_ClassDesc : public CRefCountClassDesc
{
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }

	//! Return the GUID of the class created by plugin.
	virtual REFGUID ClassID() 
	{
		return TERRAIN_VOXEL_TOOL_GUID;
	}

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() { return "EditTool.VoxelPaint"; };

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char* Category() { return "Terrain"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CTerrainVoxelTool); }
	//////////////////////////////////////////////////////////////////////////
};





//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::RegisterTool( CRegistrationContext &rc )
{
	rc.pClassFactory->RegisterClass( s_pClassTool = new CTerrainVoxelTool_ClassDesc );
	
	//rc.pCommandManager->RegisterCommand( "EditTool.TerrainVoxelTool.Activate",functor(CTerrainVoxelTool::Command_Activate) );
	//rc.pCommandManager->RegisterCommand( "EditTool.TerrainVoxelTool.Flatten",functor(CTerrainVoxelTool::Command_Flatten) );
	//rc.pCommandManager->RegisterCommand( "EditTool.TerrainVoxelTool.Smooth",functor(CTerrainVoxelTool::Command_Smooth) );
	//rc.pCommandManager->RegisterCommand( "EditTool.TerrainVoxelTool.RiseLower",functor(CTerrainVoxelTool::Command_RiseLower) );
}


void CTerrainVoxelTool::OnManipulatorDrag( CViewport *view,ITransformManipulator *pManipulator,CPoint &p0,CPoint &p1,const Vec3 &value )
{
	int editMode = GetIEditor()->GetEditMode();
	if (editMode == eEditModeMove)
	{
		GetIEditor()->RestoreUndo();

		if (CUndo::IsRecording())
			CUndo::Record( new CUndoTerrainVoxelTool(this) );

		Vec3 pos = m_AlignPlaneTM.GetTranslation();
		m_AlignPlaneTM.SetTranslation( pos + value);
		pManipulator->SetTransformation( COORDS_LOCAL, m_AlignPlaneTM );
	}
	else 
	if (editMode == eEditModeRotate)
	{
		GetIEditor()->RestoreUndo();
		if (CUndo::IsRecording())
			CUndo::Record( new CUndoTerrainVoxelTool(this) );

		Matrix34 rotateTM = Matrix34::CreateRotationXYZ( Ang3(value) );
		m_AlignPlaneTM = m_AlignPlaneTM * rotateTM;
		pManipulator->SetTransformation( COORDS_LOCAL, m_AlignPlaneTM );
	}
}


//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::SetupPosition(bool bIsSetup)
{
	if(bIsSetup)
	{
		ITransformManipulator *pManipulator = GetIEditor()->ShowTransformManipulator(true);
		if(!m_bIsPositionSetuped)
		{
			m_AlignPlaneTM.SetIdentity();
			m_AlignPlaneTM.SetTranslation( m_pointerPos );
		}
		pManipulator->SetTransformation( COORDS_LOCAL, m_AlignPlaneTM );
	}

	if(!bIsSetup && m_bIsSetupPosition)
	{
		m_bIsPositionSetuped = true;
		GetIEditor()->ShowTransformManipulator(false);
	}

	m_bIsSetupPosition = bIsSetup;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::PlaneAlign(bool bIsAlign)
{
	m_IsPlaneAlign = bIsAlign;
	if(bIsAlign)
		GetIEditor()->ShowTransformManipulator(false);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::UpdateTransformManipulator()
{
	if(m_bIsSetupPosition)
	{
		ITransformManipulator *pManipulator = GetIEditor()->ShowTransformManipulator(true);
		pManipulator->SetTransformation( COORDS_LOCAL, m_AlignPlaneTM );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelTool::InitPosition()
{
	m_bIsPositionSetuped = false;
	m_AlignPlaneTM.SetIdentity();
}