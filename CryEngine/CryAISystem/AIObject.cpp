// AIObject.cpp: implementation of the CAIObject class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AIObject.h"
#include "CAISystem.h"
#include "AILog.h"
#include "Graph.h"
#include "Leader.h"
#include "ObjectTracker.h"
#include "GoalOp.h"

#include <float.h>
#include <ISystem.h>
#include <ILog.h>
#include <ISerialize.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define _ser_value_(val) ser.Value(#val, val)


CAIObject::CAIObject(CAISystem* pAISystem):
m_entityID(0),
m_vPosition(ZERO),
m_bEnabled(true),
m_lastNavNodeIndex(0),
m_lastHideNodeIndex(0),
m_bLastNearForbiddenEdge(false),
m_bMoving(false),
m_fRadius(.0f),
m_bCanReceiveSignals(true),
m_pFormation(0),
m_bDEBUGDRAWBALLS(false),
m_pFollowed(NULL ),
m_nObjectType(0),
m_objectSubType(CAIObject::STP_NONE),
m_bUpdatedOnce(false),
m_bUncheckedBody(false),
m_vPredictedPosition(ZERO),
m_vFirePosition(ZERO),
m_vBodyDir(ZERO),
m_vMoveDir(ZERO),
m_vView(ZERO),
m_vLastPosition(ZERO),
m_bLastActionSucceed(false),
m_pAssociation(0),
m_groupId(-1),
m_bTouched(false)
{
#if !defined(USER_dejan) && !defined(USER_mikko)
	AILogComment("CAIObject (%p)", this);
#endif
}

CAIObject::~CAIObject()
{
#if !defined(USER_dejan) && !defined(USER_mikko)
	AILogComment("~CAIObject  %s (%p)", m_sName.c_str(), this);
#endif
	
	ReleaseFormation();
		
/* CRAIG: This code was useful for tracking down leader dangling pointer bugs (the infamous FPP-2333)... left here in case it's ever needed again.

	if (CLeader * pCulprit = GetAISystem()->IsPartOfAnyLeadersUnits(this))
	{
		DEBUG_BREAK;
	}
*/

/*
	if (m_pFormation)
		GetAISystem()->ReleaseFormation(m_Parameters.m_nGroup);
	m_pFormation = 0;
*/
}


//====================================================================
// CAIObject::OnObjectRemoved
//====================================================================
void CAIObject::OnObjectRemoved(CAIObject *pObject )
{
	if(pObject ==m_pAssociation)
		m_pAssociation=NULL;
}


void CAIObject::SetPos(const Vec3 &pos,const Vec3 &dirForw)
{
	if (_isnan(pos.x) || _isnan(pos.y) || _isnan(pos.z))
	{
		AIWarning("NotANumber tried to be set for position of AI entity %s",m_sName.c_str());
		return;
	}
	m_vLastPosition = m_vPosition;

	assert( m_vLastPosition.IsValid() );

	if( GetProxy() )
	{
		SAIBodyInfo bodyInfo;
		assert( bodyInfo.vEyeDir.IsValid() );
		GetProxy()->QueryBodyInfo( bodyInfo);
		assert( bodyInfo.vEyePos.IsValid() );
		assert( bodyInfo.vEyeDir.IsValid() );
		assert( bodyInfo.vFirePos.IsValid() );
		
		m_vPosition = bodyInfo.vEyePos;
		assert( m_vPosition.IsValid() );

		m_vFirePosition = bodyInfo.vFirePos;
    SetMoveDir(bodyInfo.vMoveDir);
    SetBodyDir(bodyInfo.vBodyDir);
		SetViewDir(bodyInfo.vEyeDir);
	}
	else
	{
		m_vPosition = pos;
		assert( m_vPosition.IsValid() );

		m_vMoveDir = dirForw;
    m_vBodyDir = dirForw;
		SetViewDir(dirForw);
	}

	if ( !IsEquivalent(m_vLastPosition,pos,VEC_EPSILON) )
	{
		m_bMoving = true;

		// notify smart objects, too
		if ( GetEntity() )
		{
			SEntityEvent event( ENTITY_EVENT_XFORM );
			GetAISystem()->NotifyAIObjectMoved( GetEntity(), event );
		}
    if (m_nObjectType == AIANCHOR_COMBAT_HIDESPOT || m_nObjectType == AIANCHOR_COMBAT_HIDESPOT_SECONDARY)
      m_lastNavNodeIndex = 0;
	}
	else
		m_bMoving = false;

	if (m_pFormation)
		m_pFormation->Update();
}

Vec3 CAIObject::GetPhysicsPos() const
{
	if (GetEntity())
		return GetEntity()->GetWorldPos();

	return GetPos();
}

void CAIObject::SetType(unsigned short type)
{
	m_nObjectType = type;
}

void CAIObject::SetSubType(CAIObject::ESubTypes subType)
{
	m_objectSubType = subType;
}

void CAIObject::SetAssociation(CAIObject* pAssociation)
{
	m_pAssociation = pAssociation;
}


void CAIObject::SetName(const char *pName)
{

	char str[128];
	strncpy(str, pName, sizeof(str)); str[sizeof(str) - 1] = '\0';
	/*
	int i=1;
	while (GetAISystem()->GetAIObjectByName(str))
	{
 		_snprintf(str, sizeof(str), "%s_%02d", pName, i);
		i++;
	}
#if !defined(USER_dejan) && !defined(USER_mikko)
  AILogComment("CAIObject::SetName %p from %s to %s", this, m_sName.c_str(), str);
#endif
	*/
  m_sName = str;
}


void CAIObject::SetBodyDir(const Vec3 &dir)
{
	m_vBodyDir = dir;
	//FIXME: The direction sent from game should be already normalized!
	m_vBodyDir.NormalizeSafe();
	//if (m_pFormation)
	//	m_pFormation->Update(this);
}

void CAIObject::SetMoveDir(const Vec3 &dir)
{
  m_vMoveDir = dir;
  //FIXME: The direction sent from game should be already normalized!
  m_vMoveDir.NormalizeSafe();
  //if (m_pFormation)
  //	m_pFormation->Update(this);
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::SetViewDir(const Vec3 &dir)
{
	m_vView = dir;

	//FIXME: The direction sent from game should be already normalized!
	m_vView.NormalizeSafe();

	//if (m_pFormation)
	//	m_pFormation->Update(this);
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::Reset(EObjectResetType type)
{
	m_lastNavNodeIndex = 0;
	m_lastHideNodeIndex = 0;
  m_bLastNearForbiddenEdge = false;

	ReleaseFormation();

	m_bEnabled = true;
	m_bUncheckedBody = false;
	m_bUpdatedOnce = false;
	m_bCanReceiveSignals = true;
	m_bTouched = false;
}



//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::EDITOR_DrawRanges(bool bEnable)
{
	m_bDEBUGDRAWBALLS = bEnable;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::SetRadius(float fRadius)
{
	m_fRadius = fRadius;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
  ser.Value("m_bEnabled", m_bEnabled);
	ser.Value("m_bTouched", m_bTouched);
  ser.Value("m_bUncheckedBody", m_bUncheckedBody);
  ser.Value("m_bCanReceiveSignals", m_bCanReceiveSignals);
  // Do not cache the result of GetAnchorNavNode across serialisation
  if (m_lastNavNodeIndex == ~0)
    m_lastNavNodeIndex = 0;
  GetAISystem()->GetGraph(false)->SerializeNodePointer(ser, "m_pLastNavNode", m_lastNavNodeIndex);
  // Do not cache the result of GetAnchorNavNode across serialisation
  if (m_lastHideNodeIndex == ~0)
    m_lastHideNodeIndex = 0;
  GetAISystem()->GetGraph(true)->SerializeNodePointer(ser, "m_pLastHideNode", m_lastHideNodeIndex);
  ser.Value("m_bLastNearForbiddenEdge", m_bLastNearForbiddenEdge);
  ser.Value("m_bMoving", m_bMoving);
  ser.Value("m_vLastPosition", m_vLastPosition);
  objectTracker.SerializeObjectPointer(ser, "Formation", m_pFormation, false);
  objectTracker.SerializeObjectPointer(ser, "m_pFollowed", m_pFollowed, false);

  // m_movementAbility.Serialize(ser); // not needed as ability is constant
  ser.Value("m_bUpdatedOnce", m_bUpdatedOnce);
  ser.Value("m_bLastActionSucceed", m_bLastActionSucceed);
  // todo m_listWaitGoalOps
	ser.Value("m_nObjectType",m_nObjectType);
  ser.EnumValue("m_objectSubType", m_objectSubType, STP_NONE, STP_MAXVALUE);
  ser.Value("m_vPosition",m_vPosition);
  ser.Value("m_vFirePosition",m_vFirePosition);
	ser.Value("m_vPredictedPosition",m_vPredictedPosition);
  ser.Value("m_vBodyDir",m_vBodyDir);
  ser.Value("m_vMoveDir",m_vMoveDir);
  ser.Value("m_vView",m_vView);
  // todo m_pAssociation
	ser.Value("m_fRadius",m_fRadius);
  // Danny todo m_entitiesToSkipInPathFinding
	ser.Value("m_sName",m_sName);
	ser.Value("m_entityID",m_entityID);
	ser.Value("m_groupId",m_groupId);

	objectTracker.SerializeObjectPointer(ser, "m_pAssociation", m_pAssociation, false);


	// m_listWaitGoalOps is not serialized but recreated after serializing goal pipe, when reading, in CPipeUser::Serialize()
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CAIObject::CreateFormation(const char * szName, Vec3 vTargetPos)
{
	if (m_pFormation)
	{
		GetAISystem()->ReleaseFormation(this,true);
	}

	m_pFormation = 0;

	if (!szName)
		return false;

	m_pFormation = GetAISystem()->CreateFormation(this,szName,vTargetPos);
	return (m_pFormation!= NULL);
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CAIObject::ReleaseFormation(void)
{
	if(m_pFormation)
	{
		GetAISystem()->ReleaseFormation(this,true);
		m_pFormation = 0;
		return true;
	}
	return false;
}


//
//------------------------------------------------------------------------------------------------------------------------
IAIObject* CAIObject::GetFollowedLeader()
{
	// for a chain (i.e. convoy) find the leader
	IAIObject* pFollowed = m_pFollowed;
	if(pFollowed)
		while(pFollowed->GetFollowed())
			pFollowed = pFollowed->GetFollowed();

	return pFollowed;
}


//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::Following(IAIObject* pFollowed)
{ 
	m_pFollowed = pFollowed;
}

//
//------------------------------------------------------------------------------------------------------------------------
Vec3	CAIObject::GetVelocity() const
{
	if(!GetProxy() || !GetProxy()->GetPhysics())
		return ZERO;//Vec3Constants::fVec3_Zero<float>;

  // if living entity return that vel since that is actualy the rate of
  // change of position
  pe_status_living status;
  if (GetProxy()->GetPhysics()->GetStatus(&status))
    return status.vel;

  pe_status_dynamics  dSt;
	GetProxy()->GetPhysics()->GetStatus(&dSt);
  return dSt.v;
}

//
//------------------------------------------------------------------------------------------------------------------------
const char* CAIObject::GetEventName(unsigned short eType) const
{
	switch (eType)
	{
		case AIEVENT_ONBODYSENSOR:
			return "OnBodySensors";
		case AIEVENT_ONVISUALSTIMULUS:
			return "OnVisualStimulus";
		case AIEVENT_ONPATHDECISION:
			return "OnPathDesision";
		case AIEVENT_ONSOUNDEVENT:
			return "OnSoundEvent";
		case AIEVENT_AGENTDIED:
			return "AgentDied";
		case AIEVENT_SLEEP:
			return "Sleep";
		case AIEVENT_WAKEUP:
			return "Wakeup";
		case AIEVENT_ENABLE:
			return "Enable";
		case AIEVENT_DISABLE:
			return "Disable";
		case AIEVENT_REJECT:
			return "Reject";
		case AIEVENT_PATHFINDON:
			return "PathfindOn";
		case AIEVENT_PATHFINDOFF:
			return "PathfindOff";
		case AIEVENT_CLEAR:
			return "Clear";
		case AIEVENT_DROPBEACON:
			return "DropBeacon";
		case AIEVENT_USE:
			return "Use";
		default:
			return "undefined";
	}
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::Event(unsigned short eType, SAIEVENT *pEvent)
{
	switch (eType)
	{
	case AIEVENT_DISABLE:
		m_bEnabled = false;
		break;
	case AIEVENT_ENABLE:
		m_bEnabled = true;
		break;
	case AIEVENT_SLEEP:
		m_bEnabled = false;
		break;
	case AIEVENT_WAKEUP:
		m_bEnabled = true;
		break;
	default:
		break;
	}
}



//====================================================================
// SetEntityID
//====================================================================
void CAIObject::SetEntityID(unsigned ID) 
{
	m_entityID = ID;
}

//====================================================================
// GetEntity
//
// just a little helper
//====================================================================

IEntity* CAIObject::GetEntity() const 
{
	if(gEnv->pEntitySystem)
		return gEnv->pEntitySystem->GetEntity( GetEntityID() );
	return NULL;
}


//====================================================================
// SetFormationUpdateSight
//====================================================================
void CAIObject::SetFormationUpdateSight(float range,float minTime,float maxTime)
{
	if(m_pFormation)
		m_pFormation->SetUpdateSight(range,minTime,maxTime);
}

//====================================================================
// IsHostile
//====================================================================
bool CAIObject::IsHostile(const IAIObject* pOther, bool /*bUsingAIIgnorePlayer*/) const
{
	if(!pOther)
		return false;
	unsigned short nType=((CAIObject*)pOther)->GetType();
	return (
		nType == AIOBJECT_GRENADE ||
		nType == AIOBJECT_RPG 
//		||m_Parameters.m_bSpeciesHostility && pOther && pOther->GetParameters().m_bSpeciesHostility 
//			&& m_Parameters.m_nSpecies != pOther->GetParameters().m_nSpecies &&
//			pOther->GetParameters().m_nSpecies>=0
			);
}

//====================================================================
// SetGroupId
//====================================================================
void CAIObject::SetGroupId(int id)
{
	m_groupId = id;
}

//====================================================================
// GetGroupId
//====================================================================
int CAIObject::GetGroupId() const
{
	return m_groupId;
}

//====================================================================
// Serialize
//====================================================================
void ObstacleData::Serialize(TSerialize ser)
{
	ser.Value("vPos", vPos);
	ser.Value("vDir", vDir);
	ser.Value("fApproxRadius",fApproxRadius);
	ser.Value("bCollidable",bCollidable);
  if (ser.IsReading())
  {
    navNodes.clear();
    needToEvaluateNavNodes = true;
  }
}

//===================================================================
// GetAnchorNavNode
//===================================================================
const GraphNode *CAIObject::GetAnchorNavNode()
{
  AIAssert(m_nObjectType == AIANCHOR_COMBAT_HIDESPOT || m_nObjectType == AIANCHOR_COMBAT_HIDESPOT_SECONDARY);

  if (m_nObjectType != AIANCHOR_COMBAT_HIDESPOT && m_nObjectType != AIANCHOR_COMBAT_HIDESPOT_SECONDARY)
    return 0;

  if (m_lastNavNodeIndex == ~0)
    return 0;
  else if (m_lastNavNodeIndex != 0)
    return GetAISystem()->GetGraph()->GetNode(m_lastNavNodeIndex);

	unsigned lastNavNodeIndex = GetAISystem()->GetGraph()->GetEnclosing(GetPos(), 
		IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE | IAISystem::NAV_VOLUME );
  m_lastNavNodeIndex = lastNavNodeIndex;
  if (!m_lastNavNodeIndex)
	{
    m_lastNavNodeIndex = ~0;
		return 0;
	}

  return GetAISystem()->GetGraph()->GetNode(m_lastNavNodeIndex);
}

/*
//===================================================================
// SetReachablePos
//===================================================================
void CAIObject::SetReachablePos(Vec3& pos, CAIObject* pReacher,const Vec3& startPos )
{
	int building;
	IVisArea *pArea;
	IAISystem::tNavCapMask navMask;
	float fPassRadius;
	if(pReacher)
	{
		navMask = pReacher->GetMovementAbility().pathfindingProperties.navCapMask ;
		fPassRadius = pReacher->GetParameters().m_fPassRadius;
	}
	else
	{
		navMask = 0xff;
		fPassRadius = 0.6f;
	}
	IAISystem::ENavigationType navType =  GetAISystem()->CheckNavigationType(startPos,building,pArea,navMask);
	switch(navType)
	{
	case IAISystem::NAV_TRIANGULAR:
		{
			float fDistanceToEdge = fPassRadius + GetAISystem()->m_cvExtraForbiddenRadiusDuringBeautification->GetFVal() + 0.3f;
			pos = GetAISystem()->GetPointOutsideForbidden(pos,fDistanceToEdge,&startPos);
			//			pos = pos;
			break;
			/*			ray_hit hit;
			IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
			int rwiResult = pWorld->RayWorldIntersection(startPos, pos-startPos, ent_static|ent_rigid|ent_sleeping_rigid|ent_terrain, HIT_COVER, &hit, 1);
			if(rwiResult>0)
			{
			pos = hit.pt + hit.n * (fPassRadius+0.2f);
			}
			break;*/
/*
		}
	case IAISystem::NAV_WAYPOINT_HUMAN:
	case IAISystem::NAV_WAYPOINT_3DSURFACE:
		{
			ray_hit hit;
			IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
			int rwiResult = pWorld->RayWorldIntersection(startPos, pos-startPos, ent_static|ent_rigid|ent_sleeping_rigid|ent_terrain, HIT_COVER, &hit, 1);
			if(rwiResult>0)
			{
				pos = hit.pt + hit.n * (fPassRadius+0.2f);
			}
			//			else
			{
				Vec3 actualPos = pos;
				if(pReacher && GetAISystem()->IsPathWorth( pReacher, startPos,pos,1.5f,10000,&actualPos))
					pos = actualPos;
			}
		}
		break;
	default:
		break;
	}

	SetPos(pos);
}
*/
//===================================================================
// SetNavNodes
//===================================================================
void ObstacleData::SetNavNodes(const std::vector<const GraphNode *> &nodes)
{
  navNodes = nodes;
  needToEvaluateNavNodes = false;
}

//===================================================================
// AddNavNode
//===================================================================
void ObstacleData::AddNavNode(const GraphNode * pNode)
{
  needToEvaluateNavNodes = false;
  navNodes.push_back(pNode);
}


//===================================================================
// GetNavNodes
//===================================================================
const std::vector<const GraphNode *> &ObstacleData::GetNavNodes() const
{
  if (!needToEvaluateNavNodes)
    return navNodes;

  needToEvaluateNavNodes = false;

  // If we're in a forbidden region then sample outside the obstacle just like will be
  // done when finding hidespots
  if (bCollidable && fApproxRadius > 0.1f && GetAISystem()->IsPointInForbiddenRegion(vPos))
  {
    float circ = gf_PI2 * fApproxRadius;
    static float sampleSpace = 0.5f;
    static float fakeRadius = 0.5f;
    int numSamples = (int)(circ / sampleSpace);
    Limit(numSamples, 4, 10);
    Vec3 offset(ZERO);
    Vec3 hideDir;
    std::set<GraphNode *> checkedNodes;
		unsigned navNodeIndex = 0;
    for (int iSample = 0 ; iSample < numSamples ; ++iSample)
    {
      float angle = iSample * gf_PI2 / numSamples;
  //    cry_sincosf(angle, &offset.x); // assumes a little about the layout of Vec3 :)
			sincos_tpl(angle, &offset.y,&offset.x); // assumes a little about the layout of Vec3 :)
      Vec3 pos = vPos;
      GetAISystem()->AdjustOmniDirectionalCoverPosition(pos, hideDir, max(fApproxRadius, 0.0f), fakeRadius, pos + offset);
			navNodeIndex = GetAISystem()->GetGraph()->GetEnclosing(pos, IAISystem::NAV_TRIANGULAR, 0.0f, navNodeIndex);
      GraphNode* pNavNode = GetAISystem()->GetGraph()->GetNode(navNodeIndex);
      if (!pNavNode)
        continue;
      if (checkedNodes.insert(pNavNode).second)
        navNodes.push_back(pNavNode);
    }
  }
  else
  {
		unsigned navNodeIndex = GetAISystem()->GetGraph()->GetEnclosing(vPos, IAISystem::NAV_TRIANGULAR);
    GraphNode *pNavNode = GetAISystem()->GetGraph()->GetNode(navNodeIndex);
    if (!pNavNode)
      return navNodes;

    // pNavNode points to one triangle - now walk around this obstacles adding all nodes
    std::set<GraphNode *> checkedNodes;

    navNodes.push_back(pNavNode);
    checkedNodes.insert(pNavNode);

    while (true)
    {
      //unsigned nLinks = pNavNode->links.size();
      //unsigned iLink;
      //for (iLink = 0 ; iLink < nLinks ; ++iLink)
			CGraphLinkManager& linkManager = GetAISystem()->GetGraph()->GetLinkManager();
			CGraphNodeManager& nodeManager = GetAISystem()->GetGraph()->GetNodeManager();
			unsigned linkId;
			for (linkId = pNavNode->firstLinkIndex; linkId; linkId = linkManager.GetNextLink(linkId))
      {
				unsigned nextIndex = linkManager.GetNextNode(linkId);
        GraphNode *pNext = nodeManager.GetNode(nextIndex);

        // even if pNext turns out not to be touching the obstacle there are still
        // cases where it touches two triangles that are touching the obstacle
        if (!checkedNodes.insert(pNext).second)
          continue;
        if (pNext->navType != IAISystem::NAV_TRIANGULAR)
          continue;

        STriangularNavData *triData = pNext->GetTriangularNavData();
        unsigned nVerts = triData->vertices.size();
        unsigned iVert;
        for (iVert = 0 ; iVert < nVerts ; ++iVert)
        {
          int index = triData->vertices[iVert];
          const ObstacleData &obstacle = GetAISystem()->m_VertexList.GetVertex(index);
          if (&obstacle == this)
            break;
        }
        if (iVert != nVerts)
        {
          // found another good triangle
          pNavNode = pNext;
          navNodes.push_back(pNavNode);
          checkedNodes.insert(pNavNode);
          break;
        }
      }
      if (!linkId)
        break;
    }
  }
  return navNodes;
}


//====================================================================
// AISIGNAL Serialize 
//====================================================================
void AISIGNAL::Serialize( TSerialize ser, class CObjectTracker& objectTracker )
{
	ser.Value("nSignal",nSignal);

	string	textString(strText);
	ser.Value("strText", textString);
	if(ser.IsReading())
	{
		strcpy(strText, textString.c_str());
		m_nCrcText = g_crcSignals.m_crcGen->GetCRC32(textString);
	}

	if (ser.IsReading())
	{
		if (pEData)
			delete (AISignalExtraData*) pEData;
		pEData = new AISignalExtraData;

		EntityId	senderId(0);
		ser.Value("senderId", senderId);
		pSender = gEnv->pEntitySystem->GetEntity(senderId);
	}
	else
	{
		EntityId	senderId(0);
		if(pSender)
			senderId = pSender->GetId();
		ser.Value("senderId", senderId);
	}
	
	if (pEData)
		pEData->Serialize(ser);
	else
	{
		AISignalExtraData dummy;
		dummy.Serialize(ser);
	}
}

//====================================================================
// SAIActorTargetRequest Serialize 
//====================================================================
void SAIActorTargetRequest::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("SAIActorTargetRequest");
	{
		_ser_value_(id);
		if(id != 0)
		{
			_ser_value_(approachLocation);
			_ser_value_(approachDirection);
			_ser_value_(animLocation);
			_ser_value_(animDirection);
			_ser_value_(vehicleName);
			_ser_value_(vehicleSeat);
			_ser_value_(speed);
			_ser_value_(directionRadius);
			_ser_value_(locationRadius);
			_ser_value_(startRadius);
			_ser_value_(signalAnimation);
			_ser_value_(projectEndPoint);
			_ser_value_(lowerPrecision);
			_ser_value_(animation);
			ser.EnumValue("stance", stance, STANCE_NULL, STANCE_LAST);
	// TODO: Pointers!
	//		TAnimationGraphQueryID * pQueryStart;
	//		TAnimationGraphQueryID * pQueryEnd;
		}
	}
	ser.EndGroup();
}

//====================================================================
// SAIPredictedCharacterState Serialize 
//====================================================================
void SAIPredictedCharacterState::Serialize( TSerialize ser )
{
	ser.Value("position", position);
	ser.Value("velocity", velocity);
	ser.Value("predictionTime", predictionTime);
}

//====================================================================
// SAIPredictedCharacterStates Serialize 
//====================================================================
void SAIPredictedCharacterStates::Serialize( TSerialize ser )
{
	ser.BeginGroup("SAIPredictedCharacterStates");
	{
		ser.Value("nStates", nStates);
		int counter(0);
		char stateGroupName[32];
		for(int i(0); i<maxStates; ++i, ++counter)
		{
			sprintf(stateGroupName, "State_%d", counter);
			ser.BeginGroup(stateGroupName);
			{
				states[i].Serialize(ser);
			} ser.EndGroup();
		}
	}	ser.EndGroup();
}
//====================================================================
// SOBJECTSTATE Serialize 
//====================================================================
void SOBJECTSTATE::Serialize( TSerialize ser, class CObjectTracker& objectTracker )
{
	ser.BeginGroup("SOBJECTSTATE");
	{
		ser.Value("jump", jump);		
		ser.Value("bCloseContact", bCloseContact);
		ser.ValueWithDefault("fDesiredSpeed", fDesiredSpeed, 1.f);
		ser.Value("fMovementUrgency", fMovementUrgency);
		ser.Value("fire", fire);
		ser.Value("bodystate", bodystate);
		ser.Value("vMoveDir", vMoveDir);
		ser.Value("vForcedNavigation", vForcedNavigation);
		ser.Value("vLookTargetPos", vLookTargetPos);
		ser.Value("bReevalute", bReevaluate);
		ser.Value("bTakingDamage", bTakingDamage);
		ser.EnumValue("eTargetType", eTargetType, AITARGET_NONE, AITARGET_LAST);
		ser.EnumValue("eTargetThreat", eTargetThreat, AITHREAT_NONE, AITHREAT_LAST);
		ser.Value("bTargetEnabled", bTargetEnabled);
		ser.Value("nTargetType", nTargetType);
		ser.Value("nAuxSignal", nAuxSignal);
		ser.Value("fAuxDelay", fAuxDelay);
		ser.Value("nAuxPriority", nAuxPriority);
		ser.Value("szAuxSignalText", szAuxSignalText);
		ser.Value("lean", lean);
		ser.Value("aimTargetIsValid", aimTargetIsValid);
		ser.Value("weaponAccessories", weaponAccessories);

		ser.Value("fireSecondary", fireSecondary);
		ser.Value("fireMelee", fireMelee);
		ser.Value("aimObstructed", aimObstructed);
		ser.Value("forceWeaponAlertness", forceWeaponAlertness);
		ser.Value("fHitProbability", fHitProbability);
		ser.Value("fProjectileSpeedScale", fProjectileSpeedScale);
		ser.Value("vShootTargetPos", vShootTargetPos);
		ser.Value("vAimTargetPos", vAimTargetPos);
		ser.Value("allowStrafing", allowStrafing);
		ser.Value("allowEntityClampingByAnimation", allowEntityClampingByAnimation);
		ser.Value("fDistanceToPathEnd", fDistanceToPathEnd);
		ser.Value("curActorTargetFinishPos", curActorTargetFinishPos);
//		ser.Value("remainingPath", remainingPath);
		ser.Value("bHurryNow", bHurryNow);
		ser.Value("bTakingDamage", bTakingDamage);
		ser.Value("fDistanceFromTarget", fDistanceFromTarget);
		ser.EnumValue("eTargetStuntReaction", eTargetStuntReaction, AITSR_NONE, AITSR_LAST);
		ser.EnumValue("curActorTargetPhase", curActorTargetPhase, eATP_None, eATP_Error);
		actorTargetReq.Serialize(ser, objectTracker);
		// serialize signals
		ser.BeginGroup("SIGNALS");
		{
			int signalsCount = vSignals.size();
			ser.Value("signalsCount", signalsCount);
			if(ser.IsReading())
			{
				vSignals.resize( signalsCount );
			}
			int counter(0);
			char signalGroupName[32];
			for (DynArray<AISIGNAL>::iterator ai(vSignals.begin());ai!=vSignals.end();++ai,++counter)
			{
				sprintf(signalGroupName, "Signal_%d", counter);
				ser.BeginGroup(signalGroupName);
				{
					AISIGNAL	&signal=(*ai);
					signal.Serialize(ser, objectTracker);
				}
				ser.EndGroup();
			}
		}
		ser.EndGroup();
		ser.Value("predictedCharacterStates", predictedCharacterStates);
	}
	ser.EndGroup();
}


//====================================================================
// AgentPerceptionParameters Serialize 
//====================================================================
void AgentPerceptionParameters::Serialize(TSerialize ser)
{
	ser.BeginGroup("AgentPerceptionParameters");
	{
		_ser_value_(sightRange);
		_ser_value_(sightRangeVehicle);
		_ser_value_(velBase);					
		_ser_value_(velScale);
		_ser_value_(FOVPrimary);
		_ser_value_(FOVSecondary);
		_ser_value_(stanceScale);
		_ser_value_(audioScale);
		_ser_value_(bThermalVision);
		_ser_value_(camoScale);
		_ser_value_(heatScale);
		_ser_value_(targetPersistence);
		_ser_value_(perceptionScale.audio);
		_ser_value_(perceptionScale.visual);
		_ser_value_(bulletHitRadius);
		_ser_value_(minAlarmLevel);
		_ser_value_(sightEnvScaleNormal);
		_ser_value_(sightEnvScaleAlarmed);
		_ser_value_(StuntTimeout);
	}
	ser.EndGroup();
}

//====================================================================
// AgentPerceptionParameters Serialize 
//====================================================================
void AgentParameters::Serialize(TSerialize ser)
{
	ser.BeginGroup( "AgentParameters" );
	{
		m_PerceptionParams.Serialize(ser);

		_ser_value_(m_nGroup);
		_ser_value_(m_CombatClass);
		_ser_value_(m_fAccuracy);
		_ser_value_(m_fPassRadius);
		_ser_value_(m_fStrafingPitch);		//if this > 0, will do a strafing draw line firing. 04/12/05 Tetsuji
		_ser_value_(m_fDistanceToHideFrom);
		_ser_value_(m_fAttackRange);
		_ser_value_(m_fCommRange);
		_ser_value_(m_fAttackZoneHeight);
		_ser_value_(m_fPreferredCombatDistance);
		_ser_value_(m_weaponAccessories);
		_ser_value_(m_bSpeciesHostility);
		_ser_value_(m_fGroupHostility);
		_ser_value_(m_fMeleeDistance);
		_ser_value_(m_nSpecies);
		_ser_value_(m_nRank);
		_ser_value_(m_trackPatternName);			// The pattern name to use, empty string will disable the pattern.
		_ser_value_(m_trackPatternContinue);	// Signals to continue to advance on the pattern, this is a three state flag, 0 means no change (default), 1=continue, 2=stop
		_ser_value_(m_bPerceivePlayer);
		_ser_value_(m_fAwarenessOfPlayer);
		_ser_value_(m_bSpecial);
		_ser_value_(m_bInvisible);
		_ser_value_(m_fCloakScale);
		_ser_value_(m_fCloakScaleTarget);
		_ser_value_(m_lookIdleTurnSpeed);
		_ser_value_(m_lookCombatTurnSpeed);
		_ser_value_(m_aimTurnSpeed);
		_ser_value_(m_fireTurnSpeed);
		_ser_value_(m_bAiIgnoreFgNode);
		_ser_value_(m_fGrenadeThrowDistScale);
	} 
	ser.EndGroup();
}


//--------------------------------------------------------------------
