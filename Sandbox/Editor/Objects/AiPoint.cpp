////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   AIPoint.cpp
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: CAIPoint implementation.
// -------------------------------------------------------------------------
//  History:
//				09/12/04 Kirill - added removable flag
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AIPoint.h"
#include "ErrorReport.h"

#include "..\AIPointPanel.h"
#include <IAgent.h>

#include "..\Viewport.h"
#include "Settings.h"

#include <I3DEngine.h>
//#include <I3DIndoorEngine.h>
#include <IAISystem.h>

#define AIWAYPOINT_RADIUS 0.3f
#define DIR_VECTOR Vec3(0,-1,0)
#define UP_VECTOR Vec3(0,0,1)

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CAIPoint,CBaseObject)

int CAIPoint::m_rollupId = 0;
CAIPointPanel* CAIPoint::m_panel = 0;

float CAIPoint::m_helperScale = 1;

// keep track of all AI points - so it's possible to go from an AI node to
// a CAIPoint object
typedef std::set<CAIPoint*> TAllAIPoints;
static TAllAIPoints allAIPoints;

//////////////////////////////////////////////////////////////////////////
CAIPoint::CAIPoint():
m_bRemovable(false)
{
	m_nodeIndex = 0;
	m_aiNode = 0;
	m_bIndoorEntrance = false;
	m_bIndoors = false;
	m_aiType = EAIPOINT_WAYPOINT;
  m_aiNavigationType = EAINAVIGATION_HUMAN;

	m_bLinksValid = false;
	m_bIgnoreUpdateLinks = false;

  allAIPoints.insert(this);
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::Done()
{
	m_bLinksValid = false;
	if (m_aiNode)
	{
		IGraph *aiGraph = GetIEditor()->GetSystem()->GetAISystem() ? GetIEditor()->GetSystem()->GetAISystem()->GetNodeGraph() : NULL;
		if (aiGraph)
		{
			if (m_bIndoorEntrance)
				aiGraph->RemoveEntrance( aiGraph->GetBuildingIDFromWaypointNode(m_aiNode), m_nodeIndex );
			aiGraph->Disconnect(m_nodeIndex,true); // deletes the node
		}
		m_aiNode = 0;
		m_nodeIndex = 0;
	}

	while (!m_links.empty())
	{
		CAIPoint *obj = GetLink(0);
		if (obj)
			RemoveLink( obj );
		else
			m_links.erase(m_links.begin());
	}

	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
CAIPoint::~CAIPoint()
{
  allAIPoints.erase(this);
}

//////////////////////////////////////////////////////////////////////////
// OK to pass 0
inline void SetNodeType(GraphNode* pNode, EAIPointType type)
{
	if (!pNode)
		return;
  IGraph *aiGraph = GetIEditor()->GetSystem()->GetAISystem() ? GetIEditor()->GetSystem()->GetAISystem()->GetNodeGraph() : NULL;
  if (!aiGraph)
    return;
  assert(aiGraph->GetNavType(pNode) & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE));
	switch (type)
	{
	case EAIPOINT_WAYPOINT: aiGraph->SetWaypointNodeTypeInWaypointNode(pNode, WNT_WAYPOINT); break;
	case EAIPOINT_HIDE: aiGraph->SetWaypointNodeTypeInWaypointNode(pNode, WNT_HIDE); break;
	case EAIPOINT_HIDESECONDARY: aiGraph->SetWaypointNodeTypeInWaypointNode(pNode, WNT_HIDESECONDARY); break;
	case EAIPOINT_ENTRYEXIT: aiGraph->SetWaypointNodeTypeInWaypointNode(pNode, WNT_ENTRYEXIT); break;
	case EAIPOINT_EXITONLY: aiGraph->SetWaypointNodeTypeInWaypointNode(pNode, WNT_EXITONLY); break;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAIPoint::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	m_bLinksValid = false;

	SetColor( RGB(0,128,255) );
	bool res = CBaseObject::Init( ie,prev,file );

	if (IsCreateGameObjects() && GetIEditor()->GetSystem()->GetAISystem())
	{
		// Create AI graph node in game.
		IGraph *aiGraph = GetIEditor()->GetSystem()->GetAISystem() ? GetIEditor()->GetSystem()->GetAISystem()->GetNodeGraph() : NULL;
		if (aiGraph)
		{
			m_nodeIndex = aiGraph->CreateNewNode(m_aiNavigationType == EAINAVIGATION_HUMAN ? 
				IAISystem::NAV_WAYPOINT_HUMAN : IAISystem::NAV_WAYPOINT_3DSURFACE, Vec3(ZERO));
      m_aiNode = aiGraph->GetNode(m_nodeIndex);
		}
		SetNodeType(m_aiNode, m_aiType);
	}

	if (prev)
	{
		CAIPoint *pOriginal = (CAIPoint *)prev;
		SetAIType(pOriginal->m_aiType);
    SetAINavigationType(pOriginal->m_aiNavigationType);
	}
	return res;
}

//////////////////////////////////////////////////////////////////////////
CString CAIPoint::GetTypeDescription() const
{
	if (m_aiType == EAIPOINT_HIDE)
	{
		return CString(GetClassDesc()->ClassName()) + "(Hide)";
	}
	else if (m_aiType == EAIPOINT_ENTRYEXIT)
	{
		return CString(GetClassDesc()->ClassName()) + "(EntryExit)";
	}
	else if (m_aiType == EAIPOINT_EXITONLY)
	{
		return CString(GetClassDesc()->ClassName()) + "(ExitOnly)";
	}
	return GetClassDesc()->ClassName();
};

//////////////////////////////////////////////////////////////////////////
float CAIPoint::GetRadius()
{
	return AIWAYPOINT_RADIUS*m_helperScale * gSettings.gizmo.helpersScale;
}

//////////////////////////////////////////////////////////////////////////
bool CAIPoint::HitTest( HitContext &hc )
{
	Vec3 origin = GetWorldPos();
	float radius = GetRadius();

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross( w );
	float d = w.GetLengthSquared();

	if (d < radius*radius + hc.distanceTollerance)
	{
		Vec3 i0;
		if (Intersect::Ray_SphereFirst( Ray(hc.raySrc,hc.rayDir),Sphere(origin,radius),i0))
		{
			hc.dist = hc.raySrc.GetDistance(i0);
			return true;
		}
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::GetLocalBounds( BBox &box )
{
	float r = GetRadius();
	box.min = -Vec3(r,r,r);
	box.max = Vec3(r,r,r);
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::BeginEditParams( IEditor *ie,int flags )
{
	CBaseObject::BeginEditParams( ie,flags );

	// Unselect all links.
	for (int i = 0; i < GetLinkCount(); i++)
		SelectLink(i,false);

	if (!m_panel)
	{
		m_panel = new CAIPointPanel;
		m_rollupId = ie->AddRollUpPage( ROLLUP_OBJECTS,"AIPoint Parameters",m_panel );
	}
	if (m_panel)
		m_panel->SetObject( this );
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::EndEditParams( IEditor *ie )
{
	if (m_panel)
	{
		ie->RemoveRollUpPage( ROLLUP_OBJECTS,m_rollupId );
	}
	m_rollupId = 0;
	m_panel = 0;

	// Unselect all links.
	for (int i = 0; i < GetLinkCount(); i++)
		SelectLink(i,false);

	CBaseObject::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
int CAIPoint::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown)
	{
		Vec3 pos;
		if (GetIEditor()->GetAxisConstrains() != AXIS_TERRAIN)
		{
			pos = view->MapViewToCP(point);
		}
		else
		{
			// Snap to terrain.
			bool hitTerrain;
			pos = view->ViewToWorld( point,&hitTerrain );
			if (hitTerrain)
			{
				pos.z = GetIEditor()->GetTerrainElevation(pos.x,pos.y) + GetRadius();
			}
			pos = view->SnapToGrid(pos);
		}
		SetPos( pos );
		if (event == eMouseLDown)
			return MOUSECREATE_OK;
		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback( view,event,point,flags );
}

//////////////////////////////////////////////////////////////////////////
COLORREF CAIPoint::GetColor() const 
{ 
	if(!m_bRemovable)
		return CBaseObject::GetColor(); 
	return RGB(155,20,100);
};
//////////////////////////////////////////////////////////////////////////
void CAIPoint::Display( DisplayContext &dc )
{
	COLORREF color = GetColor();
	COLORREF clrSelectedLink = GetColor();
	IAISystem*	pAISystem = GetIEditor()->GetSystem()->GetAISystem();
	if(!pAISystem)
		return;
  IGraph *aiGraph = pAISystem->GetNodeGraph();
  if (!aiGraph)
    return;

  // not really the best way to do this - display shouldn't modify state, but the node
  // validity can get modified externally (e.g. area gets deleted)
  if (m_aiNode)
    m_bIndoors = aiGraph->GetBuildingIDFromWaypointNode(m_aiNode) != -1;

	const float	agentRad(0.5f);		// Approx human agent radius.
	const	float	testHeight(0.7f);	// Approx human crouch weapon height.

	if (!m_bIndoors)
	{
		// Draw invalid node.
		color = RGB(255,0,0);
	}
	Vec3 wp = GetWorldPos();
	if (m_aiType == EAIPOINT_HIDE)
	{
		color = RGB(0,255,0);	//<<FIXME>> Timur solve this in a better way.

		if (IsSelected() && GetIEditor()->GetSelection()->GetCount() < 10)
		{
			dc.SetColor( color,1 );
			dc.DrawCircle(wp+Vec3(0,0,0.2f), agentRad);
			dc.SetColor( RGB(255,255,255),0.5f );
			dc.DrawCircle(wp+Vec3(0,0,0.2f), agentRad + 0.25f);
		}

		if (IsSelected())
			dc.SetSelectedColor();
		else
			dc.SetColor( color,1 );

		//Vec3 arrowTrg = GetWorldTM().TransformPoint(0.5f*Vec3(0,-1,0));
		//dc.DrawArrow(wp,arrowTrg);
		Matrix34 tm = GetWorldTM();
		float sz = m_helperScale * gSettings.gizmo.helpersScale;
		tm.ScaleColumn( Vec3(sz,sz,sz) );
		dc.RenderObject( STATOBJECT_HIDEPOINT,tm );

		if (IsSelected())
		{
			// Draw helper indicating how the AI system will use the hide.
			// Display it only for small selections.
			if(GetIEditor()->GetSelection()->GetCount() < 10)
			{
				Vec3	pos(tm.GetTranslation());
				Vec3	dir(tm.TransformVector(DIR_VECTOR));
				Vec3	right(dir.y, -dir.x, 0.0f);
				right.NormalizeSafe();

				pAISystem->AdjustDirectionalCoverPosition(pos, dir, agentRad, testHeight);
				// Draw the are the agent occupies.
				const int npts(20);
				Vec3	circle[npts+1];
				// Collision
				dc.SetColor(color,0.3f);
				for(int i = 0; i < npts; i++)
				{
					float	a = ((float)i / (float)npts) * gf_PI2;
					circle[i] = pos + right * (cosf(a) * agentRad) + dir * (sinf(a) * agentRad);
				}
				for(int i = 0; i < npts; i++)
					dc.DrawTri(pos, circle[i], circle[(i + 1) % npts]);
				// Safe
				dc.SetColor(RGB(255,0,0), 0.5f);
				for( int i = 0; i <= 20; i++ )
				{
					float	a = ((float)i / (float)npts) * gf_PI2;
					circle[i] = pos + right * (cosf(a) * (agentRad + 0.25f)) + dir * (sinf(a) * (agentRad + 0.25f));
				}
				dc.DrawPolyLine(circle, npts+1);

				Vec3	groundPos(pos - Vec3(0,0,testHeight));

				// Direction to ground.
				dc.SetColor(RGB(255,0,0), 0.75f);
				dc.DrawLine(pos, groundPos + Vec3(0,0,0.15f));

				// Approx strafe line
				dc.SetColor(RGB(220,16,0), 0.5f);
				Vec3	strafePos(groundPos + Vec3(0,0,0.15f));
				dc.DrawQuad(strafePos - right * 2.5f, strafePos - right * 2.5f - dir * 0.05f,
					strafePos + right * 2.5f - dir * 0.05f, strafePos + right * 2.5f);
				// Approx crouch shoot height
				Vec3	crouchPos(groundPos + Vec3(0,0,0.5f) + dir * (0.7f + agentRad));
				dc.DrawQuad(crouchPos - right * 0.6f + dir * 0.3f, crouchPos - right * 0.6f,
					crouchPos + right * 0.6f, crouchPos + right * 0.6f + dir * 0.3f);
				// Approx standing shoot height.
				Vec3	standingPos(groundPos + Vec3(0,0,1.2f) + dir * (0.7f + agentRad));
				dc.DrawQuad(standingPos - right * 0.6f + dir * 0.3f, standingPos - right * 0.6f,
					standingPos + right * 0.6f, standingPos + right * 0.6f + dir * 0.3f);
			}
		}
	}
	else if (m_aiType == EAIPOINT_HIDESECONDARY)
	{
		color = RGB(0,255,0);	//<<FIXME>> Timur solve this in a better way.
		if (IsSelected())
			dc.SetSelectedColor();
		else
			dc.SetColor( color,1 );

		//Vec3 arrowTrg = GetWorldTM().TransformPoint(0.5f*Vec3(0,-1,0));
		//dc.DrawArrow(wp,arrowTrg);
		Matrix34 tm = GetWorldTM();
		float sz = m_helperScale * gSettings.gizmo.helpersScale;
		tm.ScaleColumn( Vec3(sz,sz,sz) );
		dc.RenderObject( STATOBJECT_HIDEPOINTSECONDARY,tm );

		if (IsSelected() && GetIEditor()->GetSelection()->GetCount() < 10)
		{
			dc.SetColor( color,1 );
			dc.DrawCircle(wp+Vec3(0,0,0.2f), agentRad);
			dc.SetColor( RGB(255,255,255),0.5f );
			dc.DrawCircle(wp+Vec3(0,0,0.2f), agentRad + 0.25f);
		}

	}
	else if (m_aiType == EAIPOINT_ENTRYEXIT || m_aiType == EAIPOINT_EXITONLY)
	{
		if (IsSelected())
			dc.SetSelectedColor();
		else
			dc.SetColor( color,1 );
		Matrix34 tm = GetWorldTM();
		float sz = m_helperScale * gSettings.gizmo.helpersScale;
		tm.ScaleColumn( Vec3(sz,sz,sz) );
		dc.RenderObject( STATOBJECT_ENTRANCE,GetWorldTM() );

		if (IsSelected() && GetIEditor()->GetSelection()->GetCount() < 10)
		{
			dc.SetColor( color,1 );
			dc.DrawCircle(wp+Vec3(0,0,0.2f), agentRad);
			dc.SetColor( RGB(255,255,255),0.5f );
			dc.DrawCircle(wp+Vec3(0,0,0.2f), agentRad + 0.25f);
		}
	}
	else
	{
		dc.SetColor( color,1 );
		dc.DrawBall( wp,GetRadius() );
		if (IsSelected())
		{
			dc.SetSelectedColor( 0.3f );
			dc.DrawBall( wp,GetRadius()+0.1f );
		}

		if (IsSelected() && GetIEditor()->GetSelection()->GetCount() < 10)
		{
			dc.SetColor( color,1 );
			dc.DrawCircle(wp+Vec3(0,0,0.2f), agentRad);
			dc.SetColor( RGB(255,255,255),0.5f );
			dc.DrawCircle(wp+Vec3(0,0,0.2f), agentRad + 0.25f);
		}
	}

	// colour or the little indicators
	COLORREF AIHideColor = RGB(255,255,0);

	COLORREF EditorLinkColor = RGB(0,255,255);						// blue
	COLORREF EditorHideLinkColor = RGB(255,0,255);					// purple
	COLORREF EditorForbiddenLinkColor = RGB(255,0,0);				// red (when selected)/invisible
	COLORREF EditorForbiddenHideLinkColor = RGB(255,255,0);	// yellow
	COLORREF AILinkColor = RGB(0,0,255);										// d blue
	COLORREF AIForbiddenLinkColor = RGB(128,0,0);						// dark red
	COLORREF AIHideLinkColor = RGB(180,0,180);							// d purple
	COLORREF ErrorColor = RGB(0,0,0);
	// Draw the AI links/verts
	if (m_aiNode)
	{
		/// Draw the vertices
		if (pAISystem && IsSelected() && GetIEditor()->GetSelection()->GetCount() == 1)
		{
			static const unsigned maxHidePts = 32;
			static Vec3 hidePts[maxHidePts];
			const Vec3& reqPos = aiGraph->GetNodePos(m_aiNode);
			unsigned num = pAISystem->GetHideSpotsInRange(0, reqPos, reqPos, 0.0f, 17.0f, true, false, maxHidePts, hidePts, 0, 0, 0, 0);
			dc.SetColor( AIHideColor );
			for (int i = 0; i < num; ++i)
			{
				const Vec3& pt = hidePts[i];
				dc.DrawBall(pt + Vec3(0, 0, GetRadius()), 0.1f);
			}
		}
		int numLinks = aiGraph->GetNumNodeLinks(m_aiNode);
		for (int i = 0; i < numLinks; i++)
		{
			unsigned linkId = aiGraph->GetGraphLink(m_aiNode, i);
			GraphNode *pNextNode = aiGraph->GetNextNode(linkId);
      if (!pNextNode || !(aiGraph->GetNavType(pNextNode) & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE)))
				continue;

			bool isHide = (aiGraph->GetWaypointNodeTypeFromWaypointNode(m_aiNode) == WNT_HIDE || aiGraph->GetWaypointNodeTypeFromWaypointNode(pNextNode) == WNT_HIDE);

			EWaypointLinkType linkType = aiGraph->GetOrigWaypointLinkTypeFromLink(linkId);
			if (linkType == WLT_EDITOR_PASS)
				if (isHide)
					dc.SetColor( EditorHideLinkColor );
				else
					dc.SetColor( EditorLinkColor );
			else if (linkType == WLT_EDITOR_IMPASS)
				if (isHide)
					dc.SetColor( EditorForbiddenHideLinkColor );
				else
					if (IsSelected())
						dc.SetColor( EditorForbiddenLinkColor );
					else
						continue;
			else if (linkType == WLT_AUTO_PASS)
				if (isHide)
					dc.SetColor( AIHideLinkColor );
				else
					dc.SetColor( AILinkColor );
			else if (linkType == WLT_AUTO_PASS)
        dc.SetColor(AIForbiddenLinkColor);
      else
				dc.SetColor( ErrorColor );
			dc.DrawLine( wp, aiGraph->GetNodePos(pNextNode) );
		}
	}

	/// Draw the editor links on top (todo this better)
	int numLinks = m_links.size();
	for (int i = 0; i < numLinks; i++)
	{
		CAIPoint *pnt = GetLink(i);
		if (!pnt)
			continue;
		if (m_links[i].selected)
		{
			dc.SetSelectedColor();
			dc.DrawLine( wp+Vec3(0,0,0.05f),pnt->GetWorldPos()+Vec3(0,0,0.05f) );
		}
	}

	// if selected and using auto-links draw a circle with the connection radius
	if (IsSelected() && pAISystem && m_aiNode)
	{
		IAISystem::SBuildingInfo info;
		if (pAISystem->GetBuildingInfo(aiGraph->GetBuildingIDFromWaypointNode(m_aiNode), info))
		{
			if (info.waypointConnections >= WPCON_AUTO_NONE)
			{
				float radius = info.fNodeAutoConnectDistance;
				dc.SetColor(RGB(1, 1, 1));
				dc.DrawCircle(wp, radius);
			}
		}
	}

	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::Serialize( CObjectArchive &ar )
{
	m_bIgnoreUpdateLinks = true;
	CBaseObject::Serialize( ar );
	m_bIgnoreUpdateLinks = false;

	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		m_bIgnoreUpdateLinks = true;
		int aiType = 0;
		xmlNode->getAttr( "AIType",aiType );
    int aiNavigationType = 0;
    xmlNode->getAttr( "AINavigationType", aiNavigationType);
		int removable = 0;
		xmlNode->getAttr( "Removable",removable );

		// Load Links.
		m_links.clear();
		XmlNodeRef links = xmlNode->findChild( "Links" );
		if (links)
		{
			m_bLinksValid = false;
			for (int i = 0; i < links->getChildCount(); i++)
			{
				XmlNodeRef pnt = links->getChild(i);
				Link link;
				link.object = 0;
				pnt->getAttr( "Id",link.id );
				pnt->getAttr( "Passable", link.passable );
				if (link.passable)
					ar.SetResolveCallback( this,link.id,functor(*this,&CAIPoint::ResolveLink) );
				else
					ar.SetResolveCallback( this,link.id,functor(*this,&CAIPoint::ResolveImpassableLink) );
			}
		}
		SetAIType((EAIPointType)aiType);
    SetAINavigationType((EAINavigationType)aiNavigationType);
		MakeRemovable(removable!=0);
		m_bIgnoreUpdateLinks = false;

    // Load internal ai links (here because SetAIType updates the links and might over-ride out modifications).
		XmlNodeRef internalLinks = xmlNode->findChild( "InternalAILinks" );
		if (internalLinks)
		{
			for (int i = 0; i < internalLinks->getChildCount(); i++)
			{
				XmlNodeRef pnt = internalLinks->getChild(i);
        GUID guid;
        float radius;
				pnt->getAttr( "Id", guid );
				pnt->getAttr( "radius", radius );
				ar.SetResolveCallback( this, guid, functor(*this, &CAIPoint::ResolveInternalAILink), *((uint*) &radius));
			}
		}
	}
	else
	{
		xmlNode->setAttr( "AIType",(int)m_aiType );
		xmlNode->setAttr( "Removable",(int)m_bRemovable );
    xmlNode->setAttr( "AINavigationType", m_aiNavigationType);
		// Save Links.
		if (!m_links.empty())
		{
			XmlNodeRef links = xmlNode->newChild( "Links" );
			for (int i = 0; i < m_links.size(); i++)
			{
				XmlNodeRef pnt = links->newChild( "Link" );
				CAIPoint* link = GetLink(i);
				if (!link)
					continue;
				pnt->setAttr( "Id",link->GetId() );
				pnt->setAttr( "Passable", m_links[i].passable);
			}
		}
    // write the internal ai links so they can be recreated on loading in editor
    // (rather than trying to load half via editor and half via ai system).
		XmlNodeRef links = xmlNode->newChild( "InternalAILinks" );
    IGraph *aiGraph = GetIEditor()->GetSystem()->GetAISystem() ? GetIEditor()->GetSystem()->GetAISystem()->GetNodeGraph() : NULL;
    if (m_aiNode && aiGraph)
    {
		  int numLinks = aiGraph->GetNumNodeLinks(m_aiNode);
		  for (int i = 0; i < numLinks; i++)
		  {
			  unsigned linkId = aiGraph->GetGraphLink(m_aiNode, i);
			  GraphNode *pNextNode = aiGraph->GetNextNode(linkId);
        int j = 0;
			  for ( ; j < m_links.size(); j++)
			  {
				  CAIPoint* link = GetLink(j);
				  if (!link)
					  continue;
          if (link->m_aiNode == pNextNode)
            break;
        }
        if (j == m_links.size())
        {
          const TAllAIPoints::const_iterator itEnd = allAIPoints.end();
          TAllAIPoints::const_iterator it = allAIPoints.begin();
          for ( ; it != itEnd ; ++it)
          {
            CAIPoint *pPoint = *it;
            if (pPoint->m_aiNode == pNextNode)
              break;
          }
          if (it != itEnd)
          {
            CAIPoint *pPoint = *it;
            float radius = aiGraph->GetOrigRadiusFromLink(linkId);
				    XmlNodeRef pnt = links->newChild( "InternalAILink" );
				    pnt->setAttr( "Id",pPoint->GetId() );
				    pnt->setAttr( "radius", radius);
          }
        }
      }
    }
	}
}


//////////////////////////////////////////////////////////////////////////
void CAIPoint::ResolveLink( CBaseObject *pOtherLink )
{
	if (pOtherLink && pOtherLink->IsKindOf(RUNTIME_CLASS(CAIPoint)))
	{
		Link link;
		link.object = (CAIPoint*)pOtherLink;
		link.id = pOtherLink->GetId();
		link.passable = true;
		m_links.push_back( link );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::ResolveImpassableLink( CBaseObject *pOtherLink )
{
	if (pOtherLink && pOtherLink->IsKindOf(RUNTIME_CLASS(CAIPoint)))
	{
		Link link;
		link.object = (CAIPoint*)pOtherLink;
		link.id = pOtherLink->GetId();
		link.passable = false;
		m_links.push_back( link );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::ResolveInternalAILink( CBaseObject *pOtherLink, uint userData )
{
	if (pOtherLink && pOtherLink->IsKindOf(RUNTIME_CLASS(CAIPoint)))
	{
		CAIPoint *otherPoint = (CAIPoint*)pOtherLink;
    float radius = *((float*) &userData);
    m_internalAILinks.push_back(std::make_pair(otherPoint, radius));
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CAIPoint::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
#if 0
	XmlNodeRef objNode = CBaseObject::Export( levelPath,xmlNode );
	objNode->setAttr( "Id",GetId() );

	// Save Links.
	if (!m_links.empty())
	{
		XmlNodeRef links = xmlNode->newChild( "Links" );
		for (int i = 0; i < m_links.size(); i++)
		{
			XmlNodeRef pnt = links->newChild( "Link" );
			pnt->setAttr( "Id",m_links[i].id );
		}
	}

	return CBaseObject::Export( levelPath,xmlNode );
#endif
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::OnEvent( ObjectEvent event )
{
	if (!GetIEditor()->GetSystem()->GetAISystem())
		return;

	switch (event)
	{
	case EVENT_REFRESH: // when refreshing level.
		// Recalculate indoors.
		SetPos( GetPos() );
		// After loading reconnect all links.
		UpdateLinks();
		break;
	case EVENT_CLEAR_AIGRAPH:
		m_aiNode = 0;
		m_nodeIndex = 0;
		break;
	case EVENT_MISSION_CHANGE:
		{
			// After mission have been changed, AI graph invalidates all pointers, create them again.
			// Create AI graph node in game.
			IGraph *aiGraph = GetIEditor()->GetSystem()->GetAISystem() ? GetIEditor()->GetSystem()->GetAISystem()->GetNodeGraph() : NULL;
			if (aiGraph)
			{
				m_bLinksValid = false;
				m_nodeIndex = aiGraph->CreateNewNode(m_aiNavigationType == EAINAVIGATION_HUMAN ? 
					IAISystem::NAV_WAYPOINT_HUMAN : IAISystem::NAV_WAYPOINT_3DSURFACE, Vec3(ZERO));
        m_aiNode = aiGraph->GetNode(m_nodeIndex);
				SetNodeType(m_aiNode, m_aiType);
				UpdateLinks();
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
CAIPoint* CAIPoint::GetLink( int index )
{
	assert( index >= 0 && index < m_links.size() );
	if (!m_links[index].object)
	{
		CBaseObject *obj = FindObject(m_links[index].id);
		if (obj && obj->IsKindOf(RUNTIME_CLASS(CAIPoint)))
		{
			((CAIPoint*)obj)->AddLink( this,true,false ); // todo? passability?
			m_links[index].object = (CAIPoint*)obj;
		}
	}
	return m_links[index].object;
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::AddLink( CAIPoint* obj, bool bIsPassable, bool bNeighbour )
{
	if (obj == this)
		return;

	GUID id = obj->GetId();
	RemoveLink(obj);

	Link link;
	link.object = obj;
	link.id = id;
	link.passable = bIsPassable;
	m_links.push_back( link );
	m_bLinksValid = false;


	if (!bNeighbour)
	{
		obj->AddLink( this, bIsPassable, true );
		UpdateLinks();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::RemoveAllLinks()
{
	for (int i = 0; i < GetLinkCount();)
	{
		CAIPoint *obj = GetLink(i);
		if (obj)
			RemoveLink( obj );
		else
			++i;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::RemoveLink( CAIPoint* obj,bool bNeighbour )
{
	if (obj == this || obj == 0)
		return;

	int index = -1;
	GUID id = obj->GetId();
	for (int i = 0; i < m_links.size(); i++)
	{
		if (m_links[i].object == obj || m_links[i].id == id)
		{
			index = i;
			break;
		}
	}
	if (index < 0)
		return;

	m_links.erase( m_links.begin() + index );
	m_bLinksValid = false;

	if (!bNeighbour)
	{
		obj->RemoveLink( this,true );
		UpdateLinks();
	}
}


//////////////////////////////////////////////////////////////////////////
void CAIPoint::UpdateLinks()
{
	if (m_bLinksValid || m_bIgnoreUpdateLinks)
		return;

	IAISystem *aiSystem = GetIEditor()->GetSystem()->GetAISystem();
	if (!aiSystem)
		return;
	IGraph *aiGraph = aiSystem->GetNodeGraph();

	if (!aiGraph)
		return;
	if (!m_aiNode)
		return;

	m_bIndoors = false;

	IVisArea *pArea = NULL;
	int nBuildingId = -1;
	Vec3 wp = GetWorldPos();
  aiGraph->MoveNode(m_nodeIndex, wp);

  IAISystem::ENavigationType aiNavType = m_aiNavigationType == EAINAVIGATION_HUMAN ? 
    IAISystem::NAV_WAYPOINT_HUMAN : IAISystem::NAV_WAYPOINT_3DSURFACE;
  if (aiSystem->CheckNavigationType(wp,nBuildingId,pArea, aiNavType) == aiNavType)
	{
		m_bIndoors = true;
	}

	if (m_bIndoorEntrance)
	{
		aiGraph->RemoveEntrance( aiGraph->GetBuildingIDFromWaypointNode(m_aiNode),m_nodeIndex );
		m_bIndoorEntrance = false;
	}

	aiGraph->SetBuildingIDInWaypointNode(m_aiNode, nBuildingId);
	aiGraph->SetVisAreaInWaypointNode(m_aiNode, pArea);

	aiGraph->Disconnect(m_nodeIndex,false);

	Matrix34 m = GetWorldTM();
	m.OrthonormalizeFast();
	Vec3 up = m.TransformVector(UP_VECTOR);
	aiGraph->SetWaypointNodeUpDir(m_aiNode, up);
	if (m_aiType == EAIPOINT_ENTRYEXIT && aiGraph->GetBuildingIDFromWaypointNode(m_aiNode) >= 0)
	{
		aiGraph->AddIndoorEntrance( aiGraph->GetBuildingIDFromWaypointNode(m_aiNode),m_nodeIndex );
		m_bIndoorEntrance = true;
	}
	else if (m_aiType == EAIPOINT_EXITONLY && aiGraph->GetBuildingIDFromWaypointNode(m_aiNode) >= 0)
	{
		aiGraph->AddIndoorEntrance( aiGraph->GetBuildingIDFromWaypointNode(m_aiNode),m_nodeIndex,true);
		m_bIndoorEntrance = true;
	}
	else if (m_aiType == EAIPOINT_HIDE)
	{
		Vec3 dir = m.TransformVector(DIR_VECTOR);
		aiGraph->SetWaypointNodeDir(m_aiNode, dir);
	}

	int numLinks = GetLinkCount();
	for (int i = 0; i < numLinks; i++)
	{
		CAIPoint *lnk = GetLink(i);
		if (!lnk || !lnk->m_aiNode)
			continue;

		float radius = m_links[i].passable ? WLT_EDITOR_PASS : WLT_EDITOR_IMPASS;
		aiGraph->Connect(m_nodeIndex,lnk->m_nodeIndex, radius, radius);
	}

  // 1-off update of internal AI links after loading
  numLinks = m_internalAILinks.size();
  for (int i = 0 ; i < numLinks ; ++i)
  {
    CAIPoint *lnk = m_internalAILinks[i].first;
    float radius = m_internalAILinks[i].second;
    if (!lnk)
      continue;
		aiGraph->Connect(m_nodeIndex,lnk->m_nodeIndex, radius, radius);
  }
  m_internalAILinks.clear();

	aiGraph->MakeNodeRemovable(aiGraph->GetBuildingIDFromWaypointNode(m_aiNode),m_nodeIndex,m_bRemovable);

	m_bLinksValid = true;
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::RegenLinks()
{
	IAISystem *aiSystem = GetIEditor()->GetSystem()->GetAISystem();
	if (!aiSystem)
		return;

	// make sure editor links are valid and this node has its correct building ID
	m_bLinksValid = false;
	UpdateLinks();

	if (m_aiNode)
		aiSystem->ReconnectWaypointNodesInBuilding(aiSystem->GetNodeGraph()->GetBuildingIDFromWaypointNode(m_aiNode));
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::MakeRemovable( bool bRemovable )
{
	if( bRemovable == m_bRemovable )
		return;
	m_bRemovable = bRemovable;
	if(!m_aiNode)
		return;
	IAISystem *aiSystem = GetIEditor()->GetSystem()->GetAISystem();
	if (!aiSystem)
		return;
	IGraph *aiGraph = aiSystem->GetNodeGraph();
	aiGraph->MakeNodeRemovable(aiGraph->GetBuildingIDFromWaypointNode(m_aiNode),m_nodeIndex, m_bRemovable);
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::SetAIType( EAIPointType type )
{
	IAISystem *aiSystem = GetIEditor()->GetSystem()->GetAISystem();
	if (!aiSystem)
		return;

	if (type == m_aiType)
		return;
	StoreUndo( "Set AIPoint Type" );
	m_aiType = type;
	if (!m_aiNode)
	{
		IGraph *aiGraph = aiSystem ? aiSystem->GetNodeGraph() : NULL;
		if (aiGraph)
		{
			m_nodeIndex = aiGraph->CreateNewNode(m_aiNavigationType == EAINAVIGATION_HUMAN ? 
				IAISystem::NAV_WAYPOINT_HUMAN : IAISystem::NAV_WAYPOINT_3DSURFACE, Vec3(ZERO));
      m_aiNode = aiGraph->GetNode(m_nodeIndex);
		}
	}
	SetNodeType(m_aiNode, m_aiType);

	m_bLinksValid = false;
	UpdateLinks();
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::SetAINavigationType( EAINavigationType type )
{
	IAISystem *aiSystem = GetIEditor()->GetSystem()->GetAISystem();
	if (!aiSystem)
		return;

	if (type == m_aiNavigationType)
		return;
	StoreUndo( "Set AIPoint Navigation Type" );
	m_aiNavigationType = type;

	IGraph *aiGraph = aiSystem ? aiSystem->GetNodeGraph() : NULL;
	if (aiGraph)
  {
    if (m_aiNode)
      aiGraph->Disconnect(m_nodeIndex, true);
		m_nodeIndex = aiGraph->CreateNewNode(m_aiNavigationType == EAINAVIGATION_HUMAN ? 
			IAISystem::NAV_WAYPOINT_HUMAN : IAISystem::NAV_WAYPOINT_3DSURFACE, GetPos());
    m_aiNode = aiGraph->GetNode(m_nodeIndex);
  }
	SetNodeType(m_aiNode, m_aiType);

	m_bLinksValid = false;
	UpdateLinks();
}

//////////////////////////////////////////////////////////////////////////
//! Invalidates cached transformation matrix.
void CAIPoint::InvalidateTM( int nWhyFlags )
{
	CBaseObject::InvalidateTM(nWhyFlags);

	m_bLinksValid = false;
	UpdateLinks();
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::StartPick()
{
	if (m_panel)
		m_panel->StartPick();
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::StartPickImpass()
{
	if (m_panel)
		m_panel->StartPickImpass();
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::Validate( CErrorReport *report )
{
	CBaseObject::Validate( report );
	if (!m_bIndoors && m_aiType != EAIPOINT_HIDE)
	{
		CErrorRecord err;
		err.error.Format( "AI Point is not valid %s (Must be indoors)",(const char*)GetName() );
		err.pObject = this;
		err.severity = CErrorRecord::ESEVERITY_WARNING;
		err.flags = CErrorRecord::FLAG_AI;
		report->ReportError(err);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIPoint::PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx )
{
	CAIPoint *pFromPoint = (CAIPoint*)pFromObject;
	// Clone AI Links.
	int num = pFromPoint->GetLinkCount();
	for (int i = 0; i < num; i++)
	{
		CAIPoint *pTarget = pFromPoint->GetLink(i);
		CAIPoint *pClonedTarget = (CAIPoint*)ctx.FindClone( pTarget );
		if (!pClonedTarget)
			pClonedTarget = pTarget; // If target not cloned, link to original target.

		// Add cloned event.
		if (pClonedTarget)
		{
			AddLink( pClonedTarget, pFromPoint->m_links[i].passable );
		}
	}
}
