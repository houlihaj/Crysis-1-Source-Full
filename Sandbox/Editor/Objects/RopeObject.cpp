////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TagPoint.cpp
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: CTagPoint implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "RopeObject.h"
#include <I3DEngine.h>
#include "Material/Material.h"
#include "PropertiesPanel.h"
#include "ShapePanel.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CRopeObject,CShapeObject)

//////////////////////////////////////////////////////////////////////////

#define LINE_CONNECTED_COLOR RGB(0,255,0)
#define LINE_DISCONNECTED_COLOR RGB(255,255,0)

//////////////////////////////////////////////////////////////////////////
class CRopePanelUI : public CPropertiesPanel
{
public:
	_smart_ptr<CVarBlock> pVarBlock;
	_smart_ptr<CRopeObject> m_pObject;

	CSmartVariable<float> mv_thickness;
	CSmartVariable<int> mv_numSegments;
	CSmartVariable<int> mv_numSides;
	CSmartVariable<int> mv_numSubVerts;
	CSmartVariable<int> mv_numPhysSegments;
	CSmartVariable<float> mv_texTileU;
	CSmartVariable<float> mv_texTileV;
	CSmartVariable<float> mv_anchorsRadius;
	CSmartVariable<bool> mv_smooth;

	CSmartVariableArray mv_PhysicsGroup;

	CSmartVariable<float> mv_mass;
	CSmartVariable<float> mv_tension;
	CSmartVariable<float> mv_friction;
	CSmartVariable<float> mv_frictionPull;
	
	CSmartVariable<Vec3> mv_wind;
	CSmartVariable<float> mv_windVariation;
	
	CSmartVariable<float> mv_airResistance;
	CSmartVariable<float> mv_waterResistance;

	//CSmartVariable<float> mv_jointLimit;
	CSmartVariable<float> mv_maxForce;

	CSmartVariable<bool> mv_collision;
	CSmartVariable<bool> mv_noCollisionAtt;
	CSmartVariable<bool> mv_subdivide;
	CSmartVariable<bool> mv_bindEnds;

	CRopePanelUI()
	{
		pVarBlock = new CVarBlock;

		mv_thickness->SetLimits( 0.001f,10000 );
		mv_numSegments->SetLimits( 2,1000 );
		mv_numSides->SetLimits( 2,100 );
		mv_numSubVerts->SetLimits( 1,100 );
		mv_numPhysSegments->SetLimits( 1,100 );

		//		pVarBlock->AddVariable( mv_selfShadowing,"Self Shadowing" );
		AddVariable( pVarBlock,mv_thickness,"Radius" );
		AddVariable( pVarBlock,mv_smooth,"Smooth" );
		AddVariable( pVarBlock,mv_numSegments,"Num Segments" );
		AddVariable( pVarBlock,mv_numSides,"Num Sides" );

		AddVariable( mv_PhysicsGroup,mv_subdivide,"Subdivide" );
		AddVariable( mv_PhysicsGroup,mv_numSubVerts,"Max Subdiv Verts" );

		AddVariable( pVarBlock,mv_texTileU,"Texture U Tiling" );
		AddVariable( pVarBlock,mv_texTileV,"Texture V Tiling" );

		AddVariable( pVarBlock,mv_bindEnds,"Bind Ends Radius" );
		AddVariable( pVarBlock,mv_anchorsRadius,"Bind Radius" );

		AddVariable( pVarBlock,mv_PhysicsGroup,"Physics Params" );

		AddVariable( mv_PhysicsGroup,mv_numPhysSegments,"Physical Segments" );
		AddVariable( mv_PhysicsGroup,mv_mass,"Mass" );
		AddVariable( mv_PhysicsGroup,mv_tension,"Tension" );
		AddVariable( mv_PhysicsGroup,mv_friction,"Friction" );
		AddVariable( mv_PhysicsGroup,mv_frictionPull,"Friction Pull" );

		AddVariable( mv_PhysicsGroup,mv_wind,"Wind" );
		AddVariable( mv_PhysicsGroup,mv_windVariation,"Wind Variation" );
		AddVariable( mv_PhysicsGroup,mv_airResistance,"Air Resistance" );
		AddVariable( mv_PhysicsGroup,mv_waterResistance,"Water Resistance" );
		//AddVariable( mv_PhysicsGroup,mv_jointLimit,"Joint Limit" );
		AddVariable( mv_PhysicsGroup,mv_maxForce,"Max Force" );
		mv_PhysicsGroup->SetDescription( "Maximal force rope can withstand before breaking" );
		
		AddVariable( mv_PhysicsGroup,mv_collision,"Check Collisions" );
		AddVariable( mv_PhysicsGroup,mv_noCollisionAtt,"Ignore Attachment Collisions" );
	}

	void AddVariables()
	{
		SetVarBlock( pVarBlock,functor(*this,&CRopePanelUI::OnVarChange) );
	}
	void SyncUI( IRopeRenderNode::SRopeParams &rp,bool bCopyToUI,IVariable *pModifiedVar=NULL )
	{
		SyncValue( mv_thickness,rp.fThickness,bCopyToUI,pModifiedVar );
		SyncValue( mv_numSegments,rp.nNumSegments,bCopyToUI,pModifiedVar );
		SyncValue( mv_numSides,rp.nNumSides,bCopyToUI,pModifiedVar );
		SyncValue( mv_numSubVerts,rp.nMaxSubVtx,bCopyToUI,pModifiedVar );
		SyncValue( mv_texTileU,rp.fTextureTileU,bCopyToUI,pModifiedVar );
		SyncValue( mv_texTileV,rp.fTextureTileV,bCopyToUI,pModifiedVar );
		SyncValue( mv_anchorsRadius,rp.fAnchorRadius,bCopyToUI,pModifiedVar );
		SyncValue( mv_numPhysSegments,rp.nPhysSegments,bCopyToUI,pModifiedVar );

		SyncValue( mv_mass,rp.mass,bCopyToUI,pModifiedVar );
		SyncValue( mv_tension,rp.tension,bCopyToUI,pModifiedVar );
		SyncValue( mv_friction,rp.friction,bCopyToUI,pModifiedVar );
		SyncValue( mv_frictionPull,rp.frictionPull,bCopyToUI,pModifiedVar );
		SyncValue( mv_wind,rp.wind,bCopyToUI,pModifiedVar );
		SyncValue( mv_windVariation,rp.windVariance,bCopyToUI,pModifiedVar );
		SyncValue( mv_airResistance,rp.airResistance,bCopyToUI,pModifiedVar );
		SyncValue( mv_waterResistance,rp.waterResistance,bCopyToUI,pModifiedVar );
		//SyncValue( mv_jointLimit,rp.jointLimit,bCopyToUI,pModifiedVar );
		SyncValue( mv_maxForce,rp.maxForce,bCopyToUI,pModifiedVar );

		SyncValueFlag( mv_bindEnds,rp.nFlags,IRopeRenderNode::eRope_BindEndPoints,bCopyToUI,pModifiedVar );
		SyncValueFlag( mv_collision,rp.nFlags,IRopeRenderNode::eRope_CheckCollisinos,bCopyToUI,pModifiedVar );
		SyncValueFlag( mv_noCollisionAtt,rp.nFlags,IRopeRenderNode::eRope_NoAttachmentCollisions,bCopyToUI,pModifiedVar );
		SyncValueFlag( mv_subdivide,rp.nFlags,IRopeRenderNode::eRope_Subdivide,bCopyToUI,pModifiedVar );
		SyncValueFlag( mv_smooth,rp.nFlags,IRopeRenderNode::eRope_Smooth,bCopyToUI,pModifiedVar );
	}
	void SetObject( CRopeObject *pObject )
	{
		m_pObject = pObject;
		if (pObject)
		{
			DeleteVars();
			SyncUI( pObject->m_ropeParams,true );
			AddVariables();
		}
	}
	void OnVarChange( IVariable *pVar )
	{
		CSelectionGroup *selection = GetIEditor()->GetSelection();
		for (int i = 0; i < selection->GetCount(); i++)
		{
			CBaseObject *pObj = selection->GetObject(i);
			if (pObj->IsKindOf(RUNTIME_CLASS(CRopeObject)))
			{
				CRopeObject *pObject = (CRopeObject*)pObj;
				if (CUndo::IsRecording())
					pObject->StoreUndo("Rope Modify Undo");
				SyncUI( pObject->m_ropeParams,false,pVar );
				pObject->m_bAreaModified = true;
				pObject->UpdateGameArea(false);
			}
		}
	}
};

inline void RopeParamsToXml( IRopeRenderNode::SRopeParams &rp,XmlNodeRef &node,bool bLoad )
{
	if (bLoad)
	{
		// Load.
		node->getAttr( "flags",rp.nFlags );
		node->getAttr( "radius",rp.fThickness );
		node->getAttr( "anchor_radius",rp.fAnchorRadius );
		node->getAttr( "num_seg",rp.nNumSegments );
		node->getAttr( "num_sides",rp.nNumSides );
		node->getAttr( "radius",rp.fThickness );
		node->getAttr( "texu",rp.fTextureTileU );
		node->getAttr( "texv",rp.fTextureTileV );
		node->getAttr( "ph_num_seg",rp.nPhysSegments );
		node->getAttr( "ph_sub_vtxs",rp.nMaxSubVtx );
		node->getAttr( "mass",rp.mass );
		node->getAttr( "friction",rp.friction );
		node->getAttr( "friction_pull",rp.frictionPull );
		node->getAttr( "wind",rp.wind );
		node->getAttr( "wind_var",rp.windVariance );
		node->getAttr( "air_resist",rp.airResistance );
		node->getAttr( "water_resist",rp.waterResistance );
		node->getAttr( "joint_lim",rp.jointLimit );
		node->getAttr( "max_force",rp.maxForce );
	}
	else
	{
		// Save.
		node->setAttr( "flags",rp.nFlags );
		node->setAttr( "radius",rp.fThickness );
		node->setAttr( "anchor_radius",rp.fAnchorRadius );
		node->setAttr( "num_seg",rp.nNumSegments );
		node->setAttr( "num_sides",rp.nNumSides );
		node->setAttr( "radius",rp.fThickness );
		node->setAttr( "texu",rp.fTextureTileU );
		node->setAttr( "texv",rp.fTextureTileV );
		node->setAttr( "ph_num_seg",rp.nPhysSegments );
		node->setAttr( "ph_sub_vtxs",rp.nMaxSubVtx );
		node->setAttr( "mass",rp.mass );
		node->setAttr( "friction",rp.friction );
		node->setAttr( "friction_pull",rp.frictionPull );
		node->setAttr( "wind",rp.wind );
		node->setAttr( "wind_var",rp.windVariance );
		node->setAttr( "air_resist",rp.airResistance );
		node->setAttr( "water_resist",rp.waterResistance );
		node->setAttr( "joint_lim",rp.jointLimit );
		node->setAttr( "max_force",rp.maxForce );
	}
}

namespace
{
	CRopePanelUI* s_ropePanelUI = NULL;
	int s_ropePanelUI_Id = 0;
}


//////////////////////////////////////////////////////////////////////////
CRopeObject::CRopeObject()
{
	mv_closed = false;
	//mv_castShadow = true;
	//mv_recvShadow = true;

	UseMaterialLayersMask(true);

	m_bPerVertexHeight = false;
	SetColor( LINE_DISCONNECTED_COLOR );

	m_entityClass = "RopeEntity";

	ZeroStruct( m_ropeParams );

	m_ropeParams.nFlags = IRopeRenderNode::eRope_BindEndPoints|IRopeRenderNode::eRope_CheckCollisinos;
	m_ropeParams.fThickness = 0.1f;
	m_ropeParams.fAnchorRadius = 0.1f;
	m_ropeParams.nNumSegments = 8;
	m_ropeParams.nNumSides = 4;
	m_ropeParams.nMaxSubVtx = 3;
	m_ropeParams.nPhysSegments = 8;
	m_ropeParams.mass = 1.0f;
	m_ropeParams.friction = 2;
	m_ropeParams.frictionPull = 2;
	m_ropeParams.wind.Set(0,0,0);
	m_ropeParams.windVariance = 0;
	m_ropeParams.waterResistance = 0;
	m_ropeParams.jointLimit = 0;
	m_ropeParams.maxForce = 0;
	m_ropeParams.airResistance = 0;
	m_ropeParams.fTextureTileU = 1.0f;
	m_ropeParams.fTextureTileV = 10.0f;

	memset( &m_linkedObjects,0,sizeof(m_linkedObjects) );

	m_endLinksDisplayUpdateCounter = 0;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::InitVariables()
{
	//CEntity::InitVariables();
}

///////////////////////////////////////////////////////	///////////////////
bool CRopeObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	bool bResult = 
	__super::Init(ie,prev,file);
	if (bResult && prev)
	{
		m_ropeParams = ((CRopeObject*)prev)->m_ropeParams;
	}
	else
	{
	}
	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::Done()
{
	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CRopeObject::CreateGameObject()
{
	__super::CreateGameObject();
	if (GetIEntity())
	{
		m_bAreaModified = true;
		UpdateGameArea(false);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::InvalidateTM( int nWhyFlags )
{
	__super::InvalidateTM( nWhyFlags );

	m_endLinksDisplayUpdateCounter = 0;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::SetMaterial( CMaterial *mtl )
{
	__super::SetMaterial(mtl);
	IRopeRenderNode *pRopeNode = GetRenderNode();
	if (pRopeNode != NULL)
	{
		if (mtl)
			mtl->AssignToEntity(pRopeNode);
		else
			pRopeNode->SetMaterial(0);
		uint8 nMaterialLayersFlags = GetMaterialLayersMask();
		pRopeNode->SetMaterialLayers( nMaterialLayersFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::Display( DisplayContext &dc )
{
	bool bPrevShowIcons = gSettings.viewports.bShowIcons;
	const Matrix34 &wtm = GetWorldTM();

	bool bLineWidth = 0;

	if (m_points.size() > 1)
	{
		IRopeRenderNode *pRopeNode = GetRenderNode();
		int nLinkedMask = 0;
		if (pRopeNode)
			nLinkedMask = pRopeNode->GetLinkedEndsMask();

		m_endLinksDisplayUpdateCounter++;
		if (pRopeNode && m_endLinksDisplayUpdateCounter > 10)
		{
			m_endLinksDisplayUpdateCounter = 0;
		
			IRopeRenderNode::SEndPointLink links[2];
			pRopeNode->GetEndPointLinks(links);

			float s = m_ropeParams.fAnchorRadius;

			bool bCalcBBox = false;
			for (int i = 0; i < 2; i++)
			{
				if (m_linkedObjects[i].m_nPhysEntityId)
				{
					IPhysicalEntity *pPhysEnt = gEnv->pPhysicalWorld->GetPhysicalEntityById(m_linkedObjects[i].m_nPhysEntityId);
					if (pPhysEnt)
					{
						// check if node 1 moved.
						pe_status_pos ppos;
						pPhysEnt->GetStatus( &ppos );
						if (m_linkedObjects[i].object_pos != ppos.pos || m_linkedObjects[i].object_q != ppos.q)
						{
							m_linkedObjects[i].object_pos = ppos.pos;
							m_linkedObjects[i].object_q = ppos.q;
							Matrix34 tm(ppos.q);
							tm.SetTranslation(ppos.pos);
							int nPointIndex = (i==0) ? 0 : GetPointCount()-1;
							m_points[nPointIndex] = wtm.GetInverted().TransformPoint(tm.TransformPoint(m_linkedObjects[i].offset));
							bCalcBBox = true;
						}
					}
					else
					{
						m_linkedObjects[i].m_nPhysEntityId = 0;
					}
				}
			}
			if (bCalcBBox)
				CalcBBox();
		}

		if (IsSelected())
		{
			gSettings.viewports.bShowIcons = false; // Selected Ropes hide icons.

			Vec3 p0 = wtm.TransformPoint(m_points[0]);
			Vec3 p1 = wtm.TransformPoint(m_points[m_points.size()-1]);

			float s = m_ropeParams.fAnchorRadius;

			if (nLinkedMask&0x01)
				dc.SetColor(ColorB(0,255,0));
			else
				dc.SetColor(ColorB(255,0,0));
			dc.DrawWireBox( p0-Vec3(s,s,s),p0+Vec3(s,s,s) );

			if (nLinkedMask&0x02)
				dc.SetColor(ColorB(0,255,0));
			else
				dc.SetColor(ColorB(255,0,0));
			dc.DrawWireBox( p1-Vec3(s,s,s),p1+Vec3(s,s,s) );
		}

		if ((nLinkedMask & 3) == 3)
			SetColor(LINE_CONNECTED_COLOR);
		else
			SetColor(LINE_DISCONNECTED_COLOR);
	}

	__super::Display( dc );
	gSettings.viewports.bShowIcons = bPrevShowIcons;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::UpdateGameArea( bool bRemove )
{
	if (bRemove)
		return;
	if (!m_bAreaModified)
		return;

	IRopeRenderNode *pRopeNode = GetRenderNode();
	if (pRopeNode)
	{
		std::vector<Vec3> points;
		if (GetPointCount() > 1)
		{
			const Matrix34 &wtm = GetWorldTM();
			points.resize(GetPointCount());
			for (int i = 0; i < GetPointCount(); i++)
			{
				points[i] = GetPoint(i);
			}
			if (!m_points[0].IsEquivalent(m_points[1]))
			{
				pRopeNode->SetMatrix( GetWorldTM() );

				pRopeNode->SetName( GetName() );
				CMaterial *mtl = GetMaterial();
				if (mtl)
					mtl->AssignToEntity(pRopeNode);

				uint8 nMaterialLayersFlags = GetMaterialLayersMask();
				pRopeNode->SetMaterialLayers( nMaterialLayersFlags );

				pRopeNode->SetParams( m_ropeParams );

				pRopeNode->SetPoints( &points[0],points.size() );

				IRopeRenderNode::SEndPointLink links[2];
				pRopeNode->GetEndPointLinks(links);
				for (int i = 0; i < 2; i++)
				{
					m_linkedObjects[i].offset = links[i].offset;
					m_linkedObjects[i].m_nPhysEntityId = gEnv->pPhysicalWorld->GetPhysicalEntityId(links[i].pPhysicalEntity);
					if (links[i].pPhysicalEntity)
					{
						pe_status_pos ppos;
						links[i].pPhysicalEntity->GetStatus(&ppos);
						m_linkedObjects[i].object_pos = ppos.pos;
						m_linkedObjects[i].object_q = ppos.q;
					}
				}

				// Both ends are connected.
				if (links[0].pPhysicalEntity && links[1].pPhysicalEntity)
				{
					//m_linkedObjects[0].pObject = GetObjectManager()->FindPhysicalObjectOwner(links[0].pPhysicalEntity);

					//SetColor(LINE_CONNECTED_COLOR);
				}
				else
				{
					//SetColor(LINE_DISCONNECTED_COLOR);
				}
			}
		}
		m_bAreaModified = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::OnParamChange( IVariable *var )
{
	if (!m_bIgnoreGameUpdate)
	{
		m_bAreaModified = true;
		UpdateGameArea(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::BeginEditParams( IEditor *ie,int flags )
{
	CBaseObject::BeginEditParams( ie,flags ); // First add standart dialogs.

	if (!s_ropePanelUI)
	{
		s_ropePanelUI = new CRopePanelUI();
		s_ropePanelUI_Id = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,_T("Rope Params"),s_ropePanelUI );
	}
	s_ropePanelUI->SetMultiSelect(false);
	s_ropePanelUI->SetObject(this);

	if (!m_panel)
	{
		m_panel = new CRopePanel;
		m_panel->Create( CRopePanel::IDD );
		m_rollupId = ie->AddRollUpPage( ROLLUP_OBJECTS,"Edit Rope",m_panel );
	}
	if (m_panel)
		m_panel->SetShape( this );

	CEntity::BeginEditParams( ie,flags ); // At the end add entity dialogs.
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::EndEditParams( IEditor *ie )
{
	__super::EndEditParams( ie );

	if (s_ropePanelUI)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,s_ropePanelUI_Id );
		s_ropePanelUI = 0;
		s_ropePanelUI_Id = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::BeginEditMultiSelParams( bool bAllOfSameType )
{
	CEntity::BeginEditMultiSelParams( bAllOfSameType );
	if (bAllOfSameType)
	{
		if (!s_ropePanelUI)
		{
			s_ropePanelUI = new CRopePanelUI();
			s_ropePanelUI_Id = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,_T("Rope Params"),s_ropePanelUI );
		}
		s_ropePanelUI->SetObject(this);
		s_ropePanelUI->SetMultiSelect(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::EndEditMultiSelParams()
{
	__super::EndEditMultiSelParams();

	if (s_ropePanelUI)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,s_ropePanelUI_Id );
		s_ropePanelUI = 0;
		s_ropePanelUI_Id = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::OnEvent( ObjectEvent event )
{
	__super::OnEvent( event );
	switch (event)
	{
	case EVENT_INGAME:
	case EVENT_OUTOFGAME:
		{
			IRopeRenderNode *pRenderNode = GetRenderNode();
			if (pRenderNode)
				pRenderNode->ResetPoints();
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::Serialize( CObjectArchive &ar )
{
	if (ar.bLoading)
	{
		// Loading.
		XmlNodeRef xmlNodeRope = ar.node->findChild("Rope");
		if (xmlNodeRope)
		{
			RopeParamsToXml( m_ropeParams,xmlNodeRope,ar.bLoading );
		}
	}
	else
	{
		// Saving.
		XmlNodeRef xmlNodeRope = ar.node->newChild("Rope");
		RopeParamsToXml( m_ropeParams,xmlNodeRope,ar.bLoading );
	}

	__super::Serialize( ar );
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::PostLoad( CObjectArchive &ar )
{
	__super::PostLoad(ar);
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CRopeObject::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	XmlNodeRef objNode = __super::Export( levelPath,xmlNode );
	if (objNode)
	{
		// Export Rope params.
		XmlNodeRef xmlNodeRope = objNode->newChild( "Rope" );
		RopeParamsToXml( m_ropeParams,xmlNodeRope,false );

		// Export Points
		if (!m_points.empty())
		{
			const Matrix34 &wtm = GetWorldTM();
			XmlNodeRef points = xmlNodeRope->newChild( "Points" );
			for (int i = 0; i < m_points.size(); i++)
			{
				XmlNodeRef pnt = points->newChild( "Point" );
				pnt->setAttr( "Pos",m_points[i] );
			}
		}
	}
	return objNode;
}

//////////////////////////////////////////////////////////////////////////
IRopeRenderNode* CRopeObject::GetRenderNode()
{
	if (m_entity)
	{
		IEntityRopeProxy *pRopeProxy = (IEntityRopeProxy*)m_entity->CreateProxy(ENTITY_PROXY_ROPE);
		if (pRopeProxy)
			return pRopeProxy->GetRopeRendeNode();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::CalcBBox()
{
	if (m_points.empty())
	{
		m_bbox.min = m_bbox.max = Vec3(0,0,0);
		return;
	}

	// When UnSelecting.
	// Reposition rope, so that rope object position is in the middle of the rope.

	const Matrix34 &wtm = GetWorldTM();
	Vec3 wp0 = wtm.TransformPoint(m_points[0]);
	Vec3 wp1 = wtm.TransformPoint(m_points[1]);

	Vec3 center = (wp0 + wp1)*0.5f;
	if (!center.IsEquivalent(wtm.GetTranslation(),0.01f))
	{
		// Center should move.
		Vec3 offset = wtm.GetInverted().TransformVector( center-wtm.GetTranslation() );
		for (int i = 0; i < m_points.size(); i++)
		{
			m_points[i] -= offset;
		}
		Matrix34 ltm = GetLocalTM();
		CUndoSuspend undoSuspend;
		SetPos( GetPos()+wtm.TransformVector(offset) );
	}

	// First point must always be 0,0,0.
	m_bbox.Reset();
	for (int i = 0; i < m_points.size(); i++)
	{
		m_bbox.Add( m_points[i] );
	}
	if (m_bbox.min.x > m_bbox.max.x)
	{
		m_bbox.min = m_bbox.max = Vec3(0,0,0);
	}
	BBox box;
	box.SetTransformedAABB( GetWorldTM(),m_bbox );
	m_lowestHeight = box.min.z;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::SetSelected( bool bSelect )
{
	if (IsSelected() && !bSelect && GetPointCount() > 1)
	{
		CalcBBox();
	}
	__super::SetSelected( bSelect );
}
