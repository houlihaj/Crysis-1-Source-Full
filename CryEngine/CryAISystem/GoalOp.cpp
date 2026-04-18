// GoalOp.cpp: implementation of the CGoalOp class.
//
//////////////////////////////////////////////////////////////////////


#include "StdAfx.h"
#include "GoalOp.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include "AILog.h"
#include "ObjectTracker.h"
#include "IConsole.h"
#include "AICollision.h"
#include "IRenderAuxGeom.h"	
#include "NavRegion.h"
#include "PipeUser.h"
#include "Leader.h"
#include "TickCounter.h"
#include "AIDebugDrawHelpers.h"
#include "PathFollower.h"

#include "ISystem.h"
#include "ITimer.h"
#include "IPhysics.h"
#include "Cry_Math.h"
#include "ILog.h"
#include "ISerialize.h"

#define	C_MaxDistanceForPathOffset 2 // threshold (in m) used in COPStick and COPApproach, to detect if the returned path
// is bringing the agent too far from the expected destination

// The end point accuracy to use when tracing and we don't really care. 0 results in exact positioning being used
// a small value (0.1) allows some sloppiness. Both should work.
static float defaultTraceEndAccuracy = 0.1f;

CObjectTracker *COPStick::SSafePoint::pObjectTracker = 0;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CGoalOp::CGoalOp()
{

}

CGoalOp::~CGoalOp()
{

}

void CGoalOp::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{

}


COPAcqTarget::COPAcqTarget(const string& name)
{
	m_TargetName = name;
}

COPAcqTarget::~COPAcqTarget()
{
}

EGoalOpResult COPAcqTarget::Execute(CPipeUser *pOperand)
{
	CAIObject* pTarget = NULL;
	if(m_TargetName!="")
		pTarget = (CAIObject*)pOperand->GetSpecialAIObject(m_TargetName);
	if (pTarget)
	{
		pOperand->m_bLooseAttention = false;
		pOperand->SetAttentionTarget(pTarget);
	}
	else 
	{
		pOperand->m_bLooseAttention = false;
		pOperand->SetAttentionTarget(pOperand->m_pLastOpResult);
	}
	return AIGOR_DONE;
}

COPApproach::COPApproach(float fEndDistance, float fEndAccuracy, float fDuration,
												 bool bUseLastOpResult, bool bLookAtLastOp,
												 bool bForceReturnPartialPath, bool bStopOnAnimationStart, const char* szNoPathSignalText) :
m_fLastDistance(0.0f),
m_fInitialDistance(0.0f),
m_bPercent(false),
m_bUseLastOpResult(bUseLastOpResult),
m_bLookAtLastOp(bLookAtLastOp),
m_fEndDistance(fEndDistance),
m_fEndAccuracy(fEndAccuracy),
m_fDuration(fDuration),
m_bForceReturnPartialPath(bForceReturnPartialPath),
m_stopOnAnimationStart(bStopOnAnimationStart),
m_looseAttentionId(0),
m_pTraceDirective(0),
m_pPathfindDirective(0),
m_bPathFound(false)
{
}

COPApproach::~COPApproach()
{
	Reset(NULL);
	//if (m_pLooseAttentionTarget)
	//  GetAISystem()->RemoveDummyObject(m_pLooseAttentionTarget);
}

void COPApproach::Reset(CPipeUser *pOperand)
{
	if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
		AILogAlways("COPApproach::Reset %s", pOperand ? pOperand->GetName() : "");

	delete m_pPathfindDirective;
	m_pPathfindDirective = 0;
	delete m_pTraceDirective;
	m_pTraceDirective = 0;

	m_bPathFound = false;

	m_fInitialDistance = 0;
	if (pOperand)
	{
		pOperand->ClearPath("COPApproach::Reset m_Path");
		if (m_bLookAtLastOp)
		{
			pOperand->SetLooseAttentionTarget(0,m_looseAttentionId);
			m_looseAttentionId = 0;
		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
void COPStick::SSafePoint::Serialize(TSerialize ser)
{
	ser.BeginGroup("StickSafePoint");
	ser.Value("pos",pos);
	GetAISystem()->GetGraph()->SerializeNodePointer(ser, "node", nodeIndex);
	ser.Value("safe",safe);
	ser.Value("time",time);
	ser.EndGroup();
}


//===================================================================
// Serialize
//===================================================================
void COPApproach::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPApproach");
	{
		ser.Value("m_fLastDistance",m_fLastDistance);
		ser.Value("m_fInitialDistance",m_fInitialDistance);
		ser.Value("m_bPercent",m_bPercent);
		ser.Value("m_bUseLastOpResult",m_bUseLastOpResult);
		ser.Value("m_bLookAtLastOp",m_bLookAtLastOp);
		ser.Value("m_fEndDistance",m_fEndDistance);
		ser.Value("m_fEndAccuracy",m_fEndAccuracy);
		ser.Value("m_fDuration", m_fDuration);
		ser.Value("m_bForceReturnPartialPath",m_bForceReturnPartialPath);
		ser.Value("m_stopOnAnimationStart", m_stopOnAnimationStart);
		ser.Value("m_noPathSignalText",m_noPathSignalText);
		ser.Value("m_looseAttentionId",m_looseAttentionId);
		ser.Value("m_bPathFound", m_bPathFound);

		if(ser.IsWriting())
		{
			if(ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective!=NULL))
			{
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			if(ser.BeginOptionalGroup("PathFindDirective", m_pPathfindDirective!=NULL))
			{
				m_pPathfindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}		
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if(ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(true);
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			SAFE_DELETE(m_pPathfindDirective);
			if(ser.BeginOptionalGroup("PathFindDirective", true))
			{
				m_pPathfindDirective = new COPPathFind("");
				m_pPathfindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
	}
	ser.EndGroup();
}

//===================================================================
// OnObjectRemoved
//===================================================================
void COPApproach::OnObjectRemoved(CAIObject *pObject)
{
	if (m_pTraceDirective)
		m_pTraceDirective->OnObjectRemoved(pObject);
	if (m_pPathfindDirective)
		m_pPathfindDirective->OnObjectRemoved(pObject);
}

//===================================================================
// GetEndDistance
//===================================================================
float COPApproach::GetEndDistance(CPipeUser *pOperand) const
{
	if (m_fDuration > 0.0f)
	{
		//		float normalSpeed = pOperand->GetNormalMovementSpeed(pOperand->m_State.fMovementUrgency, false);
		float normalSpeed, smin, smax;
		pOperand->GetMovementSpeedRange(pOperand->m_State.fMovementUrgency, false, normalSpeed, smin, smax);
		if (normalSpeed > 0.0f)
			return -normalSpeed * m_fDuration;
	}
	else if (m_bPercent)
		return m_fInitialDistance;
	return m_fEndDistance;
}

//===================================================================
// Execute
//===================================================================
EGoalOpResult COPApproach::Execute(CPipeUser *pOperand)
{
	CAISystem *pSystem = GetAISystem();
	CAIObject *pTarget = (CAIObject*)pOperand->GetAttentionTarget();

	// Move strictly to the target point.
	pOperand->m_nMovementPurpose = 0;

	if (!pTarget || m_bUseLastOpResult) 
	{
		if (pOperand->m_pLastOpResult)
			pTarget = pOperand->m_pLastOpResult;
		else
		{
			// no target, nothing to approach to
			if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
				AILogAlways("COPApproach::Execute %s no target", pOperand->GetName());
			Reset(pOperand);
			return AIGOR_FAILED;
		}
	}

	// luciano - added check for formation points
	if(pTarget && !pTarget->IsEnabled())
	{
		if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
			AILogAlways("COPApproach::Execute %s target %s not enabled", pOperand->GetName(), pTarget->GetName());
		Reset(pOperand);
		return AIGOR_FAILED;
	}

	Vec3 mypos(pOperand->GetPos()) ;

	Vec3 targetpos;
	Vec3 targetdir;
	if(pOperand->m_nPathDecision == PATHFINDER_PATHFOUND && m_bForceReturnPartialPath)
	{
		targetpos = pOperand->m_Path.GetLastPathPos();
		targetdir = pOperand->m_Path.GetEndDir();
	}
	else
	{
		targetpos = pTarget->GetPhysicsPos();
		targetdir = pTarget->GetMoveDir();
	}

	if(!m_looseAttentionId && pOperand->m_pLastOpResult && m_bLookAtLastOp)
	{
		m_looseAttentionId = pOperand->SetLooseAttentionTarget(pOperand->m_pLastOpResult);
	}

	//TODO:  special heli treatment - make sure it gets up first, and then start moving
	if(pOperand->GetSubType() == CAIObject::STP_HELI)
	{
		if(CAIVehicle* pHeli = pOperand->CastToCAIVehicle())
			if(pHeli->HandleVerticalMovement( targetpos ))
				return AIGOR_IN_PROGRESS;
	}

	if (m_bPercent && (m_fInitialDistance==0))
	{
		m_fInitialDistance = (targetpos-mypos).GetLength();
		m_fInitialDistance *= m_fEndDistance;
	}

	if ( !pOperand->m_movementAbility.bUsePathfinder )
	{
		mypos -= targetpos;
		Vec3	projectedDist = mypos;
		float dist = pOperand->m_movementAbility.b3DMove ? projectedDist.GetLength() : projectedDist.GetLength2D();

		float endDistance = GetEndDistance(pOperand);

		if ( dist < endDistance )
		{
			if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
				AILogAlways("COPApproach::Execute %s No pathfinder and reached end", pOperand->GetName());
			m_fInitialDistance = 0;
			Reset(pOperand);
			return AIGOR_SUCCEED;
		}
		// no pathfinding - just approach
		pOperand->m_State.vMoveDir = targetpos - pOperand->GetPhysicsPos();
		pOperand->m_State.vMoveDir.Normalize();

		// Update the debug movement reason.
		pOperand->m_DEBUGmovementReason = CPipeUser::AIMORE_SMARTOBJECT;

		m_fLastDistance = dist;
		return AIGOR_IN_PROGRESS;
	}

	if (!m_bPathFound && !m_pPathfindDirective)
	{
		// generate path to target
		float endTol = m_bForceReturnPartialPath || m_fEndAccuracy < 0.0f ? std::numeric_limits<float>::max() : m_fEndAccuracy;

		// override end distance if a duration has been set
		float endDistance = GetEndDistance(pOperand);
		m_pPathfindDirective = new COPPathFind("", pTarget, endTol, endDistance);
		pOperand->m_nPathDecision = PATHFINDER_STILLFINDING;

		if (m_pPathfindDirective->Execute(pOperand) != AIGOR_IN_PROGRESS)
		{
			if (pOperand->m_nPathDecision == PATHFINDER_NOPATH)
			{
				if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
					AILogAlways("COPApproach::Execute %s pathfinder no path", pOperand->GetName());
				// If special nopath signal is specified, send the signal.
				if(m_noPathSignalText.size() > 0)
					pOperand->SetSignal(0,m_noPathSignalText.c_str(),NULL);
				pOperand->m_State.vMoveDir.Set(0,0,0);
				Reset(pOperand);
				return AIGOR_FAILED;
			}
		}
		return AIGOR_IN_PROGRESS;
	}

	// trace/pathfinding gets deleted when we reach the end
	if (!m_pPathfindDirective)
	{
		if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
			AILogAlways("COPApproach::Execute (%p) returning true due to no pathfinding directive %s", this, 
			pOperand ? pOperand->GetName() : "");
		Reset(pOperand);
		return AIGOR_FAILED;
	}

	// actually trace the path - continue doing this even whilst regenerating (etc) the path
	EGoalOpResult doneTracing = AIGOR_IN_PROGRESS;
	if (m_bPathFound)
	{
		if (!m_pTraceDirective)
		{
			if(pTarget->GetSubType() == CAIObject::STP_ANIM_TARGET && (m_fEndDistance > 0.0f || m_fDuration > 0.0f))
			{
				AILogAlways("COPApproach::Execute resetting approach distance from (endDist=%.1f duration=%.1f) to zero because the approach target is anim target. %s",
					m_fEndDistance, m_fDuration, pOperand ? pOperand->GetName() : "_no_operand_");
				m_fEndDistance = 0.0f;
				m_fDuration = 0.0f;
			}

			TPathPoints::const_reference lastPathNode = pOperand->m_OrigPath.GetPath().back();
			Vec3 lastPos = lastPathNode.vPos;
			Vec3 requestedLastNodePos = pOperand->m_Path.GetParams().end;
			float dist = Distance::Point_Point(lastPos,requestedLastNodePos);
			float endDistance = GetEndDistance(pOperand);
			if (lastPathNode.navType != IAISystem::NAV_SMARTOBJECT && dist > endDistance+C_MaxDistanceForPathOffset)// && pOperand->m_Path.GetPath().size() == 1 )
			{
				AISignalExtraData* pData = new AISignalExtraData;
				pData->fValue = dist - endDistance;
				pOperand->SetSignal(0,"OnEndPathOffset",pOperand->GetEntity(),pData, g_crcSignals.m_nOnEndPathOffset);
			}
			else
				pOperand->SetSignal(0,"OnPathFound",NULL, 0, g_crcSignals.m_nOnPathFound);

			bool bExact = false;
			//      m_pTraceDirective = new COPTrace(bExact, endDistance, m_fEndAccuracy, m_fDuration, m_bForceReturnPartialPath, m_stopOnAnimationStart);
			m_pTraceDirective = new COPTrace(bExact, m_fEndAccuracy, m_bForceReturnPartialPath, m_stopOnAnimationStart);
		}

		doneTracing = m_pTraceDirective->Execute(pOperand);



		// If this goal gets reseted during m_pTraceDirective->Execute it means that
		// a smart object was used for navigation which inserts a goal pipe which
		// does Reset on this goal which sets m_pTraceDirective to NULL! In this case
		// we should just report that this goal pipe isn't finished since it will be
		// reexecuted after finishing the inserted goal pipe
		if (!m_pTraceDirective)
			return AIGOR_IN_PROGRESS;

		// If the path has been traced, finish the operation
		if (doneTracing != AIGOR_IN_PROGRESS)
		{
			if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
				AILogAlways("COPApproach::Execute (%p) finishing due to finished tracing %s", this, 
				pOperand ? pOperand->GetName() : "");
			Reset(pOperand);
			return doneTracing;
		}
	}

	// check pathfinder status
	switch(pOperand->m_nPathDecision)
	{
	case PATHFINDER_STILLFINDING:
		m_pPathfindDirective->Execute(pOperand);		
		return AIGOR_IN_PROGRESS;

	case PATHFINDER_NOPATH:
		pOperand->m_State.vMoveDir.Set(0,0,0);
		if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
			AILogAlways("COPApproach::Execute (%p) resetting due to no path %s", this, 
			pOperand ? pOperand->GetName() : "");
		// If special nopath signal is specified, send the signal.
		if(m_noPathSignalText.size() > 0)
			pOperand->SetSignal(0,m_noPathSignalText.c_str(),NULL);
		Reset(pOperand);
		return AIGOR_FAILED;

	case PATHFINDER_PATHFOUND:
		if(!m_bPathFound)
		{
			m_bPathFound = true;
			return Execute(pOperand);
		}
		break;
	}

	return AIGOR_IN_PROGRESS;
}

//===================================================================
// DebugDraw
//===================================================================
void COPApproach::DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const
{
	if (m_pTraceDirective)
		m_pTraceDirective->DebugDraw(pOperand, pRenderer);
	if (m_pPathfindDirective)
		m_pPathfindDirective->DebugDraw(pOperand, pRenderer);
}

//===================================================================
// ExecuteDry
//===================================================================
void COPApproach::ExecuteDry(CPipeUser *pOperand) 
{
	if (m_pTraceDirective && m_bPathFound)
		m_pTraceDirective->ExecuteTrace(pOperand, false);
}

//====================================================================
// COPFollowPath
//====================================================================
COPFollowPath::COPFollowPath(bool pathFindToStart, bool reverse, bool startNearest, int loops, float fEndAccuracy, bool bUsePointList, bool bAvoidDynamicObjects) :
m_pathFindToStart(pathFindToStart),
m_reversePath(reverse),
m_startNearest(startNearest),
m_bUsePointList(bUsePointList),
m_bAvoidDynamicObjects(bAvoidDynamicObjects),
m_loops(loops),
m_loopCounter(0),
m_notMovingTime(0.0f),
m_returningToPath(false)
{
	m_pTraceDirective = 0;
	m_pPathFindDirective = 0;
	m_pPathStartPoint = 0;
	m_fEndAccuracy  = fEndAccuracy;

}

//====================================================================
// ~COPFollowPath
//====================================================================
COPFollowPath::~COPFollowPath()
{
	Reset(0);
	if (m_pPathStartPoint)
	{
		GetAISystem()->RemoveDummyObject(m_pPathStartPoint);
		m_pPathStartPoint = 0;
	}
}

//====================================================================
// Reset
//====================================================================
void COPFollowPath::Reset(CPipeUser *pOperand)
{
	delete m_pTraceDirective;
	m_pTraceDirective = 0;
	delete m_pPathFindDirective;
	m_pPathFindDirective = 0;
	m_loopCounter = 0;
	m_notMovingTime = 0.0f;
	m_returningToPath = false;
	m_fEndAccuracy = defaultTraceEndAccuracy;
	if (pOperand)
		pOperand->ClearPath("COPFollowPath::Reset m_Path");
}

//====================================================================
// Serialize
//====================================================================
void COPFollowPath::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPFollowPath");
	{
		ser.Value("m_pathFindToStart",m_pathFindToStart);
		ser.Value("m_reversePath",m_reversePath);
		ser.Value("m_startNearest",m_startNearest);
		ser.Value("m_loops",m_loops);
		ser.Value("m_loopCounter",m_loopCounter);
		ser.Value("m_TraceEndAccuracy",m_fEndAccuracy);
		ser.Value("m_notMovingTime",m_notMovingTime);
		ser.Value("m_returningToPath",m_returningToPath);
		ser.Value("m_bUsePointList",m_bUsePointList);
		ser.Value("m_bAvoidDynamicObjects",m_bUsePointList);
		objectTracker.SerializeObjectPointer(ser,"m_pPathStartPoint",m_pPathStartPoint,false);
		if(ser.IsWriting())
		{
			if(ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective!=NULL))
			{
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			if(ser.BeginOptionalGroup("PathFindDirective", m_pPathFindDirective!=NULL))
			{
				m_pPathFindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}		
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if(ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(false, m_fEndAccuracy);
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			SAFE_DELETE(m_pPathFindDirective);
			if(ser.BeginOptionalGroup("PathFindDirective", true))
			{
				m_pPathFindDirective = new COPPathFind("");
				m_pPathFindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		ser.EndGroup();
	}
}

bool COPFollowPath::HandleBlockInTheWay(CPipeUser *pOperand)
{	
	if(!m_returningToPath && !m_pPathFindDirective && m_pTraceDirective)
	{			
		float fPosAlongPathDistance;
		gEnv->pAISystem->GetConstant(IAISystem::eAI_CONST_fPosAlongPathDistance, fPosAlongPathDistance);

		Vec3 to;
		if(pOperand->m_Path.GetPosAlongPath(to, fPosAlongPathDistance, true, true))
		{
			float	hitDist;
			Vec3 from = pOperand->GetPhysicsPos();

			IPhysicalEntity  *pSkipEnt=pOperand->GetEntity()->GetPhysics();
			if(IntersectSweptSphere(0, hitDist, Lineseg(from, to), pOperand->m_Parameters.m_fPassRadius+1.f, AICE_DYNAMIC, &pSkipEnt, 1))
			{												
				if(pOperand->m_Path.GetPosAlongPath(to, fPosAlongPathDistance*2, true, false))
				{
					if (!m_pPathStartPoint)
						m_pPathStartPoint = GetAISystem()->CreateDummyObject( string(pOperand->GetName()) + "_PathStartPoint", CAIObject::STP_REFPOINT );
					m_pPathStartPoint->SetPos(to);
					m_pPathFindDirective = new COPPathFind("FollowPath", m_pPathStartPoint);
					delete m_pTraceDirective;
					m_pTraceDirective = 0;
					m_returningToPath = true;
					return true;
				}
			}
		}
	}
	return false;
}

//====================================================================
// Execute
//====================================================================
EGoalOpResult COPFollowPath::Execute(CPipeUser *pOperand)
{
	CAIObject *pTarget = (CAIObject*)pOperand->GetAttentionTarget();
	CAISystem *pSystem = GetAISystem();

	// for a temporary, until all functionality have made.
	if ( m_bUsePointList == true && !( m_pathFindToStart == false && m_reversePath == false && m_startNearest == false ) )
	{
		AIWarning("COPFollowPath:: bUsePointList only support false,flase,false in first 3 paramters for %s", pOperand->GetName());
		return AIGOR_FAILED;
	}

	// if we have a path, trace it. Once we get going (i.e. pathfind is finished, if we use it) we'll
	// always have a trace
	if (!m_pTraceDirective)
	{
		if (m_pPathFindDirective)
		{
			EGoalOpResult finishedPathFind = m_pPathFindDirective->Execute(pOperand);
			if (finishedPathFind != AIGOR_IN_PROGRESS)
			{
				m_pTraceDirective = new COPTrace(false, m_fEndAccuracy);
				m_lastTime = pSystem->GetFrameStartTime();
			}
			else
				return AIGOR_IN_PROGRESS;
		}
		else // no pathfind directive
		{
			//      m_pathFindToStart = pOperand->GetPathFindToStartOfPathToFollow(pathStartPos);
			if (m_pathFindToStart)
			{
				if (!m_pPathStartPoint)
					m_pPathStartPoint = GetAISystem()->CreateDummyObject( string(pOperand->GetName()) + "_PathStartPoint", CAIObject::STP_REFPOINT );

				AIAssert(m_pPathStartPoint);

				Vec3	entryPos;
				if(!pOperand->GetPathEntryPoint(entryPos, m_reversePath, m_startNearest))
				{
					AIWarning("COPFollowPath::Unable to find path entry point for %s - check path is not marked as a road", pOperand->GetName());
					return AIGOR_FAILED;
				}
				m_pPathStartPoint->SetPos(entryPos);
				m_pPathFindDirective = new COPPathFind("FollowPath", m_pPathStartPoint);

				return AIGOR_IN_PROGRESS;
			}
			else
			{
				if ( m_bUsePointList == true )
				{
					bool pathOK = pOperand->UsePointListToFollow();
					if (!pathOK)
					{
						AIWarning("COPFollowPath::Execute Unable to use point list %s", pOperand->GetName());
						return AIGOR_FAILED;
					}
				}
				else
				{
					bool pathOK = pOperand->UsePathToFollow(m_reversePath, m_startNearest, m_loops != 0);
					if (!pathOK)
					{
						AIWarning("COPFollowPath::Execute Unable to use follow path for %s - check path is not marked as a road", pOperand->GetName());
						return AIGOR_FAILED;
					}
				}
				m_pTraceDirective = new COPTrace(false, m_fEndAccuracy);
				m_lastTime = pSystem->GetFrameStartTime();
			} // m_pathFindToStart
		} // path find directive
	}
	AIAssert(m_pTraceDirective);
	EGoalOpResult done = m_pTraceDirective->Execute(pOperand);

	AIAssert(m_pTraceDirective);

	// HACK: The following code tries to put a lost or stuck agent back on track.
	// It works together with a piece of in ExecuteDry which tracks the speed relative
	// to the requested speed and if it drops dramatically for certain time, this code
	// will trigger and try to move the agent back on the path. [Mikko]

	if(m_bAvoidDynamicObjects)
	{
		if(HandleBlockInTheWay(pOperand))
			return AIGOR_IN_PROGRESS;
	}
	
	float timeout = 0.7f;
	if ( pOperand->GetType() == AIOBJECT_VEHICLE )
		timeout = 7.0f;
	if ( pOperand->GetSubType() == CAIObject::STP_2D_FLY  )
		m_notMovingTime = 0.0f;

	if (m_notMovingTime > timeout)
	{
		// Stuck or lost, move to the nearest point on path.
		AIWarning("COPFollowPath::Entity %s has not been moving fast enough for %.1fs it might be stuck, find back to path.", pOperand->GetName(), m_notMovingTime);
		if (!m_pPathStartPoint)
			m_pPathStartPoint = GetAISystem()->CreateDummyObject( string(pOperand->GetName()) + "_PathStartPoint", CAIObject::STP_REFPOINT );
		AIAssert(m_pPathStartPoint);
		Vec3	entryPos;
		if (!pOperand->GetPathEntryPoint(entryPos, m_reversePath, true))
		{
			AIWarning("COPFollowPath::Unable to find path entry point for %s - check path is not marked as a road", pOperand->GetName());
			return AIGOR_FAILED;
		}
		m_pPathStartPoint->SetPos(entryPos);
		m_pPathFindDirective = new COPPathFind("FollowPath", m_pPathStartPoint);
		delete m_pTraceDirective;
		m_pTraceDirective = 0;
		m_notMovingTime = 0.0f;
		m_returningToPath = true;
	}

	if (done != AIGOR_IN_PROGRESS || !m_pTraceDirective)
	{
		if ((m_pathFindToStart || m_returningToPath) && m_pPathFindDirective)
		{
			delete m_pPathFindDirective;
			m_pPathFindDirective = 0;
			delete m_pTraceDirective;
			bool pathOK = pOperand->UsePathToFollow(m_reversePath, (m_startNearest || m_returningToPath), m_loops > 0);
			if (!pathOK)
			{
				AIWarning("COPFollowPath::Execute Unable to use follow path for %s - check path is not marked as a road", pOperand->GetName());
				return AIGOR_FAILED;
			}
			m_pTraceDirective = new COPTrace(false, m_fEndAccuracy );
			m_lastTime = pSystem->GetFrameStartTime();
			m_returningToPath = false;
			return AIGOR_IN_PROGRESS;
		}
		else
		{
			if(m_loops != 0)
			{
				m_loopCounter++;
				if(m_loops != -1 && m_loopCounter >= m_loops)
				{
					Reset(pOperand);
					return AIGOR_SUCCEED;
				}

				delete m_pTraceDirective;
				bool pathOK = pOperand->UsePathToFollow(m_reversePath, m_startNearest, m_loops != 0);
				if (!pathOK)
				{
					AIWarning("COPFollowPath::Execute Unable to use follow path for %s - check path is not marked as a road", pOperand->GetName());
					return AIGOR_FAILED;
				}
				m_pTraceDirective = new COPTrace(false, m_fEndAccuracy);
				m_lastTime = pSystem->GetFrameStartTime();
				// For seamless operation, update trace already.
				m_pTraceDirective->Execute(pOperand);
				return AIGOR_IN_PROGRESS;
			}
		}

		Reset(pOperand);
		return AIGOR_SUCCEED;
	}

	return AIGOR_IN_PROGRESS;
}

//====================================================================
// ExecuteDry
//====================================================================
void COPFollowPath::ExecuteDry(CPipeUser *pOperand) 
{
	CAISystem *pSystem = GetAISystem();

	if (m_pTraceDirective)
	{
		m_pTraceDirective->ExecuteTrace(pOperand, false);

		// HACK: The following code together with some logic in the execute tries to keep track
		// if the agent is not moving for some time (is stuck), and pathfinds back to the path. [Mikko]
		CTimeValue	time(pSystem->GetFrameStartTime());
		float	dt((time - m_lastTime).GetSeconds());
		float	speed = pOperand->GetVelocity().GetLength();
		float	desiredSpeed = pOperand->m_State.fDesiredSpeed;
		if(desiredSpeed > 0.0f)
		{
			float ratio = clamp(speed / desiredSpeed, 0.0f, 1.0f);
			if(ratio < 0.1f)
				m_notMovingTime += dt;
			else
				m_notMovingTime -= dt;
			if(m_notMovingTime < 0.0f)
				m_notMovingTime = 0.0f;
		}
		m_lastTime = time;
	}
}

//====================================================================
// COPFollow
//====================================================================
COPFollow::COPFollow(float fDistance, bool bUseLastOpResult):
m_pLeader(NULL)
{
	m_pTargetPathMarker = NULL;
	m_bUseLastOpResult = bUseLastOpResult;
	m_fDistance = fDistance;
	m_fInitialDistance = 0;
	//	m_nTicks = 0;
	//	m_pPathfindDirective = 0;
	m_pFollowTarget = 0;
	//	m_fLastDistance = -1;// initialized later after 1st update
	m_fLastTargetSpeed = 0;
	m_lastMoveDir.Set( 0, 0, 0 );
	m_lastUpdateTime = GetAISystem()->GetFrameStartTime();

	// Allow 20% variation.
	m_fVariance = 1.0f; // + (((float)ai_rand() / (float)RAND_MAX) * 2.0f - 1.0f) * 0.2f;
}


COPFollow::~COPFollow()
{
	if(m_pTargetPathMarker)
		delete m_pTargetPathMarker;
	if (m_pFollowTarget)
		GetAISystem()->RemoveDummyObject(m_pFollowTarget);

}

void COPFollow::Reset(CPipeUser *pOperand)
{
	if (pOperand)
		pOperand->Following(NULL);

	delete m_pTargetPathMarker;
	m_pTargetPathMarker=NULL;

	m_lastUpdateTime = GetAISystem()->GetFrameStartTime();
	m_fInitialDistance = 0;
	m_fLastTargetSpeed = 0;
	m_lastMoveDir.Set( 0, 0, 0 );

	m_pLeader = NULL;

	if (m_pFollowTarget)
	{
		GetAISystem()->RemoveDummyObject(m_pFollowTarget);
		m_pFollowTarget = 0;
	}
}

EGoalOpResult COPFollow::Execute(CPipeUser *pOperand)
{
	CTimeValue currentTime = GetAISystem()->GetFrameStartTime();
	float timeStep = (currentTime - m_lastUpdateTime).GetSeconds();
	m_lastUpdateTime = currentTime;
	Vec3 opPos = pOperand->GetPhysicsPos();

	CAIObject*	pTarget = m_pLeader;
	if(!pTarget)
	{
		pTarget = (CAIObject*)pOperand->GetAttentionTarget();
		CAISystem*	pSystem = GetAISystem();
		Vec3		vFollowPos, vFollowDir;
		if (!pTarget || m_bUseLastOpResult) 
		{
			if (pOperand->m_pLastOpResult)
				pTarget = pOperand->m_pLastOpResult;
			else
			{
				// no target, nothing to follow
				Reset(pOperand);
				return AIGOR_FAILED;
			}
		}

		m_pLeader = pTarget;

		// it's a first time - create pathMaker
		if(!m_pTargetPathMarker )
		{
			float fStep = 1.0f;
			m_pTargetPathMarker = new CPathMarker( m_fDistance + m_fDistance + 1, fStep );

			Vec3 vTargetDir = pTarget->GetMoveDir();//0,-1,0);
			if(pOperand->GetType() == AIOBJECT_VEHICLE)
				vTargetDir *= m_fDistance;
			else
				vTargetDir *= -m_fDistance;

			Vec3 mid = opPos + pTarget->GetPhysicsPos();

			m_pTargetPathMarker->Init( opPos , mid * 0.5f );
		}

		if (!m_pFollowTarget)
		{
			m_pFollowTarget = GetAISystem()->CreateDummyObject(string(pOperand->GetName()) + "_FollowTarget");
			pOperand->Following(pTarget);
		}

	}

	Vec3 vTargetPos= pTarget->GetPhysicsPos();

	extern CAIObject * gVehicleToDebug;
	extern CSteeringDebugInfo gSteeringDebugInfo;
	CSteeringDebugInfo * debug = (gVehicleToDebug == pOperand) ? &gSteeringDebugInfo : 0;

	// Update the target point to the path maker.
	m_pTargetPathMarker->Update(vTargetPos);
	// calculate directions
	float alignmentWithPath;

	pOperand->m_State.vMoveDir = m_pTargetPathMarker->GetMoveDirectionAtDistance( vTargetPos, m_fDistance, opPos , 7.0f, &alignmentWithPath, debug);
	if( pOperand->m_State.vMoveDir.GetLengthSquared() > 0.00001f )
		m_lastMoveDir = pOperand->m_State.vMoveDir;
	else
		AIWarning("COPFollow::Execute MoveDir pretty well zero length");

	// calculate velocity
	float	fCurrentDistance = m_pTargetPathMarker->GetDistanceToPoint( vTargetPos, opPos );

	Vec3	vLeaderDir = vTargetPos - opPos;
	float	fDirectDistToLeader = vLeaderDir.GetLength();

	// To avoid collision with the leader, use shortest of the direct distance or distance along the follow path.
	// This will allow nicer handling in sharp corners, etc.
	if( fDirectDistToLeader < fCurrentDistance )
		fCurrentDistance = fDirectDistToLeader;

	if (1)
	{
		// TODO Danny - tidy up code, put params (work well for jeep, truck + tank) into config)
		// use PID controller
		// Use a PID controller based on the distance to the end
		static float CP = 0.05f;
		static float CI = 0.01f;
		static float CD = 0.05f;
		static float timeScale = 2.0f;
		static unsigned proportionalPower = 1;
		m_PIDController.CP = CP;
		m_PIDController.CI = CI;
		m_PIDController.CD = CD;
		m_PIDController.integralTimescale = timeScale;
		m_PIDController.proportionalPower = proportionalPower;

		float error = fCurrentDistance - m_fDistance;

		float PIDOutput = m_PIDController.Update(error, timeStep);

		// slow down if we're not aligned with the bit of the path we're aiming for
		if (alignmentWithPath < 0.5f)
			alignmentWithPath = 0.5f;

		PIDOutput *= square(alignmentWithPath);
		Limit(PIDOutput, 0.0f, 1.0f);

		float normalSpeed, minSpeed, maxSpeed;
		pOperand->GetMovementSpeedRange(pOperand->m_State.fMovementUrgency, pOperand->m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

		//		float normalSpeed = pOperand->GetNormalMovementSpeed(pOperand->m_State.fMovementUrgency, true);
		//		float slowSpeed = pOperand->GetManeuverMovementSpeed();

		if ( PIDOutput < 0.0001f )
		{
			float cof = 1.0f - ( (error * -1.0f) / (m_fDistance * 0.3f) );
			minSpeed *= cof;
			if ( cof < 0.1f )
				minSpeed = 0.0f;
		}

		if (minSpeed > normalSpeed) minSpeed = normalSpeed;
		pOperand->m_State.fDesiredSpeed = (1.0f - PIDOutput) * minSpeed + PIDOutput * normalSpeed;
	}
	else
	{
		float	fDefaultSpeed = m_fVariance;	// Vary the default speed to add random jitter to the follow formation or convoy.
		float	fFollowSpeed = 1.0f;
		float	fFollowDistance = m_fDistance;

		float	fAdjustZone = 5.0f;	// meters
		if( fAdjustZone > fFollowDistance * 0.9f )
			fAdjustZone = fFollowDistance * 0.9f;

		float	fNearZone = fFollowDistance - fAdjustZone;
		float	fSlowZone = fFollowDistance - fAdjustZone * 0.5f;
		float	fKeepZone = fFollowDistance + fAdjustZone * 0.5f;
		float	fCatchZone = fFollowDistance + fAdjustZone;

		// Classify the follower position
		if( fCurrentDistance < fNearZone ) //fFollowDistance * 0.4f )
		{
			// Stop! if too close.
			fFollowSpeed = 0;
			// Kill the movement, just wait for the leader to continue.
			//		pOperand->m_State.vMoveDir.Set( 0, 0, 0 );
		}
		else if( fCurrentDistance < fSlowZone ) //fFollowDistance * 0.8f )
		{
			// Too close to the leader, slowdown.
			float	fAlpha = (fCurrentDistance - fNearZone)/ (fSlowZone - fNearZone);
			// Use exponential decay from 50% of the target speed to zero.
			fFollowSpeed = fDefaultSpeed * (fAlpha * fAlpha) * 0.8f;
		}
		else if( fCurrentDistance < fKeepZone )
		{
			// In the follow zone, try to maintain position.
			float	fAlpha = (fCurrentDistance - fSlowZone) / (fKeepZone - fSlowZone);
			// Lag always just a little behind.
			fFollowSpeed = fDefaultSpeed * (0.8f + fAlpha * 0.4f);
		}
		else if( fCurrentDistance < fCatchZone )
		{
			// Too far away, try to catch up.
			float	fAlpha = (fCurrentDistance - fKeepZone) / (fCatchZone - fKeepZone);
			// Use exponenetial decay to accelerate from 100% to max speed.
			fFollowSpeed = fDefaultSpeed * (1.2f + fAlpha * 0.8f);
		}
		else
		{
			// Very far away, try to catch quickly.
			fFollowSpeed = fDefaultSpeed * 2.0f;	// Max speed.
		}

		// slow down if we're not aligned with the bit of the path we're aiming for
		if (alignmentWithPath < 0.5f)
			alignmentWithPath = 0.5f;

		fFollowSpeed *= alignmentWithPath;

		// Target speed
		float fTargetSpeed = 0.0f;
		IAIObject* pLeadingAI = pOperand->GetFollowed(); //GetFollowedLeader();
		if( pLeadingAI && pLeadingAI->GetProxy())
		{
			pe_status_dynamics  dSt;
			// steer ammount depends on current speed
			pLeadingAI->GetProxy()->GetPhysics()->GetStatus(&dSt);
			float	fLeaderSpeed = dSt.v.len();
			float	fSpeedError = fLeaderSpeed - m_fLastTargetSpeed;
			fTargetSpeed = m_fLastTargetSpeed + fSpeedError * 0.1f;

			m_fLastTargetSpeed = fTargetSpeed;
		}

		pOperand->m_State.fDesiredSpeed = fFollowSpeed * fTargetSpeed;
	}

	// Make sure the move dir is always valid.
	//	if( pOperand->m_State.vMoveDir.GetLengthSquared() < 0.00001f )
	//		pOperand->m_State.vMoveDir = m_lastMoveDir;


	return AIGOR_IN_PROGRESS;
}

//
//----------------------------------------------------------------------------------------------------------
void COPFollow::OnObjectRemoved(CAIObject *pObject)
{
	if(pObject == m_pLeader)
		Reset(NULL);
}



void COPFollow::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{

	ser.BeginGroup( "AICOPFollow" );
	{
		ser.Value("m_fInitialDistance",m_fInitialDistance);
		ser.Value("m_bUseLastOpResult",m_bUseLastOpResult);
		//		ser.Value("m_nTicks",m_nTicks);
		ser.Value("m_lastMoveDir",m_lastMoveDir);
		ser.Value("m_lastUpdateTime",m_lastUpdateTime);
		ser.Value("m_fLastTargetSpeed",m_fLastTargetSpeed);
		ser.Value("m_fVariance",m_fVariance);
		ser.Value("m_fInitialDistance",m_fInitialDistance);
		ser.Value("m_fDistance",m_fDistance);

		objectTracker.SerializeObjectPointer(ser, "m_pFollowTarget", m_pFollowTarget, false);
		objectTracker.SerializeObjectPointer(ser, "m_pLeader", m_pLeader, false);

		m_PIDController.Serialize(ser,objectTracker);

		if(ser.IsWriting())
		{
			if(ser.BeginOptionalGroup("TargetPathMarker", m_pTargetPathMarker!=NULL))
			{
				m_pTargetPathMarker->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		else
		{
			SAFE_DELETE(m_pTargetPathMarker);
			if(ser.BeginOptionalGroup("TargetPathMarker",true))
			{
				m_pTargetPathMarker = new CPathMarker(m_fDistance + m_fDistance + 1, 1.0f );
				m_pTargetPathMarker->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		ser.EndGroup();
	}
}


//===================================================================
// COPBackoff
//===================================================================
COPBackoff::COPBackoff(float distance, float duration, int filter, float minDistance)
{
	m_fDistance = distance;
	m_fDuration = duration;
	m_iDirection = filter & (AI_MOVE_FORWARD | AI_MOVE_BACKWARD | AI_MOVE_LEFT | AI_MOVE_RIGHT | AI_MOVE_TOWARDS_GROUP | AI_MOVE_BACKLEFT | AI_MOVE_BACKRIGHT);
	m_fMinDistance = minDistance;
	if(m_iDirection==0)
		m_iDirection = AI_MOVE_BACKWARD;

	//m_iAvailableDirections = m_iDirection;

	m_bUseLastOp = (filter & AILASTOPRES_USE) !=0;
	m_bLookForward = (filter & AI_LOOK_FORWARD) !=0;
	m_bUseTargetPosition = (filter & AI_BACKOFF_FROM_TARGET)!=0;
	m_bRandomOrder = (filter & AI_RANDOM_ORDER)!=0;
	m_bCheckSlopeDistance = (filter & AI_CHECK_SLOPE_DISTANCE) !=0;

	if(m_fDistance<0.f)
	{
		m_fDistance = -m_fDistance;
	}
	m_pPathfindDirective = 0;
	m_pTraceDirective = 0;
	m_pBackoffPoint = 0;
	m_currentDirMask = AI_MOVE_BACKWARD;
	m_vBestFailedBackoffPos.Set(0,0,0);
	m_fMaxDistanceFound = 0.f;
	m_bTryingLessThanMinDistance = false;
	m_looseAttentionId = 0;
	ResetMoveDirections();
}

//===================================================================
// COPBackoff
//===================================================================
COPBackoff::~COPBackoff()
{
	Reset(0);
}

//===================================================================
// OnObjectRemoved
//===================================================================
void COPBackoff::OnObjectRemoved(CAIObject *pObject)
{
	if (m_pTraceDirective)
		m_pTraceDirective->OnObjectRemoved(pObject);
	if (m_pPathfindDirective)
		m_pPathfindDirective->OnObjectRemoved(pObject);
}


//===================================================================
// Reset
//===================================================================
void COPBackoff::Reset(CPipeUser *pOperand)
{
	ResetNavigation(pOperand);
	m_fMaxDistanceFound = 0;
	m_bTryingLessThanMinDistance = false;
	m_vBestFailedBackoffPos.Set(0,0,0);
	m_currentDirMask = AI_MOVE_BACKWARD;
	ResetMoveDirections();
}

//===================================================================
// SetMoveDirections
//===================================================================
void COPBackoff::ResetMoveDirections()
{
	m_iCurrentDirectionIndex=0;
	m_MoveDirections.clear();

	int mask = (AI_MOVE_FORWARD | AI_MOVE_BACKWARD | AI_MOVE_LEFT | AI_MOVE_RIGHT | AI_MOVE_TOWARDS_GROUP | AI_MOVE_BACKLEFT | AI_MOVE_BACKRIGHT);
	mask &= m_iDirection;

	int currentdir = 1;
	while(currentdir <= mask)
	{
		if((mask & currentdir)!=0)
			m_MoveDirections.push_back(currentdir);
		currentdir<<=1;
	}
	if(m_bRandomOrder)
	{
		int size = m_MoveDirections.size();
		for(int i=0;i<size;++i)
		{
			int other = ai_rand() % size;
			int temp = m_MoveDirections[i];
			m_MoveDirections[i] = m_MoveDirections[other];
			m_MoveDirections[other] = temp;
		}
	}
}

//===================================================================
// ResetNavigation
//===================================================================
void COPBackoff::ResetNavigation(CPipeUser *pOperand)
{
	if(m_pPathfindDirective)
		delete m_pPathfindDirective;
	m_pPathfindDirective = 0;
	if(m_pTraceDirective)
		delete m_pTraceDirective;
	m_pTraceDirective = 0;

	if (pOperand)
		pOperand->ClearPath("COPBackoff::Reset m_Path");

	if (m_pBackoffPoint)
	{
		GetAISystem()->RemoveDummyObject(m_pBackoffPoint);
		m_pBackoffPoint = 0;
		if(m_bLookForward && pOperand )
		{
			pOperand->SetLooseAttentionTarget(0,m_looseAttentionId);
			m_looseAttentionId = 0;
		}
	}
	m_looseAttentionId=0;
}

//===================================================================
// Execute
//===================================================================
void COPBackoff::ExecuteDry(CPipeUser *pOperand)
{
	if (m_pTraceDirective)
		m_pTraceDirective->ExecuteTrace(pOperand, false);
}

//===================================================================
// Execute
//===================================================================
EGoalOpResult COPBackoff::Execute(CPipeUser *pOperand)
{ 
	m_fLastUpdateTime = GetAISystem()->GetFrameStartTime();

	if (!m_pPathfindDirective)
	{
		CAIObject *pTarget = NULL;

		if(m_bUseLastOp)
		{
			pTarget = pOperand->m_pLastOpResult;
			if (!pTarget)
			{
				Reset(pOperand);
				return AIGOR_FAILED;
			}
		}
		else
		{
			pTarget = (CAIObject*)pOperand->GetAttentionTarget();
			if (!pTarget)
			{
				if (!(pTarget = pOperand->m_pLastOpResult))
				{
					Reset(pOperand);
					return AIGOR_FAILED;
				}
			}
		}

		/*
		int currentDir = m_iAvailableDirections & m_currentDirMask;

		m_iAvailableDirections &= ~m_currentDirMask;
		int mask = (AI_MOVE_FORWARD | AI_MOVE_BACKWARD | AI_MOVE_LEFT | AI_MOVE_RIGHT | AI_MOVE_TOWARDS_GROUP | AI_MOVE_BACKLEFT | AI_MOVE_BACKRIGHT);

		while(currentDir==0 && m_currentDirMask <= mask)
		{
		m_currentDirMask<<=1;
		currentDir = m_iAvailableDirections & m_currentDirMask;
		m_iAvailableDirections &= ~m_currentDirMask;
		}
		*/
		int currentDir = m_iCurrentDirectionIndex >= m_MoveDirections.size()? 0 : m_MoveDirections[m_iCurrentDirectionIndex];
		if(m_iCurrentDirectionIndex < m_MoveDirections.size())
			m_iCurrentDirectionIndex++;

		if(currentDir == 0)
		{
			if(m_fMaxDistanceFound>0.f)
			{
				// a path less than mindistance required has been found anyway
				m_pPathfindDirective = new COPPathFind("Backoff", m_pBackoffPoint, std::numeric_limits<float>::max(), 0.0f, false, m_fDistance);
				pOperand->m_nPathDecision = PATHFINDER_STILLFINDING;
				m_bTryingLessThanMinDistance = true;
				if (m_pPathfindDirective->Execute(pOperand) != AIGOR_IN_PROGRESS)
				{
					if (pOperand->m_nPathDecision == PATHFINDER_NOPATH)
					{
						pOperand->m_State.vMoveDir.Set(0,0,0);
						pOperand->SetSignal(1,"OnBackOffFailed" ,0, 0, g_crcSignals.m_nOnBackOffFailed);
						Reset(pOperand);	
						return AIGOR_FAILED;
					}
				}
			}
			else
			{
				pOperand->m_State.vMoveDir.Set(0,0,0);
				pOperand->SetSignal(1,"OnBackOffFailed",0, 0, g_crcSignals.m_nOnBackOffFailed);
				Reset(pOperand);	
				return AIGOR_FAILED;
			}
		}

		Vec3 mypos = pOperand->GetPhysicsPos();
		Vec3 tgpos = pTarget->GetPhysicsPos();

		// evil hack
		CAIActor* pTargetActor = pTarget->CastToCAIActor();
		if (pTargetActor && pTarget->GetType()==AIOBJECT_VEHICLE)
			tgpos+=pTargetActor->m_State.vMoveDir*10.f;

		Vec3 vMoveDir = tgpos - mypos;
		// zero the z value because for human backoffs it seems that when the z values differ 
		// by a large amount at close range the direction tracing doesn't work so well
		if (!pOperand->IsUsing3DNavigation())
			vMoveDir.z = 0.0f;

		Vec3 avoidPos;

		if (m_bUseTargetPosition)
			avoidPos = tgpos;
		else
			avoidPos = mypos;

		// This check assumes that the reason of the backoff
		// if to move away to specified direction away from a point to avoid.
		// If the agent is already far enough, stop immediately.
		if (Distance::Point_Point(avoidPos, mypos) > m_fDistance)
		{
			Reset(pOperand);	
			return AIGOR_SUCCEED;
		}


		switch(currentDir)
		{
		case AI_MOVE_FORWARD:
			break;
		case AI_MOVE_RIGHT:
			vMoveDir.Set(vMoveDir.y, -vMoveDir.x, 0.0f);
			break;
		case AI_MOVE_LEFT:
			vMoveDir.Set(-vMoveDir.y, vMoveDir.x, 0.0f);
			break;
		case AI_MOVE_BACKLEFT:
			vMoveDir = vMoveDir.GetRotated(Vec3Constants<float>::fVec3_OneZ,gf_PI/4);
			break;
		case AI_MOVE_BACKRIGHT:
			vMoveDir = vMoveDir.GetRotated(Vec3Constants<float>::fVec3_OneZ,-gf_PI/4);
			break;
		case AI_MOVE_TOWARDS_GROUP:
			{
				// Average direction from the target towards the group.
				vMoveDir.Set(0,0,0);
				int	groupId = pOperand->GetGroupId();
				AIObjects::iterator it = GetAISystem()->m_mapGroups.find(groupId);
				AIObjects::iterator end = GetAISystem()->m_mapGroups.end();
				for (; it != end && it->first == groupId; ++it)
				{
					CAIObject* pObject = it->second;
					if (!pObject->IsEnabled() || pObject->GetType() != AIOBJECT_PUPPET)
						continue;
					vMoveDir += pObject->GetPos() - tgpos;
				}
				if (!pOperand->IsUsing3DNavigation())
					vMoveDir.z = 0.0f;
			}
			break;
		case AI_MOVE_BACKWARD:
		default://
			vMoveDir = -vMoveDir;
			break;
		}
		vMoveDir.NormalizeSafe(Vec3Constants<float>::fVec3_OneX);
		vMoveDir *= m_fDistance;
		Vec3 backoffPos = avoidPos + vMoveDir;

		// do some distance adjustment for slopes, backoff along terrain
		if(m_bCheckSlopeDistance)
		{
			unsigned thisNodeIndex = GetAISystem()->GetGraph()->GetEnclosing(backoffPos,
				pOperand->m_movementAbility.pathfindingProperties.navCapMask,
				pOperand->m_Parameters.m_fPassRadius,
				pOperand->m_lastNavNodeIndex, 0.0f, 0, false, pOperand->GetName());
			pOperand->m_lastNavNodeIndex = thisNodeIndex;

			GraphNode* pThisNode = GetAISystem()->GetGraph()->GetNodeManager().GetNode(thisNodeIndex);
			if (pThisNode && (pThisNode->navType & IAISystem::NAV_TRIANGULAR))
			{
				float terrainLevel = gEnv->p3DEngine->GetTerrainElevation(backoffPos.x, backoffPos.y);
				Vec3 adjustedBackoffPos(backoffPos.x,backoffPos.y,terrainLevel);
				Vec3 dirOnTerrain( adjustedBackoffPos - avoidPos );

				float distOnTerrain = dirOnTerrain.GetLength();
				if(distOnTerrain> 0 )
				{
					vMoveDir = dirOnTerrain * m_fDistance / distOnTerrain;
					backoffPos = avoidPos + vMoveDir;
				}
			}
		}

		m_moveStart = mypos;
		m_moveEnd = backoffPos;

		// create a dummy object to pathfind towards
		if (!m_pBackoffPoint)
			m_pBackoffPoint = GetAISystem()->CreateDummyObject( string(pOperand->GetName()) + "_BackoffPoint" );
		AIAssert(m_pBackoffPoint);

		m_pBackoffPoint->SetPos(backoffPos);

		if(m_bLookForward )
			m_looseAttentionId = pOperand->SetLooseAttentionTarget(m_pBackoffPoint);

		// start pathfinding
		m_pPathfindDirective = new COPPathFind("Backoff", m_pBackoffPoint, std::numeric_limits<float>::max(), 0.0f, false, m_fDistance);
		pOperand->m_nPathDecision = PATHFINDER_STILLFINDING;
		if (m_pPathfindDirective->Execute(pOperand) != AIGOR_IN_PROGRESS)
		{
			if (pOperand->m_nPathDecision == PATHFINDER_NOPATH)
			{
				//				pOperand->m_State.vMoveDir.Set(0,0,0);
				//				pOperand->SetSignal(1,"OnBackOffFailed");
				ResetNavigation(pOperand);	
				return AIGOR_IN_PROGRESS;// check next direction
			}
		}
		return AIGOR_IN_PROGRESS;
	} // !m_pPathfindDirective

	if (pOperand->m_nPathDecision==PATHFINDER_PATHFOUND)
	{
		if(m_fMinDistance>0)
		{
			float dist = Distance::Point_Point(pOperand->GetPos(),pOperand->m_OrigPath.GetLastPathPos());
			if(dist<m_fMinDistance)
			{
				if(m_fMaxDistanceFound < dist)
				{
					m_fMaxDistanceFound = dist;
					m_vBestFailedBackoffPos = m_pBackoffPoint->GetPos();
				}
				ResetNavigation(pOperand);	
				return AIGOR_IN_PROGRESS;// check next direction
			}
		}
		if (!m_pTraceDirective)
		{
			m_pTraceDirective = new COPTrace(false, defaultTraceEndAccuracy);
			pOperand->m_Path.GetParams().precalculatedPath = true; // prevent path regeneration
			pOperand->SetSignal(0,"OnPathFound",NULL, 0, g_crcSignals.m_nOnPathFound);
			m_fInitTime = GetAISystem()->GetFrameStartTime();
		}

		if(m_fDuration>0 && (GetAISystem()->GetFrameStartTime() - m_fInitTime).GetSeconds() > m_fDuration)
		{
			Reset(pOperand);
			return AIGOR_SUCCEED;
		}

		// keep tracing
		EGoalOpResult done = m_pTraceDirective->Execute(pOperand);
		if (done != AIGOR_IN_PROGRESS)
		{
			Reset(pOperand);
			return done;
		}
		// If this goal gets reseted during m_pTraceDirective->Execute it means that
		// a smart object was used for navigation which inserts a goal pipe which
		// does Reset on this goal which sets m_pTraceDirective to NULL! In this case
		// we should just report that this goal pipe isn't finished since it will be
		// reexecuted after finishing the inserted goal pipe
		if (!m_pTraceDirective)
			return AIGOR_IN_PROGRESS;
	}
	else if (pOperand->m_nPathDecision == PATHFINDER_NOPATH)
	{
		if (m_bTryingLessThanMinDistance)
			pOperand->SetSignal(1,"OnBackOffFailed",0, 0, g_crcSignals.m_nOnBackOffFailed);	

		//redo next time for next direction...
		if (m_bTryingLessThanMinDistance)
		{
			ResetNavigation(pOperand); 
			return AIGOR_IN_PROGRESS;
		}
		else
		{//... unless the less-than-min distance path has already been tried
			Reset(pOperand);	
			return AIGOR_FAILED;
		}

	}
	else
	{
		m_pPathfindDirective->Execute(pOperand);
	}

	return AIGOR_IN_PROGRESS;
}

void COPBackoff::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{

	ser.BeginGroup("COPBackoff");
	{
		ser.Value("m_bUseTargetPosition",m_bUseTargetPosition);
		ser.Value("m_iDirection",m_iDirection);
		ser.Value("m_iCurrentDirectionIndex",m_iCurrentDirectionIndex);
		objectTracker.SerializeValueContainer(ser,"m_MoveDirections",m_MoveDirections);
		ser.Value("m_fDistance",m_fDistance);
		ser.Value("m_fDuration",m_fDuration);
		ser.Value("m_fInitTime",m_fInitTime);
		ser.Value("m_fLastUpdateTime",m_fLastUpdateTime);
		ser.Value("m_bUseLastOp",m_bUseLastOp);
		ser.Value("m_bLookForward",m_bLookForward);
		ser.Value("m_moveStart",m_moveStart);
		ser.Value("m_moveEnd",m_moveEnd);
		ser.Value("m_currentDirMask",m_currentDirMask);
		ser.Value("m_fMinDistance",m_fMinDistance);
		ser.Value("m_vBestFailedBackoffPos",m_vBestFailedBackoffPos);
		ser.Value("m_fMaxDistanceFound",m_fMaxDistanceFound);
		ser.Value("m_bTryingLessThanMinDistance",m_bTryingLessThanMinDistance);
		ser.Value("m_looseAttentionId",m_looseAttentionId);
		ser.Value("m_bRandomOrder",m_bRandomOrder);
		ser.Value("m_bCheckSlopeDistance",m_bCheckSlopeDistance);

		objectTracker.SerializeObjectPointer(ser,"m_pBackoffPoint",m_pBackoffPoint,false);
		if(ser.IsWriting())
		{
			if(ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective!=NULL))
			{
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			if(ser.BeginOptionalGroup("PathFindDirective", m_pPathfindDirective!=NULL))
			{
				m_pPathfindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}		
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if(ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(true);
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			SAFE_DELETE(m_pPathfindDirective);
			if(ser.BeginOptionalGroup("PathFindDirective", true))
			{
				m_pPathfindDirective = new COPPathFind("");
				m_pPathfindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		ser.EndGroup();

	}
}

void COPBackoff::DebugDraw(CPipeUser* pOperand, IRenderer* pRenderer) const
{
	Vec3	dir(m_moveEnd - m_moveStart);
	dir.NormalizeSafe();
	ColorB	col(255, 255, 255);
	pRenderer->GetIRenderAuxGeom()->DrawLine(m_moveStart + Vec3(0,0,0.25f), col, m_moveEnd + Vec3(0,0,0.25f), col);
	pRenderer->GetIRenderAuxGeom()->DrawCone(m_moveEnd + Vec3(0,0,0.25f), dir, 0.3f, 0.8f, col);
}

COPTimeout::COPTimeout(float intervalMin, CAISystem *pAISystem, float intervalMax)
{
	m_fIntervalMin = intervalMin;
	if( intervalMax > 0 )
		m_fIntervalMax = intervalMax;
	else
		m_fIntervalMax = m_fIntervalMin;
	Reset(0);
}

COPTimeout::~COPTimeout()
{
}

void COPTimeout::Reset(CPipeUser *pOperand)
{
	m_fCurrentInterval = m_fIntervalMin + (m_fIntervalMax - m_fIntervalMin) * ((float)ai_rand() / (float)RAND_MAX);
	m_startTime.SetSeconds(0.0f);
}

EGoalOpResult COPTimeout::Execute(CPipeUser *pOperand)
{
	CTimeValue time = GetAISystem()->GetFrameStartTime();
	if (m_startTime.GetSeconds() < 0.001f)
		m_startTime = time;

	CTimeValue timeElapsed = time - m_startTime;
	if (timeElapsed.GetSeconds() > m_fCurrentInterval)
		return AIGOR_DONE;

	return AIGOR_IN_PROGRESS;
}


COPStrafe::COPStrafe(float distanceStart, float distanceEnd, bool strafeWhileMoving)
{
	m_fDistanceStart = distanceStart;
	m_fDistanceEnd = distanceEnd;
	m_bStrafeWhileMoving = strafeWhileMoving;
}

COPStrafe::~COPStrafe()
{
}

EGoalOpResult COPStrafe::Execute(CPipeUser *pOperand)
{
	CPuppet *pPuppet = pOperand->CastToCPuppet();
	if(pPuppet)
		pPuppet->SetAllowedStrafeDistances(m_fDistanceStart, m_fDistanceEnd, m_bStrafeWhileMoving);
	return AIGOR_DONE;
}

COPFireCmd::COPFireCmd(int firemode, bool useLastOpResult, float intervalMin, float intervalMax):
m_command((EFireMode)firemode),
m_bUseLastOpResult(useLastOpResult),
m_fIntervalMin(intervalMin),
m_fIntervalMax(intervalMax),
m_fCurrentInterval(-1.f)
{
	m_startTime.SetSeconds(0.0f);
}

COPFireCmd::~COPFireCmd()
{
}

void  COPFireCmd::Reset(CPipeUser *pOperand)
{
	m_fCurrentInterval = -1.f;
	m_startTime.SetSeconds(0.0f);
}

EGoalOpResult COPFireCmd::Execute(CPipeUser *pOperand)
{
	CTimeValue time = GetAISystem()->GetFrameStartTime();
	// if this is a first time
	if (m_startTime.GetSeconds() < 0.001f)
	{
		m_startTime = time;
		if (m_fIntervalMin<0.f)
			m_fCurrentInterval = -1.f;
		else if (m_fIntervalMax>0.f)
			m_fCurrentInterval = Random(m_fIntervalMin, m_fIntervalMax);
		else
			m_fCurrentInterval = m_fIntervalMin;
		pOperand->SetFireMode(m_command);
		if (m_bUseLastOpResult)
			pOperand->SetFireTarget(pOperand->m_pLastOpResult);
		else
			pOperand->SetFireTarget(0);
	}

	CTimeValue timeElapsed = time - m_startTime;
	if (m_fCurrentInterval<0.f || timeElapsed.GetSeconds()>m_fCurrentInterval)
	{
		// stop firing if was timed
		if (m_fCurrentInterval>0.f)
			pOperand->SetFireMode(FIREMODE_OFF);
		Reset(pOperand);
		return AIGOR_DONE;
	}

	return AIGOR_IN_PROGRESS;

	//	pOperand->SetFireMode(m_command, m_bUseLastOpResult);
	//	return true;
}


COPBodyCmd::COPBodyCmd(int bodypos, bool delayed):
m_nBodyState(bodypos),
m_bDelayed(delayed)
{
}

EGoalOpResult COPBodyCmd::Execute(CPipeUser *pOperand)
{
	CPuppet* pPuppet = pOperand->CastToCPuppet();
	if (m_bDelayed)
	{
		if (pPuppet)
			pPuppet->SetDelayedStance(m_nBodyState);
	}
	else
	{
		pOperand->m_State.bodystate = m_nBodyState;
		if (pPuppet)
			pPuppet->SetDelayedStance(STANCE_NULL);
	}

	return AIGOR_DONE;
}


COPRunCmd::COPRunCmd(float maxUrgency, float minUrgency, float scaleDownPathLength) :
m_fMaxUrgency(ConvertUrgency(maxUrgency)),
m_fMinUrgency(ConvertUrgency(minUrgency)),
m_fScaleDownPathLength(scaleDownPathLength)
{
}

float COPRunCmd::ConvertUrgency(float speed)
{
	// does NOT match CFlowNode_AIBase<TDerived, TBlocking>::SetSpeed
	if (speed < -4.5 )
		return -AISPEED_SPRINT;
	else if (speed < -3.5 )
		return -AISPEED_RUN;
	else if (speed < -2.5 )
		return -AISPEED_WALK;
	else if (speed < -1.5 )
		return -AISPEED_SLOW;
	else if (speed < -0.5 )
		return AISPEED_SLOW;
	else if (speed <= 0.5)
		return AISPEED_WALK;
	else if (speed < 1.5)
		return AISPEED_RUN;
	else 
		return AISPEED_SPRINT;
}

EGoalOpResult COPRunCmd::Execute(CPipeUser *pOperand)
{
	pOperand->m_State.fMovementUrgency = m_fMaxUrgency;

	if (m_fScaleDownPathLength > 0.00001f)
	{
		// Set adaptive
		CPuppet *pPuppet = pOperand->CastToCPuppet();
		if (pPuppet)
			pPuppet->SetAdaptiveMovementUrgency(m_fMinUrgency, m_fMaxUrgency, m_fScaleDownPathLength);
	}
	else
	{
		// Reset adaptive
		CPuppet *pPuppet = pOperand->CastToCPuppet();
		if (pPuppet)
			pPuppet->SetAdaptiveMovementUrgency(0,0,0);
	}

	return AIGOR_DONE;
}

// todo luc fixme
COPLookAt::COPLookAt(float startangle, float endangle, int mode, bool bUseLastOp) 
{
	m_fStartAngle = startangle; 
	m_fEndAngle = endangle;
	m_bUseLastOp = bUseLastOp;
	m_bContinuous = (mode & AI_LOOKAT_CONTINUOUS)!=0;
	m_bUseBodyDir = (mode & AI_LOOKAT_USE_BODYDIR)!=0;
	//m_looseAttentionId = 0;
	m_bInitialized = false;
}

COPLookAt::~COPLookAt()
{
}
void COPLookAt::ResetLooseTarget(CPipeUser *pOperand, bool bForceReset)
{
	if(bForceReset || !m_bContinuous)// || m_looseAttentionId)
	{
		pOperand->SetLooseAttentionTarget(0);//, (bForceReset || m_bContinuous? -1 :m_looseAttentionId));
		//m_looseAttentionId = 0;
	}
}
void COPLookAt::Reset(CPipeUser *pOperand)
{
	ResetLooseTarget(pOperand);
	m_bInitialized = false;
}

//
//----------------------------------------------------------------------------------------------------------
void COPLookAt::OnObjectRemoved(CAIObject *pObject)
{
	// not needed because m_pDummyAttTarget is member of COPLookAt
	// and could only be deleted from COPLookAt::Reset
	/*	if (m_pDummyAttTarget == pObject)
	{
	m_pDummyAttTarget = NULL;
	m_fStartAngle = -1000.f; // makes the Execute return true immediately
	}*/
}


EGoalOpResult COPLookAt::Execute(CPipeUser *pOperand)
{
	if (m_fStartAngle < -450.0f)	// stop looking at - disable looseAttentionTarget
	{
		ResetLooseTarget(pOperand, true);
		return AIGOR_SUCCEED;
	}

	// first time	
	//if (!m_looseAttentionId)
	if (!m_bInitialized)
	{ 
		m_bInitialized = true;
		if (!m_fStartAngle && !m_fEndAngle && !m_bUseLastOp)
		{
			//			Reset(pOperand);
			return AIGOR_SUCCEED;
		}

		Vec3 mypos = pOperand->GetPos();
		Vec3 myangles = pOperand->GetMoveDir();

		if ((m_fEndAngle == 0) && (m_fStartAngle == 0))
		{
			CAIObject *pOrient;	// the guy who will provide our orientation
			if (pOperand->GetAttentionTarget() && !m_bUseLastOp)
				pOrient = (CAIObject*)pOperand->GetAttentionTarget();
			else if (pOperand->m_pLastOpResult)
				pOrient = pOperand->m_pLastOpResult;
			else
			{
				Reset(pOperand);
				return AIGOR_FAILED;	// sorry no target and no last operation target
			}

			/*m_looseAttentionId =*/ pOperand->SetLooseAttentionTarget(pOrient);
			if(m_bContinuous) // will keep the look at target after this goalop ends
			{
				Reset(pOperand);// Luc 1-aug-07: this reset was missing and it broke non continuous lookat with looping goalpipes (never finishing in the second loop)
				return AIGOR_SUCCEED;
			} // else, wait for orienting like the loose att target
		}
		else
		{
			// lets place it at a random spot around the operand
			myangles.z = 0;

			float fi = (((float)(ai_rand() & 255) / 255.0f ) * (float)fabs(m_fEndAngle - m_fStartAngle));
			fi+=m_fStartAngle;

			Matrix33 m = Matrix33::CreateRotationXYZ( DEG2RAD(Ang3(0,0,fi)) ); 
			myangles = m * (Vec3)myangles;
			myangles*=20.f;
			Vec3 pos = mypos+(Vec3)myangles;
			pOperand->m_vDEBUG_VECTOR = pos;
			/*m_looseAttentionId =*/ pOperand->SetLooseAttentionTarget(pos);//use operand LookAtTarget
		}

	}
	else	// keep on looking, orient like the loose att target
	{
		if (m_bContinuous)
			return AIGOR_SUCCEED;
		Vec3 mypos = pOperand->GetPos();
		Vec3 myDir = m_bUseBodyDir ? pOperand->GetBodyDir() : pOperand->GetViewDir();
		Vec3 otherpos = pOperand->GetLooseAttentionPos();
		if(otherpos.IsZero(1.f) && pOperand->GetAttentionTarget())
			otherpos = pOperand->GetAttentionTarget()->GetPos();

		Vec3 otherDir = otherpos - mypos;
		if(m_bUseBodyDir && !pOperand->GetMovementAbility().b3DMove)
		{
			myDir.z =0;
			myDir.NormalizeSafe();
			otherDir.z = 0;
		}
		otherDir.Normalize();

		float f = myDir.Dot(otherDir);
		if ((f > 0.98f && !m_bContinuous) )
		{
			Reset(pOperand);
			return AIGOR_SUCCEED;
		}
	}
	return AIGOR_IN_PROGRESS;
}

//
//----------------------------------------------------------------------------------------------------------
void COPLookAt::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{

	ser.BeginGroup("COPLookAt");
	{
		ser.Value("m_fStartAngle",m_fStartAngle);
		ser.Value("m_fEndAngle",m_fEndAngle);
		ser.Value("m_fLastDot",m_fLastDot);
		ser.Value("m_bUseLastOp",m_bUseLastOp);
		ser.Value("m_bContinuous",m_bContinuous);
		ser.Value("m_bUseBodyDir",m_bUseBodyDir);
		//ser.Value("m_looseAttentionId",m_looseAttentionId);
		ser.Value("m_bInitialized",m_bInitialized);

		/*		if(ser.IsReading() && m_pDummyAttTarget)
		{
		GetAISystem()->RemoveDummyObject(m_pDummyAttTarget);
		m_pDummyAttTarget = NULL;
		}
		objectTracker.SerializeObjectPointer(ser, "m_pDummyAttTarget", m_pDummyAttTarget, false);
		*/
		ser.EndGroup();
	}

}

//
//----------------------------------------------------------------------------------------------------------
COPLookAround::COPLookAround(float lookAtRange, float scanIntervalRange, float intervalMin, float intervalMax, bool breakOnLiveTarget, bool bUseLastOp) :
//m_pDummyAttTarget(0),
m_fLookAroundRange(lookAtRange),
m_fTimeOut(0.0f),
m_fScanTimeOut(0.0f),
m_fScanIntervalRange(scanIntervalRange),
m_fIntervalMin(intervalMin),
m_fIntervalMax(intervalMax),
m_breakOnLiveTarget(breakOnLiveTarget),
m_useLastOp(bUseLastOp),
m_lookAngle(0.0f),
m_lookZOffset(0.0f),
m_lastLookAngle(0.0f),
m_lastLookZOffset(0.0f),
m_bInitialized(false),
m_looseAttentionId(0),
m_initialDir(1,0,0)
{
	if(m_fIntervalMax < m_fIntervalMin) m_fIntervalMax = m_fIntervalMin;
	m_fTimeOut = m_fIntervalMin + (m_fIntervalMax - m_fIntervalMin) * ai_frand();
	m_fScanTimeOut = m_fScanIntervalRange + (0.2f + ai_frand() * 0.8f);
}

COPLookAround::~COPLookAround()
{
}

void COPLookAround::UpdateLookAtTarget(CPipeUser *pOperand)
{
	m_lastLookAngle = m_lookAngle;
	m_lastLookZOffset = m_lookZOffset;

	const Vec3& opPos = pOperand->GetPos();
	Vec3	dir;

	int limitRays = 4;
	do
	{
		// Random angle withing range.
		float	range(gf_PI);
		if(m_fLookAroundRange > 0)
			range = DEG2RAD(m_fLookAroundRange);
		range = clamp(range, 0.0f, gf_PI);
		m_lookAngle = (1.0f - ai_frand() * 2.0f) * range;
		m_lookZOffset = (1.1f * ai_frand() - 1.0f) * 3.0f;	// Look more down then up

		dir = GetLookAtDir(pOperand, m_lookAngle, m_lookZOffset);
	}
	while (limitRays-- && !GetAISystem()->CheckPointsVisibility(opPos, opPos + dir, 5.0f));
}

Vec3 COPLookAround::GetLookAtDir(CPipeUser *pOperand, float angle, float dz) const
{
	Vec3 forward = m_initialDir;
	if(!pOperand->GetState()->vMoveDir.IsZero())
		forward = pOperand->GetState()->vMoveDir;
	if(m_useLastOp && pOperand->m_pLastOpResult)
		forward = (pOperand->m_pLastOpResult->GetPos() - pOperand->GetPos()).GetNormalizedSafe();
	forward.z = 0.0f;
	forward.NormalizeSafe();
	Vec3 right(forward.y, -forward.x, 0.0f);

	const float lookAtDist = 20.0f;
	float	dx = (float)cry_sinf(angle) * lookAtDist;
	float	dy = (float)cry_cosf(angle) * lookAtDist;

	return right * dx + forward * dy + Vec3(0,0,dz);
}

EGoalOpResult COPLookAround::Execute(CPipeUser *pOperand)
{
	if (!m_bInitialized)
	{
		m_initialDir = pOperand->GetBodyDir();
		m_bInitialized = true;
	}

	SAIWeaponInfo	weaponInfo;
	pOperand->GetProxy()->QueryWeaponInfo(weaponInfo);
	if(weaponInfo.isTrainMounted)
		m_initialDir=weaponInfo.trainMountedDir;

	//DebugDraw(pOperand, gEnv->pRenderer);

	if (!m_looseAttentionId)
	{
		m_fLastDot = 10.0f;

		UpdateLookAtTarget(pOperand);
		Vec3	dir = GetLookAtDir(pOperand, m_lookAngle, m_lookZOffset);
		Vec3 pos(pOperand->GetPos() + dir);
		m_looseAttentionId = pOperand->SetLooseAttentionTarget(pos);
		m_startTime = m_scanStartTime = GetAISystem()->GetFrameStartTime();
	}
	else
	{
		int id = pOperand->GetLooseAttentionId();
		if (id && id != m_looseAttentionId)
		{
			//something else is using the operand loose target; aborting
			Reset(pOperand);
			return AIGOR_FAILED;
		}

		if (m_breakOnLiveTarget && pOperand->GetAttentionTarget() && pOperand->GetAttentionTarget()->IsAgent())
		{
			pOperand->SetLooseAttentionTarget(0,m_looseAttentionId);
			m_looseAttentionId = 0;
			Reset(pOperand);
			return AIGOR_SUCCEED;
		}

		ExecuteDry(pOperand);

		if (m_fTimeOut < 0.0f)
		{
			// If no time out is specified, we bail out once the target is reached.
			const Vec3& opPos = pOperand->GetPos();
			const Vec3& opViewDir = pOperand->GetViewDir();
			Vec3 dirToTarget = pOperand->GetLooseAttentionPos() - opPos;
			dirToTarget.NormalizeSafe();

			float f = opViewDir.Dot(dirToTarget);
			if (f > 0.99f || fabsf(f - m_fLastDot) < 0.001f)
			{
				Reset(pOperand);
				return AIGOR_SUCCEED;
			}
			else
				m_fLastDot = f;
		}
		else
		{
			// If time out is specified, keep looking around until the time out finishes.
			float	elapsed = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds();
			if (elapsed > m_fTimeOut)
			{
				Reset(pOperand);
				return AIGOR_SUCCEED;
			}
		}
	}

	return AIGOR_IN_PROGRESS;
}

void COPLookAround::ExecuteDry(CPipeUser *pOperand)
{
	if (!m_bInitialized)
		return;

	float	scanElapsed = (GetAISystem()->GetFrameStartTime() - m_scanStartTime).GetSeconds();

	if (pOperand->GetAttentionTarget())
	{
		// Interpolate the initial dir towards the current attention target slowly.
		Vec3	reqDir = pOperand->GetAttentionTarget()->GetPos() - pOperand->GetPos();
		reqDir.Normalize();

		const float maxRatePerSec = DEG2RAD(25.0f);
		const float maxRate = maxRatePerSec * GetAISystem()->GetFrameDeltaTime();
		const float thr = cosf(maxRate);

		float cosAngle = m_initialDir.Dot(reqDir);
		if (cosAngle > thr)
		{
			m_initialDir = reqDir;
		}
		else
		{
			float angle = acos_tpl(cosAngle);
			float t = 0.0f;
			if(angle != 0.0f)
				t = maxRate / angle;
			Quat	curView;
			curView.SetRotationVDir(m_initialDir);
			Quat	reqView;
			reqView.SetRotationVDir(reqDir);
			Quat	view;
			view.SetSlerp(curView, reqView, t);
			m_initialDir = view * FORWARD_DIRECTION;
		}
	}

	// Smooth transition from last value to new value if scanning, otherwise just as fast as possible.
	float	t = 1.0f;
	if (m_fScanTimeOut > 0.0f)
		t = (1.0f - cosf(clamp(scanElapsed / m_fScanTimeOut, 0.0f, 1.0f) * gf_PI)) / 2.0f;
	Vec3 dir = GetLookAtDir(pOperand, m_lastLookAngle + (m_lookAngle - m_lastLookAngle) * t, m_lastLookZOffset + (m_lookZOffset - m_lastLookZOffset) * t);
	Vec3 pos = pOperand->GetPos() + dir;

	m_looseAttentionId = pOperand->SetLooseAttentionTarget(pos);

	// Once one sweep is finished, start another.
	if(scanElapsed > m_fScanTimeOut)
	{
		UpdateLookAtTarget(pOperand);
		m_scanStartTime = GetAISystem()->GetFrameStartTime();
		m_fScanTimeOut = m_fScanIntervalRange + (0.2f + ai_frand() * 0.8f);
	}
}

void COPLookAround::Reset(CPipeUser *pOperand)
{
	m_bInitialized = false;
	if(pOperand)
	{
		pOperand->SetLooseAttentionTarget(0,m_looseAttentionId);
		m_looseAttentionId = 0;
	}

	m_fTimeOut = m_fIntervalMin + (m_fIntervalMax - m_fIntervalMin) * ai_frand();
	m_fScanTimeOut = m_fScanIntervalRange + (0.2f + ai_frand() * 0.8f);

	m_lookAngle = 0.0f;
	m_lookZOffset = 0.0f;
	m_lastLookAngle = 0.0f;
	m_lastLookZOffset = 0.0f;

	m_fLastDot = 0;
	//	m_fTimeOut = 0;
	//m_fScanTimeOut = 0;
}

//===================================================================
// DebugDraw
//===================================================================
void COPLookAround::DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const
{
	float	scanElapsed = (GetAISystem()->GetFrameStartTime() - m_scanStartTime).GetSeconds();
	float	t(1.0f);
	if(m_fScanTimeOut > 0.0f)
		t = (1.0f - cosf(clamp(scanElapsed / m_fScanTimeOut, 0.0f, 1.0f) * gf_PI)) / 2.0f;
	Vec3	dir = GetLookAtDir(pOperand, m_lastLookAngle + (m_lookAngle - m_lastLookAngle) * t, m_lastLookZOffset + (m_lookZOffset - m_lastLookZOffset) * t);

	Vec3 pos = pOperand->GetPos() - Vec3(0,0,1);

	// Initial dir
	pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,255,255), pos + m_initialDir * 3.0f, ColorB(255,255,255,128));

	// Current dir
	pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,0,0), pos + dir * 3.0f, ColorB(255,0,0,128), 3.0f);

}


void COPLookAround::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{

	ser.BeginGroup("COPLookAround");
	{
		ser.Value("m_fLastDot",m_fLastDot);
		ser.Value("m_fLookAroundRange",m_fLookAroundRange);
		ser.Value("m_fIntervalMin",m_fIntervalMin);
		ser.Value("m_fIntervalMax",m_fIntervalMax);
		ser.Value("m_fTimeOut",m_fTimeOut);
		ser.Value("m_fScanIntervalRange",m_fScanIntervalRange);
		ser.Value("m_fScanTimeOut",m_fScanTimeOut);
		ser.Value("m_startTime",m_startTime);
		ser.Value("m_scanStartTime",m_scanStartTime);
		ser.Value("m_breakOnLiveTarget",m_breakOnLiveTarget);
		ser.Value("m_useLastOp",m_useLastOp);
		ser.Value("m_lookAngle",m_lookAngle);
		ser.Value("m_lookZOffset",m_lookZOffset);
		ser.Value("m_lastLookAngle",m_lastLookAngle);
		ser.Value("m_lastLookZOffset",m_lastLookZOffset);
		//objectTracker.SerializeObjectPointer(ser, "m_pDummyAttTarget", m_pDummyAttTarget, false);
		ser.Value("m_looseAttentionId",m_looseAttentionId);

		ser.EndGroup();
	}

}
//
//-------------------------------------------------------------------------------------------------------------------------------
COPPathFind::COPPathFind(const char *szName, IAIObject *pTarget, float fEndTol, float fEndDistance, bool bKeepMoving, float fDirectionOnlyDistance):
m_pTarget(pTarget),
m_bWaitingForResult(false),
m_sObjectName(szName),
m_bKeepMoving(bKeepMoving),
m_fDirectionOnlyDistance(fDirectionOnlyDistance),
m_nForceTargetBuildingID(-1),
m_fEndTol(fEndTol),
m_fEndDistance(fEndDistance),
m_TargetPos(ZERO),
m_TargetOffset(ZERO)
{ 
}

//
//-------------------------------------------------------------------------------------------------------------------------------
void COPPathFind::OnObjectRemoved(CAIObject* pObject)
{
	if (m_pTarget == pObject)
		m_pTarget = 0;
}


//
//-------------------------------------------------------------------------------------------------------------------------------
COPPathFind::~COPPathFind() 
{ 
}

//
//-------------------------------------------------------------------------------------------------------------------------------
void COPPathFind::Reset(CPipeUser *pOperand)
{
	m_bWaitingForResult = false;
	m_nForceTargetBuildingID = -1;
	//m_pTarget = NULL; 
}

//
//-------------------------------------------------------------------------------------------------------------------------------
EGoalOpResult COPPathFind::Execute(CPipeUser *pOperand)
{
	if (!m_bWaitingForResult)
	{
		if (m_sObjectName.size()>0)
		{
			IAIObject* pSpecialTarget = pOperand->GetSpecialAIObject(m_sObjectName);
			if(pSpecialTarget)
			{
				// else just issue request to pathfind to target
				m_TargetPos = pSpecialTarget->GetPos();
				m_TargetPos += m_TargetOffset;
				Vec3 opdir = pSpecialTarget->GetMoveDir();
				if (m_fDirectionOnlyDistance > 0.0f)
				{
					pOperand->RequestPathInDirection(m_TargetPos, m_fDirectionOnlyDistance, 0, m_fEndDistance);
				}
				else
				{
					pOperand->RequestPathTo(m_TargetPos, opdir, (static_cast<CAIObject*>(pSpecialTarget))->GetSubType()!=CAIObject::STP_FORMATION, 
						m_nForceTargetBuildingID, m_fEndTol, m_fEndDistance, static_cast<CAIObject*>(pSpecialTarget));
				}
				m_bWaitingForResult = true;
			}
		}
	}
	if (!m_bWaitingForResult)
	{
		if (!m_pTarget)
		{
			/*			if (m_sPathName.size()>0)
			{
			if (GetAISystem()->GetDesignerPath(m_sPathName.c_str(),pOperand->m_lstPath))
			return true; // it was, so just store it in the operand
			}
			*/
			// it target not specified, use last op result
			if  (!pOperand->m_pLastOpResult)
			{
				pOperand->SetSignal(0,"OnNoPathFound" ,0, 0, g_crcSignals.m_nOnNoPathFound);
				pOperand->m_nPathDecision = PATHFINDER_NOPATH;
				Reset(pOperand);
				return AIGOR_FAILED;	// no last op result, return...
			}
			//m_pTarget = pOperand->m_pLastOpResult;
			m_TargetPos = pOperand->m_pLastOpResult->GetPos();
			m_TargetPos += m_TargetOffset;
			Vec3 opdir = pOperand->m_pLastOpResult->GetMoveDir();

			if (m_fDirectionOnlyDistance > 0.0f)
			{
				pOperand->RequestPathInDirection(m_TargetPos, m_fDirectionOnlyDistance, 0, m_fEndDistance);
			}
			else
			{
				pOperand->RequestPathTo(m_TargetPos, opdir, pOperand->m_pLastOpResult->GetSubType()!=CAIObject::STP_FORMATION, 
					m_nForceTargetBuildingID, m_fEndTol, m_fEndDistance, pOperand->m_pLastOpResult);
			}
			m_bWaitingForResult = true;
		}
		else
		{
			// else just issue request to pathfind to target
			m_TargetPos = m_pTarget->GetPos();
			m_TargetPos += m_TargetOffset;
			Vec3 opdir = m_pTarget->GetMoveDir();

			if (m_fDirectionOnlyDistance > 0.0f)
			{
				pOperand->RequestPathInDirection(m_TargetPos, m_fDirectionOnlyDistance, 0, m_fEndDistance);
			}
			else
			{
				pOperand->RequestPathTo(m_TargetPos, opdir, (static_cast<CAIObject*>(m_pTarget))->GetSubType()!=CAIObject::STP_FORMATION, 
					m_nForceTargetBuildingID, m_fEndTol, m_fEndDistance, static_cast<CAIObject*>(m_pTarget));
			}
			m_bWaitingForResult = true;
		}
	}

	if (m_bWaitingForResult)
	{
		if (pOperand->m_nPathDecision)
		{
			m_bWaitingForResult = false;
			if (pOperand->m_nPathDecision == PATHFINDER_NOPATH)// || !pOperand->m_Path.GetPathEndIsAsRequested())
			{
				pOperand->SetSignal(0,"OnNoPathFound" ,0, 0, g_crcSignals.m_nOnNoPathFound);

				Reset(pOperand);
				return AIGOR_FAILED;
			}
			else
			{
				if (pOperand->m_Path.GetPath().empty())
					pOperand->SetSignal(0,"OnNoPathFound");
				// if the path is found, but the end is far from what's requested, then
				// approach/stick will send an OnEndPathOffset signal if necessary

				Reset(pOperand);
				return AIGOR_SUCCEED;
			}
		}
	}
	return AIGOR_IN_PROGRESS;
}



void COPPathFind::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{

	ser.BeginGroup("COPPathFind");
	{
		ser.Value("m_sObjectName",m_sObjectName);
		ser.Value("m_bWaitingForResult",m_bWaitingForResult);
		ser.Value("m_bKeepMoving",m_bKeepMoving);
		ser.Value("m_TargetPos",m_TargetPos);
		ser.Value("m_fDirectionOnlyDistance",m_fDirectionOnlyDistance);
		ser.Value("m_nForceTargetBuildingID",m_nForceTargetBuildingID);
		objectTracker.SerializeObjectPointer(ser, "m_pTarget", m_pTarget, false);
		ser.Value("m_fEndTol",m_fEndTol);
		ser.Value("m_TargetOffset",m_TargetOffset);
		ser.EndGroup();
	}
}


EGoalOpResult COPLocate::Execute(CPipeUser *pOperand)
{
	if (m_sName.empty())
	{
		pOperand->SetLastOpResult((CAIObject*)GetAISystem()->GetNearestObjectOfTypeInRange(pOperand->GetPos(), m_nObjectType, 0, m_fRange ? m_fRange : 50));
		if (pOperand->GetLastOpResult())
			return AIGOR_SUCCEED;
		else
			return AIGOR_FAILED;
	}

	IAIObject* pObject = pOperand->GetSpecialAIObject(m_sName, m_fRange);

	// don't check the range if m_fRange doesn't really represent a range :-/
	if (m_sName != "formation_id")
	{
		// check with m_fRange optional parametar
		if (m_fRange && pObject && (pOperand->GetPos() - pObject->GetPos()).GetLengthSquared() > m_fRange*m_fRange )
			pObject = NULL;
	}

	// always set lastOpResult even if pObject is NULL!!!
	pOperand->SetLastOpResult(static_cast< CAIObject* >( pObject ));

	if (pObject)
		return AIGOR_SUCCEED;
	else
		return AIGOR_FAILED;
}


//===================================================================
// COPTrace
//===================================================================
COPTrace::COPTrace( bool bExactFollow, float fEndAccuracy, bool bForceReturnPartialPath, bool bStopOnAnimationStart ) : 
m_bDisabledPendingFullUpdate(false),
m_bExactFollow(bExactFollow),
//	m_fEndDistance(fEndDistance),
m_fEndAccuracy(fEndAccuracy),
//  m_fDuration(fDuration),
m_bForceReturnPartialPath(bForceReturnPartialPath),
m_stopOnAnimationStart(bStopOnAnimationStart),
m_Maneuver(eMV_None),
m_ManeuverDir(eMVD_Clockwise),
m_ManeuverDist(0),
m_ManeuverTime(0.0f),
m_landHeight(0.0f),
m_landingDir(ZERO),
m_landingPos(ZERO),
m_workingLandingHeightOffset(0.0f), 
m_TimeStep(0.1f),
m_lastTime(-1.0f),
m_startTime(-1.0f),
m_pNavTarget(0),
m_fTotalTracingTime(0),
m_lastPosition(ZERO),
m_fTravelDist(0.0f),
m_pOperand(0),
m_inhibitPathRegen(false),
m_looseAttentionId(0),
m_waitingForPathResult(false), 
m_actorTargetRequester(eTATR_None), 
m_pendingActorTargetRequester(eTATR_None),
m_earlyPathRegen(false), 
m_passingStraightNavSO(false),
m_bSteerAroundPathTarget(true)
{
	if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
		AILogAlways("COPTrace::COPTrace %p", this);
}


//===================================================================
// ~COPTrace
//===================================================================
COPTrace::~COPTrace()
{
	if (m_pOperand)
	{
		GetAISystem()->CancelAnyPathsFor(m_pOperand);
		m_pOperand->SetLooseAttentionTarget(0,m_looseAttentionId);
		m_looseAttentionId = 0;
		m_pOperand->m_State.fDesiredSpeed = 0.0f;

		m_pOperand->ClearPath("COPTrace::~COPTrace m_Path");
	}
	if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
		AILogAlways("COPTrace::~COPTrace %p %s", this, m_pOperand ? m_pOperand->GetName() : "");
	GetAISystem()->RemoveDummyObject(m_pNavTarget);
	m_pNavTarget = 0;
}


//===================================================================
// Reset
//===================================================================
void COPTrace::Reset(CPipeUser *pOperand)
{
	if (pOperand && GetAISystem()->m_cvDebugPathFinding->GetIVal())
		AILogAlways("COPTrace::Reset %s", pOperand->GetName());

	if (pOperand)
	{
		pOperand->SetLooseAttentionTarget(0,m_looseAttentionId);
		m_looseAttentionId = 0;
		// If we have regenerated the path as part of the periodic regeneration
		// then the request must be cancelled. However, don't always cancel requests
		// because if stick is trying to regenerate the path and we're
		// resetting because we reached the end of the current one, then we'll stop
		// stick from getting its pathfind request.
		// Generally the m_Path.Empty condition decides between these cases, but
		// Danny todo: not sure if it will always work.
		if (!pOperand->m_Path.Empty())
			GetAISystem()->CancelAnyPathsFor(pOperand);
		pOperand->m_State.fDesiredSpeed = 0.0f;
		pOperand->ClearPath("COPTrace::Reset m_Path");
	}

	m_fTotalTracingTime = 0.0f;
	m_Maneuver = eMV_None;
	m_ManeuverDist = 0;
	m_ManeuverTime = GetAISystem()->GetFrameStartTime();
	m_landHeight = 0.0f;
	m_landingDir.zero();
	m_workingLandingHeightOffset = 0.0f;
	m_inhibitPathRegen = false;
	m_bDisabledPendingFullUpdate = false;
	m_actorTargetRequester = eTATR_None;
	m_pendingActorTargetRequester = eTATR_None;
	m_waitingForPathResult = false;
	m_earlyPathRegen = false;
	m_passingStraightNavSO = false;
	m_fTravelDist = 0;

	m_TimeStep =  0.1f;
	m_lastTime = -1.0f;
	if (m_bExactFollow && pOperand)
		pOperand->m_bLooseAttention = false;

	pOperand->ClearInvalidatedSOLinks();
}

//===================================================================
// Serialize
//===================================================================
void COPTrace::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.Value("m_bDisabledPendingFullUpdate",m_bDisabledPendingFullUpdate);
	ser.Value("m_ManeuverDist",m_ManeuverDist);
	ser.Value("m_ManeuverTime",m_ManeuverTime);
	ser.Value("m_landHeight",m_landHeight);
	ser.Value("m_workingLandingHeightOffset",m_workingLandingHeightOffset);
	ser.Value("m_landingPos",m_landingPos);
	ser.Value("m_landingDir",m_landingDir);

	ser.Value("m_bExactFollow",m_bExactFollow);
	ser.Value("m_bForceReturnPartialPath",m_bForceReturnPartialPath);
	ser.Value("m_lastPosition",m_lastPosition);
	ser.Value("m_lastTime",m_lastTime);
	ser.Value("m_startTime",m_startTime);
	ser.Value("m_fTravelDist",m_fTravelDist);
	ser.Value("m_TimeStep",m_TimeStep);
	objectTracker.SerializeObjectPointer(ser,"m_pOperand",m_pOperand,false);
	ser.EnumValue("m_Maneuver",m_Maneuver,eMV_None,  eMV_Fwd);
	ser.EnumValue("m_ManeuverDir",m_ManeuverDir,eMVD_Clockwise, eMVD_AntiClockwise);

	ser.Value("m_fTotalTracingTime",m_fTotalTracingTime);
	ser.Value("m_inhibitPathRegen",m_inhibitPathRegen);
	ser.Value("m_fEndAccuracy",m_fEndAccuracy);
	ser.Value("m_looseAttentionId",m_looseAttentionId);

	ser.Value("m_waitingForPathResult", m_waitingForPathResult);
	ser.Value("m_earlyPathRegen", m_earlyPathRegen);
	ser.Value("m_bSteerAroundPathTarget",m_bSteerAroundPathTarget);
	ser.Value("m_passingStraightNavSO", m_passingStraightNavSO);
	ser.EnumValue("m_actorTargetRequester", m_actorTargetRequester, eTATR_None, eTATR_EndOfPath);
	ser.EnumValue("m_pendingActorTargetRequester", m_pendingActorTargetRequester, eTATR_None, eTATR_EndOfPath);
	ser.Value("m_stopOnAnimationStart", m_stopOnAnimationStart);

	if (m_pNavTarget && ser.IsWriting())
	{
		if (false == GetAISystem()->IsAIObjectRegistered(m_pNavTarget))
		{
			AIWarning("Unable to find m_pNavTarget during serialisation - setting to 0");
			m_pNavTarget = 0;
		}
	}
	objectTracker.SerializeObjectPointer(ser, "pNavTarget", m_pNavTarget, false);
}


//===================================================================
// Execute
//===================================================================
EGoalOpResult COPTrace::Execute(CPipeUser *pOperand)
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_AI );

	bool done = ExecuteTrace( pOperand, true );

	// if waiting for a path (especially with continuous stick) continue to
	// try to follow any path that we have.
	if (pOperand->m_nPathDecision == PATHFINDER_STILLFINDING)
	{
		if (done)
			pOperand->m_State.fDesiredSpeed = 0.0f;
		return AIGOR_IN_PROGRESS;
	}
	else
	{
		// Done tracing, allow to try to use invalid objects again.
		if (done)
		{
			pOperand->ClearInvalidatedSOLinks();
			if ( pOperand->m_State.curActorTargetPhase == eATP_Error )
				return AIGOR_FAILED;
		}
		return done ? AIGOR_SUCCEED : AIGOR_IN_PROGRESS;
	}
}

//===================================================================
// ExecuteTrace
// this is the same old COPTrace::Execute method
// but now, smart objects should control when it
// should be executed and how its return value
// will be interpreted
//===================================================================
bool COPTrace::ExecuteTrace(CPipeUser *pOperand, bool fullUpdate)
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_AI );
	m_pOperand = pOperand;

	// HACK: Special case fix for tranquilized drivers in Crysis.
	if (m_pOperand->GetType() == AIOBJECT_VEHICLE)
	{
		// Check if the vehicle driver is tranq'd and do not move if it is.
		CPipeUser* pDriverAI = 0;
		if (EntityId driverId = m_pOperand->GetProxy()->GetLinkedDriverEntityId())
			if (IEntity* pDriverEntity = gEnv->pEntitySystem->GetEntity(driverId))
				pDriverAI = CastToCPipeUserSafe(pDriverEntity->GetAI());

		if (pDriverAI && pDriverAI->GetProxy())
		{
			if (pDriverAI->GetProxy()->GetActorIsFallen())
			{
				// The driver is unable to drive, do not drive.
				pOperand->m_State.vMoveDir.Set(0,0,0);
				pOperand->m_State.fDesiredSpeed = 0.0f;
				pOperand->m_State.predictedCharacterStates.nStates = 0;
				return false; // Trace not finished.
			}
		}
	}

	// Wait for pathfinder to return a result.
	if(m_waitingForPathResult)
	{
		if(pOperand->m_nPathDecision == PATHFINDER_PATHFOUND)
		{
			// Path found, continue.
			m_waitingForPathResult = false;
		}
		else if(pOperand->m_nPathDecision == PATHFINDER_NOPATH)
		{
			// Could not find path, fail.
			m_waitingForPathResult = false;
			m_bDisabledPendingFullUpdate = true;
			return true;
		}
		else
		{
			// Wait for the path finder result, disable movement and wait.
			pOperand->m_State.vMoveDir.Set(0,0,0);
			pOperand->m_State.fDesiredSpeed = 0.0f;
			pOperand->m_State.predictedCharacterStates.nStates = 0;
			return false;
		}
	}

	// Wait until in full update, and terminate the goalop.
	if (m_bDisabledPendingFullUpdate)
	{
		if (fullUpdate)
		{
			m_bDisabledPendingFullUpdate = false;
			Reset(pOperand);
			return true;
		}
		else
		{
			pOperand->m_State.vMoveDir.Set(0,0,0);
			pOperand->m_State.fDesiredSpeed = 0.0f;
			pOperand->m_State.predictedCharacterStates.nStates = 0;
			return true; // gets ignored
		}
	}

	bool	isUsing3DNavigation = pOperand->IsUsing3DNavigation();

	// Handle exact positioning and vaSOs.
	bool	forceRegenPath = false;
	bool	exactPositioning = false;

	// Handle exact positioning.
	if(pOperand->m_State.curActorTargetPhase == eATP_Error)
	{
		if(m_actorTargetRequester == eTATR_None)
		{
			m_actorTargetRequester = m_pendingActorTargetRequester;
			m_pendingActorTargetRequester = eTATR_None;
		}
		if(m_actorTargetRequester == eTATR_EndOfPath)
		{
			if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
				AILogAlways("COPTrace::ExecuteTrace resetting since error occurred during exact positioning %s", 
				pOperand ? pOperand->GetName() : "");

			if(fullUpdate)
			{
				Reset(pOperand);
				return true;
			}
			else
			{
				// TODO: Handle better the error case!
				pOperand->m_State.vMoveDir.Set(0,0,0);
				pOperand->m_State.fDesiredSpeed = 0.0f;
				pOperand->m_State.predictedCharacterStates.nStates = 0;
				m_bDisabledPendingFullUpdate = true;
				return true;
			}
		}
		else if(m_actorTargetRequester == eTATR_NavSO)
		{
			// Exact positioning has been failed at navigation smart object, regenerate path.
			forceRegenPath = true;
			m_inhibitPathRegen = false;
			m_earlyPathRegen = false;
		}
		m_actorTargetRequester = eTATR_None;
		m_pendingActorTargetRequester = eTATR_None;
	}
	else if(pOperand->m_State.curActorTargetPhase == eATP_Playing)
	{
		// While playing, keep the trace & prediction running.
		exactPositioning = true;
		// Do not update the trace while waiting for the animation to finish.
		if ( !pOperand->m_navSOEarlyPathRegen )
			return false;
	}
	else if(pOperand->m_State.curActorTargetPhase == eATP_Starting || pOperand->m_State.curActorTargetPhase == eATP_Started)
	{
		exactPositioning = true;

		// Regenerate path while playing the animation.
		if (m_pendingActorTargetRequester != eTATR_None)
		{
			m_actorTargetRequester = m_pendingActorTargetRequester;
			m_pendingActorTargetRequester = eTATR_None;
		}
		//		AIAssert(!m_earlyPathRegen);
		if(pOperand->m_State.curActorTargetPhase == eATP_Started && m_actorTargetRequester == eTATR_NavSO)
		{
			if(pOperand->m_navSOEarlyPathRegen && !pOperand->m_State.curActorTargetFinishPos.IsZero())
			{
				// Path find from the predicted animation end position.
				pOperand->m_forcePathGenerationFrom = pOperand->m_State.curActorTargetFinishPos;
				forceRegenPath = true;
				m_earlyPathRegen = true;
				// Do not allow to regenerate the path until the current animation has finished.
				m_inhibitPathRegen = true;

				// Update distance traveled during the exactpos anim.
				Vec3 futurePos = pOperand->m_forcePathGenerationFrom;
				m_fTravelDist += !isUsing3DNavigation ? Distance::Point_Point2D(futurePos, m_lastPosition) : Distance::Point_Point(futurePos, m_lastPosition);
				m_lastPosition = futurePos;
			}
		}

		if (m_stopOnAnimationStart && m_actorTargetRequester == eTATR_EndOfPath)
		{
			if (fullUpdate)
			{
				Reset(pOperand);
				return true;
			}
			else
			{
				pOperand->m_State.vMoveDir.Set(0,0,0);
				pOperand->m_State.fDesiredSpeed = 0.0f;
				pOperand->m_State.predictedCharacterStates.nStates = 0;
				m_bDisabledPendingFullUpdate = true;
				return true;
			}
		}
	}
	else if(pOperand->m_State.curActorTargetPhase == eATP_Finished || pOperand->m_State.curActorTargetPhase == eATP_StartedAndFinished)
	{
		if(m_actorTargetRequester == eTATR_EndOfPath)
		{
			m_actorTargetRequester = eTATR_None;
			// Exact positioning has been finished at the end of the path, the trace is completed.
			if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
				AILogAlways("COPTrace::ExecuteTrace resetting since exact position reached/animation finished %s", 
				pOperand ? pOperand->GetName() : "");

			if (fullUpdate)
			{
				Reset(pOperand);
				return true;
			}
			else
			{
				pOperand->m_State.vMoveDir.Set(0,0,0);
				pOperand->m_State.fDesiredSpeed = 0.0f;
				pOperand->m_State.predictedCharacterStates.nStates = 0;
				m_bDisabledPendingFullUpdate = true;
				return true;
			}
		}
		else if(m_actorTargetRequester == eTATR_NavSO)
		{
			// Exact positioning has been finished at navigation smart object, regenerate path.
			if(!m_earlyPathRegen || pOperand->m_State.curActorTargetPhase == eATP_StartedAndFinished)
			{
				forceRegenPath = true;
				m_actorTargetRequester = eTATR_None;

				// Update distance traveled during the exactpos anim.
				Vec3 opPos = pOperand->GetPhysicsPos();
				m_fTravelDist += !isUsing3DNavigation ? Distance::Point_Point2D(opPos, m_lastPosition) : Distance::Point_Point(opPos, m_lastPosition);
				m_lastPosition = opPos;
			}

			m_waitingForPathResult = true;
			m_inhibitPathRegen = false;
			m_earlyPathRegen = false;
		}
		else
		{
			// A pending exact positioning request, maybe from previously interrupted trace.
			// Regenerate path, since the current path may be bogus because it was generated
			// before the animation had finished.
			forceRegenPath = true;
			m_actorTargetRequester = eTATR_None;
			m_waitingForPathResult = true;
			m_inhibitPathRegen = false;
			m_earlyPathRegen = false;
		}
	}
	else if(pOperand->m_State.curActorTargetPhase == eATP_Waiting)
	{
		/*		if(m_pendingActorTargetRequester == eTATR_NavSO)
		{
		// Check if the navSO is still valid.
		PathPointDescriptor::SmartObjectNavDataPtr pSmartObjectNavData = pOperand->m_Path.GetLastPathPointAnimNavSOData();
		// Navigational SO at the end of the current path.
		const GraphNode* pFromNode = pSmartObjectNavData->pFrom;
		const GraphNode* pToNode = pSmartObjectNavData->pTo;

		if(GetAISystem()->GetNavigationalSmartObjectActionType(pOperand, pFromNode, pToNode) == nSOmNone)
		{
		// The navSO state has changed, regenerate path.
		pOperand->SetNavSOFailureStates();
		const SSmartObjectNavData* pNavData = pSmartObjectNavData->pFrom->GetSmartObjectNavData();
		pOperand->InvalidateSOLink( pNavData->pSmartObject, pNavData->pHelper, pSmartObjectNavData->pTo->GetSmartObjectNavData()->pHelper );

		forceRegenPath = true;
		m_inhibitPathRegen = false;
		m_earlyPathRegen = false;
		m_actorTargetRequester = eTATR_None;
		m_pendingActorTargetRequester = eTATR_None;

		if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
		{
		CSmartObject* pObject = pFromNode->GetSmartObjectNavData()->pSmartObject;
		AILogAlways("COPTrace::ExecuteTrace %s regenerating path because the navSO '%s' state does not match anymore.",
		pOperand->GetName(), pObject->GetEntity()->GetName());
		}

		}
		} */
		exactPositioning = true;
	}

	if(m_lastTime>0.0f)
	{
		m_TimeStep = (GetAISystem()->GetFrameStartTime() - m_lastTime).GetSeconds();
		if(m_TimeStep<=0)
			m_TimeStep = 0.0f;
	}
	else
	{
		// Reset the action input before starting to move.
		// Calling SetAGInput instead of ResetAGInput since we want to reset
		// the input even if some other system has set it to non-idle
		pOperand->GetProxy()->SetAGInput( AIAG_ACTION, "idle" );

		// Change the SO state to match the movement.
		IEntity* pEntity = pOperand->GetEntity();
		IEntity* pNullEntity = NULL;
		if ( GetAISystem()->SmartObjectEvent( "OnMove", pEntity, pNullEntity ) != 0 )
			return false;
	}

	if (m_startTime < 0.0f)
		m_startTime  = GetAISystem()->GetFrameStartTime();
	m_lastTime = GetAISystem()->GetFrameStartTime();
	m_fTotalTracingTime += m_TimeStep;

	float distToSmartObject = std::numeric_limits<float>::max();
	if(!exactPositioning)
	{
		if (pOperand->GetPathFollower() && GetAISystem()->m_cvPredictivePathFollowing->GetIVal())
			distToSmartObject = pOperand->GetPathFollower()->GetDistToSmartObject();
		else
			distToSmartObject = pOperand->m_Path.GetDistToSmartObject(!isUsing3DNavigation);
	}
	m_passingStraightNavSO = distToSmartObject < 1.0f;

	// interpret -ve end distance as the total distance to trace by subtracting it from the
	// original path length
	/*	if (m_fEndDistance < 0.0f && pOperand->m_nPathDecision == PATHFINDER_PATHFOUND)
	{
	float pathLen = pOperand->m_Path.GetPathLength(true); // todo danny use 2/3D and cache
	m_fEndDistance += pathLen;
	if (m_fEndDistance < 0.0f)
	m_fEndDistance = 0.0f;

	}
	// override end distance if a duration has been set
	if (m_fDuration > 0.0f)
	{
	float pathLen = pOperand->m_Path.GetPathLength(true); // todo danny use 2/3D and cache
	float timeLeft = m_fDuration - (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds();
	float normalSpeed = pOperand->GetNormalMovementSpeed(pOperand->m_State.fMovementUrgency, true);
	if (normalSpeed > 0.0f)
	{
	float distance = max(normalSpeed * timeLeft, 0.0f);
	m_fEndDistance = max(pathLen - distance, 0.0f);
	}
	}*/

	static float exactPosTriggerDistance = 5.0f; //10.0f;

	// Try to trigger exact positioning
	// Trigger the positioning when path is found (sometimes it is still in progress because of previous navSO usage),
	// and when close enough or just finished trace without following it.
	if((pOperand->m_State.fDistanceToPathEnd >= 0.0f && pOperand->m_State.fDistanceToPathEnd <= exactPosTriggerDistance) ||
		pOperand->m_Path.GetPathLength(isUsing3DNavigation) <= exactPosTriggerDistance)
	{
		if(pOperand->m_State.curActorTargetPhase == eATP_None)
		{
			// Handle the exact positioning request
			PathPointDescriptor::SmartObjectNavDataPtr pSmartObjectNavData = pOperand->m_Path.GetLastPathPointAnimNavSOData();

			if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
			{
				if(!pOperand->m_pendingNavSOStates.IsEmpty())
				{
					IEntity*	pEnt = gEnv->pEntitySystem->GetEntity(pOperand->m_pendingNavSOStates.objectEntId);
					if(pEnt)
						AILogAlways("COPTrace::ExecuteTrace %s trying to use exact positioning while a navSO (entity=%s) is still active.",
						pOperand->GetName(), pEnt->GetName());
					else
						AILogAlways("COPTrace::ExecuteTrace %s trying to use exact positioning while a navSO (entityId=%d) is still active.",
						pOperand->GetName(), pOperand->m_pendingNavSOStates.objectEntId);
				}
			}

			if(pSmartObjectNavData && pSmartObjectNavData->fromIndex && pSmartObjectNavData->toIndex)
			{
				// Navigational SO at the end of the current path.
				const GraphNode* pFromNode = GetAISystem()->GetGraph()->GetNode(pSmartObjectNavData->fromIndex);
				const GraphNode* pToNode = GetAISystem()->GetGraph()->GetNode(pSmartObjectNavData->toIndex);
				// Fill in the actor target request, and figure out the navSO method.
				if(GetAISystem()->PrepareNavigateSmartObject(pOperand, pFromNode, pToNode) && pOperand->m_eNavSOMethod != nSOmNone)
				{
					pOperand->m_State.actorTargetReq.id = ++pOperand->m_actorTargetReqId;
					pOperand->m_State.actorTargetReq.lowerPrecision = true;
					m_pendingActorTargetRequester = eTATR_NavSO;

					// In case we hit here because the path following has finished, keep the trace alive.
					exactPositioning = true;

					// Enforce to use the current path.
					m_pOperand->m_Path.GetParams().inhibitPathRegeneration = true;
					GetAISystem()->CancelAnyPathsFor(m_pOperand);

					// TODO: these are debug variables, should be perhaps initialised somewhere else.
					pOperand->m_DEBUGCanTargetPointBeReached.clear();
					pOperand->m_DEBUGUseTargetPointRequest.zero();
				}
				else
				{
					// Failed to use the navSO. Instead of resetting the goalop, set the state
					// to error, to prevent the link being reused.
					forceRegenPath = true;
					m_earlyPathRegen = false;
					pOperand->m_State.actorTargetReq.Reset();
					m_actorTargetRequester = eTATR_None;
					m_pendingActorTargetRequester = eTATR_None;

					const SSmartObjectNavData* pNavData = GetAISystem()->GetGraph()->GetNode(pSmartObjectNavData->fromIndex)->GetSmartObjectNavData();
					pOperand->InvalidateSOLink( pNavData->pSmartObject, pNavData->pHelper, GetAISystem()->GetGraph()->GetNode(pSmartObjectNavData->toIndex)->GetSmartObjectNavData()->pHelper );

				}
			}
			else if(const SAIActorTargetRequest* pReq = pOperand->GetActiveActorTargetRequest())
			{
				// Actor target requested at the end of the path.
				pOperand->m_State.actorTargetReq = *pReq;
				pOperand->m_State.actorTargetReq.id = ++pOperand->m_actorTargetReqId;
				pOperand->m_State.actorTargetReq.lowerPrecision = false;
				m_pendingActorTargetRequester = eTATR_EndOfPath;
				// In case we hit here because the path following has finished, keep the trace alive.
				exactPositioning = true;

				// Enforce to use the current path.
				m_pOperand->m_Path.GetParams().inhibitPathRegeneration = true;
				GetAISystem()->CancelAnyPathsFor(m_pOperand);

				// TODO: these are debug variables, should be perhaps initialised somewhere else.
				pOperand->m_DEBUGCanTargetPointBeReached.clear();
				pOperand->m_DEBUGUseTargetPointRequest.zero();
			}
		}
		else if(pOperand->m_State.curActorTargetPhase != eATP_Error)
		{
			// The exact positioning is in motion but not yet playing, keep the trace alive.
			exactPositioning = true;
		}
	}

	// If this path was generated with the pathfinder quietly regenerate the path
	// periodically in case something has moved.
	if (forceRegenPath || (fullUpdate && GetAISystem()->m_cvCrowdControlInPathfind->GetIVal() != 0 && 
		pOperand->m_movementAbility.pathRegenIntervalDuringTrace > 0.01f &&
		!m_pOperand->m_Path.GetParams().precalculatedPath &&
		!m_pOperand->m_Path.GetParams().inhibitPathRegeneration &&
		!m_inhibitPathRegen && !m_passingStraightNavSO))
	{
		if (forceRegenPath || m_fTotalTracingTime > pOperand->m_movementAbility.pathRegenIntervalDuringTrace)
		{
			// only regenerate if there isn't a new path that's about to come our way anyway
			if (!GetAISystem()->IsFindingPathFor(pOperand))
			{
				// Store the request params in the path
				const SNavPathParams& params = pOperand->m_Path.GetParams();
				int origPathDecision = pOperand->m_nPathDecision;
				if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
				{
					AILogAlways("COPTrace::ExecuteTrace %s regenerating path to (%5.2f, %5.2f, %5.2f) (origPathDecision = %d)",
						pOperand->GetName(), params.end.x, params.end.y, params.end.z, origPathDecision);
				}
				float endTol = m_bForceReturnPartialPath || m_fEndAccuracy < 0.0f ? std::numeric_limits<float>::max() : m_fEndAccuracy;
				float endDist = params.endDistance;
				if (endDist < 0.0f)
					endDist = min(-0.01f, endDist + m_fTravelDist);

				if (params.isDirectional)
				{
					float searchRange = Distance::Point_Point(params.end, pOperand->GetPhysicsPos());
					pOperand->RequestPathInDirection(params.end, searchRange, pOperand->m_pPathFindTarget, endDist);
				}
				else
					pOperand->RequestPathTo(params.end, params.endDir, params.allowDangerousDestination, params.nForceBuildingID, endTol, endDist, pOperand->m_pPathFindTarget);

				// Keep the travel dist per path to keep the end distance calculation correct over multiple regenerations.
				m_fTravelDist = 0.0f;

				// If not forcing the regen because of navSO, pretend that the state of the path is still the same.
				if(!forceRegenPath)
					pOperand->m_nPathDecision = origPathDecision;
			}
			m_fTotalTracingTime = 0.0f;

			// The path was forced to be regenerated when a navigational smartobject has been passed.
			if(forceRegenPath)
			{
				m_waitingForPathResult = true;
				return false;
			}
		}
	}


	bool traceResult = false;

	if (pOperand->GetPathFollower() && GetAISystem()->m_cvPredictivePathFollowing->GetIVal())
	{
		traceResult = ExecutePathFollower(pOperand, fullUpdate);
	}
	else
	{
		// If the current path segment involves 3D, then use 3D following
		if (isUsing3DNavigation)
			traceResult = Execute3D(pOperand, fullUpdate);
		else
			traceResult = Execute2D(pOperand, fullUpdate);
	}

	// If exact positioning is in motion, do not allow to finish the goal op, just yet.
	// This may happen in some cases when the exact positioning start position is just over
	// the starting point of a navSO, or during the animation playback to keep the
	// prediction warm.
	if(exactPositioning)
		traceResult = false;

	// prevent future updates unless we get an external reset
	if (traceResult && pOperand->m_nPathDecision != PATHFINDER_STILLFINDING)
	{
		if (fullUpdate)
		{
			Reset(pOperand);
			return true;
		}
		else
		{
			pOperand->m_State.vMoveDir.Set(0,0,0);
			pOperand->m_State.fDesiredSpeed = 0.0f;
			pOperand->m_State.predictedCharacterStates.nStates = 0;
			m_bDisabledPendingFullUpdate = true;
			return false;
		}
	}

	return traceResult;
}

//===================================================================
// ExecuteManeuver
//===================================================================
void COPTrace::ExecuteManeuver(CPipeUser *pOperand, const Vec3& steerDir)
{
	if (fabs(pOperand->m_State.fDesiredSpeed) < 0.001f)
	{
		m_Maneuver = eMV_None;
		return;
	}

	// Update the debug movement reason.
	pOperand->m_DEBUGmovementReason = CPipeUser::AIMORE_MANEUVER;

	// first work out which direction to rotate (normally const over the whole
	// maneuver). Subsequently work out fwd/backwards. Then steer based on
	// the combination of the two

	float	cosTrh = pOperand->m_movementAbility.maneuverTrh;
	if(pOperand->m_movementAbility.maneuverTrh >= 1.f || pOperand->m_IsSteering)
		return;

	Vec3 myDir = pOperand->GetMoveDir();
	if (pOperand->m_State.fMovementUrgency < 0.0f)
		myDir *= -1.0f;
	myDir.z = 0.0f;
	myDir.NormalizeSafe();
	Vec3 reqDir = steerDir;
	reqDir.z = 0.0f;
	reqDir.NormalizeSafe();
	Vec3 myVel = pOperand->GetVelocity();
	Vec3 myPos = pOperand->GetPhysicsPos();

	float diffCos = reqDir.Dot(myDir);
	// if not maneuvering then require a big angle change to enter it
	if(diffCos>cosTrh && m_Maneuver == eMV_None)
		return;

	// prevent very small wiggles.
	static float maneuverTimeMinLimit = 0.3f;
	static float maneuverTimeMaxLimit = 5.0f;
	float manTime =  m_Maneuver != eMV_None ? (GetAISystem()->GetFrameStartTime() - m_ManeuverTime).GetSeconds() : 0.0f;


	// if maneuvering only stop when closely lined up
	static float exitDiffCos = 0.98f;
	if (diffCos > exitDiffCos && m_Maneuver != eMV_None && manTime > maneuverTimeMinLimit)
	{
		m_Maneuver = eMV_None;
		return;
	}

	// hack for instant turning
	Vec3 camPos = GetISystem()->GetViewCamera().GetPosition();
	Vec3 opPos = pOperand->GetPhysicsPos();
	float distSq = Distance::Point_Point(camPos, opPos);
	static float minDistSq = square(5.0f);
	if (distSq > minDistSq)
	{
		bool visible = GetISystem()->GetViewCamera().IsSphereVisible_F(Sphere(opPos, pOperand->GetRadius()));
		if (!visible)
		{
			float x = reqDir.Dot(myDir);
			float y = reqDir.Dot(Vec3(-myDir.y, myDir.x, 0.0f));
			// do it in steps to help physics resolve penetrations...
			float rotAngle = 0.02f * atan2f(y, x);
			Quat q;
			q.SetRotationAA(rotAngle, Vec3(0, 0, 1));

			IEntity * pEntity = pOperand->GetEntity();
			Quat qCur = pEntity->GetRotation();
			pEntity->SetRotation(q * qCur);
			m_Maneuver = eMV_None;
			return;
		}
	}

	// set the direction
	Vec3 dirCross = myDir.Cross(reqDir);
	m_ManeuverDir = dirCross.z > 0.0f ? eMVD_AntiClockwise : eMVD_Clockwise;

	bool movingFwd = myDir.Dot(myVel) > 0.0f;

	static float maneuverDistLimit = 5;

	// start a new maneuver?
	if (m_Maneuver == eMV_None)
	{
		m_Maneuver = eMV_Back;
		m_ManeuverDist = 0.5f * maneuverDistLimit;
		m_ManeuverTime = GetAISystem()->GetFrameStartTime();
	}
	else
	{
		// reverse direction when we accumulate sufficient distance in the direction
		// we're supposed to be going in
		Vec3 delta = myPos - m_lastPosition;
		float dist = fabs(delta.Dot(myDir));
		if (movingFwd && m_Maneuver == eMV_Back)
			dist = 0.0f;
		else if (!movingFwd && m_Maneuver == eMV_Fwd)
			dist = 0.0f;
		m_ManeuverDist += dist;

		if ( manTime > maneuverTimeMaxLimit )
		{
			m_Maneuver = m_Maneuver == eMV_Fwd ? eMV_Back : eMV_Fwd;
			m_ManeuverDist = 0.0f;
			m_ManeuverTime = GetAISystem()->GetFrameStartTime();
		}
		else if ( m_Maneuver == eMV_Back )
		{
			if ( fabsf( reqDir.Dot( myDir ) ) < cosf( DEG2RAD( 85.0f ) ) )
			{
				m_Maneuver = eMV_Fwd;
				m_ManeuverDist = 0.0f;
				m_ManeuverTime = GetAISystem()->GetFrameStartTime();
			}
		}
		else
		{
			if ( fabsf( reqDir.Dot( myDir ) ) > cosf( DEG2RAD( 5.0f ) ) )
			{
				m_Maneuver = eMV_Back;
				m_ManeuverDist = 0.0f;
				m_ManeuverTime = GetAISystem()->GetFrameStartTime();
			}
		}

	}

	// now turn these into actual requests
	float normalSpeed, minSpeed, maxSpeed;
	pOperand->GetMovementSpeedRange(AISPEED_WALK, false, normalSpeed, minSpeed, maxSpeed);

	pOperand->m_State.fDesiredSpeed = minSpeed; //pOperand->GetManeuverMovementSpeed();
	if (m_Maneuver == eMV_Back)
		pOperand->m_State.fDesiredSpeed = -5.0;

	Vec3 leftDir(-myDir.y, myDir.x, 0.0f);

	if (m_ManeuverDir == eMVD_AntiClockwise)
		pOperand->m_State.vMoveDir = leftDir;
	else
		pOperand->m_State.vMoveDir = -leftDir;

	if (pOperand->m_State.fMovementUrgency < 0.0f)
		pOperand->m_State.vMoveDir *= -1.0f;
}

//===================================================================
// ExecutePreamble
//===================================================================
bool COPTrace::ExecutePreamble(CPipeUser *pOperand)
{
	if (m_lastPosition.IsZero())
		m_lastPosition = pOperand->GetPhysicsPos();

	if (!m_pNavTarget && !pOperand->m_Path.Empty())
	{
		CreateDummyFromNode(pOperand->GetPhysicsPos(),GetAISystem(), pOperand->GetName());
		pOperand->m_pReservedNavPoint = m_pNavTarget;
	}
	else if (!m_pNavTarget && pOperand->m_Path.Empty())
	{
		pOperand->m_State.fDesiredSpeed = 0.0f;
		m_inhibitPathRegen = true;
		return true;
	}

	m_inhibitPathRegen = false;
	AIAssert(m_pNavTarget);
	return false;
}

//===================================================================
// ExecutePostamble
//===================================================================
bool COPTrace::ExecutePostamble(CPipeUser *pOperand, bool &reachedEnd, bool fullUpdate, bool twoD, float distToEnd)
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_AI );
	/*  if (reachedEnd)
	if (pOperand->m_State.eActorTargetPhase >= eATP_Waiting && pOperand->m_State.eActorTargetPhase <= eATP_Finished
	|| pOperand->GetProxy()->HasFinalAnimation() || pOperand->GetProxy()->IsPlayingFinalAnimation())
	reachedEnd = false;

	if (pOperand->m_State.eActorTargetPhase == eATP_Playing)
	{
	AIWarning("COPTrace::ExecutePostamble being called for %s whilst playing exact pos animation", pOperand->GetName());
	pOperand->m_State.fDesiredSpeed = 0.0f;
	m_inhibitPathRegen = true;
	pOperand->m_State.predictedCharacterStates.nStates = 0;
	return false;
	}*/

	// do this after maneuver since that needs to track how far we moved
	Vec3 opPos = pOperand->GetPhysicsPos();
	m_fTravelDist += twoD ? Distance::Point_Point2D(opPos, m_lastPosition) : Distance::Point_Point(opPos, m_lastPosition);
	m_lastPosition = opPos;

	// only consider trace to be done once the agent has stopped.
	if (reachedEnd && m_fEndAccuracy >= 0.0f)
	{
		Vec3 vel = pOperand->GetVelocity();
		vel.z = 0.0f;
		float speed = vel.Dot(pOperand->m_State.vMoveDir);
		static float criticalSpeed = 0.01f;
		if (speed > criticalSpeed)
		{
			if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
				AILogAlways("COPTrace reached end but waiting for speed %5.2f to fall below %5.2f %s", 
				speed, criticalSpeed,
				pOperand ? pOperand->GetName() : "");
			reachedEnd = false;
			pOperand->m_State.fDesiredSpeed = 0.0f;
			m_inhibitPathRegen = true;
		}
	}

	// only consider trace to be done once the agent has stopped.
	if (reachedEnd)
	{
		pOperand->m_State.fDesiredSpeed = 0.0f;
		m_inhibitPathRegen = true;
	}

	// [Mikko] Commented out since the path is already cut after pathfinding.
	// Leaving this on, will prevent the path regen when it should not.
	/*  if (distToEnd < m_fEndAccuracy)
	m_inhibitPathRegen = true;*/

	if (reachedEnd)
	{
		// we reached the end of the path
		/*    if (!pOperand->m_Path.Empty())
		{
		if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
		AILogAlways("COPTrace reached end so clearing path %s", 
		pOperand ? pOperand->GetName() : "");
		pOperand->ClearPath("COPTrace m_Path reached end");
		}*/
		pOperand->m_State.fDesiredSpeed = 0.0f;
		return true;
	}

	if (fullUpdate)
	{
		unsigned thisNodeIndex = GetAISystem()->GetGraph()->GetEnclosing(opPos,
			pOperand->m_movementAbility.pathfindingProperties.navCapMask,
			pOperand->m_Parameters.m_fPassRadius,
			pOperand->m_lastNavNodeIndex, 0.0f, 0, false, pOperand->GetName());
		pOperand->m_lastNavNodeIndex = thisNodeIndex;

		static float forbiddenTol = 1.5f;
		GraphNode* pThisNode = GetAISystem()->GetGraph()->GetNodeManager().GetNode(thisNodeIndex);
		if (pThisNode && 
			pThisNode->navType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD) && 
			GetAISystem()->IsPointOnForbiddenEdge(opPos, forbiddenTol, 0, 0, false))
			pOperand->m_bLastNearForbiddenEdge = true;
		else
			pOperand->m_bLastNearForbiddenEdge = false;
	}

	// code below here checks/handles dynamic objects
	if (pOperand->m_Path.GetParams().precalculatedPath)
		return false;

	static bool doSteering = true;
	if (doSteering && !pOperand->m_bLastNearForbiddenEdge)
	{
		pOperand->m_IsSteering = pOperand->NavigateAroundObjects(opPos, fullUpdate,m_bSteerAroundPathTarget);
		if (pOperand->m_IsSteering)
			pOperand->m_State.predictedCharacterStates.nStates = 0;
	}
	return false;
}

//===================================================================
// ExecutePathFollower
//===================================================================
bool COPTrace::ExecutePathFollower(CPipeUser *pOperand, bool fullUpdate)
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_AI );
	if (m_TimeStep <= 0)
		return false;

	if (ExecutePreamble(pOperand))
		return true;

	static float predictionDeltaTime = 0.1f;
	static float predictionTime = 1.0f;

	CPathFollower *pathFollower = pOperand->GetPathFollower();
	AIAssert(pathFollower);

	float normalSpeed, minSpeed, maxSpeed;
	pOperand->GetMovementSpeedRange(pOperand->m_State.fMovementUrgency, pOperand->m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

	//  float normalSpeed = pOperand->GetNormalMovementSpeed(pOperand->m_State.fMovementUrgency, true);
	//	float slowSpeed = pOperand->GetManeuverMovementSpeed();

	//  float endDistance = max(0.0f, m_fEndDistance - pOperand->m_Path.GetDiscardedPathLength());

	// Update the path follower - really these things shouldn't change
	CPathFollower::SPathFollowerParams &params = pathFollower->GetParams();
	params.minSpeed = minSpeed; //min(slowSpeed, normalSpeed);
	params.maxSpeed = maxSpeed; //normalSpeed * 1.1f;

	/*	params.minSpeed = normalSpeed * 0.5f;
	params.maxSpeed = normalSpeed * 1.1f;*/

	/*	IPuppetProxy* pPuppetProxy =  0;
	if (pOperand->GetProxy())
	pOperand->GetProxy()->QueryProxy(AIPROXY_PUPPET, (void**)&pPuppetProxy);

	if (!pPuppetProxy || !pPuppetProxy->QueryCurrentAnimationSpeedRange(params.minSpeed, params.maxSpeed))
	{
	params.minSpeed = normalSpeed * 0.75f;
	params.maxSpeed = normalSpeed * 1.1f;
	}*/

	params.normalSpeed = clamp(normalSpeed, params.minSpeed, params.maxSpeed);

	params.endDistance = 0.0f; //endDistance;

	// [Mikko] Slight hack to make the formation following better.
	// When following formation, the agent is often very close to the formation.
	// This code forces the actor to lag a bit behind, which helps to follow the formation much more smoothly.
	// TODO: Revive approach-at-distance without cutting the path and add that as extra parameter for stick.
	if (m_pendingActorTargetRequester == eTATR_None && pOperand->m_Path.GetParams().continueMovingAtEnd && 
		pOperand->m_pPathFindTarget && pOperand->m_pPathFindTarget->GetSubType() == IAIObject::STP_FORMATION)
	{
		params.endDistance = 1.0f;
	}

	params.maxAccel = pOperand->m_movementAbility.maxAccel;
	params.maxDecel = pOperand->m_movementAbility.maxDecel;
	params.stopAtEnd = !pOperand->m_Path.GetParams().continueMovingAtEnd;

	CPathFollower::SPathFollowResult result;

	static CPathFollower::SPathFollowResult::TPredictedStates predictedStates;

	// Lower the prediction calculations for puppets which have lo update priority (means invisible or distant).
	bool highPriority = true;
	if (CPuppet* pPuppet = pOperand->CastToCPuppet())
	{
		if (pPuppet->GetUpdatePriority() != AIPUP_VERY_HIGH && pPuppet->GetUpdatePriority() != AIPUP_HIGH)
			highPriority = false;
	}
	result.desiredPredictionTime = highPriority ? predictionTime : 0.0f;
	int nPredictedStates = (int)(result.desiredPredictionTime / predictionDeltaTime + 0.5f);
	predictedStates.resize(nPredictedStates);
	result.predictedStates = highPriority ? &predictedStates: 0;
	result.predictionDeltaTime = predictionDeltaTime;

	Vec3 curPos = pOperand->GetPhysicsPos();
	Vec3 curVel = pOperand->GetVelocity();

	pathFollower->Update(result, curPos, curVel, m_TimeStep);

	Vec3 desiredMoveDir = result.velocityOut;
	float desiredSpeed = desiredMoveDir.NormalizeSafe();

	pOperand->m_State.fDesiredSpeed = desiredSpeed;
	pOperand->m_State.vMoveDir = desiredMoveDir;
	pOperand->m_State.fDistanceToPathEnd = pathFollower->GetDistToEnd(&curPos);

	int num = min((int) predictedStates.size(), (int) SAIPredictedCharacterStates::maxStates);
	pOperand->m_State.predictedCharacterStates.nStates = num;
	for (int i = 0 ; i < num ; ++i)
	{
		const CPathFollower::SPathFollowResult::SPredictedState &state = predictedStates[i];
		pOperand->m_State.predictedCharacterStates.states[i].Set(state.pos, state.vel, (1 + i) * predictionDeltaTime);
	}

	bool retVal = ExecutePostamble(pOperand, result.reachedEnd, fullUpdate, pathFollower->GetParams().use2D, pOperand->m_State.fDistanceToPathEnd);
	return retVal;
}

//====================================================================
// Execute2D
//====================================================================
bool COPTrace::Execute2D(CPipeUser *pOperand, bool fullUpdate)
{
	if (ExecutePreamble(pOperand))
		return true;

	// input
	Vec3 fwdDir = pOperand->GetMoveDir();
	if (pOperand->m_State.fMovementUrgency < 0.0f)
		fwdDir *= -1.0f;
	Vec3 opPos = pOperand->GetPhysicsPos();
	pe_status_dynamics  dSt;
	pOperand->GetPhysics()->GetStatus(&dSt);
	Vec3 opVel = m_Maneuver==eMV_None ? dSt.v : fwdDir*5.0f;
	float lookAhead = pOperand->m_movementAbility.pathLookAhead;
	float pathRadius = pOperand->m_movementAbility.pathRadius;
	bool resolveSticking = pOperand->m_movementAbility.resolveStickingInTrace;

	// output
	Vec3	steerDir(ZERO);
	float distToEnd = 0.0f;
	float distToPath = 0.0f;
	Vec3 pathDir(ZERO);
	Vec3 pathAheadDir(ZERO);
	Vec3 pathAheadPos(ZERO);

	bool isResolvingSticking = false;

	bool stillTracingPath = pOperand->m_Path.UpdateAndSteerAlongPath(
		steerDir, distToEnd, distToPath, isResolvingSticking,
		pathDir, pathAheadDir, pathAheadPos,
		opPos, opVel, lookAhead, pathRadius, m_TimeStep, resolveSticking, true);
	pOperand->m_State.fDistanceToPathEnd = max(0.0f, distToEnd);

	pathAheadDir.z = 0.0f;
	pathAheadDir.NormalizeSafe();
	Vec3 steerDir2D(steerDir);
	steerDir2D.z = 0.0f;
	steerDir2D.NormalizeSafe();

	// Update the debug movement reason.
	pOperand->m_DEBUGmovementReason = CPipeUser::AIMORE_TRACE;

	distToEnd -= /*m_fEndDistance*/ -pOperand->m_Path.GetDiscardedPathLength();
	bool reachedEnd = false;
	if (stillTracingPath && distToEnd>.1f)
	{
		Vec3 targetPos;
		if (m_pNavTarget && pOperand->m_Path.GetPosAlongPath(targetPos, lookAhead, true, true))
			m_pNavTarget->SetPos(targetPos);

		//turning maneuvering 
		static bool doManouever = true;
		if (doManouever)
			ExecuteManeuver(pOperand, steerDir);

		if(m_Maneuver!=eMV_None )
		{
			Vec3 curPos = pOperand->GetPhysicsPos();
			m_fTravelDist += Distance::Point_Point2D(curPos, m_lastPosition);
			m_lastPosition = curPos;
			// prevent path regen
			m_fTotalTracingTime = 0.0f;
			return false;
		}

		//    float normalSpeed = pOperand->GetNormalMovementSpeed(pOperand->m_State.fMovementUrgency, true);
		//    float slowSpeed = pOperand->GetManeuverMovementSpeed();
		//    if (slowSpeed > normalSpeed) slowSpeed = normalSpeed;
		float normalSpeed, minSpeed, maxSpeed;
		pOperand->GetMovementSpeedRange(pOperand->m_State.fMovementUrgency, pOperand->m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

		// These will be adjusted to the range 0-1 to select a speed between slow and normal
		float dirSpeedMod = 1.0f;
		float curveSpeedMod = 1.0f;
		float endSpeedMod = 1.0f;
		float slopeMod = 1.0f;
		float moveDirMod = 1.0f;

		// speed falloff - slow down if current direction is too different from desired
		if (pOperand->GetType()== AIOBJECT_VEHICLE)
		{
			static float offset = 1.0f; // if this is > 1 then there is a "window" where no slow-down occurs
			float	velFalloff = offset * pathAheadDir.Dot(fwdDir);
			float	velFalloffD=1-velFalloff;
			if (velFalloffD > 0.0f && pOperand->m_movementAbility.velDecay > 0.0f)
				dirSpeedMod = velFalloff/(velFalloffD*pOperand->m_movementAbility.velDecay);
		}

		// slow down due to the current/desired move direction - is sensitive to glitchy anim transitions
		// Shouldn't need this anyway with a spiffy animation system...

		// [mikko] Commented out since this was causing the character to get stuck if the velocity is really small.
		// TODO Danny: Delete once new path following is in place.
		/*    if (GetAISystem()->m_cvPuppetDirSpeedControl->GetIVal() && pOperand->GetType()== AIOBJECT_PUPPET && distToEnd > lookAhead)
		{
		Vec3 vel = pOperand->GetVelocity();
		static float moveDirScale = 1.0f;
		Vec3 curDir(vel.x, vel.y, 0.0f);
		float	len = curDir.GetLength();
		if(len > 0.0001f)
		{
		curDir /= len;
		float dot = curDir.Dot(steerDir2D);
		moveDirMod = dot;
		}
		}*/

		// slow down due to the path curvature
		float lookAheadForSpeedControl;
		if (pOperand->m_movementAbility.pathSpeedLookAheadPerSpeed < 0.0f)
			lookAheadForSpeedControl = -pOperand->m_movementAbility.pathSpeedLookAheadPerSpeed * lookAhead * pOperand->m_State.fMovementUrgency;
		else
			lookAheadForSpeedControl = pOperand->m_movementAbility.pathSpeedLookAheadPerSpeed * pOperand->GetVelocity().GetLength();

		if (lookAheadForSpeedControl > 0.0f)
		{
			Vec3 pos, dir;
			float lowestPathDot = 0.0f;
			bool curveOK = pOperand->m_Path.GetPathPropertiesAhead(lookAheadForSpeedControl, true, pos, dir, 0, lowestPathDot, true);
			Vec3 thisPathSegDir = (pOperand->m_Path.GetNextPathPoint()->vPos - pOperand->m_Path.GetPrevPathPoint()->vPos);
			thisPathSegDir.z = 0.0f;
			thisPathSegDir.NormalizeSafe();
			float thisDot = thisPathSegDir.Dot(steerDir2D);
			if (thisDot < lowestPathDot)
				lowestPathDot = thisDot;
			if (curveOK)
			{
				float a = 1.0f - 2.0f * pOperand->m_movementAbility.cornerSlowDown; // decrease this to make the speed drop off quicker with angle
				float b = 1.0f - a;
				curveSpeedMod = a + b * lowestPathDot;
			}

			// slow down at end
			if (m_fEndAccuracy >= 0.0f)
			{
				static float slowDownDistScale = 2.0f;
				static float minEndSpeedMod = 0.1f;
				float slowDownDist = slowDownDistScale * lookAheadForSpeedControl;
				float workingDistToEnd = m_fEndAccuracy + distToEnd - 0.2f * lookAheadForSpeedControl;
				if (slowDownDist > 0.1f && workingDistToEnd < slowDownDist)
				{
					endSpeedMod = workingDistToEnd / slowDownDist;
					Limit(endSpeedMod, minEndSpeedMod, 1.0f);
				}
			}
		}

		float slopeModCoeff = pOperand->m_movementAbility.slopeSlowDown;
		// slow down when going down steep slopes (especially stairs!)
		if (pOperand->m_lastNavNodeIndex && slopeModCoeff > 0 && GetAISystem()->GetGraph()->GetNode(pOperand->m_lastNavNodeIndex)->navType == IAISystem::NAV_WAYPOINT_HUMAN)
		{
			static float slowDownSlope = 0.5f;
			float pathHorDist = steerDir.GetLength2D();
			if (pathHorDist > 0.0f && steerDir.z < 0.0f)
			{
				float slope = -steerDir.z / pathHorDist * slopeModCoeff;
				slopeMod = 1.0f - slope / slowDownSlope;
				static float minSlopeMod = 0.5f;
				Limit(slopeMod, minSlopeMod, 1.0f);
			}
		}

		// slow down when going up steep slopes
		if(slopeModCoeff > 0)
    {
      IPhysicalEntity * pPhysics = pOperand->GetPhysics();
      pe_status_living status;
      int valid = pPhysics->GetStatus(&status);
			if (valid)
			{
				if (!status.bFlying)
				{
					Vec3 sideDir(-steerDir2D.y, steerDir2D.x, 0.0f);
					Vec3 slopeN = status.groundSlope - status.groundSlope.Dot(sideDir) * sideDir;
					slopeN.NormalizeSafe();
					// d is -ve for uphill
					float d = steerDir2D.Dot(status.groundSlope);
					Limit(d, -0.99f, 0.99f);
					// map d=-1 -> -inf, d=0 -> 1, d=1 -> inf
					float uphillSlopeMod = (1 + d / (1.0f - square(d)))*slopeModCoeff;
					static float minUphillSlopeMod = 0.5f;
					if (uphillSlopeMod < minUphillSlopeMod)
						uphillSlopeMod = minUphillSlopeMod;
					if (uphillSlopeMod < 1.0f)
						slopeMod = min(slopeMod, uphillSlopeMod);
				}
			}
    }

		float maxMod = min(min(min(min(dirSpeedMod, curveSpeedMod), endSpeedMod), slopeMod), moveDirMod);
		Limit(maxMod, 0.0f, 1.0f);
		//    maxMod = pow(maxMod, 2.0f);
		float newDesiredSpeed = (1.0f - maxMod) * minSpeed + maxMod * normalSpeed;

		if(pOperand->m_State.fMovementUrgency==AISPEED_ZERO && newDesiredSpeed==0.f)
		{//Special case: AIFollowPathSpeedsStance Speed set to 0, (run property set to -5)
			//decrease the vehicle speed slowly, not to slide sideways from path during brake
			if(fabs(pOperand->m_State.fDesiredSpeed)>2.f)
				pOperand->m_State.fDesiredSpeed=sgn(pOperand->m_State.fDesiredSpeed)*2.f;
			else if(pOperand->m_State.fDesiredSpeed!=0.f)
			{
				float change=sgn(pOperand->m_State.fDesiredSpeed)*m_TimeStep*1.f;
				if(fabs(pOperand->m_State.fDesiredSpeed)>fabs(change))
					pOperand->m_State.fDesiredSpeed -= change;
				else 
					pOperand->m_State.fDesiredSpeed=0.f;
			}
		}
		else
		{
		float change = newDesiredSpeed - pOperand->m_State.fDesiredSpeed;
		if (change > m_TimeStep * pOperand->m_movementAbility.maxAccel)
			change = m_TimeStep * pOperand->m_movementAbility.maxAccel;
		else if (change < -m_TimeStep * pOperand->m_movementAbility.maxDecel)
			change = -m_TimeStep * pOperand->m_movementAbility.maxDecel;
		pOperand->m_State.fDesiredSpeed += change;
		}
		pOperand->m_State.vMoveDir = steerDir2D;
		if (pOperand->m_State.fMovementUrgency < 0.0f)
			pOperand->m_State.vMoveDir *= -1.0f;

		// prediction
		static bool doPrediction = true;
		pOperand->m_State.predictedCharacterStates.nStates = 0;
	}
	else
	{
		reachedEnd = true;
	}

	return ExecutePostamble(pOperand, reachedEnd, fullUpdate, true, distToEnd);
}

//====================================================================
// Execute3D
//====================================================================
bool COPTrace::Execute3D(CPipeUser *pOperand, bool fullUpdate)
{
	if (ExecutePreamble(pOperand))
		return true;

	// Ideally do this check less... but beware we might regen the path and the end might change a bit(?)
	if (fullUpdate)
	{
		if (pOperand->GetType() == AIOBJECT_VEHICLE && m_fEndAccuracy == 0.0f && !pOperand->m_Path.GetPath().empty())
		{
			Vec3 endPt = pOperand->m_Path.GetPath().back().vPos;
			// ideally we would use AICE_ALL here, but that can result in intersection with the heli when it
			// gets to the end of the path...
			bool gotFloor = GetFloorPos(m_landingPos, endPt, 0.5f, 1.0f, 1.0f, AICE_STATIC);
			if (gotFloor)
				m_landHeight = 2.0f;
			else
				m_landHeight = 0.0f;
			if (m_workingLandingHeightOffset > 0.0f)
				m_inhibitPathRegen = true;
		}
		else
		{
			m_landHeight = 0.0f;
			m_inhibitPathRegen = false;
		}
	}

	// input
	Vec3 opPos = pOperand->GetPhysicsPos();
	Vec3 fakeOpPos = opPos;
	fakeOpPos.z -= m_workingLandingHeightOffset;
	if (pOperand->m_IsSteering)
		fakeOpPos.z -= pOperand->m_flightSteeringZOffset;

	pe_status_dynamics  dSt;
	pOperand->GetPhysics()->GetStatus(&dSt);
	Vec3 opVel = dSt.v;
	float lookAhead = pOperand->m_movementAbility.pathLookAhead;
	float pathRadius = pOperand->m_movementAbility.pathRadius;
	bool resolveSticking = pOperand->m_movementAbility.resolveStickingInTrace;

	// output
	Vec3	steerDir(ZERO);
	float distToEnd = 0.0f;
	float distToPath = 0.0f;
	Vec3 pathDir(ZERO);
	Vec3 pathAheadDir(ZERO);
	Vec3 pathAheadPos(ZERO);

	bool isResolvingSticking = false;

	bool stillTracingPath = pOperand->m_Path.UpdateAndSteerAlongPath(steerDir, distToEnd, distToPath, isResolvingSticking,
		pathDir, pathAheadDir, pathAheadPos,
		fakeOpPos, opVel, lookAhead, pathRadius, m_TimeStep, resolveSticking, false);
	pOperand->m_State.fDistanceToPathEnd = max(0.0f, distToEnd);

	// Update the debug movement reason.
	pOperand->m_DEBUGmovementReason = CPipeUser::AIMORE_TRACE;

	distToEnd -= distToPath;
	distToEnd -= m_landHeight * 2.0f;
	if (distToEnd < 0.0f)
		stillTracingPath = false;

	if (!stillTracingPath && m_landHeight > 0.0f)
	{
		return ExecuteLanding(pOperand, m_landingPos);
	}

	distToEnd -= /*m_fEndDistance*/ -pOperand->m_Path.GetDiscardedPathLength();
	bool reachedEnd = !stillTracingPath;
	if (stillTracingPath && distToEnd > 0.0f)
	{
		Vec3 targetPos;
		if (m_pNavTarget &&pOperand->m_Path.GetPosAlongPath(targetPos, lookAhead, true, true))
			m_pNavTarget->SetPos(targetPos);

		//    float normalSpeed = pOperand->GetNormalMovementSpeed(pOperand->m_State.fMovementUrgency, true);
		//    float slowSpeed = pOperand->GetManeuverMovementSpeed();
		//    if (slowSpeed > normalSpeed) slowSpeed = normalSpeed;
		float normalSpeed, minSpeed, maxSpeed;
		pOperand->GetMovementSpeedRange(pOperand->m_State.fMovementUrgency, pOperand->m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

		// These will be adjusted to the range 0-1 to select a speed between slow and normal
		float dirSpeedMod = 1.0f;
		float curveSpeedMod = 1.0f;
		float endSpeedMod = 1.0f;
		float moveDirMod = 1.0f;

		// slow down due to the path curvature
		float lookAheadForSpeedControl;
		if (pOperand->m_movementAbility.pathSpeedLookAheadPerSpeed < 0.0f)
			lookAheadForSpeedControl = lookAhead * pOperand->m_State.fMovementUrgency;
		else
			lookAheadForSpeedControl = pOperand->m_movementAbility.pathSpeedLookAheadPerSpeed * pOperand->GetVelocity().GetLength();

		lookAheadForSpeedControl -= distToPath;
		if (lookAheadForSpeedControl < 0.0f)
			lookAheadForSpeedControl = 0.0f;

		if (lookAheadForSpeedControl > 0.0f)
		{
			Vec3 pos, dir;
			float lowestPathDot = 0.0f;
			bool curveOK = pOperand->m_Path.GetPathPropertiesAhead(lookAheadForSpeedControl, true, pos, dir, 0, lowestPathDot, true);
			Vec3 thisPathSegDir = (pOperand->m_Path.GetNextPathPoint()->vPos - pOperand->m_Path.GetPrevPathPoint()->vPos).GetNormalizedSafe();
			float thisDot = thisPathSegDir.Dot(steerDir);
			if (thisDot < lowestPathDot)
				lowestPathDot = thisDot;
			if (curveOK)
			{
				float a = 1.0f - 2.0f * pOperand->m_movementAbility.cornerSlowDown; // decrease this to make the speed drop off quicker with angle
				float b = 1.0f - a;
				curveSpeedMod = a + b * lowestPathDot;
			}
		}

		// slow down at end 
		if (m_fEndAccuracy >= 0.0f)
		{
			static float slowDownDistScale = 1.0f;
			float slowDownDist = slowDownDistScale * lookAheadForSpeedControl;
			float workingDistToEnd = m_fEndAccuracy + distToEnd - 0.2f * lookAheadForSpeedControl;
			if (slowDownDist > 0.1f && workingDistToEnd < slowDownDist)
			{
				// slow speeds are for manouevering - here we want something that will actually be almost stationary
				minSpeed *= 0.1f;
				endSpeedMod = workingDistToEnd / slowDownDist;
				Limit(endSpeedMod, 0.0f, 1.0f);
				m_workingLandingHeightOffset = (1.0f - endSpeedMod) * m_landHeight;
			}
			else
			{
				m_workingLandingHeightOffset = 0.0f;
			}
		}

		float maxMod = min(min(min(dirSpeedMod, curveSpeedMod), endSpeedMod), moveDirMod);
		Limit(maxMod, 0.0f, 1.0f);

		float newDesiredSpeed = (1.0f - maxMod) * minSpeed + maxMod * normalSpeed;
		float change = newDesiredSpeed - pOperand->m_State.fDesiredSpeed;
		if (change > m_TimeStep * pOperand->m_movementAbility.maxAccel)
			change = m_TimeStep * pOperand->m_movementAbility.maxAccel;
		else if (change < -m_TimeStep * pOperand->m_movementAbility.maxDecel)
			change = -m_TimeStep * pOperand->m_movementAbility.maxDecel;
		pOperand->m_State.fDesiredSpeed += change;

		pOperand->m_State.vMoveDir = steerDir;
		if (pOperand->m_State.fMovementUrgency < 0.0f)
			pOperand->m_State.vMoveDir *= -1.0f;

		// prediction
		static bool doPrediction = true;
		pOperand->m_State.predictedCharacterStates.nStates = 0;
	}
	else
	{
		reachedEnd = true;
	}

	return ExecutePostamble(pOperand, reachedEnd, fullUpdate, false, distToEnd);
}

//===================================================================
// ExecuteLanding
//===================================================================
bool COPTrace::ExecuteLanding(CPipeUser *pOperand, const Vec3 &pathEnd)
{
	m_inhibitPathRegen = true;
	float normalSpeed, minSpeed, maxSpeed;
	pOperand->GetMovementSpeedRange(pOperand->m_State.fDesiredSpeed, false, normalSpeed, minSpeed, maxSpeed);
	//  float slowSpeed = pOperand->GetManeuverMovementSpeed();
	Vec3 opPos = pOperand->GetPhysicsPos();

	Vec3 horMoveDir = pathEnd - opPos;
	horMoveDir.z = 0.0f;
	float error = horMoveDir.NormalizeSafe();

	Limit(error, 0.0f, 1.0f);
	float horSpeed = 0.3f * minSpeed * error;
	float verSpeed = 1.0f;

	pOperand->m_State.vMoveDir = horSpeed * horMoveDir - Vec3(0, 0, verSpeed);
	pOperand->m_State.vMoveDir.NormalizeSafe();
	pOperand->m_State.fDesiredSpeed = sqrtf(square(horSpeed) + square(verSpeed));

	if (pOperand->m_State.fMovementUrgency < 0.0f)
		pOperand->m_State.vMoveDir *= -1.0f;

	// set look dir
	if (m_landingDir.IsZero())
	{
		if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
			AILogAlways("COPTrace::ExecuteLanding starting final landing %s", 
			pOperand ? pOperand->GetName() : "");
		m_landingDir = pOperand->GetMoveDir();
		m_landingDir.z = 0.0f;
		m_landingDir.NormalizeSafe(Vec3Constants<float>::fVec3_OneX);
	}
	Vec3 navTargetPos = opPos + 100.0f * m_landingDir;
	m_pNavTarget->SetPos(navTargetPos);

	if (!pOperand->m_bLooseAttention)
	{
		//pOperand->m_bLooseAttention = true;
		//pOperand->m_pLooseAttentionTarget = m_pNavTarget;
		m_looseAttentionId= pOperand->SetLooseAttentionTarget(m_pNavTarget);
	}

	// 
	pe_status_collisions stat;
	stat.pHistory = 0;
	int collisions = pOperand->GetPhysics()->GetStatus(&stat);
	if (collisions > 0)
		return true;
	else 
		return false;
}

//===================================================================
// DebugDraw
//===================================================================
void COPTrace::DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const
{
	if(IsPathRegenerationInhibited())
		pRenderer->DrawLabel(pOperand->GetPhysicsPos(), 1.5f, "PATH LOCKED\n%s %s", m_inhibitPathRegen ? "Inhibit" : "", m_passingStraightNavSO ? "NavSO" : "");
}

//===================================================================
// CreateDummyFromNode
//===================================================================
void COPTrace::CreateDummyFromNode(const Vec3 &pos, CAISystem *pSystem, const char* ownerName)
{
	static const char * NAV_TARGET_STRING = "navTarget_";
	static const size_t NAV_TARGET_STRING_LENGTH = strlen("navTarget_");
	string name;
	name.reserve(NAV_TARGET_STRING_LENGTH+strlen(ownerName)+1);
	name += NAV_TARGET_STRING;
	name += ownerName;
	m_pNavTarget = pSystem->CreateDummyObject(name, CAIObject::STP_REFPOINT);
	m_pNavTarget->SetPos(pos);
}


EGoalOpResult COPIgnoreAll::Execute(CPipeUser *pOperand)
{
	//	pOperand->m_bUpdateInternal = !m_bParam;
	pOperand->m_bCanReceiveSignals = !m_bParam;
	return AIGOR_DONE;
}


EGoalOpResult COPSignal::Execute(CPipeUser *pOperand)
{
	if (m_bSent)
	{
		m_bSent = false;
		return AIGOR_DONE;
	}

	AISignalExtraData* pData = new AISignalExtraData;
	pData->iValue = m_iDataValue;

	if (!m_cFilter)	// if you are sending to yourself
	{
		pOperand->SetSignal(m_nSignalID,m_sSignal.c_str(), pOperand->GetEntity(), pData );
		m_bSent = true;
		return AIGOR_IN_PROGRESS;
	}

	switch (m_cFilter)
	{
	case SIGNALFILTER_LASTOP:
		// signal to last operation target
		if (pOperand->m_pLastOpResult)
		{
			CAIActor* pOperandActor = pOperand->m_pLastOpResult->CastToCAIActor();
			if(pOperandActor)
				pOperandActor->SetSignal(m_nSignalID, m_sSignal.c_str(), pOperand->GetEntity(), pData);
		}
		break;
	case SIGNALFILTER_TARGET:
		{
			CAIActor* pTargetActor = m_pTarget->CastToCAIActor();
			if(pTargetActor)
				pTargetActor->SetSignal(m_nSignalID, m_sSignal.c_str(),pOperand->GetEntity(), pData);
		}
		break;
	default:
		// signal to species, group or anyone within comm range
		GetAISystem()->SendSignal(m_cFilter, m_nSignalID, m_sSignal.c_str(), pOperand, pData);
		break;

	}

	m_bSent = true;
	return AIGOR_IN_PROGRESS; 
}


EGoalOpResult COPDeValue::Execute(CPipeUser *pOperand)
{
	if(m_bClearDevalued)
		pOperand->ClearDevalued();
	else if (pOperand->GetAttentionTarget())
	{
		if( CPuppet *pPuppet = pOperand->CastToCPuppet() )
			pPuppet->Devalue(pOperand->GetAttentionTarget(), bDevaluePuppets);
	}
	return AIGOR_DONE;
}

EGoalOpResult COPHide::Execute(CPipeUser *pOperand)
{
	CAISystem *pSystem = GetAISystem();

	if ( m_iActionId )
	{
		m_bEndEffect = false; // never use any end effects with smart objects
		pOperand->m_bHiding = pOperand->m_bLastActionSucceed;
		m_iActionId = 0;

		// this code block is just copied from below
		Reset(pOperand);
		if (IsBadHiding(pOperand))
		{
			pOperand->SetSignal(1,"OnBadHideSpot" ,0, 0, g_crcSignals.m_nOnBadHideSpot);
			return AIGOR_FAILED;
		}
		else
		{
			pOperand->SetSignal(1,"OnHideSpotReached", 0, 0, g_crcSignals.m_nOnHideSpotReached);
			return AIGOR_SUCCEED;
		}
	}

	pOperand->m_bHiding = true;

	if (!m_pHideTarget)
	{
		m_bEndEffect = false;
		pOperand->ClearPath("COPHide::Execute m_Path");
		m_bAttTarget = true;

		int flagmask = 	(HM_INCLUDE_SOFTCOVERS | HM_IGNORE_ENEMIES | HM_BACK | HM_AROUND_LASTOP | HM_FROM_LASTOP);
		int flags = m_nEvaluationMethod & flagmask;
		m_nEvaluationMethod &= ~flagmask;

		if (!pOperand->GetAttentionTarget())
		{
			m_bAttTarget = false;
			if (m_nEvaluationMethod != HM_USEREFPOINT)
			{
				if (!pOperand->m_pLastOpResult)
				{
					Reset(pOperand);
					pOperand->m_bHiding = false;
					return AIGOR_FAILED;
				}
			}
		}

		// lets create the place where we will hide

		CLeader* pLeader = GetAISystem()->GetLeader(pOperand);

		Vec3 vHidePos;

		if (m_nEvaluationMethod == HM_USEREFPOINT)
		{
			IPipeUser *pPipeUser = pOperand->CastToCPipeUser();
			if (pPipeUser)
			{
				IAIObject* pRefPoint = pPipeUser->GetRefPoint();
				if (pRefPoint)
				{
					// Setup the hide object.
					vHidePos = pRefPoint->GetPos();
					Vec3	target(pOperand->GetAttentionTarget() ? pOperand->GetAttentionTarget()->GetPos() : vHidePos);
					SHideSpot	hs(SHideSpot::HST_DYNAMIC, vHidePos, (target - vHidePos).GetNormalizedSafe());
					pOperand->m_CurrentHideObject.Set(&hs, hs.pos, hs.dir);

					m_pHideTarget = pSystem->CreateDummyObject(string(pOperand->GetName()) + "_HideTarget");
					m_pHideTarget->SetPos(vHidePos);
					return AIGOR_IN_PROGRESS;
				}
			}
			else
			{
				Reset(pOperand);
				pOperand->m_bHiding = false;
				return AIGOR_FAILED;
			}
		}
		else if (m_nEvaluationMethod == HM_ASKLEADER || m_nEvaluationMethod == HM_ASKLEADER_NOSAME)
		{
			if (pLeader)
			{
				Vec3 vDir = pOperand->GetPos() - pLeader->GetAIGroup()->GetEnemyAveragePosition();
				Vec3 enemyPos = pLeader->GetAIGroup()->GetForemostEnemyPosition(vDir);
				if(enemyPos.IsZero())
					enemyPos = pLeader->GetAIGroup()->GetEnemyPositionKnown();

				pLeader->m_pObstacleAllocator->SetSearchDistance(m_fSearchDistance);

				CObstacleRef obstacle;
				obstacle = pLeader->m_pObstacleAllocator->ReallocUnitObstacle(pOperand, enemyPos, m_nEvaluationMethod != HM_ASKLEADER_NOSAME);
				if (obstacle)
				{
					// [Mikko] This is not exactly the way it should be done, but for consistency,
					// set the the current hide object to some meaningful values.
					Vec3	target(pOperand->GetAttentionTarget() ? pOperand->GetAttentionTarget()->GetPos() : vHidePos);
					vHidePos = pLeader->GetHidePoint(pOperand, obstacle);

					// Move the hidepos away from a wall.
					Vec3	vHideDir(target - vHidePos);
					vHideDir.NormalizeSafe();
					GetAISystem()->AdjustDirectionalCoverPosition(vHidePos, vHideDir, pOperand->GetParameters().m_fPassRadius, 0.75f);

					// Setup the hide object.
					SHideSpot	hs(SHideSpot::HST_DYNAMIC, vHidePos, vHideDir);
					pOperand->m_CurrentHideObject.Set(&hs, hs.pos, hs.dir);

					m_vHidePos = vHidePos;
					m_pHideTarget = pSystem->CreateDummyObject(string(pOperand->GetName()) + "_HideTarget");
					m_pHideTarget->SetPos(vHidePos);
					return AIGOR_IN_PROGRESS;
				}
			}

			m_nEvaluationMethod = HM_NEAREST;
		}

		int nbid;
		IVisArea *iva;
		Vec3 searchPos = pOperand->m_pLastOpResult && (flags & HM_AROUND_LASTOP)!=0 ? pOperand->m_pLastOpResult->GetPos():pOperand->GetPos();
		IAISystem::ENavigationType	navType = GetAISystem()->CheckNavigationType(searchPos, nbid, iva, pOperand->m_movementAbility.pathfindingProperties.navCapMask);

		Vec3 hideFrom;
		if (m_bAttTarget)
			hideFrom = pOperand->GetAttentionTarget()->GetPos();
		else if (pOperand->m_pLastOpResult && (flags & HM_FROM_LASTOP) != 0)
			hideFrom = pOperand->m_pLastOpResult->GetPos();
		else if (CAIObject* pBeacon = (CAIObject*)GetAISystem()->GetBeacon(pOperand->GetGroupId()))
			hideFrom = pBeacon->GetPos();
		//		else if (pOperand->m_pLastOpResult && (flags & HM_AROUND_LASTOP)==0)
		//			hideFrom = pOperand->m_pLastOpResult->GetPos();
		else
			hideFrom = pOperand->GetPos();	// Just in case there is nothing to hide from (not even beacon), at least try to hide.

		m_nEvaluationMethod |= flags;
		vHidePos = pOperand->FindHidePoint( m_fSearchDistance, hideFrom, m_nEvaluationMethod, navType, true, m_fMinDistance );

		if (!pOperand->m_CurrentHideObject.IsValid())
		{
			Reset(pOperand);
			pOperand->m_bHiding = false;
			return AIGOR_FAILED;
		}

		// shouldn't do this because FindHidePoint() has already set the right value in m_vLastHidePos
		//pOperand->m_vLastHidePos = vHidePos;

		// is it a smart object?
		if ( pOperand->m_CurrentHideObject.GetSmartObject().pRule )
		{
			int id = GetAISystem()->AllocGoalPipeId();
			GetAISystem()->GetSmartObjects()->UseSmartObject( pOperand->m_CurrentHideObject.GetSmartObject().pUser,
				pOperand->m_CurrentHideObject.GetSmartObject().pObject, pOperand->m_CurrentHideObject.GetSmartObject().pRule, id );
			return AIGOR_SUCCEED;
		}

		if(m_bLookAtLastOp && pOperand->m_pLastOpResult )
		{
			/*if (0 == m_pLooseAttentionTarget)
			m_pLooseAttentionTarget = pSystem->CreateDummyObject(string(pOperand->GetName()) + "_HideLooseAttTarget");
			pOperand->m_pLooseAttentionTarget = m_pLooseAttentionTarget;
			pOperand->m_pLooseAttentionTarget->SetPos(pOperand->m_pLastOpResult->GetPos());
			pOperand->m_bLooseAttention = true;*/

			//TO DO luc: now it always looks at the last op - is it ok?
			m_looseAttentionId = pOperand->SetLooseAttentionTarget(pOperand->m_pLastOpResult);
		}

		m_pHideTarget = pSystem->CreateDummyObject(string(pOperand->GetName()) + "_HideTarget");
		m_pHideTarget->SetPos(vHidePos);
		pOperand->m_nPathDecision = PATHFINDER_STILLFINDING;
	}
	else
	{

		if (!m_pPathFindDirective)
		{
			if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
			{
				AILogAlways("COPHide::Execute %s pathfinding to (%5.2f, %5.2f, %5.2f)", pOperand->GetName(),
					m_pHideTarget->GetPos().x, m_pHideTarget->GetPos().y, m_pHideTarget->GetPos().z);
			}

			// request the path
			m_pPathFindDirective = new COPPathFind("",m_pHideTarget);
			return AIGOR_IN_PROGRESS;
		}
		else
		{
			if (!m_pTraceDirective)
			{
				if (m_pPathFindDirective->Execute(pOperand) != AIGOR_IN_PROGRESS)
				{
					if (pOperand->m_nPathDecision == PATHFINDER_PATHFOUND)
					{
						if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
						{
							AILogAlways("COPHide::Execute %s Creating trace to hide target (%5.2f, %5.2f, %5.2f)", pOperand->GetName(),
								m_pHideTarget->GetPos().x, m_pHideTarget->GetPos().y, m_pHideTarget->GetPos().z);
						}
						m_pTraceDirective = new COPTrace(m_bLookAtHide, defaultTraceEndAccuracy);
					}
					else
					{
						// Could not reach the point, mark it unreachable so that we do not try to pick it again.
						pOperand->SetCurrentHideObjectUnreachable();

						Reset(pOperand);
						if (IsBadHiding(pOperand))
						{
							pOperand->SetSignal(1,"OnBadHideSpot" ,0, 0, g_crcSignals.m_nOnBadHideSpot);
							return AIGOR_FAILED;
						}
						else
						{
							pOperand->SetSignal(1,"OnHideSpotReached", 0, 0, g_crcSignals.m_nOnHideSpotReached);
							return AIGOR_SUCCEED;
						}
					}
				}
			}
			else
			{
				if (m_pTraceDirective->Execute(pOperand) != AIGOR_IN_PROGRESS)
				{
					Reset(pOperand);
					if (IsBadHiding(pOperand))
					{
						pOperand->SetSignal(1,"OnBadHideSpot" ,0, 0, g_crcSignals.m_nOnBadHideSpot);
						return AIGOR_FAILED;
					}
					else
					{
						pOperand->SetSignal(1,"OnHideSpotReached", 0, 0, g_crcSignals.m_nOnHideSpotReached);
						return AIGOR_SUCCEED;
					}
				}
				else
				{
					if (m_pHideTarget && !m_bEndEffect)
					{
						// we are on our last approach
						if ( (pOperand->GetPos()-m_pHideTarget->GetPos()).GetLengthSquared() < 9.f)
						{
							// you are less than 3 meters from your hide target
							Vec3 vViewDir = pOperand->GetMoveDir();
							Vec3 vHideDir = (m_pHideTarget->GetPos() - pOperand->GetPos()).GetNormalized();

							if (vViewDir.Dot(vHideDir) > 0.8f) 
							{
								//	pOperand->SetSignal(1,"HIDE_END_EFFECT");
								m_bEndEffect = true;
							}
						}
					}
				}
			}
		}
	}
	return AIGOR_IN_PROGRESS;
}

void COPHide::ExecuteDry(CPipeUser *pOperand) 
{
	if (m_pTraceDirective)
		m_pTraceDirective->ExecuteTrace(pOperand, false);
}

bool COPHide::IsBadHiding(CPipeUser *pOperand)
{
	IAIObject *pTarget = pOperand->GetAttentionTarget();

	if (!pTarget)
		return false;

	IPhysicalWorld *pWorld = GetAISystem()->GetPhysicalWorld();

	Vec3 ai_pos = pOperand->GetPos();
	Vec3 target_pos = pTarget->GetPos();
	ray_hit hit;

	std::vector<IPhysicalEntity*> skipList;
	pOperand->GetPhysicsEntitiesToSkip(skipList);
	if (CAIActor* pActor = pTarget->CastToCAIActor())
		pActor->GetPhysicsEntitiesToSkip(skipList);

	/*	IPhysicalEntity	*skipList[4];
	int skipCount(1);
	skipList[0] = pOperand->GetProxy()->GetPhysics();
	if(pTarget->GetProxy() && pTarget->GetProxy()->GetPhysics())
	{
	skipList[skipCount++] = pTarget->GetProxy()->GetPhysics();
	if(pTarget->GetProxy()->GetPhysics(true))
	skipList[skipCount++] = pTarget->GetProxy()->GetPhysics(true);
	}*/

	int rayresult = 0;
	TICK_COUNTER_FUNCTION
		TICK_COUNTER_FUNCTION_BLOCK_START
		rayresult = pWorld->RayWorldIntersection(target_pos,ai_pos-target_pos,COVER_OBJECT_TYPES,HIT_COVER|HIT_SOFT_COVER,
		&hit,1, skipList.empty() ? 0 : &skipList[0], skipList.size());
	TICK_COUNTER_FUNCTION_BLOCK_END
		if (rayresult)
		{
			// check possible leaning direction
			if (Distance::Point_PointSq(hit.pt, ai_pos) < sqr(3.0f))
			{
				Vec3 dir = ai_pos - target_pos;
				float zcross =  dir.y*hit.n.x - dir.x*hit.n.y;
				if (zcross < 0)
					pOperand->SetSignal(1,"OnRightLean", 0, 0, g_crcSignals.m_nOnRightLean);
				else
					pOperand->SetSignal(1,"OnLeftLean", 0, 0, g_crcSignals.m_nOnLeftLean);
			}
			return false;
		}

		/*
		// check if it is prone-here spot
		Vec3	floorPos;
		if( GetFloorPos(floorPos, ai_pos, walkabilityFloorUpDist, walkabilityFloorDownDist, walkabilityDownRadius, AICE_ALL))
		{
		floorPos.z += .2f;
		rayresult = pWorld->RayWorldIntersection(floorPos,target_pos-floorPos,COVER_OBJECT_TYPES,HIT_COVER|HIT_SOFT_COVER,&hit,1);
		if (rayresult)
		{
		pOperand->SetSignal(1,"OnProneHideSpot");
		return false;
		}
		}
		*/
		// try lowering yourself 1 meter
		// no need to lover self - just check if the hide is as high as you are; if not - it is a lowHideSpot
		ai_pos = pOperand->GetPos();
		//	ai_pos.z-=1.f;
		TICK_COUNTER_FUNCTION_BLOCK_START
			rayresult = pWorld->RayWorldIntersection(ai_pos,target_pos-ai_pos,COVER_OBJECT_TYPES,HIT_COVER|HIT_SOFT_COVER,
			&hit,1, skipList.empty() ? 0 : &skipList[0], skipList.size());
		TICK_COUNTER_FUNCTION_BLOCK_END
			if (!rayresult)
			{
				// also switch bodypos automagically
				//		if (!pOperand->GetParameters().m_bSpecial)

				// try lowering yourself 1 meter - see if this hides me
				ai_pos.z-=1.f;
				TICK_COUNTER_FUNCTION_BLOCK_START
					rayresult = pWorld->RayWorldIntersection(ai_pos,target_pos-ai_pos,COVER_OBJECT_TYPES,HIT_COVER|HIT_SOFT_COVER,
					&hit,1, skipList.empty() ? 0 : &skipList[0], skipList.size());
				TICK_COUNTER_FUNCTION_BLOCK_END
					//		if (rayresult)
				{
					pOperand->SetSignal(1,"OnLowHideSpot", 0, 0, g_crcSignals.m_nOnLowHideSpot);
					return false;
				}
				return true;
			}
			return false;
			//	return true;
}

void COPHide::OnObjectRemoved(CAIObject *pObject)
{
	if (m_pTraceDirective)
		m_pTraceDirective->OnObjectRemoved(pObject);
	if (m_pPathFindDirective)
		m_pPathFindDirective->OnObjectRemoved(pObject);
	if (m_pHideTarget == pObject)
		m_pHideTarget = 0; // should prompt recalculation of hiding place on execute
	//if (m_pLooseAttentionTarget == pObject)
	//    m_pLooseAttentionTarget = 0; 
}

COPHide::~COPHide() 
{
	if (m_pHideTarget)
		GetAISystem()->RemoveDummyObject(m_pHideTarget);
	m_pHideTarget=0;
	//if (m_pLooseAttentionTarget)
	//    GetAISystem()->RemoveDummyObject(m_pLooseAttentionTarget);
	//  m_pLooseAttentionTarget=0;
}


void COPHide::Reset(CPipeUser *pOperand)
{ 
	if (m_pPathFindDirective)
		delete m_pPathFindDirective;
	m_pPathFindDirective = 0;

	if (m_pTraceDirective) 
		delete m_pTraceDirective;
	m_pTraceDirective = 0;

	if (pOperand)
	{
		pOperand->m_bHiding = false;
		pOperand->ClearPath("COPHide::Reset m_Path");
		if ( m_bLookAtLastOp )
		{
			pOperand->SetLooseAttentionTarget(0,m_looseAttentionId);
			m_looseAttentionId = 0;
		}
		//	pOperand->m_bLooseAttention = false;
		/* Should be only last op result 
		if (pOperand->m_pLooseAttentionTarget == m_pLooseAttentionTarget)
		{
		pOperand->m_pLooseAttentionTarget = 0;
		pOperand->m_bLooseAttention = false;
		}
		*/
	}

	if (m_pHideTarget)
		GetAISystem()->RemoveDummyObject(m_pHideTarget);
	m_pHideTarget=0;
}

void COPHide::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPHide");
	{
		objectTracker.SerializeObjectPointer(ser,"m_pHideTarget",m_pHideTarget,false);
		//objectTracker.SerializeObjectPointer(ser,"m_pLooseAttentionTarget",m_pLooseAttentionTarget,false);
		ser.Value("m_vHidePos",m_vHidePos);
		ser.Value("m_vLastPos",m_vLastPos);
		ser.Value("m_fSearchDistance",m_fSearchDistance);
		ser.Value("m_fMinDistance",m_fMinDistance);
		ser.Value("m_nEvaluationMethod",m_nEvaluationMethod);
		ser.Value("m_bLookAtHide",m_bLookAtHide);
		ser.Value("m_bLookAtLastOp",m_bLookAtLastOp);
		ser.Value("m_bAttTarget",m_bAttTarget);
		ser.Value("m_bEndEffect",m_bEndEffect);
		ser.Value("m_iActionId",m_iActionId);
		ser.Value("m_looseAttentionId",m_looseAttentionId);
		if(ser.IsWriting())
		{
			if(ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective!=NULL))
			{
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			if(ser.BeginOptionalGroup("PathFindDirective", m_pPathFindDirective!=NULL))
			{
				m_pPathFindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}		
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if(ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(true);
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			SAFE_DELETE(m_pPathFindDirective);
			if(ser.BeginOptionalGroup("PathFindDirective", true))
			{
				m_pPathFindDirective = new COPPathFind("");
				m_pPathFindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}

		ser.EndGroup();
	}
}


EGoalOpResult COPForm::Execute(CPipeUser *pOperand)
{
	pOperand->CreateFormation(m_sName.c_str());
	return AIGOR_DONE;
}

EGoalOpResult COPClear::Execute(CPipeUser *pOperand)
{
	return AIGOR_DONE;
}


//
//----------------------------------------------------------------------------------------------------------
COPStick::COPStick(float fStickDistance, float fEndAccuracy, float fDuration, int flags, int flagsAux):
/*                   bool bUseLastOp, bool bLookatLastOp, bool bContinuous, 
bool bTryShortcutNavigation, bool bForceReturnPartialPath,
bool bStopOnAnimationStart, bool bConstantSpeed):
(params.nValue & AILASTOPRES_USE) != 0, (params.nValue & AILASTOPRES_LOOKAT) != 0,
(params.nValueAux & 0x01) == 0, (params.nValueAux & 0x02) != 0, (params.nValue & AI_REQUEST_PARTIAL_PATH)!=0,
(params.nValue & AI_STOP_ON_ANIMATION_START)!=0, (params.nValue & AI_CONSTANT_SPEED)!=0);
*/
m_vLastUsedTargetPos(0,0,0),
m_fTrhDistance(1.0f),	// regenerate path if target moved for more than this
m_pStickTarget(NULL),
m_pSightTarget(NULL),
//m_pLooseAttentionTarget(0),
m_fStickDistance(fStickDistance),
m_fEndAccuracy(fEndAccuracy),
m_fDuration(fDuration),
m_bContinuous((flagsAux & 0x01) == 0),
m_bTryShortcutNavigation((flagsAux & 0x02) != 0),
m_bUseLastOpResult((flags & AILASTOPRES_USE) != 0),
m_bLookAtLastOp((flags & AILASTOPRES_LOOKAT) != 0),
m_bInitialized(false),
m_bForceReturnPartialPath((flags & AI_REQUEST_PARTIAL_PATH)!=0),
m_stopOnAnimationStart((flags & AI_STOP_ON_ANIMATION_START)!=0),
m_targetPredictionTime(0.0f),
m_pTraceDirective(NULL),
m_pPathfindDirective(NULL),
m_looseAttentionId(0),
m_bPathFound(false),
m_bSteerAroundPathTarget((flags & AI_DONT_STEER_AROUND_TARGET)==0)
{
	if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
		AILogAlways("COPStick::COPStick %p", this);
	m_smoothedTargetVel.zero();
	m_lastTargetPos.zero();
	m_safePointInterval = 1.0f;
	m_maxTeleportSpeed = 10.0f;
	m_pathLengthForTeleport = 20.0f;
	m_playerDistForTeleport = 3.0f;

	m_lastVisibleTime.SetValue(0);
	ClearTeleportData();

	if (m_bContinuous)
	{
		// Continuous stick defaults to speed adjustment, allow to make it constant.
		if ((flags & AI_CONSTANT_SPEED) !=0)
			m_bConstantSpeed = true;
		else
			m_bConstantSpeed = false;
	}
	else
	{
		// Non-continuous stick defaults to constant speed, allow to enable adjustment.
		if ((flags & AI_ADJUST_SPEED) !=0)
			m_bConstantSpeed = false;
		else
			m_bConstantSpeed = true;
	}
}

//
//----------------------------------------------------------------------------------------------------------
COPStick::~COPStick()
{
	if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
		AILogAlways("COPStick::~COPStick %p", this);
	//if (m_pLooseAttentionTarget)
	//GetAISystem()->RemoveDummyObject(m_pLooseAttentionTarget);
}


//
//----------------------------------------------------------------------------------------------------------
void COPStick::Reset(CPipeUser *pOperand)
{
	if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
		AILogAlways("COPStick::Reset %s", pOperand ? pOperand->GetName() : "");
	m_pStickTarget = NULL;
	m_pSightTarget = NULL;


	delete m_pPathfindDirective;
	m_pPathfindDirective = NULL;
	delete m_pTraceDirective;
	m_pTraceDirective = NULL;

	m_bPathFound = false;
	m_vLastUsedTargetPos.zero();	

	m_smoothedTargetVel.zero();
	m_lastTargetPos.zero();

	ClearTeleportData();

	if (pOperand)
	{
		pOperand->ClearPath("COPStick::Reset m_Path");
		if(m_bLookAtLastOp)
		{
			pOperand->SetLooseAttentionTarget(0,m_looseAttentionId);
			m_looseAttentionId = 0;
		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
void COPStick::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{

	ser.BeginGroup("COPStick");
	{
		ser.Value("m_vLastUsedTargetPos",m_vLastUsedTargetPos);
		ser.Value("m_fTrhDistance",m_fTrhDistance);
		ser.Value("m_fStickDistance",m_fStickDistance);
		ser.Value("m_fEndAccuracy",m_fEndAccuracy);
		ser.Value("m_fDuration", m_fDuration);
		ser.Value("m_bContinuous",m_bContinuous);
		ser.Value("m_bLookAtLastOp",m_bLookAtLastOp);
		ser.Value("m_bTryShortcutNavigation",m_bTryShortcutNavigation);
		ser.Value("m_bUseLastOpResult",m_bUseLastOpResult);
		ser.Value("m_targetPredictionTime", m_targetPredictionTime);
		ser.Value("m_bPathFound", m_bPathFound);
		ser.Value("m_bInitialized",m_bInitialized);
		ser.Value("m_bConstantSpeed",m_bConstantSpeed);
		ser.Value("m_teleportCurrent",m_teleportCurrent);
		ser.Value("m_teleportEnd",m_teleportEnd);
		ser.Value("m_lastTeleportTime",m_lastTeleportTime);
		ser.Value("m_lastVisibleTime",m_lastVisibleTime);
		ser.Value("m_maxTeleportSpeed",m_maxTeleportSpeed);
		ser.Value("m_pathLengthForTeleport",m_pathLengthForTeleport);
		ser.Value("m_playerDistForTeleport",m_playerDistForTeleport);
		ser.Value("m_bForceReturnPartialPath",m_bForceReturnPartialPath);
		ser.Value("m_stopOnAnimationStart", m_stopOnAnimationStart);
		ser.Value("m_lastTargetPosTime",m_lastTargetPosTime);
		ser.Value("m_lastTargetPos",m_lastTargetPos);
		ser.Value("m_smoothedTargetVel",m_smoothedTargetVel);
		ser.Value("m_looseAttentionId",m_looseAttentionId);
		ser.Value("m_bSteerAroundPathTarget",m_bSteerAroundPathTarget);

		SSafePoint::pObjectTracker = &objectTracker; ser.Value("m_stickTargetSafePoints", m_stickTargetSafePoints);
		ser.Value("m_safePointInterval",m_safePointInterval);
		objectTracker.SerializeObjectPointer(ser, "m_pStickTarget", m_pStickTarget, false);
		objectTracker.SerializeObjectPointer(ser, "m_pSightTarget", m_pSightTarget, false);

		if(ser.IsWriting())
		{
			if(ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective!=NULL))
			{
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			if(ser.BeginOptionalGroup("PathFindDirective", m_pPathfindDirective!=NULL))
			{
				m_pPathfindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}		
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if(ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(true);
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			SAFE_DELETE(m_pPathfindDirective);
			if(ser.BeginOptionalGroup("PathFindDirective", true))
			{
				m_pPathfindDirective = new COPPathFind("");
				m_pPathfindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
	}
	ser.EndGroup();
}

//
//----------------------------------------------------------------------------------------------------------
void COPStick::OnObjectRemoved(CAIObject *pObject)
{
	if (m_pTraceDirective)
		m_pTraceDirective->OnObjectRemoved(pObject);
	if (m_pPathfindDirective)
		m_pPathfindDirective->OnObjectRemoved(pObject);
	if (pObject == m_pStickTarget || pObject == m_pSightTarget)
	{
		if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
			AILogAlways("COPStick::OnObjectRemoved (%p) resetting due stick/sight target removed", this);
		Reset(NULL);
	}
}

//===================================================================
// GetEndDistance
//===================================================================
float COPStick::GetEndDistance(CPipeUser *pOperand) const
{
	if (m_fDuration > 0.0f)
	{
		float normalSpeed, minSpeed, maxSpeed;
		pOperand->GetMovementSpeedRange(pOperand->m_State.fMovementUrgency, false, normalSpeed, minSpeed, maxSpeed);
		if (normalSpeed > 0.0f)
			return -normalSpeed * m_fDuration;
	}
	return m_fStickDistance;
}

//
//----------------------------------------------------------------------------------------------------------
void COPStick::RegeneratePath(CPipeUser *pOperand, const Vec3 &destination)
{
	if (!pOperand)
		return;

	if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
		AILogAlways("COPStick::RegeneratePath %s", pOperand ? pOperand->GetName() : "");
	m_pPathfindDirective->Reset(pOperand);
	m_pTraceDirective->m_fEndAccuracy = m_fEndAccuracy;
	m_vLastUsedTargetPos = destination;
	pOperand->m_nPathDecision = PATHFINDER_STILLFINDING;

	const Vec3 opPos = pOperand->GetPhysicsPos();

	// Check for direct connection first
	if (m_bTryShortcutNavigation)
	{
		int nbid;
		IVisArea *iva;
		IAISystem::ENavigationType navType = GetAISystem()->CheckNavigationType(opPos, nbid, iva, pOperand->m_movementAbility.pathfindingProperties.navCapMask);
		CNavRegion *pRegion = GetAISystem()->GetNavRegion(navType, GetAISystem()->GetGraph());
		if (pRegion)
		{
			Vec3	from = opPos;
			Vec3	to = destination;

			NavigationBlockers	navBlocker;
			if(pRegion->CheckPassability(from, to, pOperand->GetParameters().m_fPassRadius, navBlocker))
			{
				pOperand->ClearPath("COPStick::RegeneratePath m_Path");

				if (navType == IAISystem::NAV_TRIANGULAR)
				{
					// Make sure not to enter forbidden area.
					if (GetAISystem()->IsPathForbidden(opPos, destination))
						return;
					if (GetAISystem()->IsPointForbidden(destination, pOperand->GetParameters().m_fPassRadius))
						return;
				}

				PathPointDescriptor	pt;
				pt.navType = navType;

				pt.vPos = from;
				pOperand->m_Path.PushBack(pt);

				float endDistance = GetEndDistance(pOperand);
				if (fabsf(endDistance) > 0.0001f)
				{
					// Handle end distance.
					float dist;
					if (pOperand->IsUsing3DNavigation())
						dist = Distance::Point_Point(from, to);
					else
						dist = Distance::Point_Point2D(from, to);

					float d;
					if (endDistance > 0.0f)
						d = dist - endDistance;
					else
						d = -endDistance;
					float u = clamp(d / dist, 0.0001f, 1.0f);

					pt.vPos = from + u * (to - from);

					pOperand->m_Path.PushBack(pt);
				}
				else
				{
					pt.vPos = to;
					pOperand->m_Path.PushBack(pt);
				}

				pOperand->m_Path.SetParams(SNavPathParams(from, to, Vec3(ZERO), Vec3(ZERO), -1, false, endDistance, true));

				pOperand->m_OrigPath = pOperand->m_Path;
				pOperand->m_nPathDecision = PATHFINDER_PATHFOUND;
			}
		}
	}
}

//===================================================================
// DebugDraw
//===================================================================
void COPStick::DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const
{
	ColorF col;
	unsigned nPts = m_stickTargetSafePoints.Size();
	unsigned iPt = 0;
	TStickTargetSafePoints::SConstIterator itEnd = m_stickTargetSafePoints.End();
	for (TStickTargetSafePoints::SConstIterator it = m_stickTargetSafePoints.Begin() ; it != itEnd ; ++it, ++iPt)
	{
		const SSafePoint &safePoint = *it;
		float frac = ((float) iPt ) / nPts;
		if (safePoint.safe)
			col.Set(0.0f, frac, 1.0f - frac);
		else
			col.Set(1.0f, 0.0f, 0.0f);
		pRenderer->GetIRenderAuxGeom()->DrawSphere(safePoint.pos, 0.2f, col);
	}
	if(m_pPathfindDirective)
		m_pPathfindDirective->DebugDraw(pOperand, pRenderer);
	if(m_pTraceDirective)
		m_pTraceDirective->DebugDraw(pOperand, pRenderer);
}

//===================================================================
// UpdateStickTargetSafePositions
//===================================================================
void COPStick::UpdateStickTargetSafePoints(CPipeUser *pOperand)
{
	Vec3 curPos;
	CLeader* pLeader = GetAISystem()->GetLeader(pOperand->GetGroupId());
	CAIObject *pOwner = pLeader ? pLeader->GetFormationOwner() : 0;
	if (pOwner)
		curPos = pOwner->GetPhysicsPos();
	else
		curPos = m_pStickTarget->GetPhysicsPos();

	Vec3 opPos = pOperand->GetPhysicsPos();
	if (GetAISystem()->WouldHumanBeVisible(opPos, false))
		m_lastVisibleTime = GetAISystem()->GetFrameStartTime();

	unsigned lastNavNodeIndex = m_pStickTarget->m_lastNavNodeIndex;
	if (!m_stickTargetSafePoints.Empty())
	{
		Vec3 delta = curPos - m_stickTargetSafePoints.Front().pos;
		float dist = delta.GetLength();
		if (dist < m_safePointInterval)
			return;
		lastNavNodeIndex = m_stickTargetSafePoints.Front().nodeIndex;
	}

	unsigned int curNodeIndex = GetAISystem()->GetGraph()->GetEnclosing(
		curPos, pOperand->m_movementAbility.pathfindingProperties.navCapMask,
		pOperand->m_Parameters.m_fPassRadius, lastNavNodeIndex, 0.0f, 0, false, pOperand->GetName());
	const GraphNode *pCurNode = GetAISystem()->GetGraph()->GetNode(curNodeIndex);

	if (pCurNode && pCurNode->navType == IAISystem::NAV_TRIANGULAR && pCurNode->GetTriangularNavData()->isForbidden)
	{
		pCurNode = 0;
		curNodeIndex = 0;
	}

	int maxNumPoints = 1 + (int)(m_pathLengthForTeleport / m_safePointInterval);
	if (m_stickTargetSafePoints.Size() >= maxNumPoints || m_stickTargetSafePoints.Full())
		m_stickTargetSafePoints.PopBack();

	// incur copy cost by adding to the start, but after the initial hit, no allocation costs. 
	// Also, this way allows us to easily remove points from the end of the list. 
	// This list should contain 
	bool safe = pCurNode != 0;

	Vec3 floorPos;
	if (safe && !GetFloorPos(floorPos, curPos, 0.0f, 3.0f, 0.1f, AICE_STATIC))
		safe = false;

	m_stickTargetSafePoints.PushFront(SSafePoint(curPos, curNodeIndex, safe, GetAISystem()->GetFrameStartTime()));
}

//===================================================================
// ClearTeleportData
//===================================================================
void COPStick::ClearTeleportData()
{
	m_teleportCurrent.zero();
	m_teleportEnd.zero();
}

//===================================================================
// TryToTeleport
// This works by first detecting if we need to teleport. If we do, then
// we identify and store the teleport destination location. However, we
// don't move pOperand there immediately - we continue following our path. But we
// move a "ghost" from the initial position to the destination. If it
// reaches the destination without either the ghost or the real pOperand
// being seen by the player then pOperand (may) get teleported. If teleporting
// happens then m_stickTargetSafePoints needs to be updated.
//===================================================================
bool COPStick::TryToTeleport(CPipeUser * pOperand)
{
	if (m_stickTargetSafePoints.Size() < 2)
	{
		ClearTeleportData();
		return false;
	}

	if (pOperand->m_nPathDecision != PATHFINDER_NOPATH)
	{
		float curPathLen = pOperand->m_Path.GetPathLength(false);
		Vec3 stickPos = m_pStickTarget->GetPhysicsPos();
		if (pOperand->m_Path.Empty())
			curPathLen += Distance::Point_Point(pOperand->GetPhysicsPos(), stickPos);
		else
			curPathLen += Distance::Point_Point(pOperand->m_Path.GetLastPathPos(), stickPos);
		if (curPathLen < m_pathLengthForTeleport)
		{
			ClearTeleportData();
			return false;
		}
		else
		{
			// Danny/Luc todo issue some readability by sending a signal - the response to that signal 
			// should ideally stop the path following 
		}
	}

	if (m_lastVisibleTime == GetAISystem()->GetFrameStartTime())
	{
		ClearTeleportData();
		return false;
	}

	if (m_teleportEnd.IsZero() && !m_stickTargetSafePoints.Empty())
	{
		const Vec3 playerPos = GetAISystem()->GetPlayer()->GetPhysicsPos();
		unsigned nPts = m_stickTargetSafePoints.Size();

		int index = 0;
		TStickTargetSafePoints::SIterator itEnd = m_stickTargetSafePoints.End();
		for (TStickTargetSafePoints::SIterator it = m_stickTargetSafePoints.Begin() ; it != itEnd ; ++it, ++index)
		{
			const SSafePoint &sp = *it;
			if (!sp.safe)
				return false;

			float playerDistSq = Distance::Point_PointSq(sp.pos, playerPos);
			if (playerDistSq < square(m_playerDistForTeleport))
				continue;

			TStickTargetSafePoints::SIterator itNext = it;
			++itNext;
			if (itNext == itEnd)
			{
				m_teleportEnd = sp.pos;
				break;
			}
			else if (!itNext->safe)
			{
				m_teleportEnd = sp.pos;
				break;
			}
		}
	}

	if (m_teleportEnd.IsZero())
		return false;

	Vec3 curPos = pOperand->GetPhysicsPos();
	if (m_teleportCurrent.IsZero())
	{
		m_teleportCurrent = curPos;
		m_lastTeleportTime = GetAISystem()->GetFrameStartTime();

		// If player hasn't seen operand for X seconds then move along by that amount
		Vec3 moveDir = m_teleportEnd - m_teleportCurrent;
		float distToEnd = moveDir.NormalizeSafe();

		float dt = (GetAISystem()->GetFrameStartTime() - m_lastVisibleTime).GetSeconds();
		float dist = dt * m_maxTeleportSpeed;
		if (dist > distToEnd)
			dist = distToEnd;
		m_teleportCurrent += dist * moveDir;

		return false;
	}

	// move the ghost
	Vec3 moveDir = m_teleportEnd - m_teleportCurrent;
	float distToEnd = moveDir.NormalizeSafe();

	CTimeValue thisTime = GetAISystem()->GetFrameStartTime();
	float dt = (thisTime - m_lastTeleportTime).GetSeconds();
	m_lastTeleportTime = thisTime;

	float dist = dt * m_maxTeleportSpeed;
	bool reachedEnd = false;
	if (dist > distToEnd)
	{
		reachedEnd = true;
		dist = distToEnd;
	}
	m_teleportCurrent += dist * moveDir;

	if (GetAISystem()->WouldHumanBeVisible(m_teleportCurrent, false))
	{
		ClearTeleportData();
		return false;
	}

	if (reachedEnd)
	{
		AILogEvent("COPStick::TryToTeleport teleporting %s to (%5.2f, %5.2f, %5.2f)", 
			pOperand->GetName(), m_teleportEnd.x, m_teleportEnd.y, m_teleportEnd.z);
		Vec3 floorPos = m_teleportEnd;
		GetFloorPos(floorPos, m_teleportEnd, 0.0f, 3.0f, 0.1f, AICE_ALL);
		pOperand->GetEntity()->SetPos(floorPos );
		pOperand->m_State.fDesiredSpeed = 0;
		pOperand->m_State.vMoveDir.zero();
		pOperand->ClearPath("Teleport");
		RegeneratePath(pOperand, m_pStickTarget->GetPos());

		unsigned nPts = m_stickTargetSafePoints.Size();
		unsigned nBack = 10;
		unsigned iPt = 0;
		TStickTargetSafePoints::SIterator itEnd = m_stickTargetSafePoints.End();
		for (TStickTargetSafePoints::SIterator it = m_stickTargetSafePoints.Begin() ; it != itEnd ; ++it, ++iPt)
		{
			SSafePoint &sp =  *it;
			if (iPt == nPts-1 || sp.pos == m_teleportEnd)
			{
				iPt -= min(iPt, nBack);
				TStickTargetSafePoints::SIterator itFirst = m_stickTargetSafePoints.Begin();
				itFirst += iPt;
				m_stickTargetSafePoints.Erase(itFirst, m_stickTargetSafePoints.End());
				break;
			}
		}
		ClearTeleportData();
		return true;
	}

	return false;
}

//
//----------------------------------------------------------------------------------------------------------
EGoalOpResult COPStick::Execute(CPipeUser *pOperand)
{
	CAISystem *pSystem = GetAISystem();

	// Do not mind the target direction when approaching.
	pOperand->m_nMovementPurpose = 1;

	CPuppet *pPuppet = pOperand->CastToCPuppet();

	const Vec3 opPos = pOperand->GetPhysicsPos();

	if(!m_pStickTarget)// first time = lets stick to target
	{
		m_pStickTarget = (CAIObject*)pOperand->GetAttentionTarget();

		if( !m_pStickTarget || m_bUseLastOpResult )
		{
			if (pOperand->m_pLastOpResult)
				m_pStickTarget = pOperand->m_pLastOpResult;
			else
			{
				if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
					AILogAlways("COPStick::Execute resetting due to no stick target %s", 
					pOperand ? pOperand->GetName() : "");
				// no target, nothing to approach to
				Reset(pOperand);
				return AIGOR_FAILED;
			}
		}

		// keep last op. result as sight target
		if( !m_pSightTarget && m_bLookAtLastOp && pOperand->m_pLastOpResult )
			m_pSightTarget = pOperand->m_pLastOpResult;

		if(m_pStickTarget->GetSubType() == CAIObject::STP_ANIM_TARGET && m_fStickDistance > 0.0f)
		{
			AILogAlways("COPStick::Execute resetting stick distance from %.1f to zero because the stick target is anim target. %s",
				m_fStickDistance, pOperand ? pOperand->GetName() : "_no_operand_");
			m_fStickDistance = 0.0f;
		}

		// Create pathfinder operation
		Vec3 stickPos = m_pStickTarget->GetPhysicsPos();
		if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
			AILogAlways("COPStick::Execute (%p) Creating pathfind/trace directives to (%5.2f, %5.2f, %5.2f) %s", this,
			stickPos.x, stickPos.y, stickPos.z, 
			pOperand ? pOperand->GetName() : "");
		float endTol = m_bForceReturnPartialPath || m_fEndAccuracy < 0.0f ? std::numeric_limits<float>::max() : m_fEndAccuracy;
		// override end distance if a duration has been set
		float endDistance = GetEndDistance(pOperand);
		m_pPathfindDirective = new COPPathFind("", m_pStickTarget, endTol, endDistance);
		bool exactFollow = !m_pSightTarget && !pOperand->m_bLooseAttention;
		m_pTraceDirective = new COPTrace(exactFollow, m_fEndAccuracy, m_bForceReturnPartialPath, m_stopOnAnimationStart ); 
		if(m_pTraceDirective)
			m_pTraceDirective->SetSteerAroundPathTarget(m_bSteerAroundPathTarget);
		RegeneratePath(pOperand, stickPos);

		// TODO: This is hack to prevent the Alien Scouts not to use the speed control.
		// Use better test to use the speed control _only_ when it is really needed (human formations).
		if (pPuppet && !pOperand->m_movementAbility.b3DMove && !m_bInitialized)
		{
			pPuppet->ResetSpeedControl();
			m_bInitialized = true;
		}

		if (m_pStickTarget == pOperand)
		{
			AILogAlways("COPStick::Execute sticking to self %s ", pOperand ? pOperand->GetName() : "_no_operand_");
			Reset(pOperand);
			return AIGOR_SUCCEED;
		}
	}

	// Special case for formation points, do not stick to disabled points.
	if (m_pStickTarget && m_pStickTarget->GetSubType() == IAIObject::STP_FORMATION)
	{
		if (!m_pStickTarget->IsEnabled())
		{
			// Wait until the formation becomes active again.
			pOperand->m_State.vMoveDir.Set(0,0,0);
			return AIGOR_IN_PROGRESS;
		}
	}


	// luciano - added check for formation points
	/*
	if (m_pStickTarget && !m_pStickTarget->IsEnabled())
	{
	if (!m_bContinuous)
	{
	if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
	AILogAlways("COPStick::Execute (%p) resetting due to disabled stick target and non-continuous %s", this, 
	pOperand ? pOperand->GetName() : "");
	Reset(pOperand);
	return AIGOR_SUCCEED;
	}
	else
	return AIGOR_IN_PROGRESS;
	}
	*/
	if (!m_bContinuous && pOperand->m_nPathDecision == PATHFINDER_NOPATH)
	{
		if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
			AILogAlways("COPStick::Execute (%p) resetting due to non-continuous and no path %s", this, 
			pOperand ? pOperand->GetName() : "");
		Reset(pOperand);
		return AIGOR_FAILED;
	}

	//make sure the guy looks in correct direction
	if (m_bLookAtLastOp && m_pSightTarget)
	{
		m_looseAttentionId = pOperand->SetLooseAttentionTarget(m_pSightTarget);
	}

	// trace gets deleted when we reach the end and it's not continuous
	if (!m_pTraceDirective)
	{
		if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
			AILogAlways("COPStick::Execute (%p) returning true due to no trace directive %s", this, 
			pOperand ? pOperand->GetName() : "");
		Reset(pOperand);
		return AIGOR_FAILED;
	}

	bool b3D = pOperand->IsUsing3DNavigation();

	// actually trace the path - continue doing this even whilst regenerating (etc) the path
	EGoalOpResult doneTracing = AIGOR_IN_PROGRESS;
	if (m_bPathFound)
	{
		// if using AdjustSpeed then force sprint at this point - will get overridden later
		if (!m_bConstantSpeed && pPuppet && (!m_pTraceDirective || m_pTraceDirective->m_Maneuver == COPTrace::eMV_None) &&
			pPuppet->GetType() == AIOBJECT_PUPPET && !b3D)
			pPuppet->m_State.fMovementUrgency = AISPEED_SPRINT;

		doneTracing = m_pTraceDirective->Execute(pOperand);

		if (pPuppet && !m_bConstantSpeed && (!m_pTraceDirective || m_pTraceDirective->m_Maneuver == COPTrace::eMV_None))
			pPuppet->AdjustSpeed(m_pStickTarget,m_fStickDistance);

		// If the path has been traced, finish the operation if the operand is not sticking continuously.
		if (doneTracing != AIGOR_IN_PROGRESS && !m_bContinuous)
		{
			if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
				AILogAlways("COPStick::Execute (%p) finishing due to non-continuous and finished tracing %s", this, 
				pOperand ? pOperand->GetName() : "");
			Reset(pOperand);
			return doneTracing;
		}

		if (doneTracing != AIGOR_IN_PROGRESS)
			m_pPathfindDirective->m_bWaitingForResult = false;

		// m_pStickTarget might be set to NULL as result of passing thru a navigation smart objects
		// on the path by executing an AI action which inserts a goal pipe which calls Reset() for
		// all active goals including this one... in this case it would be fine if we just end this
		// Execute() cycle and wait for the next one
		if (!m_pStickTarget)
			return AIGOR_IN_PROGRESS;
	}

	// We should never get asserted here! If this assert hits, then the m_pStickTarget was changed during trace,
	// which should never happen!
	AIAssert(m_pStickTarget );

	Vec3 targetPos = m_pStickTarget->GetPhysicsPos();

	if (m_maxTeleportSpeed > 0.0f && pOperand->m_movementAbility.teleportEnabled)
	{
		UpdateStickTargetSafePoints(pOperand);
		if (TryToTeleport(pOperand))
		{
			// teleport has happened
		}
	}

	switch(pOperand->GetType())
	{
		// Danny disabled this prediction for squadmates - it makes them tend to overshoot the end of their paths
		//  case AIOBJECT_PUPPET:
		//    m_targetPredictionTime = pOperand->m_pLastNavNode && pOperand->m_pLastNavNode->navType == IAISystem::NAV_TRIANGULAR ? 1.0f : 0.0f;
		//    break;
	case AIOBJECT_VEHICLE:
		m_targetPredictionTime = 2.0f;
		break;
	default:
		m_targetPredictionTime = 0.0f;
		break;
	}

	if (m_targetPredictionTime > 0.0f)
	{
		CTimeValue curTime = GetAISystem()->GetFrameStartTime();
		if (m_lastTargetPos.IsZero())
		{
			m_lastTargetPos = targetPos;
			m_lastTargetPosTime = curTime;
			m_smoothedTargetVel.zero();
		}
		else
		{
			float dt = (curTime - m_lastTargetPosTime).GetSeconds();
			Vec3 targetVel(ZERO);
			if (dt > 0.0f)
			{
				targetVel = (targetPos - m_lastTargetPos);
				if (targetVel.GetLengthSquared() > 5.0f) // try to catch sudden jumps
					targetVel.zero();
				else
					targetVel /= dt;
				// Danny todo make this time timestep independent (exp dependency on dt)
				float frac = 0.1f;
				m_smoothedTargetVel = frac * targetVel + (1.0f - frac) * m_smoothedTargetVel;
				m_lastTargetPos = targetPos;
				m_lastTargetPosTime = curTime;
			}
		}
	}
	else
	{
		m_smoothedTargetVel.zero();
		m_lastTargetPos = targetPos;
	}
	Vec3 targetOffset = m_smoothedTargetVel * m_targetPredictionTime;

	if (pOperand->m_lastNavNodeIndex && GetAISystem()->GetGraph()->GetNode(pOperand->m_lastNavNodeIndex)->navType == IAISystem::NAV_TRIANGULAR)
	{
		// ensure offset doesn't cross forbidden
		Vec3 newPt;
		if (GetAISystem()->IntersectsForbidden(targetPos, targetPos+targetOffset, newPt))
		{
			float prevDist = targetOffset.GetLength2D();
			float newDist = Distance::Point_Point2D(targetPos, newPt);
			if (newDist > prevDist && newDist > 0.0f)
				newPt = targetPos + (prevDist / newDist) * (newPt - targetPos);
			targetOffset = newPt - targetPos;
		}
	}
	m_pPathfindDirective->SetTargetOffset(targetOffset);
	targetPos += targetOffset;

	Vec3 targetVector;
	if(pOperand->m_nPathDecision == PATHFINDER_PATHFOUND && m_bForceReturnPartialPath)
		targetVector = (pOperand->m_Path.GetLastPathPos() - opPos);
	else
		targetVector = (targetPos - opPos);

	if(!b3D)
		targetVector.z = 0.0f;
	float curDist = targetVector.GetLength();

	if (pOperand->m_State.vMoveDir.IsZero(.05f) && pOperand->GetAttentionTarget() && pOperand->GetAttentionTarget() != m_pStickTarget)
		pOperand->m_bLooseAttention = false;

	// check if need to regenerate path
	float targetMoveDist = b3D ? (m_vLastUsedTargetPos - targetPos).GetLength() : (m_vLastUsedTargetPos - targetPos).GetLength2D();

	// If the target is moving approximately to the same direction, do not update the path so often.
	bool	targetDirty = false;
	if (!pOperand->m_Path.Empty())
	{
		// Use the stored destination point instead of the last path node since the path may be cut because of navSO.
		Vec3 pathEnd = pOperand->m_PathDestinationPos;
		Vec3	dir(pathEnd - opPos);
		if(!b3D)
			dir.z = 0;
		dir.NormalizeSafe();

		Vec3	dirToTarget(targetPos - pathEnd);
		if(!b3D)
			dirToTarget.z = 0;
		dirToTarget.NormalizeSafe();

		float	regenDist = m_fTrhDistance;
		if( dirToTarget.Dot( dir ) < cosf(DEG2RAD(8.0f)) )
			regenDist *= 5.0f;

		if( targetMoveDist > regenDist )
			targetDirty = true;

		// when near the path end force more frequent updates
		float pathDistLeft = pOperand->m_Path.GetPathLength(false);
		float pathEndError = b3D ? (pathEnd - targetPos).GetLength() : (pathEnd - targetPos).GetLength2D();

		// TODO/HACK! [Mikko] This prevent the path to regenerated every frame in some special cases in Crysis Core level
		// where quite a few behaviors are sticking to a long distance (7-10m).
		// The whole stick regeneration logic is flawed mostly because pOperand->m_PathDestinationPos is not always
		// the actual target position. The pathfinder may adjust the end location and not keep the requested end pos
		// if the target is not reachable. I'm sure there are other really nasty cases about this path invalidation logic too.
		const GraphNode* pLastNode = GetAISystem()->GetGraph()->GetNode(pOperand->m_lastNavNodeIndex);
		if (pLastNode && pLastNode->navType == IAISystem::NAV_VOLUME)
			pathEndError = max(0.0f, pathEndError - GetEndDistance(pOperand));

		if (pathEndError > 0.1f && pathDistLeft < 2.0f * pathEndError)
		{
			targetDirty = true;
		}

	}
	else if( targetMoveDist > m_fTrhDistance )
	{
		targetDirty = true;
	}

	// If the target pos moves substantially update
	// If we're not already close to the target but it's moved a bit then update.
	// Be careful about forcing an update too often - especially if we're nearly there.
	if (targetDirty && m_pTraceDirective->m_Maneuver == COPTrace::eMV_None && !m_pTraceDirective->m_passingStraightNavSO &&
		(pOperand->m_State.curActorTargetPhase == eATP_None || pOperand->m_State.curActorTargetPhase == eATP_Error) &&
		!pOperand->m_Path.GetParams().precalculatedPath && !pOperand->m_Path.GetParams().inhibitPathRegeneration)
	{
		if (pOperand->m_nPathDecision != PATHFINDER_STILLFINDING)
			RegeneratePath(pOperand, targetPos);
	}

	// check pathfinder status
	switch(pOperand->m_nPathDecision)
	{
	case PATHFINDER_STILLFINDING:
		{
			IVisArea *pGoalArea; 
			int leaderBuilding = -1;
			int nBuilding;
			CAIActor* pStickActor = m_pStickTarget->CastToCAIActor();
			bool targetIndoor = GetAISystem()->CheckNavigationType(m_pStickTarget->GetPhysicsPos(),nBuilding,pGoalArea,
				pStickActor ? pStickActor->m_movementAbility.pathfindingProperties.navCapMask & IAISystem::NAV_WAYPOINT_HUMAN : IAISystem::NAV_WAYPOINT_HUMAN) 
				== IAISystem::NAV_WAYPOINT_HUMAN;
			m_pPathfindDirective->SetForceTargetBuildingId(targetIndoor ? leaderBuilding : -1);
			m_pPathfindDirective->Execute(pOperand);		
			m_pPathfindDirective->SetForceTargetBuildingId(-1);
			return AIGOR_IN_PROGRESS;
		}

	case PATHFINDER_NOPATH:
		pOperand->m_State.vMoveDir.Set(0,0,0);
		if (!m_bContinuous)
		{
			if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
				AILogAlways("COPStick::Execute (%p) resetting due to no path %s", this, 
				pOperand ? pOperand->GetName() : "");
			Reset(pOperand);
			return AIGOR_FAILED;
		}
		else 
		{
			return AIGOR_IN_PROGRESS;
		}

	case PATHFINDER_PATHFOUND:
		// do not spam the AI with the OnPathFound signal (stick goal regenerates the path frequently)
		if (!m_bPathFound)
		{
			m_bPathFound = true;
			TPathPoints::const_reference lastPathNode = pOperand->m_OrigPath.GetPath().back();
			Vec3 lastPos = lastPathNode.vPos;
			Vec3 requestedLastNodePos = pOperand->m_Path.GetParams().end;
			// send signal to AI
			float dist = b3D ? Distance::Point_Point(lastPos,requestedLastNodePos) : Distance::Point_Point2D(lastPos,requestedLastNodePos);
			if (lastPathNode.navType != IAISystem::NAV_SMARTOBJECT && dist > m_fStickDistance+C_MaxDistanceForPathOffset)
			{
				AISignalExtraData* pData = new AISignalExtraData;
				pData->fValue = dist - m_fStickDistance;
				pOperand->SetSignal(0,"OnEndPathOffset",pOperand->GetEntity(),pData ,g_crcSignals.m_nOnEndPathOffset);
			}
			else 
				pOperand->SetSignal(0,"OnPathFound",NULL, 0, g_crcSignals.m_nOnPathFound);

			return Execute(pOperand);
		}
	}

	return AIGOR_IN_PROGRESS;
}

//===================================================================
// ExecuteDry
// Note - it is very important we don't call Reset on ourself from here
// else we might restart ourself subsequently
//===================================================================
void COPStick::ExecuteDry(CPipeUser *pOperand) 
{
	if (m_pTraceDirective && m_pStickTarget)
	{
		Vec3 targetVector;
		if(pOperand->m_nPathDecision == PATHFINDER_PATHFOUND && m_bForceReturnPartialPath)
			targetVector = (pOperand->m_Path.GetLastPathPos() - pOperand->GetPhysicsPos());
		else
			targetVector = (m_pStickTarget->GetPhysicsPos() - pOperand->GetPhysicsPos());

		if(!pOperand->m_movementAbility.b3DMove)
			targetVector.z = 0.0f;
		float curDist = targetVector.GetLength();

		CPuppet *pPuppet = pOperand->CastToCPuppet();

		if (!m_bConstantSpeed && pPuppet && (!m_pTraceDirective || m_pTraceDirective->m_Maneuver == COPTrace::eMV_None) &&
			pPuppet->GetType() == AIOBJECT_PUPPET && !pPuppet->IsUsing3DNavigation())
			pPuppet->m_State.fMovementUrgency = AISPEED_SPRINT;

		if(m_bPathFound)
			m_pTraceDirective->ExecuteTrace(pOperand, false);

		if (pPuppet && !m_bConstantSpeed && (!m_pTraceDirective || m_pTraceDirective->m_Maneuver == COPTrace::eMV_None))
			pPuppet->AdjustSpeed(m_pStickTarget,m_fStickDistance);
	}
}


//
//----------------------------------------------------------------------------------------------------------
COPContinuous::COPContinuous(bool bKeeoMoving):
m_bKeepMoving(bKeeoMoving)
{

}

//
//----------------------------------------------------------------------------------------------------------
COPContinuous::~COPContinuous()
{

}

//
//----------------------------------------------------------------------------------------------------------
void COPContinuous::Reset(CPipeUser *pOperand)
{
}

//
//----------------------------------------------------------------------------------------------------------
EGoalOpResult COPContinuous::Execute(CPipeUser *pOperand)
{
	pOperand->m_bKeepMoving = m_bKeepMoving;
	return AIGOR_DONE;
}


//
//----------------------------------------------------------------------------------------------------------
COPMove::COPMove(float distance, float speed, bool useLastOp, bool lookatLastOp, bool continuous) :
m_moveTarget( 0 ),
m_sightTarget( 0 ),
//m_pLooseAttentionTarget( 0 ),
m_distance( distance ),
m_speed( speed ),
m_continuous( continuous ),
m_useLastOpResult( useLastOp ),
m_lookAtLastOp( lookatLastOp ),
m_initialized( false ),
m_stuckTime(0),
m_allStuckCounter(0),
m_looseAttentionId(0)
{
	for (unsigned i = 0; i < 5; i++ )
		m_debug[i].Set(0,0,0);
}

//
//----------------------------------------------------------------------------------------------------------
COPMove::~COPMove()
{
	/*if (m_pLooseAttentionTarget)
	GetAISystem()->RemoveDummyObject(m_pLooseAttentionTarget);
	m_pLooseAttentionTarget=0;
	*/
}

//
//----------------------------------------------------------------------------------------------------------
EGoalOpResult COPMove::Execute(CPipeUser *pOperand)
{
	CAISystem *pSystem = GetAISystem();
	float timeStep = GetAISystem()->GetFrameDeltaTime();

	// Do not mind the target direction when approaching.
	pOperand->m_nMovementPurpose = 1;

	CPuppet *pPuppet = pOperand->CastToCPuppet();

	if(!m_moveTarget)// first time = lets stick to target
	{
		//		AILogComment( "COPStick(): dist:%d, lastop:%s cont:%s, targetoffset:%f", m_fStickDistance, m_bLookAtLastOp ? "yes" : "no", m_bContinuous ? "yes" : "no", m_fStickTargetOffset );

		m_moveTarget = (CAIObject*)pOperand->GetAttentionTarget();

		if( !m_moveTarget || m_useLastOpResult )
		{
			if (pOperand->m_pLastOpResult)
				m_moveTarget = pOperand->m_pLastOpResult;
			else
			{
				// no target, nothing to approach to
				Reset(pOperand);
				return AIGOR_FAILED;
			}
		}

		// keep last op. result as sight target
		if( !m_sightTarget && m_lookAtLastOp && pOperand->m_pLastOpResult )
			m_sightTarget = pOperand->m_pLastOpResult;

		// TODO: This is hack to prevent the Alien Scouts not to use the speed control.
		// Use better test to use the speed control _only_ when it is really needed (human formations).
		if (pPuppet && !pOperand->m_movementAbility.b3DMove && !m_initialized)
		{
			pPuppet->ResetSpeedControl();
			m_initialized = true;
		}

	}

	//make sure the guy looks in correct direction
	if (m_lookAtLastOp && m_sightTarget)
	{
		/*if (0 == m_pLooseAttentionTarget)
		m_pLooseAttentionTarget = pSystem->CreateDummyObject(string(pOperand->GetName()) + "_OPMoveLooseAttTarget");
		pOperand->m_pLooseAttentionTarget = m_pLooseAttentionTarget;
		pOperand->m_pLooseAttentionTarget->SetPos(m_sightTarget->GetPos());
		pOperand->m_bLooseAttention = true;
		*/
		//TO DO luc: now it always look at last op; is this ok?
		m_looseAttentionId= pOperand->SetLooseAttentionTarget(m_sightTarget);
	}

	if(m_moveTarget && !m_moveTarget->IsEnabled())
	{
		if(!m_continuous)
		{
			Reset(pOperand);
			return AIGOR_FAILED;
		}
		else
			return AIGOR_IN_PROGRESS;
	}

	Vec3 targetVector = (m_moveTarget->GetPos() - pOperand->GetPos());
	if(!pOperand->IsUsing3DNavigation())
		targetVector.z = 0.0f;
	float curDist = targetVector.GetLength();

	//see if we have to stop sticking
	if(!m_continuous)
	{
		if(curDist <= m_distance)
		{
			Reset(pOperand);
			return AIGOR_SUCCEED;
		}
	}

	ExecuteDry(pOperand);

	// Check if got stuck while charging, and cancel the attack.
	if(m_allStuckCounter > 3 || m_stuckTime > 0.6f)
	{
		Reset(pOperand);
		return AIGOR_FAILED;
	}

	return AIGOR_IN_PROGRESS;
}

//
//----------------------------------------------------------------------------------------------------------
void COPMove::ExecuteDry(CPipeUser *pOperand)
{
	// Move towards the current target.
	if (!m_moveTarget)
		return;
	bool b3d = pOperand->IsUsing3DNavigation();
	const Vec3&	opPos = pOperand->GetPos();
	Vec3	targetPos = m_moveTarget->GetPos();
	Vec3	delta = targetPos - opPos;
	if(!b3d)
		delta.z = 0;
	float	dist = delta.GetLength();

	//	if( m_speed < 0 )
	//		delta = -delta;
	float normalSpeed, minSpeed, maxSpeed;
	pOperand->GetMovementSpeedRange(pOperand->m_State.fMovementUrgency, true, normalSpeed, minSpeed, maxSpeed);
	//  float normalSpeed = pOperand->GetNormalMovementSpeed(pOperand->m_State.fMovementUrgency, true);

	if( dist > m_distance )
	{
		Vec3	dir	= delta.GetNormalizedSafe();
		Vec3	checkDelta(dir * pOperand->m_Parameters.m_fPassRadius);

		m_debug[0] = opPos;
		if(b3d)
		{
			ray_hit hit;
			IPhysicalWorld*		pWorld = GetAISystem()->GetPhysicalWorld();
			IPhysicalEntity*	pPhysEntity[2];
			int n = 0;
			if(pOperand->GetProxy())
			{
				// Get both the collision proxy and the animation proxy
				pPhysEntity[n] = pOperand->GetProxy()->GetPhysics(false);
				if(pPhysEntity[n]) n++;
				pPhysEntity[n] = pOperand->GetProxy()->GetPhysics(true);
				if(pPhysEntity[n]) n++;
			}

			// Use 4 antenna to steer around obstacles.

			Matrix33	dirMat;
			dirMat.SetRotationVDir(dir);

			Vec3	antenna[4];
			const float	angle = DEG2RAD(40.0f);
			const	float	r = pOperand->m_Parameters.m_fPassRadius * 4.0f;
			float	sn = sinf(angle) * r;
			float	cs = cosf(angle) * r;

			antenna[0].Set(sn, cs, 0);
			antenna[1].Set(-sn, cs, 0);
			antenna[2].Set(0, cs, sn);
			antenna[3].Set(0, cs, -sn);

			Vec3	steerDir(dir);
			Vec3	physPos = pOperand->GetPhysicsPos();

			// Steer away from the hit surface.
			int	nhits = 0;
			for (unsigned i = 0; i < 4; i++)
			{
				Vec3	ant = dirMat * antenna[i];

				m_debug[i+1].Set(0,0,0);

				int rayresult;
				TICK_COUNTER_FUNCTION
					TICK_COUNTER_FUNCTION_BLOCK_START
					rayresult	= pWorld->RayWorldIntersection(physPos, ant, ent_terrain|ent_static|ent_living, geom_colltype_player<<rwi_colltype_bit, &hit, 1, pPhysEntity, n);
				TICK_COUNTER_FUNCTION_BLOCK_END
					if(!rayresult)
					{
						steerDir += ant;
						m_debug[i+1] = ant;
					}
					else
						nhits++;
			}

			// Try to figure out if the alien is stuck.
			// If most of the antennae are hit, increase a counter and after a while, signal failure in the main handler.
			float	dt = GetAISystem()->GetFrameDeltaTime();
			if(nhits > 2)
			{
				m_stuckTime += dt;
			}
			else
			{
				m_stuckTime -= dt;
				if(m_stuckTime < 0.0f) m_stuckTime = 0.0f;
			}

			if(nhits == 4)
				m_allStuckCounter++;

			if (!steerDir.IsZero())
				steerDir.Normalize();
			dir = steerDir;
		}

		pOperand->m_State.fDesiredSpeed = m_speed * normalSpeed; //0.05f + speed * 0.95f;
		pOperand->m_State.vMoveDir = dir;
		//		pOperand->m_State.bExactPositioning = false;
		pOperand->m_State.fDistanceToPathEnd = 0; //dist;

		pOperand->m_DEBUGmovementReason = CPipeUser::AIMORE_MOVE;
	}
	else
	{
		pOperand->m_State.fDistanceToPathEnd = 0;
		pOperand->m_State.fDesiredSpeed = normalSpeed;
		pOperand->m_State.vMoveDir.Set(0,0,0);
	}
}

//
//----------------------------------------------------------------------------------------------------------
void COPMove::DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const
{
	const ColorB	col(255,255,255);
	for (unsigned i = 0; i < 4; i++ )
	{
		pRenderer->GetIRenderAuxGeom()->DrawLine( m_debug[0], col, m_debug[0] + m_debug[i + 1], col );
	}
}

//
//----------------------------------------------------------------------------------------------------------
void COPMove::Reset(CPipeUser *pOperand)
{
	m_allStuckCounter = 0;
	m_stuckTime = 0;
	m_moveTarget = NULL;
	m_sightTarget = NULL;
	if (pOperand)
	{
		/*if (pOperand->m_pLooseAttentionTarget == m_pLooseAttentionTarget)
		{
		pOperand->m_pLooseAttentionTarget = 0;
		pOperand->m_bLooseAttention = false;
		}*/
		if(m_lookAtLastOp)
		{
			pOperand->SetLooseAttentionTarget(0,m_looseAttentionId);
			m_looseAttentionId = 0;
		}
	}
	/*if (m_pLooseAttentionTarget)
	{
	GetAISystem()->RemoveDummyObject(m_pLooseAttentionTarget);
	m_pLooseAttentionTarget = 0;
	}*/
}

//
//----------------------------------------------------------------------------------------------------------
void COPMove::OnObjectRemoved(CAIObject *pObject)
{
	//if (pObject == m_pLooseAttentionTarget)
	//    m_pLooseAttentionTarget = 0;
	if (pObject == m_moveTarget || pObject == m_sightTarget)
		Reset(NULL);
}


//
//----------------------------------------------------------------------------------------------------------
void COPMove::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPMove");
	{
		objectTracker.SerializeObjectPointer(ser,"m_moveTarget",m_moveTarget,false);
		//objectTracker.SerializeObjectPointer(ser,"m_pLooseAttentionTarget",m_pLooseAttentionTarget,false);
		objectTracker.SerializeObjectPointer(ser,"m_sightTarget",m_sightTarget,false);

		ser.Value("m_distance",m_distance);
		ser.Value("m_speed",m_speed);
		ser.Value("m_continuous",m_continuous);
		ser.Value("m_useLastOpResult",m_useLastOpResult);
		ser.Value("m_lookAtLastOp",m_lookAtLastOp);
		ser.Value("m_initialized",m_initialized);
		ser.Value("m_stuckTime",m_stuckTime);
		ser.Value("m_allStuckCounter",m_allStuckCounter);
		ser.Value("m_looseAttentionId",m_looseAttentionId);
		ser.EndGroup();
	}
}

//
//----------------------------------------------------------------------------------------------------------
COPCharge::COPCharge(float distanceFront, float distanceBack, bool useLastOp, bool lookatLastOp) :
m_moveTarget(0),
m_sightTarget(0),
//m_pLooseAttentionTarget(0),
m_distanceFront(distanceFront),
m_distanceBack(distanceBack),
m_useLastOpResult(useLastOp),
m_lookAtLastOp(lookatLastOp),
m_initialized(false),
m_state(STATE_APPROACH),
m_pOperand(0),
m_stuckTime(0),
m_debugHitRes(false),
m_bailOut(false),
m_looseAttentionId(0),
m_lastOpPos(0,0,0)
{
	for (unsigned i = 0; i < 5; i++ )
		m_debugAntenna[i].Set(0,0,0);
	m_debugRange[0].Set(0,0,0);
	m_debugRange[1].Set(0,0,0);
	m_debugHit[0].Set(0,0,0);
	m_debugHit[1].Set(0,0,0);
}

//
//----------------------------------------------------------------------------------------------------------
COPCharge::~COPCharge()
{
	//if (m_pLooseAttentionTarget)
	// GetAISystem()->RemoveDummyObject(m_pLooseAttentionTarget);
	//m_pLooseAttentionTarget=0;
}

//
//----------------------------------------------------------------------------------------------------------
EGoalOpResult COPCharge::Execute(CPipeUser *pOperand)
{
	m_pOperand = pOperand;

	CAISystem *pSystem = GetAISystem();
	float timeStep = GetAISystem()->GetFrameDeltaTime();

	// Do not mind the target direction when approaching.
	m_pOperand->m_nMovementPurpose = 1;

	CPuppet *pPuppet = pOperand->CastToCPuppet();

	if (!m_moveTarget)// first time = lets stick to target
	{
		m_moveTarget = (CAIObject*)m_pOperand->GetAttentionTarget();

		if( !m_moveTarget || m_useLastOpResult )
		{
			if (m_pOperand->m_pLastOpResult)
				m_moveTarget = m_pOperand->m_pLastOpResult;
			else
			{
				// no target, nothing to approach to
				Reset(m_pOperand);
				return AIGOR_FAILED;
			}
		}

		// keep last op. result as sight target
		if( !m_sightTarget && m_lookAtLastOp && m_pOperand->m_pLastOpResult )
			m_sightTarget = m_pOperand->m_pLastOpResult;

		// TODO: This is hack to prevent the Alien Scouts not to use the speed control.
		// Use better test to use the speed control _only_ when it is really needed (human formations).
		if (pPuppet && !m_pOperand->m_movementAbility.b3DMove && !m_initialized)
		{
			pPuppet->ResetSpeedControl();
			m_initialized = true;
		}

		// Start approach animation.
		if (m_pOperand->GetProxy())
			m_pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "meleeMoveBender");

		m_approachStartTime = GetAISystem()->GetFrameStartTime();
	}

	//make sure the guy looks in correct direction
	if (m_lookAtLastOp && m_sightTarget)
	{
		//if (0 == m_pLooseAttentionTarget)
		//m_pLooseAttentionTarget = pSystem->CreateDummyObject(string(pOperand->GetName()) + "_OPMoveLooseAttTarget");
		//pOperand->m_pLooseAttentionTarget = m_pLooseAttentionTarget;
		//m_pOperand->m_pLooseAttentionTarget->SetPos(m_sightTarget->GetPos());
		//m_pOperand->m_bLooseAttention = true;
		m_looseAttentionId= pOperand->SetLooseAttentionTarget(m_sightTarget);
	}

	if(m_moveTarget && !m_moveTarget->IsEnabled())
	{
		Reset(m_pOperand);
		return AIGOR_FAILED;
	}

	ExecuteDry(m_pOperand);

	if(m_state == STATE_APPROACH)
	{
		// Make sure we do not sped too much time in the approach
		CTimeValue currentTime = GetAISystem()->GetFrameStartTime();
		float elapsed = (currentTime - m_approachStartTime).GetSeconds();
		if(elapsed > 7.0f)
		{
			Reset(m_pOperand);
			return AIGOR_FAILED;
		}
	}

	if (m_state == STATE_CHARGE || m_state == STATE_FOLLOW_TROUGH)
	{
		// If the entity is past the plane at the end of the charge range, we're done!
		Vec3	normal = m_chargeEnd - m_chargeStart;
		float	d = normal.NormalizeSafe();
		float d2 = normal.Dot(m_pOperand->GetPos() - m_chargeStart);
		if (d2 > (d - m_pOperand->m_Parameters.m_fPassRadius))
		{
			Reset(m_pOperand);
			return AIGOR_SUCCEED;
		}
	}

	// Check if got stuck while charging, and cancel the attack.
	if(m_stuckTime > 0.3f)
	{
		Reset(m_pOperand);
		return AIGOR_FAILED;
	}

	return AIGOR_IN_PROGRESS;
}

void COPCharge::ValidateRange()
{
	AIAssert(m_moveTarget);
	if(!m_pOperand)
		return;
	ray_hit						hit;
	IPhysicalWorld*		pWorld = GetAISystem()->GetPhysicalWorld();

	Vec3	delta = m_chargeEnd - m_chargeStart;

	Vec3 hitPos;
	float hitDist;

	if (IntersectSweptSphere(&hitPos, hitDist, Lineseg(m_chargeStart, m_chargeEnd), m_pOperand->m_Parameters.m_fPassRadius*1.1f, AICE_ALL))
	{
		m_chargeEnd = m_chargeStart + (m_chargeEnd - m_chargeStart).GetNormalizedSafe() * max(0.0f, hitDist-0.3f);
	}

	/*	int rayresult;
	TICK_COUNTER_FUNCTION
	TICK_COUNTER_FUNCTION_BLOCK_START
	rayresult = pWorld->RayWorldIntersection(m_chargeStart, delta,
	ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid,
	(geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any,
	&hit, 1);
	TICK_COUNTER_FUNCTION_BLOCK_END
	if(rayresult)
	{
	m_chargeEnd = m_chargeStart + (hit.dist - m_pOperand->m_Parameters.m_fPassRadius * 1.2f) * delta.GetNormalizedSafe(); 
	//		m_chargeEnd = hit.pt;
	}*/
}

void COPCharge::SetChargeParams()
{
	if(!m_pOperand)
		return;
	const Vec3 physPos = m_pOperand->GetPhysicsPos();
	// Set charge based on the current charge pos.
	Vec3	dirToChargePos = m_chargePos - physPos;
	m_chargeStart = physPos;
	m_chargeEnd = physPos + dirToChargePos * ((m_distanceFront + m_distanceBack) / dirToChargePos.GetLength());
	ValidateRange();
}

void COPCharge::UpdateChargePos()
{
	if(!m_pOperand)
		return;
	/*
	Vec3 dirToTarget = (m_moveTarget->GetPos() - pOperand->GetPos());
	if (!pOperand->m_movementAbility.b3DMove)
	dirToTarget.z = 0.0f;
	float curDist = dirToTarget.GetLength();
	const Vec3&	forw = pOperand->GetMoveDir();
	dirToTarget.Normalize();
	//		m_chargePos = (pOperand->GetPos() + forw * curDist) * 0.25f + m_moveTarget->GetPos() * 0.75f;
	*/

	m_chargePos = m_moveTarget->GetPos();

	m_debugRange[0] = m_moveTarget->GetPos();
	m_debugRange[1] = m_chargePos;
}

//
//----------------------------------------------------------------------------------------------------------
void COPCharge::ExecuteDry(CPipeUser *pOperand)
{
	m_pOperand = pOperand;

	// Move towards the current target.
	if (!m_moveTarget)
		return;

	float	maxMoveSpeed = 1.0f;
	const Vec3&	opPos = m_pOperand->GetPos();

	// Do not move during the anticipate time
	if (m_state == STATE_ANTICIPATE)
	{
		// Keep updating the jump direction.
		//		UpdateChargePos(pOperand);
		//		SetChargeParams(pOperand->GetPos());

		maxMoveSpeed = 0.01f;
	}
	else if (m_state == STATE_APPROACH)
	{
		UpdateChargePos();

		Vec3 dirToTarget = (m_moveTarget->GetPos() - opPos);
		if (!pOperand->m_movementAbility.b3DMove)
			dirToTarget.z = 0.0f;
		dirToTarget.Normalize();

		m_chargeStart = m_chargePos - dirToTarget * m_distanceFront;
		m_chargeEnd = m_chargePos + dirToTarget * m_distanceBack;

		ValidateRange();

		maxMoveSpeed = 0.6f;
	}

	Vec3	targetPos;

	if (m_state == STATE_APPROACH)
		targetPos	= m_moveTarget->GetPos();
	else
		targetPos	= m_chargeEnd;

	Vec3	delta = targetPos - opPos;
	float	dist = delta.len();

	if (m_state == STATE_APPROACH)
	{
		Vec3 dirToTarget = (m_moveTarget->GetPos() - opPos);
		if (!m_pOperand->m_movementAbility.b3DMove)
			dirToTarget.z = 0.0f;
		float curDist = dirToTarget.GetLength();
		if (curDist > 0.0)
			dirToTarget /= curDist;

		// validate the charge range
		ValidateRange();

		// Check if the charge should be executed or if the charge fails.
		float	t;
		float	distToLine = Distance::Point_Lineseg(opPos, Lineseg(m_chargeStart, m_chargeEnd), t);

		//		else if (curDist <= m_distanceFront && Distance::Point_Point(m_chargePos, m_moveTarget->GetPos()) < m_distanceFront * 0.3f)
		if (t > 0.0f && distToLine < m_distanceFront * 0.3f)
		{
			// Succeed -> Charge sequence.
			SetChargeParams();
			m_state = STATE_CHARGE; //STATE_ANTICIPATE;
			m_anticipateTime = GetAISystem()->GetFrameStartTime();
			m_pOperand->SetSignal(0, "OnChargeStart", pOperand->GetEntity(), 0, g_crcSignals.m_nOnChargeStart);

			// Start jump approach animation.
			if (m_pOperand->GetProxy())
				m_pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "meleeAttackBender");
		}
		else if (t > 0.2f) // && distToLine < m_distanceFront * 0.75f)
		{
			// Fail -> charge anyway.
			SetChargeParams();
			m_state = STATE_CHARGE; //STATE_ANTICIPATE;
			m_anticipateTime = GetAISystem()->GetFrameStartTime();
			m_pOperand->SetSignal(0,"OnChargeStart",pOperand->GetEntity(), 0, g_crcSignals.m_nOnChargeStart);

			// Start jump approach animation.
			if (m_pOperand->GetProxy())
				m_pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "meleeAttackBender");
		}
	}
	else if (m_state == STATE_ANTICIPATE)
	{
		CTimeValue currentTime = GetAISystem()->GetFrameStartTime();
		float elapsed = (currentTime - m_anticipateTime).GetSeconds();

		if (elapsed > 0.3f)
			m_state = STATE_CHARGE;
	}
	else if (m_state == STATE_CHARGE || m_state == STATE_FOLLOW_TROUGH)
	{
		Lineseg	charge(m_chargeStart, m_chargeEnd);
		float	tOp = 0;
		float	tTarget = 0;
		Distance::Point_LinesegSq(opPos, charge, tOp);
		Distance::Point_LinesegSq(m_moveTarget->GetPos(), charge, tTarget);

		if (m_state == STATE_CHARGE)
		{
			CAIActor* pActor = m_moveTarget->CastToCAIActor();
			const float	rad = pOperand->m_Parameters.m_fPassRadius + (pActor ? pActor->m_Parameters.m_fPassRadius : 0.f);
			if (Distance::Point_Point(opPos, m_moveTarget->GetPos()) < rad * 1.5f)
			{
				// Send the target that was hit along with the signal.
				AISignalExtraData* pData = new AISignalExtraData;
				pData->nID = m_moveTarget->GetEntityID();
				m_pOperand->SetSignal(0, "OnChargeHit", m_pOperand->GetEntity(), pData, g_crcSignals.m_nOnChargeHit);

				m_state = STATE_FOLLOW_TROUGH;
			}
			else if (tOp > tTarget)
			{
				if(!m_bailOut)
				{
					m_pOperand->SetSignal(0, "OnChargeMiss", m_pOperand->GetEntity(), 0, g_crcSignals.m_nOnChargeMiss);
					m_state = STATE_FOLLOW_TROUGH;

					if (m_pOperand->GetProxy())
					{
						m_pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
						m_pOperand->GetProxy()->SetAGInput(AIAG_SIGNAL, "meleeRollOutBender");
					}
					m_bailOut = true;
				}
			}
		}
		else if (m_state == STATE_FOLLOW_TROUGH)
		{
			if (tOp > 0.5f)
			{
				// Start jump approach animation.
				//				SetAGInput("Persistent", "none");
				//				SetAGInput("Signal", "meleeRollOutBender");

				if(!m_bailOut)
				{
					m_pOperand->SetSignal(0, "OnChargeBailOut", m_pOperand->GetEntity(), 0, g_crcSignals.m_nOnChargeBailOut);
					m_bailOut = true;
				}
			}
		}
	}



	//	if( dist > m_distance )
	{
		Vec3	checkDelta( delta.GetNormalizedSafe() );
		checkDelta *= m_pOperand->m_Parameters.m_fPassRadius;

		Vec3	dir	= delta.GetNormalizedSafe();

		/*		{
		ray_hit hit;
		IPhysicalWorld*		pWorld = GetAISystem()->GetPhysicalWorld();

		Matrix33	dirMat;
		dirMat.SetRotationVDir(dir, gf_PI/4);

		Vec3	antenna[4];
		Vec3	steerOffset[4];
		const float	outer = DEG2RAD(45.0f);
		const float	inner = DEG2RAD(10.0f);
		const	float	r = 1.0f * 4.0f;

		const float osn = sinf(outer) * r;
		const float ocs = cosf(outer) * r;

		antenna[0].Set(osn, ocs, 0);	steerOffset[0].Set(1, 0, 0);
		antenna[1].Set(-osn, ocs, 0);	steerOffset[1].Set(-1, 0, 0);
		antenna[2].Set(0, ocs, osn);	steerOffset[2].Set(0, 0, 1);
		antenna[3].Set(0, ocs, -osn);	steerOffset[3].Set(0, 0, -1);

		Vec3	newSteer(dir);
		Vec3	lastPass(dir);

		Vec3	physPos = m_pOperand->GetPhysicsPos();

		m_debugAntenna[0] = physPos;

		// Steer away from the hit surface.
		int	nhits = 0;
		for (unsigned i = 0; i < 4; i++)
		{
		Vec3	ant = dirMat * antenna[i];
		Vec3	off = dirMat * steerOffset[i];

		m_debugAntenna[i+1].Set(0,0,0);

		if(!pWorld->RayWorldIntersection(physPos, ant, ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid, (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any, &hit, 1))
		{
		m_debugAntenna[i+1] = ant;
		lastPass = off;
		newSteer += off;
		}
		else
		{
		float	a = hit.dist / r;
		newSteer += off * a;
		nhits++;
		}
		}

		// Test center
		if(pWorld->RayWorldIntersection(physPos, dir * r, ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid, (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any, &hit, 1))
		{
		float	a = 1.0f - (hit.dist / r);
		newSteer += lastPass * a * 4.0f;
		}
		newSteer.NormalizeSafe();

		dir = newSteer;

		// Try to figure out if the alien is stuck.
		// If most of the antennae are hit, increase a counter and after a while, signal failure in the main handler.
		float	dt = GetAISystem()->GetFrameDeltaTime();

		Vec3	hitPos;
		float	hitDist;
		m_debugHit[0] = physPos;
		m_debugHit[1] = physPos + dir * 0.9f;
		m_debugHitRes = false;
		m_debugHitRad = 0.3f;
		if(IntersectSweptSphere(&hitPos, hitDist, Lineseg(physPos, physPos + dir * 0.9f), 0.3f, AICE_ALL))
		{
		m_stuckTime += dt;
		m_debugHitRes = true;
		}
		}*/

		float normalSpeed, minSpeed, maxSpeed;
		pOperand->GetMovementSpeedRange(pOperand->m_State.fMovementUrgency, pOperand->m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

		//    float normalSpeed = pOperand->GetNormalMovementSpeed(pOperand->m_State.fMovementUrgency, true);

		m_pOperand->m_State.fDesiredSpeed = normalSpeed * maxMoveSpeed; //0.05f + speed * 0.95f;
		m_pOperand->m_State.vMoveDir = dir;
		//		pOperand->m_State.bExactPositioning = false;
		m_pOperand->m_State.fDistanceToPathEnd = 0; //dist;

		m_pOperand->m_DEBUGmovementReason = CPipeUser::AIMORE_MOVE;
	}


	if (m_state == STATE_APPROACH || m_state == STATE_CHARGE || m_state == STATE_FOLLOW_TROUGH)
	{
		Vec3 deltaMove = pOperand->GetPhysicsPos() - m_lastOpPos;
		float speed = deltaMove.GetLength();
		if (GetAISystem()->GetFrameDeltaTime() > 0.0f)
			speed /= GetAISystem()->GetFrameDeltaTime();
	}

	m_lastOpPos = pOperand->GetPhysicsPos();

	/*	else
	{
	pOperand->m_State.fDistanceToPathEnd = 0;
	pOperand->m_State.fDesiredSpeedMod = 1.0;
	pOperand->m_State.vTargetPos = pOperand->GetPos();
	pOperand->m_State.vMoveDir.Set(0,0,0);
	}*/
}

//
//----------------------------------------------------------------------------------------------------------
void COPCharge::DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const
{
	// Draw antennae
	const ColorB	col(255,255,255);
	for (unsigned i = 0; i < 4; i++ )
	{
		pRenderer->GetIRenderAuxGeom()->DrawLine( m_debugAntenna[0], col, m_debugAntenna[0] + m_debugAntenna[i + 1], col );
	}

	pRenderer->GetIRenderAuxGeom()->DrawLine(m_debugRange[0], col, m_debugRange[1], col );

	// Draw range
	ColorB	col2(255,0,0);
	ColorB	col3(255,255,0);

	if (m_state == STATE_ANTICIPATE)
	{
		col2.Set(64,0,0);
		col3.Set(64,64,0);
	}

	pRenderer->GetIRenderAuxGeom()->DrawLine(m_chargeStart, col2, m_chargeEnd, col3 );

	if (m_state == STATE_APPROACH)
	{
		pRenderer->GetIRenderAuxGeom()->DrawSphere(m_chargeStart, 0.2f, col2);
		pRenderer->GetIRenderAuxGeom()->DrawSphere(m_chargeEnd, 0.2f, col3);


		Lineseg	charge(m_chargeStart, m_chargeEnd);
		float	t;
		float	distToLineSq = Distance::Point_LinesegSq(pOperand->GetPos(), charge, t);

		pRenderer->GetIRenderAuxGeom()->DrawSphere(charge.GetPoint(0.2f), 0.1f, col2);

		if (t > 0.0f && t < 1.0f)
		{
			pRenderer->GetIRenderAuxGeom()->DrawSphere(charge.GetPoint(t), 0.1f, col);
			pRenderer->GetIRenderAuxGeom()->DrawLine(pOperand->GetPos(), col, charge.GetPoint(t), col );
		}
	}

	/*	if(m_stuckTime > 0.6f)
	{
	pRenderer->GetIRenderAuxGeom()->DrawSphere(m_pOperand->GetPos(), 2.5f, ColorB(255, 0, 0));
	}*/

	ColorB	hitCol(0,0,0,128);
	if(m_debugHitRes)
		hitCol.Set(255,0,0,128);
	pRenderer->GetIRenderAuxGeom()->DrawSphere(m_debugHit[0], m_debugHitRad, hitCol);
	pRenderer->GetIRenderAuxGeom()->DrawSphere((m_debugHit[0] + m_debugHit[1])/2, m_debugHitRad, hitCol);
	pRenderer->GetIRenderAuxGeom()->DrawSphere(m_debugHit[1], m_debugHitRad, hitCol);
}

//
//----------------------------------------------------------------------------------------------------------
void COPCharge::Reset(CPipeUser *pOperand)
{
	m_moveTarget = NULL;
	m_sightTarget = NULL;
	m_state = STATE_APPROACH;
	m_stuckTime = 0;
	m_bailOut = false;
	m_lastOpPos.Set(0,0,0);

	if (pOperand)
	{
		if (pOperand->GetProxy())
			pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);

		/*if (pOperand->m_pLooseAttentionTarget == m_pLooseAttentionTarget)
		{
		pOperand->m_pLooseAttentionTarget = 0;
		pOperand->m_bLooseAttention = false;
		}*/
		if(m_lookAtLastOp && m_sightTarget)
		{
			pOperand->SetLooseAttentionTarget(0,m_looseAttentionId);
			m_looseAttentionId = 0;
		}
	}
	/*if (m_pLooseAttentionTarget)
	{
	GetAISystem()->RemoveDummyObject(m_pLooseAttentionTarget);
	m_pLooseAttentionTarget = 0;
	}*/
}

//
//----------------------------------------------------------------------------------------------------------
void COPCharge::OnObjectRemoved(CAIObject *pObject)
{
	//if (pObject == m_pLooseAttentionTarget)
	//m_pLooseAttentionTarget = 0;
	if (pObject == m_moveTarget || pObject == m_sightTarget)
		Reset(m_pOperand);
}


COPWaitSignal::COPWaitSignal( const char* sSignal, float fInterval /*= 0*/ )
{
	m_edMode = edNone;
	m_sSignal = sSignal;
	m_fInterval = fInterval;
	Reset( NULL );
}

COPWaitSignal::COPWaitSignal( const char* sSignal, const char* sObjectName, float fInterval /*= 0*/ )
{
	m_edMode = edString;
	m_sObjectName = sObjectName;

	m_sSignal = sSignal;
	m_fInterval = fInterval;
	Reset( NULL );
}

COPWaitSignal::COPWaitSignal( const char* sSignal, int iValue, float fInterval /*= 0*/ )
{
	m_edMode = edInt;
	m_iValue = iValue;

	m_sSignal = sSignal;
	m_fInterval = fInterval;
	Reset( NULL );
}

COPWaitSignal::COPWaitSignal( const char* sSignal, EntityId nID, float fInterval /*= 0*/ )
{
	m_edMode = edId;
	m_nID = nID;

	m_sSignal = sSignal;
	m_fInterval = fInterval;
	Reset( NULL );
}

bool COPWaitSignal::NotifySignalReceived( CAIObject* pOperand, const char* szText, IAISignalExtraData* pData )
{
	if ( !m_bSignalReceived && m_sSignal == szText )
	{
		if ( m_edMode == edNone )
			m_bSignalReceived = true;
		else if ( pData )
		{
			if ( m_edMode == edString && m_sObjectName == pData->GetObjectName() )
				m_bSignalReceived = true;
			else if ( m_edMode == edInt && m_iValue == pData->iValue )
				m_bSignalReceived = true;
			else if ( m_edMode == edId && m_nID == pData->nID.n )
				m_bSignalReceived = true;
		}
	}
	return m_bSignalReceived;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPWaitSignal::Reset( CPipeUser* pOperand )
{
	if ( pOperand )
		pOperand->m_listWaitGoalOps.remove( this );
	else
	{
		// only reset this on initialize!
		// later it may happen that this is called from signal handler (by inserting a goal pipe)
		m_bSignalReceived = false;
	}
	m_startTime.SetSeconds(0.0f);
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPWaitSignal::Execute(CPipeUser *pOperand)
{
	if (m_bSignalReceived)
	{
		m_bSignalReceived = false;
		return AIGOR_DONE;
	}

	if ( m_fInterval <= 0.0f )
	{
		if ( !m_startTime.GetValue() )
		{
			pOperand->m_listWaitGoalOps.push_front( this );
			m_startTime.SetMilliSeconds(1);
		}
		return AIGOR_IN_PROGRESS;
	}

	CTimeValue time = GetAISystem()->GetFrameStartTime();
	if ( !m_startTime.GetValue() )
	{
		m_startTime = time;
		pOperand->m_listWaitGoalOps.push_front( this );
	}

	CTimeValue timeElapsed = time - m_startTime;
	if ( timeElapsed.GetSeconds() > m_fInterval )
		return AIGOR_DONE;

	return AIGOR_IN_PROGRESS;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPWaitSignal::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	// please ask me when you want to change [tetsuji]
	ser.BeginGroup("COPWaitSignal");
	{
		ser.Value("m_bSignalReceived",m_bSignalReceived);
		if (ser.IsReading())
		{
			m_startTime.SetSeconds(0.0f);
		}
		ser.EndGroup();
	}
	// please ask me when you want to change [dejan]
}



//----------------------------------------------------------------------------------------------------------
// COPAnimAction
//----------------------------------------------------------------------------------------------------------
COPAnimation::COPAnimation( EAnimationMode mode, const char* value )
: m_eMode( mode )
, m_sValue( value )
, m_bAGInputSet( false )
{
}

//
//-------------------------------------------------------------------------------------------------------------
void COPAnimation::Reset(CPipeUser* pOperand)
{
	m_bAGInputSet = false;
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPAnimation::Execute( CPipeUser* pOperand )
{
	AIAssert(pOperand);
	if (!pOperand->GetProxy())
		return AIGOR_FAILED;

	if (m_bAGInputSet)
	{
		switch( m_eMode )
		{
		case AIANIM_SIGNAL:
			return pOperand->GetProxy()->IsSignalAnimationPlayed(m_sValue) ? AIGOR_SUCCEED : AIGOR_IN_PROGRESS;
		case AIANIM_ACTION:
			return pOperand->GetProxy()->IsActionAnimationStarted(m_sValue) ? AIGOR_SUCCEED : AIGOR_IN_PROGRESS;
		}
		AIAssert( !"Setting an invalid AG input has returned true!" );
		return AIGOR_FAILED;
	}

	// set the value here
	if (!pOperand->GetProxy()->SetAGInput(m_eMode == AIANIM_ACTION ? AIAG_ACTION : AIAG_SIGNAL, m_sValue))
	{
		AIWarning("Can't set value '%s' on AG input '%s' of PipeUser '%s'", m_sValue.c_str(), m_eMode == AIANIM_ACTION ? "Action" : "Signal", pOperand->GetName());
		return AIGOR_FAILED;
	}
	m_bAGInputSet = true;

	return AIGOR_IN_PROGRESS;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPAnimation::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPAnimation");
	{
		ser.EnumValue("m_eMode",m_eMode,AIANIM_SIGNAL,AIANIM_ACTION);
		ser.Value("m_sValue",m_sValue);
		ser.Value("m_bAGInputSet",m_bAGInputSet);
		ser.EndGroup();
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void COPAnimTarget::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPAnimTarget");
	{
		ser.Value("m_bSignal",m_bSignal);
		ser.Value("m_sAnimName",m_sAnimName);
		ser.Value("m_fStartRadius",m_fStartRadius);
		ser.Value("m_fDirectionTolerance",m_fDirectionTolerance);
		ser.Value("m_fTargetRadius",m_fTargetRadius);
		ser.EndGroup();
	}
}


//----------------------------------------------------------------------------------------------------------
// COPAnimTarget
//----------------------------------------------------------------------------------------------------------
COPAnimTarget::COPAnimTarget( bool signal, const char* animName, float startRadius, float dirTolerance, float targetRadius, const Vec3& approachPos )
: m_bSignal( signal )
, m_sAnimName( animName )
, m_fStartRadius( startRadius )
, m_fDirectionTolerance( dirTolerance )
, m_fTargetRadius( targetRadius )
, m_vApproachPos( approachPos )
{
}

EGoalOpResult COPAnimTarget::Execute( CPipeUser* pOperand )
{
	IAIObject* pTarget = pOperand->GetLastOpResult();
	if ( pTarget )
	{
		SAIActorTargetRequest	req;
		if ( !m_vApproachPos.IsZero() )
		{
			req.approachLocation = m_vApproachPos;
			req.approachDirection = pTarget->GetPos() - m_vApproachPos;
			req.approachDirection.NormalizeSafe(Vec3(0,1,0));
		}
		else
		{
			req.approachLocation = pTarget->GetPos();
			req.approachDirection = pTarget->GetMoveDir();
		}
		req.animLocation = pTarget->GetPos();
		req.animDirection = pTarget->GetMoveDir();
		if ( pOperand->IsUsing3DNavigation() == false )
		{
			req.animDirection.z = 0.0f;
			req.animDirection.NormalizeSafe();
		}
		req.directionRadius = DEG2RAD(m_fDirectionTolerance);
		req.locationRadius = m_fTargetRadius;
		req.startRadius = m_fStartRadius;
		req.signalAnimation = m_bSignal;
		req.animation = m_sAnimName;

		pOperand->SetActorTargetRequest(req);
	}
	return AIGOR_DONE;
}


//
//-------------------------------------------------------------------------------------------------------------
COPUseCover::COPUseCover(bool unHideNow, bool useLastOpAsBackup, float intervalMin, float intervalMax, bool speedUpTimeOutWhenNoTarget):
m_fTimeOut(0),
m_bUnHide(unHideNow),
m_useLastOpAsBackup(useLastOpAsBackup), 
m_speedUpTimeOutWhenNoTarget(speedUpTimeOutWhenNoTarget),
m_curCoverUsage(0),
m_weaponOffset(0.3f),
m_coverCompromised(false),
m_coverCompromisedSignalled(false),
m_targetReached(false),
//	m_leftInvalidTime(0.0f),
//	m_rightInvalidTime(0.0f),
m_leftInvalidDist(1000.0f),
m_rightInvalidDist(-1000.0f),
m_DEBUG_checkValid(false),
m_leftEdgeCheck(true),
m_rightEdgeCheck(true),
m_maxPathLen(0)
{
	m_intervalMin = intervalMin;
	if(intervalMax > 0 )
		m_intervalMax = intervalMax;
	else
		m_intervalMax = m_intervalMin;
	m_startTime.SetSeconds(0.0f);

	m_ucs = m_ucsLast = UCS_NONE;
	m_ucsStartTime.SetSeconds(0.0f);
	m_leftInvalidStartTime.SetSeconds(0.0f);
	m_rightInvalidStartTime.SetSeconds(0.0f);
	m_curCoverUsage = USECOVER_NONE;
	//	m_ucsTime = 0.0f;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPUseCover::Reset(CPipeUser *pOperand)
{
	if(pOperand && !pOperand->m_CurrentHideObject.IsSmartObject())
	{
		pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
		pOperand->m_State.lean = 0.0f;
	}

	if(pOperand && pOperand->m_CurrentHideObject.IsSmartObject())
	{
		if ( m_fTimeOut >= 0.0f )
			return;
	}

	m_startTime.SetSeconds(0.0f);
	m_curCoverUsage = USECOVER_NONE;
	m_coverCompromised = false;
	m_coverCompromisedSignalled = false;
	m_targetReached = false;
	m_leftInvalidStartTime.SetSeconds(0.0f);
	m_rightInvalidStartTime.SetSeconds(0.0f);
	m_leftInvalidDist = 1000.0f;
	m_rightInvalidDist = -1000.0f;
	m_leftEdgeCheck = true;
	m_rightEdgeCheck = true;
	m_DEBUG_checkValid = false;
	m_fTimeOut = 0;
	m_ucs = m_ucsLast = UCS_NONE;
	m_ucsStartTime.SetSeconds(0.0f);
	m_startTime.SetSeconds(0.0f);
	m_maxPathLen = 0;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPUseCover::RandomizeTimeOut(CPipeUser *pOperand)
{
	m_fTimeOut = m_intervalMin + (m_intervalMax - m_intervalMin) * ((float)ai_rand() / (float)RAND_MAX);
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPUseCover::Execute(CPipeUser *pOperand)
{
	if (!pOperand->m_CurrentHideObject.IsValid())
	{
		m_fTimeOut = -1.0f;
		Reset(pOperand);
		return AIGOR_FAILED;
	}

	if (m_fTimeOut < 0.0001f)
	{
		RandomizeTimeOut(pOperand);
		m_startTime = GetAISystem()->GetFrameStartTime();
	}

	if (m_targetReached)
	{
		//		m_fTimeOut -= dt;
		//		if(m_fTimeOut < 0.0f)
		float	elapsed = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds();
		if (elapsed > m_fTimeOut)
		{
			Reset(pOperand);
			return AIGOR_SUCCEED;
		}
	}

	if (pOperand->m_CurrentHideObject.IsSmartObject())
		return UseCoverSO(pOperand);
	else
		return UseCoverEnv(pOperand);
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPUseCover::UseCoverSO(CPipeUser *pOperand)
{
	if (!m_targetReached)
	{
		// on first call of Execute just return false
		// this will prevent calling Execute twice in
		// only one Update in the beginning of goalop
		m_targetReached = true;
		return AIGOR_IN_PROGRESS;
	}
	if (m_bUnHide)
		return UseCoverSOUnHide(pOperand);
	else
		return UseCoverSOHide(pOperand);
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPUseCover::UseCoverSOUnHide(CPipeUser *pOperand)
{
	//	if(!pOperand->m_CurrentHideObject.IsUsingCover())
	//		return false;

	IEntity* pUser = pOperand->GetEntity();
	IEntity* pObject = pOperand->m_CurrentHideObject.GetSmartObject().pObject->GetEntity();
	int id = GetAISystem()->SmartObjectEvent( "Unhide", pUser, pObject );
	if ( !id )
	{
		pOperand->m_CurrentHideObject.Invalidate();
		pOperand->SetSignal( 0, "OnCoverCompromised", NULL, 0, g_crcSignals.m_nOnCoverCompromised );
		m_fTimeOut = -1.0f;
		return AIGOR_FAILED;
	}

	pOperand->m_CurrentHideObject.SetUsingCover( false );
	return AIGOR_IN_PROGRESS;
}
//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPUseCover::UseCoverSOHide(CPipeUser *pOperand)
{
	//	if(pOperand->m_CurrentHideObject.IsUsingCover())
	//		return false;

	IEntity* pUser = pOperand->GetEntity();
	IEntity* pObject = pOperand->m_CurrentHideObject.GetSmartObject().pObject->GetEntity();
	int id = GetAISystem()->SmartObjectEvent( "Hide", pUser, pObject );
	if ( !id )
	{
		pOperand->m_CurrentHideObject.Invalidate();
		pOperand->SetSignal( 0, "OnCoverCompromised", NULL, 0, g_crcSignals.m_nOnCoverCompromised );
		m_fTimeOut = -1.0f;
		return AIGOR_FAILED;
	}

	pOperand->m_CurrentHideObject.SetUsingCover( true );
	return AIGOR_IN_PROGRESS;
}

//
//-------------------------------------------------------------------------------------------------------------
inline const Vec3& ChooseLeftSide(bool leftClipped, bool rightClipped, const Vec3& peekPosLeft, const Vec3& peekPosRight, int& sideOut)
{
	/*	if(leftClipped && !rightClipped)
	{
	// Right
	sideOut = 1;
	return peekPosRight;
	}
	else*/
	{
		// Left
		sideOut = -1;
		return peekPosLeft;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
inline const Vec3& ChooseRightSide(bool leftClipped, bool rightClipped, const Vec3& peekPosLeft, const Vec3& peekPosRight, int& sideOut)
{
	/*	if(rightClipped && !leftClipped)
	{
	// Left
	sideOut = -1;
	return peekPosLeft;
	}
	else*/
	{
		// Right
		sideOut = 1;
		return peekPosRight;
	}
}

inline bool ShouldMoveOut(float error, int side)
{
	// Get outside the range
	if(side == 0)
	{
		if(fabsf(error) > 0.3f)
			return true;
	}
	else if(side < 0)
	{
		if(error > 0)
			return true;
	}
	else if(side > 0)
	{
		if(error < 0)
			return true;
	}

	return false;
}

inline bool ShouldMoveIn(float error, int side)
{
	if(side == 0)
	{
		if(fabsf(error) > 0.3f)
			return true;
	}
	else if(side < 0)
	{
		if(error < 0)
			return true;
	}
	else if(side > 0)
	{
		if(error > 0)
			return true;
	}

	return false;
}


//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPUseCover::UseCoverEnv(CPipeUser *pOperand)
{
	CAIHideObject& hide = pOperand->m_CurrentHideObject;

	CPuppet*	puppet = pOperand->CastToCPuppet();
	if(!puppet)
		return AIGOR_FAILED;

	if(pOperand->m_CurrentHideObject.IsSmartObject())
		return AIGOR_FAILED;

	if(!hide.IsCoverPathComplete())
	{
		hide.HurryUpCoverPathGen();
		return AIGOR_IN_PROGRESS;
	}

	IAIObject* pTarget = pOperand->GetAttentionTarget();
	if(!pTarget)
	{
		if(m_useLastOpAsBackup)
			pTarget = pOperand->GetLastOpResult();
		if(!pTarget)
		{
			Reset(pOperand);
			return AIGOR_FAILED;
		}
	}

	Vec3	targetPos = pTarget->GetPos();


	bool	cannotUnhide(false);
	if(m_bUnHide && m_curCoverUsage == USECOVER_NONE)
	{
		ChooseCoverUsage(pOperand);
		// If the cover usage is none it means there is no way to unhide, signal to change cover.
		cannotUnhide = m_curCoverUsage == USECOVER_NONE;
	}

	//	if(m_bUnHide)
	//		assert(m_curCoverUsage >= USECOVER_STRAFE_LEFT_STANDING && m_curCoverUsage <= USECOVER_CENTER_CROUCHED);

	UpdateInvalidSeg(pOperand);

	if(fabsf(m_leftInvalidDist) > hide.GetMaxCoverPathLen()/2.0f)
		m_leftInvalidStartTime = GetAISystem()->GetFrameStartTime();	//reset
	if(fabsf(m_rightInvalidDist) > hide.GetMaxCoverPathLen()/2.0f)
		m_rightInvalidStartTime = GetAISystem()->GetFrameStartTime();	//reset

	float	elapsed = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds();
	float	timeLeft = m_fTimeOut - elapsed;

	const Vec3&	opPos = pOperand->GetPhysicsPos();

	if(m_ucs == UCS_NONE)
	{
		m_ucsStartTime = GetAISystem()->GetFrameStartTime();
		m_ucsSide = 0;

		m_moveTarget = opPos;

		float	peekOverLeft = -0.1f; // + m_weaponOffset;
		float	peekOverRight = -0.1f; // - m_weaponOffset;

		if(m_bUnHide)
		{
			/*			if(m_curCoverUsage == USECOVER_STRAFE_TOP_LEFT_STANDING || m_curCoverUsage == USECOVER_STRAFE_TOP_RIGHT_STANDING)
			{
			// Stay inside the cover
			peekOverLeft += -0.6f;
			peekOverRight += -0.6f;
			}*/
		}
		else
		{
			// Stay inside the cover
			peekOverLeft += -0.6f;
			peekOverRight += -0.6f;
		}

		m_coverCompromised = false;
		bool	umbraClampedLeft = false, umbraClampedRight = false;
		bool	useLowCover = hide.HasLowCover();

		hide.GetCoverPoints(useLowCover, peekOverLeft, peekOverRight, targetPos, m_hidePos,
			m_peekPosLeft, m_peekPosRight, umbraClampedLeft, umbraClampedRight, m_coverCompromised);

		if(m_coverCompromised)
		{
			m_coverCompromised = true;
		}

		// TODO: Enable again once the animations can keep the AI at right spot.
		// If drifting too far away from the path, mark the cover compromised.
		//	if(!hide.IsNearCover(pOperand))
		//		m_coverCompromised = true;

		if(!hide.IsLeftEdgeValid(useLowCover))
			umbraClampedLeft = true;
		if(!hide.IsRightEdgeValid(useLowCover))
			umbraClampedRight = true;

		// Clamp to movement range.
		float	leftDist = hide.GetDistanceAlongCoverPath(m_peekPosLeft);
		float	rightDist = hide.GetDistanceAlongCoverPath(m_peekPosRight);

		float leftInvalidTime = (GetAISystem()->GetFrameStartTime() - m_leftInvalidStartTime).GetSeconds();
		float rightInvalidTime = (GetAISystem()->GetFrameStartTime() - m_rightInvalidStartTime).GetSeconds();

		if(leftInvalidTime > 0.01f)
		{
			if(leftDist < m_leftInvalidDist)
			{
				m_peekPosLeft += hide.GetCoverPathDir() * (m_leftInvalidDist - leftDist);
				umbraClampedLeft = true;
			}
			if(rightDist < m_leftInvalidDist)
			{
				m_peekPosRight += hide.GetCoverPathDir() * (m_leftInvalidDist - rightDist);
				umbraClampedRight = true;
			}
		}

		if(rightInvalidTime > 0.01f)
		{
			if(leftDist > m_rightInvalidDist)
			{
				m_peekPosLeft += hide.GetCoverPathDir() * (m_rightInvalidDist - leftDist);
				umbraClampedLeft = true;
			}
			if(rightDist > m_rightInvalidDist)
			{
				m_peekPosRight += hide.GetCoverPathDir() * (m_rightInvalidDist - rightDist);
				umbraClampedRight = true;
			}
		}

		umbraClampedLeft = umbraClampedLeft || !m_leftEdgeCheck;
		umbraClampedRight = umbraClampedRight || !m_rightEdgeCheck;

		m_hidePos = (m_peekPosLeft + m_peekPosRight) * 0.5f;
		m_moveStance = STANCE_STAND;

		if(m_bUnHide)
		{
			switch(m_curCoverUsage)
			{
			case USECOVER_STRAFE_LEFT_STANDING:
				m_moveTarget = ChooseLeftSide(umbraClampedLeft, umbraClampedRight, m_peekPosLeft, m_peekPosRight, m_ucsSide);
				//				m_moveStance = STANCE_CROUCH;
				m_pose = m_moveTarget + Vec3(0,0,1.5f);
				break;
			case USECOVER_STRAFE_RIGHT_STANDING:
				m_moveTarget = ChooseRightSide(umbraClampedLeft, umbraClampedRight, m_peekPosLeft, m_peekPosRight, m_ucsSide);
				//				m_moveStance = STANCE_CROUCH;
				m_pose = m_moveTarget + Vec3(0,0,1.5f);
				break;
			case USECOVER_STRAFE_TOP_STANDING:
				m_ucsSide = 0;
				m_moveTarget = m_hidePos;
				//				m_moveStance = STANCE_CROUCH;
				m_pose = m_moveTarget + Vec3(0,0,1.5f);
				break;
			case USECOVER_STRAFE_TOP_LEFT_STANDING:
				m_moveTarget = ChooseLeftSide(umbraClampedLeft, umbraClampedRight, m_peekPosLeft, m_peekPosRight, m_ucsSide);
				//				m_moveStance = STANCE_CROUCH;
				m_pose = m_moveTarget + Vec3(0,0,1.5f);
				break;
			case USECOVER_STRAFE_TOP_RIGHT_STANDING:
				m_moveTarget = ChooseRightSide(umbraClampedLeft, umbraClampedRight, m_peekPosLeft, m_peekPosRight, m_ucsSide);
				//				m_moveStance = STANCE_CROUCH;
				m_pose = m_moveTarget + Vec3(0,0,1.5f);
				break;
			case USECOVER_STRAFE_LEFT_CROUCHED:
				m_moveTarget = ChooseLeftSide(umbraClampedLeft, umbraClampedRight, m_peekPosLeft, m_peekPosRight, m_ucsSide);
				//				m_moveStance = STANCE_CROUCH;
				m_pose = m_moveTarget + Vec3(0,0,0.7f);
				break;
			case USECOVER_STRAFE_RIGHT_CROUCHED:
				m_moveTarget = ChooseRightSide(umbraClampedLeft, umbraClampedRight, m_peekPosLeft, m_peekPosRight, m_ucsSide);
				//				m_moveStance = STANCE_CROUCH;
				m_pose = m_moveTarget + Vec3(0,0,0.7f);
				break;
			case USECOVER_CENTER_CROUCHED:
				m_ucsSide = 0;
				//				m_moveTarget = m_hidePos;
				m_moveStance = STANCE_CROUCH;
				m_pose = m_moveTarget + Vec3(0,0,0.7f);
				break;
			default:
				// Unhandled case.
				//				AIAssert(0);
				m_ucsSide = 0;
				m_moveStance = STANCE_CROUCH;
				m_pose = m_moveTarget + Vec3(0,0,0.7f);
				break;
			}

			float	moveTargetOnLine = hide.GetDistanceAlongCoverPath(m_moveTarget);
			float	posOnLine = hide.GetDistanceAlongCoverPath(opPos);
			float	dist = fabsf(moveTargetOnLine - posOnLine);
			//			if(dist > 0.4f && hide.HasLowCover())
			//				m_moveStance = STANCE_CROUCH;
			if(dist > 1.5f)
				m_moveStance = STANCE_STEALTH;
			else
				m_moveStance = STANCE_CROUCH;
		}
		else
		{
			if(hide.GetCoverWidth(hide.HasLowCover()) > 1.6f)
			{
				if(Distance::Point_Point2DSq(opPos, m_peekPosLeft) < Distance::Point_Point2DSq(opPos, m_peekPosRight))
				{
					m_moveTarget = ChooseLeftSide(umbraClampedLeft, umbraClampedRight, m_peekPosLeft, m_peekPosRight, m_ucsSide);
					m_pose = m_moveTarget + Vec3(0,0,1.5f);
				}
				else
				{
					m_moveTarget = ChooseRightSide(umbraClampedLeft, umbraClampedRight, m_peekPosLeft, m_peekPosRight, m_ucsSide);
					m_pose = m_moveTarget + Vec3(0,0,1.5f);
				}
			}
			else
			{
				m_ucsSide = 0;
				m_moveTarget = m_hidePos;
				m_pose = m_moveTarget + Vec3(0,0,0.7f);
			}

			float	moveTargetOnLine = hide.GetDistanceAlongCoverPath(m_moveTarget);
			float	posOnLine = hide.GetDistanceAlongCoverPath(opPos);
			float	dist = fabsf(moveTargetOnLine - posOnLine);

			if(dist > 2.0f && hide.HasHighCover())
				m_moveStance = STANCE_STAND;
			else
				m_moveStance = STANCE_CROUCH;
		}



		// Steer towards move target.
		Vec3	delta = m_moveTarget - opPos;
		float	dist = delta.GetLength2D();
		float	moveTargetOnLine = hide.GetDistanceAlongCoverPath(m_moveTarget);
		float	posOnLine = hide.GetDistanceAlongCoverPath(opPos);
		float	error = posOnLine - moveTargetOnLine;

		if(fabsf(error) > 0.3f || m_ucsLast == UCS_NONE)
		{
			// Move
			m_ucsLast = m_ucs = UCS_MOVE;
			m_maxPathLen = 0.0f;

			/*			if(m_bUnHide)
			m_ucsMoveIn = !ShouldMoveOut(error, m_ucsSide);
			else
			m_ucsMoveIn = ShouldMoveIn(error, m_ucsSide);*/
			m_ucsMoveIn = error < 0.0f;

			pOperand->m_State.bodystate = m_moveStance;
			pOperand->m_State.lean = 0.0f;
			pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);

		}
		else
		{
			// Hold
			m_ucsLast = m_ucs = UCS_HOLD;
			m_targetReached = true;

			m_ucsWaitTime = 0.6f + ai_frand() * 0.4f;

			EStance	curStance = (EStance)pOperand->m_State.bodystate;
			float	curLean = pOperand->m_State.lean;

			//			if(puppet->GetFireMode() != FIREMODE_OFF)
			if(m_bUnHide)
			{
				// Update stance
				EStance	stance;
				float	lean;
				if(puppet->SelectAimPosture(targetPos, stance, lean))
				{
					pOperand->m_State.bodystate = stance;
					pOperand->m_State.lean = lean;
					if(lean < -0.01f)
						pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "peekLeft");
					else if(lean > 0.01f)
						pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "peekRight");
					else
						pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
				}
				else
				{
					// Could not find new pose, keep stance and remove peek.
					pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
					pOperand->m_State.lean = 0;
				}
			}
			else
			{
				// Update stance
				EStance	stance;
				float	lean;
				if(puppet->SelectHidePosture(targetPos, stance, lean))
				{
					pOperand->m_State.bodystate = stance;
					pOperand->m_State.lean = lean;
					if(lean < -0.01f)
						pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "peekLeft");
					else if(lean > 0.01f)
						pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "peekRight");
					else
						pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
				}
				else
				{
					// Could not find new pose, keep stance and remove peek.
					pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
					pOperand->m_State.lean = 0;
				}
			}

			if((timeLeft - m_ucsWaitTime) < 1.0f)
			{
				// Try to avoid really small timeout at the end.
				m_ucsWaitTime = timeLeft + 0.1f;
			}
			else
			{
				// Compensate for time taken to change the stance.
				if(curStance != (EStance)pOperand->m_State.bodystate)
					m_ucsWaitTime += 0.4f;
				// Compensate for time taken to change the lean.
				if(fabsf(curLean - pOperand->m_State.lean) > 0.01f)
					m_ucsWaitTime += 0.5f;

				if((timeLeft - m_ucsWaitTime) < 0.5f)
				{
					// Try to avoid really small timeout at the end.
					m_ucsWaitTime = timeLeft + 0.1f;
				}

			}
		}
	}

	if(m_ucs != UCS_NONE)
	{
		float	ucsElapsed = (GetAISystem()->GetFrameStartTime() - m_ucsStartTime).GetSeconds();

		if(m_ucs == UCS_MOVE)
		{
			// TODO: Use trace
			ExecuteDry(pOperand);
			// Failsafe
			if(ucsElapsed > 3.0f)
			{
				m_ucs = UCS_NONE;
				m_coverCompromised = true;
			}
		}
		else if(m_ucs == UCS_HOLD)
		{
			pOperand->m_State.fDistanceToPathEnd = 0;
			pOperand->m_State.fDesiredSpeed = 0;
			pOperand->m_State.vMoveDir.Set(0,0,0);

			// Change decision after a short timeout.
			if(ucsElapsed > m_ucsWaitTime)
				m_ucs = UCS_NONE;
		}
	}


	float leftInvalidTime = (GetAISystem()->GetFrameStartTime() - m_leftInvalidStartTime).GetSeconds();
	float rightInvalidTime = (GetAISystem()->GetFrameStartTime() - m_rightInvalidStartTime).GetSeconds();

	if(leftInvalidTime > 3.0f && rightInvalidTime > 3.0f)
		m_coverCompromised = true;

	if(!hide.IsValid())
		m_coverCompromised = true;

	if((m_coverCompromised || cannotUnhide) && !m_coverCompromisedSignalled)
	{
		pOperand->SetSignal(0,"OnCoverCompromised",NULL, 0, g_crcSignals.m_nOnCoverCompromised);
		m_coverCompromisedSignalled = true;
		hide.Invalidate();
		return AIGOR_FAILED;
	}

	return AIGOR_IN_PROGRESS;
}

void COPUseCover::ExecuteDry(CPipeUser *pOperand)
{
	if(pOperand->m_CurrentHideObject.IsSmartObject())
		return;
	if(!pOperand->m_CurrentHideObject.IsCoverPathComplete())
		return;

	CAIHideObject& hide = pOperand->m_CurrentHideObject;

	CPuppet*	puppet = pOperand->CastToCPuppet();
	if(!puppet)
		return;

	const Vec3&	opPos = pOperand->GetPhysicsPos();

	if(m_ucs == UCS_MOVE)
	{
		// TODO: Use trace


		// Steer towards move target.

		Vec3	delta = m_moveTarget - opPos;
		float	dist = delta.GetLength2D();

		float	moveTargetOnLine = hide.GetDistanceAlongCoverPath(m_moveTarget);
		float	posOnLine = hide.GetDistanceAlongCoverPath(opPos);
		float	error = posOnLine - moveTargetOnLine;
		float	errorNorm = error;
		if(m_ucsMoveIn) errorNorm = -errorNorm;

		puppet->AdjustMovementUrgency(pOperand->m_State.fMovementUrgency, error, &m_maxPathLen);

		Vec3	dir(0,0,0);
		float	speed = 0.25f;
		if(m_ucsSide != 0 && errorNorm > -0.3f)
		{
			if(m_ucsMoveIn)
				dir = hide.GetCoverPathDir();
			else
				dir = -hide.GetCoverPathDir();
			/*			if(!m_ucsMoveIn)
			{
			if(m_ucsSide < 0)
			dir = -hide.GetCoverPathDir();
			else
			dir = hide.GetCoverPathDir();
			}
			else
			{
			if(m_ucsSide < 0)
			dir = hide.GetCoverPathDir();
			else
			dir = -hide.GetCoverPathDir();
			}*/
		}
		else
		{
			//			delta = m_moveTarget - hide.ProjectPointOnCoverPath(opPos);
			dir	= delta;
			dir.z = 0;
			dir.NormalizeSafe();
			speed = clamp(0.5f + (dist/1.5f), 0.0f, 1.0f);
		}

		float normalSpeed, minSpeed, maxSpeed;
		pOperand->GetMovementSpeedRange(pOperand->m_State.fMovementUrgency, pOperand->m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

		//		float normalSpeed = pOperand->GetNormalMovementSpeed(pOperand->m_State.fMovementUrgency, true);

		pOperand->m_State.fDesiredSpeed = speed * normalSpeed;
		pOperand->m_State.vMoveDir = dir;
		pOperand->m_State.fDistanceToPathEnd = dist;

		pOperand->m_DEBUGmovementReason = CPipeUser::AIMORE_MOVE;

		if(m_ucsMoveIn)
		{
			if(error > 0.0f)
				//			if(!ShouldMoveOut(error, m_ucsSide))
				m_ucs = UCS_NONE;
		}
		else
		{
			if(error < 0.0f)
				//			if(!ShouldMoveIn(error, m_ucsSide))
				m_ucs = UCS_NONE;
		}
	}

}

//
//-------------------------------------------------------------------------------------------------------------
void COPUseCover::ChooseCoverUsage(CPipeUser *pOperand)
{
	// Collect all possible options for this hide spot.
	CAIHideObject& hide = pOperand->m_CurrentHideObject;

	// Check if the sides are obstructed.
	IAIObject* pTarget = pOperand->GetAttentionTarget();
	if(!pTarget)
	{
		if(m_useLastOpAsBackup)
			pTarget = pOperand->GetLastOpResult();
	}
	if(!pTarget)
	{
		m_curCoverUsage = USECOVER_NONE;
		hide.SetCoverUsage(m_curCoverUsage);
		return;
	}

	const Vec3&	targetPos = pTarget->GetPos();
	const Vec3&	opPos = pOperand->GetPhysicsPos();

	float	peekOverLeft = max(0.1f, 0.3f + m_weaponOffset);
	float	peekOverRight = max(0.1f, 0.3f - m_weaponOffset);

	enum ECheckPosIdx
	{
		CHECK_HIGH_MIDDLE,
		CHECK_HIGH_LEFT,
		CHECK_HIGH_RIGHT,
		CHECK_LOW_MIDDLE,
		CHECK_LOW_LEFT,
		CHECK_LOW_RIGHT,
		CHECK_NONE,
	};

	Vec3	tmp;
	Vec3	checkPos[6];
	bool	checkRes[6] = { false, false, false, false, false, false };
	bool	umbraLeftClamped, umbraRightClamped, comp;

	hide.GetCoverPoints(true, peekOverLeft, peekOverRight, targetPos, checkPos[CHECK_LOW_MIDDLE],
		checkPos[CHECK_LOW_LEFT], checkPos[CHECK_LOW_RIGHT], umbraLeftClamped, umbraRightClamped, comp);
	checkPos[CHECK_LOW_MIDDLE].z += 0.7f;
	checkPos[CHECK_LOW_LEFT].z += 0.7f;
	checkPos[CHECK_LOW_RIGHT].z += 0.7f;
	checkRes[CHECK_LOW_LEFT] = !umbraLeftClamped;
	checkRes[CHECK_LOW_RIGHT] = !umbraLeftClamped;


	hide.GetCoverPoints(false, peekOverLeft, peekOverRight, targetPos, checkPos[CHECK_HIGH_MIDDLE],
		checkPos[CHECK_HIGH_LEFT], checkPos[CHECK_HIGH_RIGHT], umbraLeftClamped, umbraRightClamped, comp);
	checkPos[CHECK_HIGH_MIDDLE].z += 1.5f;
	checkPos[CHECK_HIGH_LEFT].z += 1.5f;
	checkPos[CHECK_HIGH_RIGHT].z += 1.5f;
	checkRes[CHECK_HIGH_LEFT] = !umbraLeftClamped;
	checkRes[CHECK_HIGH_RIGHT] = !umbraLeftClamped;

	IPhysicalWorld *pWorld = GetAISystem()->GetPhysicalWorld();
	for(unsigned i = 0; i < 6; ++i)
	{
		ray_hit hit;

		if(!checkRes[i]) continue;

		Vec3	dir = targetPos - checkPos[i];
		float len = dir.GetLength();
		if(len > 6.0f)
			dir *= 6.0f / len;

		TICK_COUNTER_FUNCTION
			TICK_COUNTER_FUNCTION_BLOCK_START
			checkRes[i] = pWorld->RayWorldIntersection(checkPos[i], dir, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1) == 0;
		TICK_COUNTER_FUNCTION_BLOCK_END

			m_DEGUG_checkPos[i] = checkPos[i];
		m_DEGUG_checkDir[i] = dir;
		m_DEBUG_checkRes[i] = checkRes[i];
	}

	m_DEBUG_checkValid = true;

	typedef	std::vector<std::pair<int, int> >	VectorIntInt;

	VectorIntInt	coverUsage;

	bool	isWideCover = hide.GetCoverWidth(true) > 2.0f;

	// Allow peeking only when the enemy is a further than this specified value.
	const float	minPeekDistance(5.0f);
	float	distToTarget(10000.0f);
	if(pOperand->GetAttentionTarget())
		distToTarget = Distance::Point_Point(pOperand->GetPos(), pOperand->GetAttentionTarget()->GetPos());

	//	bool	useLowPeeks = hide.GetCoverWidth(true) > 0.5f && hide.IsObjectCollidable() && distToTarget > minPeekDistance;
	//	bool	useHighPeeks = hide.GetCoverWidth(false) > 0.5f && hide.IsObjectCollidable() && distToTarget > minPeekDistance;

	if(hide.HasLowCover() && hide.GetCoverWidth(true) > 0.1f)
	{
		if(hide.HasHighCover() && hide.GetCoverWidth(false) > 0.1f)
		{
			if(hide.IsLeftEdgeValid(false))
			{
				if(checkRes[CHECK_HIGH_LEFT])
				{
					coverUsage.push_back(std::make_pair((int)USECOVER_STRAFE_LEFT_STANDING, (int)CHECK_HIGH_LEFT));
					//					if(useHighPeeks)
					//							coverUsage.push_back(std::make_pair((int)USECOVER_PEEK_LEFT_STANDING, CHECK_HIGH_LEFT));
				}
			}
			if(hide.IsRightEdgeValid(false))
			{
				if(checkRes[CHECK_HIGH_RIGHT])
				{
					coverUsage.push_back(std::make_pair((int)USECOVER_STRAFE_RIGHT_STANDING, (int)CHECK_HIGH_RIGHT));
					//					if(useHighPeeks)
					//						coverUsage.push_back(std::make_pair((int)USECOVER_PEEK_RIGHT_STANDING, CHECK_HIGH_RIGHT));
				}
			}
		}
		else
		{
			if(checkRes[CHECK_HIGH_MIDDLE])
				coverUsage.push_back(std::make_pair((int)USECOVER_STRAFE_TOP_STANDING, (int)CHECK_HIGH_MIDDLE));
			/*			if(isWideCover)
			{
			if(checkRes[CHECK_HIGH_LEFT])
			coverUsage.push_back(std::make_pair((int)USECOVER_STRAFE_TOP_LEFT_STANDING, (int)CHECK_HIGH_LEFT));
			if(checkRes[CHECK_HIGH_RIGHT])
			coverUsage.push_back(std::make_pair((int)USECOVER_STRAFE_TOP_RIGHT_STANDING, (int)CHECK_HIGH_RIGHT));
			}*/
		}

		if(hide.IsLeftEdgeValid(true))
		{
			if(checkRes[CHECK_LOW_LEFT])
			{
				coverUsage.push_back(std::make_pair((int)USECOVER_STRAFE_LEFT_CROUCHED, (int)CHECK_LOW_LEFT));
				//				if(useLowPeeks)
				//					coverUsage.push_back(std::make_pair((int)USECOVER_PEEK_LEFT_CROUCHED, CHECK_LOW_LEFT));
			}
		}
		if(hide.IsRightEdgeValid(true))
		{
			if(checkRes[CHECK_LOW_RIGHT])
			{
				coverUsage.push_back(std::make_pair((int)USECOVER_STRAFE_RIGHT_CROUCHED, (int)CHECK_LOW_RIGHT));
				//				if(useLowPeeks)
				//					coverUsage.push_back(std::make_pair((int)USECOVER_PEEK_RIGHT_CROUCHED, CHECK_LOW_RIGHT));
			}
		}
	}
	else
	{
		if(checkRes[CHECK_LOW_MIDDLE])
			coverUsage.push_back(std::make_pair((int)USECOVER_CENTER_CROUCHED, (int)CHECK_LOW_MIDDLE));
	}

	// Fail safe in case the better checks failed completely.
	if(coverUsage.empty())
	{
		if(hide.HasLowCover() && hide.GetCoverWidth(true) > 0.1f)
		{
			if(hide.HasHighCover() && hide.GetCoverWidth(false) > 0.1f)
			{
				if(hide.IsLeftEdgeValid(false))
					coverUsage.push_back(std::make_pair((int)USECOVER_STRAFE_LEFT_STANDING, (int)CHECK_NONE));
				if(hide.IsRightEdgeValid(false))
					coverUsage.push_back(std::make_pair((int)USECOVER_STRAFE_RIGHT_STANDING, (int)CHECK_NONE));
			}
			else
				coverUsage.push_back(std::make_pair(USECOVER_STRAFE_TOP_STANDING, (int)CHECK_NONE));

			if(hide.IsLeftEdgeValid(true))
				coverUsage.push_back(std::make_pair((int)USECOVER_STRAFE_LEFT_CROUCHED, (int)CHECK_NONE));
			if(hide.IsRightEdgeValid(true))
				coverUsage.push_back(std::make_pair((int)USECOVER_STRAFE_RIGHT_CROUCHED, (int)CHECK_NONE));
		}
		else
			coverUsage.push_back(std::make_pair((int)USECOVER_CENTER_CROUCHED, (int)CHECK_NONE));
	}

	// Remove the last option if it does not
	size_t	removeCount = 0;
	for(VectorIntInt::iterator it = coverUsage.begin(); it != coverUsage.end(); ++it)
	{
		if(it->first == hide.GetCoverUsage())
			removeCount++;
	}

	if(removeCount < coverUsage.size())
	{
		for(VectorIntInt::iterator it = coverUsage.begin(); it != coverUsage.end();)
		{
			if(it->first == hide.GetCoverUsage())
				it = coverUsage.erase(it);
			else
				++it;
		}
	}

	// Shuffle.
	if(!coverUsage.empty())
	{
		std::random_shuffle(coverUsage.begin(), coverUsage.end());
		m_curCoverUsage = coverUsage.front().first;

		switch(coverUsage.front().second)
		{
		case CHECK_HIGH_MIDDLE:
			m_leftEdgeCheck = true;
			m_rightEdgeCheck = true;
			break;
		case CHECK_HIGH_LEFT:
			m_leftEdgeCheck = true;
			m_rightEdgeCheck = checkRes[CHECK_HIGH_RIGHT];
			break;
		case CHECK_HIGH_RIGHT:
			m_leftEdgeCheck = checkRes[CHECK_HIGH_LEFT];
			m_rightEdgeCheck = true;
			break;
		case CHECK_LOW_MIDDLE:
			m_leftEdgeCheck = true;
			m_rightEdgeCheck = true;
			break;
		case CHECK_LOW_LEFT:
			m_leftEdgeCheck = true;
			m_rightEdgeCheck = checkRes[CHECK_LOW_RIGHT];
			break;
		case CHECK_LOW_RIGHT:
			m_leftEdgeCheck = checkRes[CHECK_LOW_LEFT];
			m_rightEdgeCheck = true;
			break;
		default:
			m_leftEdgeCheck = true;
			m_rightEdgeCheck = true;
			break;
		}
	}
	else
		m_curCoverUsage = USECOVER_NONE;

	hide.SetCoverUsage(m_curCoverUsage);

	// Reset state
	m_ucs = UCS_NONE;
	m_ucsStartTime.SetSeconds(0.0f);
	//	m_ucsTime = 0.0f;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPUseCover::UpdateInvalidSeg(CPipeUser *pOperand)
{
	CAIHideObject& hide = pOperand->m_CurrentHideObject;
	Vec3	opPos(pOperand->GetPhysicsPos());
	float	opDistAlongCoverPath = hide.GetDistanceAlongCoverPath(opPos);
	float	opRad = pOperand->GetParameters().m_fPassRadius + 0.3f;

	m_leftInvalidDist = -1000.0f;
	m_rightInvalidDist = 1000.0f;

	// Loop through the nearby puppets and check if they are invalidating some of the cover path.
	AIObjects::iterator aiEnd = GetAISystem()->m_Objects.end();
	for (AIObjects::iterator ai = GetAISystem()->m_Objects.begin() ; ai != aiEnd ; ++ai )
	{
		// Skip every object that is not a puppet.
		CAIObject* object = ai->second;
		unsigned short type = object->GetType();
		if (type != AIOBJECT_PLAYER && type != AIOBJECT_PUPPET)
			continue;
		CPuppet*	puppet = object->CastToCPuppet();
		if(!puppet)
			continue;
		// Skip self.
		if(puppet == pOperand)
			continue;
		// Skip puppets which are too far away from use and the path.
		Vec3	otherPos(puppet->GetPhysicsPos());
		if(Distance::Point_Point2DSq(opPos, otherPos) > sqr(6.0f))
			continue;
		if(hide.GetDistanceToCoverPath(otherPos) > 1.5f)
			continue;
		float	otherDistAlongCoverPath = hide.GetDistanceAlongCoverPath(otherPos);
		if(fabsf(otherDistAlongCoverPath) > (hide.GetMaxCoverPathLen()/2.0f))
			continue;

		float	rad = puppet->GetParameters().m_fPassRadius + opRad;

		if(otherDistAlongCoverPath < opDistAlongCoverPath)
		{
			// Left side
			otherDistAlongCoverPath += rad;
			if(otherDistAlongCoverPath > m_leftInvalidDist)
			{
				m_leftInvalidDist = otherDistAlongCoverPath;
				m_leftInvalidPos = hide.ProjectPointOnCoverPath(otherPos) + rad * hide.GetCoverPathDir();
			}
		}
		else
		{
			// Right side
			otherDistAlongCoverPath -= rad;
			if(otherDistAlongCoverPath < m_rightInvalidDist)
			{
				m_rightInvalidDist = otherDistAlongCoverPath;
				m_rightInvalidPos = hide.ProjectPointOnCoverPath(otherPos) - rad * hide.GetCoverPathDir();
			}
		}
	}

}


//
//-------------------------------------------------------------------------------------------------------------
void COPUseCover::OnObjectRemoved(CAIObject *pObject)
{

}

//
//-------------------------------------------------------------------------------------------------------------
void COPUseCover::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPUseCover");
	{
		ser.Value("m_speedUpTimeOutWhenNoTarget",m_speedUpTimeOutWhenNoTarget);
		ser.Value("m_targetReached",m_targetReached);
		ser.Value("m_coverCompromised",m_coverCompromised);
		ser.Value("m_coverCompromisedSignalled",m_coverCompromisedSignalled);
		ser.Value("m_hidePos",m_hidePos);
		ser.Value("m_peekPosRight",m_peekPosRight);
		ser.Value("m_peekPosLeft",m_peekPosLeft);
		ser.Value("m_pose",m_pose);
		ser.Value("m_moveTarget",m_moveTarget);
		ser.Value("m_curCoverUsage",m_curCoverUsage);
		ser.Value("m_weaponOffset",m_weaponOffset);
		ser.Value("m_intervalMin",m_intervalMin);
		ser.Value("m_intervalMax",m_intervalMax);
		ser.Value("m_fTimeOut",m_fTimeOut);
		ser.Value("m_startTime",m_startTime);
		ser.Value("m_bUnHide",m_bUnHide);
		ser.Value("m_useLastOpAsBackup",m_useLastOpAsBackup);
		//		ser.Value("m_leftInvalidTime",m_leftInvalidTime);
		ser.Value("m_leftInvalidStartTime",m_leftInvalidStartTime);
		ser.Value("m_leftInvalidDist",m_leftInvalidDist);
		ser.Value("m_leftInvalidPos",m_leftInvalidPos);
		//		ser.Value("m_rightInvalidTime",m_rightInvalidTime);
		ser.Value("m_rightInvalidStartTime",m_rightInvalidStartTime);
		ser.Value("m_rightInvalidDist",m_rightInvalidDist);
		ser.Value("m_rightInvalidPos",m_rightInvalidPos);
		ser.Value("m_leftEdgeCheck",m_leftEdgeCheck);
		ser.Value("m_rightEdgeCheck",m_rightEdgeCheck);
		ser.EnumValue("m_ucs",m_ucs, UCS_NONE, UCS_HOLD);
		ser.EnumValue("m_ucsLast",m_ucs, UCS_NONE, UCS_HOLD);
		//		ser.Value("m_ucsTime",m_ucsTime);
		ser.Value("m_ucsStartTime",m_ucsStartTime);
		ser.Value("m_ucsWaitTime",m_ucsWaitTime);
		ser.Value("m_ucsSide",m_ucsSide);
		ser.Value("m_ucsMoveIn", m_ucsMoveIn);
		ser.EnumValue("m_moveStance",m_moveStance, STANCE_NULL, STANCE_LAST);
	}
	ser.EndGroup();
}

//
//-------------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------------
void COPUseCover::DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const
{
	if(!pOperand)
		return;

	CAIHideObject& hide = pOperand->m_CurrentHideObject;

	if(!hide.IsCoverPathComplete())
		return;

	// Hide pos
	pRenderer->GetIRenderAuxGeom()->DrawCone(m_hidePos + Vec3(0,0,0.3f), Vec3(0, 0, -1), 0.1f, 0.3f, ColorB(255,0,0));
	// Unhide pos
	pRenderer->GetIRenderAuxGeom()->DrawCone(m_peekPosLeft + Vec3(0,0,0.3f), Vec3(0,0,-1), 0.1f, 0.3f, ColorB(0,255,0));
	pRenderer->GetIRenderAuxGeom()->DrawCone(m_peekPosRight + Vec3(0,0,0.3f), Vec3(0,0,-1), 0.1f, 0.3f, ColorB(0,255,0));
	// Pos
	pRenderer->GetIRenderAuxGeom()->DrawSphere(m_pose, 0.2f, ColorB(255,255,255));

	// Draw invalid segments.
	float leftInvalidTime = (GetAISystem()->GetFrameStartTime() - m_leftInvalidStartTime).GetSeconds();
	float rightInvalidTime = (GetAISystem()->GetFrameStartTime() - m_rightInvalidStartTime).GetSeconds();

	if(leftInvalidTime > 0.001f)
		DebugDrawArrow(pRenderer, m_leftInvalidPos + Vec3(0,0,0.25f), -hide.GetCoverPathDir() * (0.25f + leftInvalidTime), 0.3f, ColorB(255,0,0,200));
	if(rightInvalidTime > 0.001f)
		DebugDrawArrow(pRenderer, m_rightInvalidPos + Vec3(0,0,0.25f), hide.GetCoverPathDir() * (0.25f + rightInvalidTime), 0.3f, ColorB(255,0,0,200));

	if(m_DEBUG_checkValid)
	{
		for(unsigned i = 0; i < 6; ++i)
		{
			ColorB	col(255,255,255,255);
			if(!m_DEBUG_checkRes[i])
				col.Set(255,0,0,255);
			pRenderer->GetIRenderAuxGeom()->DrawCone(m_DEGUG_checkPos[i], m_DEGUG_checkDir[i].GetNormalized(), 0.05f, 0.2f, col);
		}
	}
	const char*	szMsg = "";
	if(m_ucs == UCS_NONE)
		szMsg = "UCS_NONE";
	else if(m_ucs == UCS_MOVE)
	{
		if(m_ucsMoveIn)
			szMsg = "UCS_MOVE in";
		else
			szMsg = "UCS_MOVE out";
	}
	else if(m_ucs == UCS_HOLD)
		szMsg = "UCS_HOLD";

	float	elapsed = (GetAISystem()->GetFrameStartTime() - m_ucsStartTime).GetSeconds();

	pRenderer->DrawLabel(pOperand->GetPhysicsPos() + Vec3(0,0,0.3f), 1.0f, "%s %s - %.2fs", m_bUnHide ? "UNHIDE" : "HIDE", szMsg, elapsed);


	/*
	IAIObject* pTarget = pOperand->GetAttentionTarget();
	if(!pTarget)
	{
	if(m_useLastOpAsBackup)
	pTarget = pOperand->GetLastOpResult();
	if(!pTarget)
	return;
	}

	Vec3	targetPos = pTarget->GetPos();

	Vec3	hidePos, peekPosLeft, peekPosRight;
	bool	umbraClampedLeft = false, umbraClampedRight = false, coverCompromised = false;

	hide.GetCoverPoints(true, -0.1f, -0.1f, targetPos, hidePos,
	peekPosLeft, peekPosRight, umbraClampedLeft, umbraClampedRight, coverCompromised);

	pRenderer->GetIRenderAuxGeom()->DrawLine(hidePos + Vec3(0,0,0.5f), ColorB(255,255,255), hidePos + Vec3(0,0,0.5f) + hide.GetCoverPathDir(),ColorB(255,255,255));

	pRenderer->GetIRenderAuxGeom()->DrawCone(hidePos + Vec3(0,0,0.5f), Vec3(0,0,-1), 0.05f, 0.2f, ColorB(255,255,255,coverCompromised?128:255));
	pRenderer->GetIRenderAuxGeom()->DrawCone(peekPosLeft + Vec3(0,0,0.5f), Vec3(0,0,-1), 0.05f, 0.2f, ColorB(255,64,0,umbraClampedLeft?128:255));
	pRenderer->GetIRenderAuxGeom()->DrawCone(peekPosRight + Vec3(0,0,0.5f), Vec3(0,0,-1), 0.05f, 0.2f, ColorB(255,128,0,umbraClampedRight?128:255));
	*/
}

//
//-------------------------------------------------------------------------------------------------------------
COPWait::COPWait( int waitType, int blockCount ):
m_WaitType(static_cast<EOPWaitType>(waitType)),
m_BlockCount(blockCount)
{

}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPWait::Execute(CPipeUser *pOperand)
{
	int	currentBlockCount = pOperand->CountGroupedActiveGoals();
	bool	done = false;
	switch(m_WaitType)
	{
	case WAIT_ALL:
		done = currentBlockCount == 0;
		break;
	case WAIT_ANY:
		done = currentBlockCount < m_BlockCount;
		break;
	case WAIT_ANY_2:
		done = currentBlockCount+1 < m_BlockCount;
		break;
	default:
		done = true;
	}
	if(done)
		pOperand->ClearGroupedActiveGoals();

	return done ? AIGOR_DONE : AIGOR_IN_PROGRESS;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPWait::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPWait");
	{
		ser.EnumValue("m_WaitType", m_WaitType, WAIT_ALL, WAIT_LAST);
		ser.Value("m_BlockCount", m_BlockCount);	
		ser.EndGroup();
	}
}


//
//-------------------------------------------------------------------------------------------------------------
COPAdjustAim::COPAdjustAim(bool hide, bool useLastOpAsBackup, bool allowProne) :
m_hide(hide),
m_useLastOpAsBackup(useLastOpAsBackup),
m_allowProne(allowProne)
{
	m_startTime.SetMilliSeconds(0);
	m_evalTime = RandomizeEvalTime();
}

//
//-------------------------------------------------------------------------------------------------------------
float COPAdjustAim::RandomizeEvalTime()
{
	return 0.4f + ai_frand() * 0.3f;
}

//
//-------------------------------------------------------------------------------------------------------------
EGoalOpResult COPAdjustAim::Execute( CPipeUser* pOperand )
{
	CPuppet*	puppet = pOperand->CastToCPuppet();
	if(!puppet)
		return AIGOR_FAILED;

	Vec3	targetPos;
	if(pOperand->GetAttentionTarget())
	{
		targetPos = pOperand->GetAttentionTarget()->GetPos();
	}
	else if(m_useLastOpAsBackup && pOperand->m_pLastOpResult)
	{
		targetPos = pOperand->m_pLastOpResult->GetPos();
	}
	else
	{
		// Cannot adjust, no target at all.
		return AIGOR_FAILED;
	}

	if(m_startTime.GetSeconds() < 0.001f)
	{
		// Set time in past to force the first eval.
		m_startTime = GetAISystem()->GetFrameStartTime() - CTimeValue(10.0f);
	}

	float	elapsed = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds();


	float	distToTarget = Distance::Point_Point(targetPos, pOperand->GetPos());

	bool isMoving = pOperand->GetVelocity().GetLength() > 0.01f || (pOperand->m_State.vMoveDir.GetLength() > 0.01f && pOperand->m_State.fDesiredSpeed > 0.01f);

	SAIWeaponInfo	weaponInfo;
	pOperand->GetProxy()->QueryWeaponInfo(weaponInfo);
	if (elapsed > m_evalTime && weaponInfo.isReloading)
		m_evalTime = elapsed + 0.2f;

	if (isMoving)
	{
		int idx = MovementUrgencyToIndex(pOperand->m_State.fMovementUrgency);
		if (idx <= AISPEED_WALK)
		{
			if (elapsed > m_evalTime)
			{
				EStance	stance = STANCE_CROUCH;
				float lean = -1.0f;

				bool	stanceOk = false;
				if (m_hide)
					stanceOk = puppet->SelectHidePosture(targetPos, stance, lean, false, m_allowProne && pOperand->m_State.fDistanceToPathEnd < 1.3f);
				else
					stanceOk = puppet->SelectAimPosture(targetPos, stance, lean, false, m_allowProne && pOperand->m_State.fDistanceToPathEnd < 1.3f);

				if (stanceOk)
					pOperand->m_State.bodystate = stance;

				m_startTime = GetAISystem()->GetFrameStartTime();
				m_evalTime = RandomizeEvalTime();
				if (pOperand->m_State.bodystate == STANCE_PRONE)
					m_evalTime += 2.5f;
			}

		}
		else
		{
			if (elapsed > m_evalTime)
			{
				m_evalTime = 1.0f;
				m_startTime = GetAISystem()->GetFrameStartTime() - CTimeValue(10.0f);

				if (m_hide)
					pOperand->m_State.bodystate = distToTarget > 8.0f ? STANCE_STEALTH : STANCE_STAND;
				else
					pOperand->m_State.bodystate = STANCE_STAND;
			}
		}

		pOperand->m_State.lean = 0;
		pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
	}
	else
	{
		if (elapsed > m_evalTime)
		{
			EStance	stance = STANCE_CROUCH;
			float		lean = -1.0f;

			bool	stanceOk = false;
			if (m_hide)
				stanceOk = puppet->SelectHidePosture(targetPos, stance, lean, true, m_allowProne);
			else
				stanceOk = puppet->SelectAimPosture(targetPos, stance, lean, true, m_allowProne);

			if (stanceOk)
			{
				pOperand->m_State.bodystate = stance;
				pOperand->m_State.lean = lean;
				if (lean < -0.01f)
					pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "peekLeft");
				else if (lean > 0.01f)
					pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "peekRight");
				else
					pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
			}
			else
			{
				// Could not find new pose, keep stance and remove peek.
				pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
				pOperand->m_State.lean = 0;
			}

			m_startTime = GetAISystem()->GetFrameStartTime();
			m_evalTime = RandomizeEvalTime();
			if (pOperand->m_State.bodystate == STANCE_PRONE)
				m_evalTime += 2.5f;
		}
	}


	/*
	int	n = (int)floorf((float)elapsed / 5.0f) % 4;

	if(n == 0)
	{
	pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "peekLeft");
	pOperand->m_State.lean = -1.0f;
	}
	else if(n == 1)
	{
	pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
	pOperand->m_State.lean = 0;
	}
	else if(n == 2)
	{
	pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "peekRight");
	pOperand->m_State.lean = 1.0f;
	}
	else
	{
	pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
	pOperand->m_State.lean = 0;
	}
	*/

	return AIGOR_IN_PROGRESS;
}

//
//-------------------------------------------------------------------------------------------------------------
void COPAdjustAim::Reset( CPipeUser* pOperand )
{
	pOperand->m_State.lean = 0.0f;
	pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
}

//
//-------------------------------------------------------------------------------------------------------------
void COPAdjustAim::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPAdjustAim");
	{
		ser.Value("m_startTime", m_startTime);	
		ser.Value("m_evalTime", m_evalTime);	
		ser.Value("m_hide", m_hide);	
		ser.Value("m_useLastOpAsBackup", m_useLastOpAsBackup);	
	}
	ser.EndGroup();
}


//
//-------------------------------------------------------------------------------------------------------------
void COPAdjustAim::DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const
{
	if(!pOperand)
		return;

	//	m_selector.DebugDraw(pOperand, pRenderer);

	Vec3	basePos = pOperand->GetPhysicsPos();
	Vec3	targetPos = pOperand->GetProbableTargetPosition();

	/*	for(unsigned i = 0; i < 8; ++i)
	{
	if(!m_postures[i].valid) continue;
	pRenderer->GetIRenderAuxGeom()->DrawCone(m_postures[i].weapon, m_postures[i].weaponDir, 0.05f, m_postures[i].targetAim ? 0.4f : 0.2f, ColorB(255,0,0, m_postures[i].targetAim ? 255 : 128));
	pRenderer->GetIRenderAuxGeom()->DrawSphere(m_postures[i].eye, 0.1f, ColorB(255,255,255, m_postures[i].targetVis ? 255 : 128));
	pRenderer->GetIRenderAuxGeom()->DrawLine(basePos, ColorB(255,255,255), m_postures[i].eye, ColorB(255,255,255));

	if((int)i == m_bestPosture)
	{
	pRenderer->GetIRenderAuxGeom()->DrawLine(m_postures[i].weapon, ColorB(255,0,0), targetPos, ColorB(255,0,0,0));
	pRenderer->GetIRenderAuxGeom()->DrawLine(m_postures[i].eye, ColorB(255,255,255), targetPos, ColorB(255,255,255,0));
	}
	}
	*/

	Vec3	toeToHead = pOperand->GetPos() - pOperand->GetPhysicsPos();
	float	len = toeToHead.GetLength();
	if(len > 0.0001f)
		toeToHead *= (len - 0.3f) / len;
	pRenderer->GetIRenderAuxGeom()->DrawSphere(pOperand->GetPhysicsPos() + toeToHead, 0.4f, ColorB(200,128,0, 90));

	pRenderer->GetIRenderAuxGeom()->DrawLine(pOperand->GetFirePos(), ColorB(255,0,0), targetPos, ColorB(255,0,0,64));
	pRenderer->GetIRenderAuxGeom()->DrawLine(pOperand->GetPos(), ColorB(255,255,255), targetPos, ColorB(255,255,255,64));
}


//====================================================================
// COPSeekCover
//====================================================================
COPSeekCover::COPSeekCover(bool uncover, float radius, int iterations, bool useLastOpAsBackup, bool towardsLastOpResult) :
	m_uncover(uncover),
	m_radius(radius),
	m_iterations(iterations),
	m_useLastOpAsBackup(useLastOpAsBackup),
	m_towardsLastOpResult(towardsLastOpResult),
	m_endAccuracy(0.0f),
	m_notMovingTime(0),
	m_pRoot(0),
	m_state(0),
	m_center(0,0,0),
	m_forward(0,0,0),
	m_right(0,0,0),
	m_target(0,0,0)
{
	Limit(m_iterations, 1, 5);
	m_pTraceDirective = 0;
}

//====================================================================
// ~COPSeekCover
//====================================================================
COPSeekCover::~COPSeekCover()
{
	Reset(0);
	delete m_pRoot;
}

//====================================================================
// Reset
//====================================================================
void COPSeekCover::Reset(CPipeUser *pOperand)
{
	delete m_pTraceDirective;
	m_pTraceDirective = 0;
	m_notMovingTime = 0;
	m_state = 0;
	if (pOperand)
		pOperand->ClearPath("COPSeekCover::Reset m_Path");
}

//====================================================================
// Serialize
//====================================================================
void COPSeekCover::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPSeekCover");
	{
		ser.Value("m_radius", m_radius);
		ser.Value("m_iterations", m_iterations);
		ser.Value("m_endAccuracy", m_endAccuracy);
		ser.Value("m_useLastOpAsBackup", m_useLastOpAsBackup);
		ser.Value("m_towardsLastOpResult", m_towardsLastOpResult);
		ser.Value("m_lastTime", m_lastTime);
		ser.Value("m_notMovingTime", m_notMovingTime);
		ser.Value("m_state", m_state);
		if(ser.IsWriting())
		{
			if(ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective!=NULL))
			{
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if(ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(false, m_endAccuracy);
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			// Restart calculating
			if (m_state >= 0)
			{
				m_state = 0;
				delete m_pTraceDirective;
				m_pTraceDirective = 0;
			}
		}
		ser.EndGroup();
	}
}

//====================================================================
// IsSegmentValid
//====================================================================
bool COPSeekCover::IsSegmentValid(IAISystem::tNavCapMask navCap, float rad, const Vec3& posFrom, Vec3& posTo, IAISystem::ENavigationType& navTypeFrom)
{
	int nBuildingID = -1;
	IVisArea* pVisArea;

	navTypeFrom = GetAISystem()->CheckNavigationType(posFrom, nBuildingID, pVisArea, navCap);

	if (navTypeFrom == IAISystem::NAV_TRIANGULAR)
	{
		// Make sure not to enter forbidden area.
		Vec3 nearest;
		if (GetAISystem()->IntersectsForbidden(posFrom, posTo, nearest))
			return false;
		if (GetAISystem()->IsPointForbidden(posTo, rad))
			return false;
	}

	if (IsInDeepWater(posTo))
		return false;

	Vec3 initPos(posTo);
	if (!GetFloorPos(posTo, initPos, walkabilityFloorUpDist, 1.0f, walkabilityDownRadius, AICE_ALL))
		return false;

	SWalkPosition from(posFrom, true);
	SWalkPosition to(posTo, true);

	return CheckWalkabilitySimple(from, to, rad - walkabilityRadius, AICE_ALL_EXCEPT_TERRAIN);


/*	const SpecialArea* sa = 0;
	if (nBuildingID != -1)
		sa = GetAISystem()->GetSpecialArea(nBuildingID);


	if (sa)
	{
		const ListPositions	& buildingPolygon = sa->GetPolygon();
		SCheckWalkabilityState state;
		state.state = SCheckWalkabilityState::CWS_NEW_QUERY;
		state.numIterationsPerCheck = 100000;
		bool res = CheckWalkability(posFrom, posTo, rad + 0.15f - walkabilityRadius, true, buildingPolygon, AICE_ALL, 0, &state);
		if (res)
			posTo = state.toFloor;
		return res;
	}
	else
	{
		ListPositions buildingPolygon;
		SCheckWalkabilityState state;
		state.state = SCheckWalkabilityState::CWS_NEW_QUERY;
		state.numIterationsPerCheck = 100000;
		bool res = CheckWalkability(posFrom, posTo, rad + 0.15f - walkabilityRadius, true, buildingPolygon, AICE_ALL, 0, &state);
		if (res)
			posTo = state.toFloor;
		return res;
	}*/


	return false;
}

inline float Ease(float a)
{
	return (a + sqr(a))/2.0f;
}

//====================================================================
// IsTargetVisibleFrom
//====================================================================
bool COPSeekCover::IsTargetVisibleFrom(const Vec3& from, const Vec3& targetPos)
{
	ray_hit	hit;
	Vec3 waistPos = from + Vec3(0,0,m_uncover ? 1.2f : 0.6f);
	Vec3 delta = targetPos - waistPos;
	if (gEnv->pPhysicalWorld->RayWorldIntersection(waistPos, delta, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1))
	{
		if (hit.dist < delta.GetLength() * 0.95f)
			return false;
	}
	return true;
}

//====================================================================
// CheckSegmentAgainstAvoidPos
//====================================================================
bool COPSeekCover::OverlapSegmentAgainstAvoidPos(const Vec3& from, const Vec3& to, float rad, const std::vector<Vec3FloatPair>& avoidPos)
{
	Lineseg	movement(from, to);
	float t;
	for (unsigned i = 0, ni = avoidPos.size(); i < ni; ++i)
	{
		const float radSq = sqr(avoidPos[i].second + rad);
		if (Distance::Point_Lineseg2DSq(avoidPos[i].first, movement, t) < radSq)
			return true;
	}
	return false;
}

//====================================================================
// OverlapCircleAgaintsAvoidPos
//====================================================================
bool COPSeekCover::OverlapCircleAgaintsAvoidPos(const Vec3& pos, float rad, const std::vector<Vec3FloatPair>& avoidPos)
{
	for (unsigned i = 0, ni = avoidPos.size(); i < ni; ++i)
	{
		const float radSq = sqr(avoidPos[i].second + rad);
		if (Distance::Point_Point2DSq(avoidPos[i].first, pos) < radSq)
			return true;
	}
	return false;
}

//====================================================================
// GetNearestPuppets
//====================================================================
void COPSeekCover::GetNearestPuppets(CPuppet* pSelf, const Vec3& pos, float radius, std::vector<Vec3FloatPair>& positions)
{
	const float radiusSq = sqr(radius);
	const VectorSet<CPuppet*>& enabledPuppetsSet = GetAISystem()->GetEnabledPuppetSet();
	for (VectorSet<CPuppet*>::const_iterator it = enabledPuppetsSet.begin(), itend = enabledPuppetsSet.end(); it != itend; ++it)
	{
		CPuppet* pPuppet= *it;
		if (pPuppet == pSelf) continue;
		Vec3 delta = pPuppet->GetPhysicsPos() - pos;
		if (delta.GetLengthSquared2D() < radiusSq && delta.z >= -1.0f && delta.z < 2.0f)
		{
			const float passRadius = pPuppet->GetParameters().m_fPassRadius;
			positions.push_back(std::make_pair(pPuppet->GetPos(), passRadius));

			if (Distance::Point_Point2DSq(pPuppet->m_Path.GetLastPathPos(), pos) < radiusSq)
				positions.push_back(std::make_pair(pPuppet->m_Path.GetLastPathPos(), passRadius));
		}
	}
}

//====================================================================
// CalcStrafeCoverTree
//====================================================================
bool COPSeekCover::IsInDeepWater(const Vec3& refPos)
{
	float terrainLevel = gEnv->p3DEngine->GetTerrainElevation(refPos.x, refPos.y);
	if (refPos.z + 2.0f < terrainLevel)
	{
		// Completely under terrain, skip water level checks.
		// This is not consistent with the player code, but for performance reasons do not do the ray check here.
		return false;
	}
	float waterLevel = gEnv->p3DEngine->GetWaterLevel(&refPos);
	return (waterLevel - terrainLevel) > 0.8f;
}

//====================================================================
// CalcStrafeCoverTree
//====================================================================
void COPSeekCover::CalcStrafeCoverTree(SAIStrafeCoverPos& pos, int i, int j,
																			 const Vec3& center, const Vec3& forw, const Vec3& right, float radius,
																			 const Vec3& target, IAISystem::tNavCapMask navCap, float passRadius, bool insideSoftCover,
																			 const std::vector<Vec3FloatPair>& avoidPos, const std::vector<Vec3FloatPair>& softCoverPos)
{
	if (i >= m_iterations)
		return;

	unsigned n = 1 << (i+1);
	unsigned b = j * 2;
	for (unsigned k = 0; k < 2; ++k)
	{
		float u = Ease((float)(i+1) / (float)m_iterations) * radius * (0.9f + 0.1f*ai_frand());
		float v = ((float)(b+k) - ((n-1)/2.0f));
		float a = v / (float)n*gf_PI*(0.75f + ai_frand()*0.05f);
		float x = cosf(a) * u;
		float y = sinf(a) * u;
		Vec3 p = center + x*right + y*forw;
		p.z = pos.pos.z + 0.5f;

		IAISystem::ENavigationType navType;
		bool overlapSoftCover = false;
		if (!insideSoftCover)
			overlapSoftCover = OverlapSegmentAgainstAvoidPos(pos.pos, p, passRadius, softCoverPos);

		if (!OverlapSegmentAgainstAvoidPos(pos.pos, p, passRadius, avoidPos) && !overlapSoftCover &&
			IsSegmentValid(navCap, passRadius, pos.pos, p, navType))
		{
			insideSoftCover = OverlapCircleAgaintsAvoidPos(p, passRadius/2, softCoverPos);
			bool valid = !insideSoftCover && !OverlapSegmentAgainstAvoidPos(target, p, 0.0f, avoidPos);
			bool vis = IsTargetVisibleFrom(p, target);

			if (m_uncover)
			{
				pos.child.push_back(SAIStrafeCoverPos(p, !vis, valid, navType));
				if (!vis || !valid)
					CalcStrafeCoverTree(pos.child.back(), i+1, b+k, center, forw, right, radius, target, navCap, passRadius, insideSoftCover, avoidPos, softCoverPos);
			}
			else
			{
				pos.child.push_back(SAIStrafeCoverPos(p, vis, valid, navType));
				if (vis || !valid)
					CalcStrafeCoverTree(pos.child.back(), i+1, b+k, center, forw, right, radius, target, navCap, passRadius, insideSoftCover, avoidPos, softCoverPos);
			}

		}
		else
		{
			Vec3 p2 = (p+pos.pos)*0.5f;

			overlapSoftCover = false;
			if (!insideSoftCover)
				overlapSoftCover = OverlapSegmentAgainstAvoidPos(pos.pos, p2, passRadius, softCoverPos);

			if (!OverlapSegmentAgainstAvoidPos(pos.pos, p2, passRadius, avoidPos) && !overlapSoftCover &&
					IsSegmentValid(navCap, passRadius, pos.pos, p2, navType))
			{
				insideSoftCover = OverlapCircleAgaintsAvoidPos(p, passRadius/2, softCoverPos);
				bool valid = !insideSoftCover && !OverlapSegmentAgainstAvoidPos(target, p2, 0.0f, avoidPos);
				bool vis = IsTargetVisibleFrom(p2, target);

				if (m_uncover)
				{
					pos.child.push_back(SAIStrafeCoverPos(p2, !vis, valid, navType));
				}
				else
				{
					pos.child.push_back(SAIStrafeCoverPos(p2, vis, valid, navType));
				}
			}
		}
	}

	return;
}

//====================================================================
// GetNearestHidden
//====================================================================
COPSeekCover::SAIStrafeCoverPos* COPSeekCover::GetNearestHidden(SAIStrafeCoverPos& pos, const Vec3& from, float& minDist)
{
	SAIStrafeCoverPos* minPos = 0;
	for (unsigned i = 0, ni = pos.child.size(); i < ni; ++i)
	{
		float d = minDist;
		SAIStrafeCoverPos* res = GetNearestHidden(pos.child[i], from, d);
		if (res && d < minDist)
		{
			minPos = res;
			minDist = d;
		}

		if (pos.child[i].valid && !pos.child[i].vis)
		{
			d = Distance::Point_PointSq(from, pos.child[i].pos);
			if (d < minDist)
			{
				minPos = &pos.child[i];
				minDist = d;
			}
		}
	}
	return minPos;
}

//====================================================================
// GetFurthestFromTarget
//====================================================================
COPSeekCover::SAIStrafeCoverPos* COPSeekCover::GetFurthestFromTarget(SAIStrafeCoverPos& pos, const Vec3& center, const Vec3& forw, const Vec3& right, float& maxDist)
{
	SAIStrafeCoverPos* maxPos = 0;
	for (unsigned i = 0, ni = pos.child.size(); i < ni; ++i)
	{
		float d = maxDist;
		SAIStrafeCoverPos* res = GetFurthestFromTarget(pos.child[i], center, forw, right, d);
		if (res && d > maxDist)
		{
			maxPos = res;
			maxDist = d;
		}

		if (pos.child[i].valid)
		{
			//		d = Distance::Point_PointSq(target, pos.child[i].pos);
			d = fabsf(right.Dot(pos.child[i].pos - center)) + max(0.0f, -forw.Dot(pos.child[i].pos - center));
			if (d > maxDist)
			{
				maxPos = &pos.child[i];
				maxDist = d;
			}
		}
	}
	return maxPos;
}

//====================================================================
// GetNearestToTarget
//====================================================================
COPSeekCover::SAIStrafeCoverPos* COPSeekCover::GetNearestToTarget(SAIStrafeCoverPos& pos, const Vec3& center, const Vec3& forw, const Vec3& right, float& minDist)
{
	SAIStrafeCoverPos* minPos = 0;
	for (unsigned i = 0, ni = pos.child.size(); i < ni; ++i)
	{
		float d = minDist;
		SAIStrafeCoverPos* res = GetNearestToTarget(pos.child[i], center, forw, right, d);
		if (res && d < minDist)
		{
			minPos = res;
			minDist = d;
		}

		if (pos.child[i].valid)
		{
			d = Distance::Point_PointSq(center, pos.child[i].pos);
			if (d < minDist)
			{
				minPos = &pos.child[i];
				minDist = d;
			}
		}
	}
	return minPos;
}

//====================================================================
// GetPathTo
//====================================================================
bool COPSeekCover::GetPathTo(SAIStrafeCoverPos& pos, SAIStrafeCoverPos* pTarget, CNavPath& path, const Vec3& dirToTarget, IAISystem::tNavCapMask navCap, float passRadius)
{
	for (unsigned i = 0, ni = pos.child.size(); i < ni; ++i)
	{
		if (&pos.child[i] == pTarget)
		{
			// Try to assure that the point at the end of the path is a point where the character can aim.
			Vec3 adjPos = pos.child[i].pos;
			GetAISystem()->AdjustDirectionalCoverPosition(adjPos, dirToTarget, 0.5f, 0.7f);

			IAISystem::ENavigationType navType;
			if (!IsSegmentValid(navCap, passRadius, pos.pos, adjPos, navType))
			{
				adjPos = pos.child[i].pos;
				navType = pos.child[i].navType;
			}

			path.PushFront(PathPointDescriptor(navType, adjPos));

			return true;
		}
		if (GetPathTo(pos.child[i], pTarget, path, dirToTarget, navCap, passRadius))
		{
			path.PushFront(PathPointDescriptor(pos.child[i].navType, pos.child[i].pos));
			return true;
		}
	}
	return false;
}

//====================================================================
// UsePath
//====================================================================
void COPSeekCover::UsePath(CPipeUser* pOperand, SAIStrafeCoverPos* pPos, IAISystem::ENavigationType navType)
{
	GetAISystem()->CancelAnyPathsFor(pOperand);
	pOperand->ClearPath("COPSeekCover::Execute generate path");

	m_target = pPos->pos;

	Vec3 opPos = pOperand->GetPhysicsPos();

	SNavPathParams params(opPos, pPos->pos);
	params.precalculatedPath = true;
	pOperand->m_Path.SetParams(params);

	IAISystem::tNavCapMask navCap = pOperand->m_movementAbility.pathfindingProperties.navCapMask;
	const float passRadius = pOperand->GetParameters().m_fPassRadius;

	GetPathTo(*m_pRoot, pPos, pOperand->m_Path, m_forward, navCap, passRadius);

	pOperand->m_nPathDecision = PATHFINDER_PATHFOUND;
	pOperand->m_Path.PushFront(PathPointDescriptor(navType, opPos));

	pOperand->m_OrigPath = pOperand->m_Path;

	if (CPuppet* pPuppet = pOperand->CastToCPuppet())
	{
		// Update adaptive urgency control before the path gets processed further
		pPuppet->AdjustMovementUrgency(pPuppet->m_State.fMovementUrgency,
			pPuppet->m_Path.GetPathLength(pPuppet->IsUsing3DNavigation()));
	}
}

//====================================================================
// Execute
//====================================================================
EGoalOpResult COPSeekCover::Execute(CPipeUser *pOperand)
{
	if (!m_pTraceDirective)
	{
		FRAME_PROFILER( "SeekCover/CalculatePathTree", gEnv->pSystem, PROFILE_AI);

		if (m_state >= 0)
		{
			IAIObject* pTarget = pOperand->GetAttentionTarget();
			if (!pTarget && m_useLastOpAsBackup)
				pTarget = pOperand->m_pLastOpResult;

			if (!pTarget)
			{
				Reset(pOperand);
				return AIGOR_FAILED;
			}

			Vec3 center = pOperand->GetPhysicsPos();
			Vec3 target = pTarget->GetPos();

			Vec3 forw, right;
			float radiusLeft = m_radius;
			float radiusRight = m_radius;

			forw = target - center;
			forw.z = 0;
			forw.Normalize();
			right.Set(forw.y, -forw.x, 0);

			const Vec3& refPointPos = pOperand->GetRefPoint()->GetPos();

			if (m_towardsLastOpResult)
			{
				// Make the side which is towards the refpoint more dominant.
				if (right.Dot(refPointPos - center) > 0.0f)
					radiusLeft *= 0.5f;
				else
					radiusRight *= 0.5f;
			}

			// Choose the side first which is away from the target
			const Vec3& targetView = pTarget->GetViewDir();
			if (right.Dot(targetView) > 0.0f)
				right = -right;

			m_center = center;
			m_forward = forw;
			m_right = right;

			IAISystem::tNavCapMask navCap = pOperand->m_movementAbility.pathfindingProperties.navCapMask;
			const float passRadius = pOperand->GetParameters().m_fPassRadius;

			int nBuildingID;
			IVisArea* pVisArea;
			IAISystem::ENavigationType navType = GetAISystem()->CheckNavigationType(center, nBuildingID, pVisArea, navCap);

			if (m_state == 0)
			{
				m_avoidPos.clear();
				GetNearestPuppets(pOperand->CastToCPuppet(), pOperand->GetPhysicsPos(), m_radius + 2.0f, m_avoidPos);

				// Get soft vegetation from navigation
				if (navType == IAISystem::NAV_TRIANGULAR)
				{
					m_softCoverPos.clear();
					GetAISystem()->GetObstaclePositionsInRadius(pOperand->GetPhysicsPos(), m_radius+2.0f, 0.4f, OBSTACLES_SOFT_COVER, m_softCoverPos);
				}

				// Iteration 0
				if (m_pRoot)
					delete m_pRoot;
				m_pRoot = new SAIStrafeCoverPos(center, false, true, navType);

				bool insideSoftCover = OverlapCircleAgaintsAvoidPos(center, passRadius/2, m_softCoverPos);
				CalcStrafeCoverTree(*m_pRoot, 0, 0, center, forw, right, radiusRight, target, navCap, passRadius,
					insideSoftCover, m_avoidPos, m_softCoverPos);	// right

				// Check to see if we can use the path already
				// Try to get to hidden point first
				float dist = FLT_MAX;
				SAIStrafeCoverPos* nearestHidden = GetNearestHidden(*m_pRoot, center, dist);
				if (nearestHidden)
				{
					UsePath(pOperand, nearestHidden, navType);
					m_pTraceDirective = new COPTrace(false, m_endAccuracy);
					m_lastTime = GetAISystem()->GetFrameStartTime();
					// Done calculating
					m_state = -1;
				}
				else
				{
					m_state++;
					return AIGOR_IN_PROGRESS;
				}
			}
			else if (m_state == 1)
			{
				// Iteration 1
				bool insideSoftCover = OverlapCircleAgaintsAvoidPos(center, passRadius/2, m_softCoverPos);
				CalcStrafeCoverTree(*m_pRoot, 0, 0, center, forw, -right, radiusLeft, target, navCap, passRadius,
					insideSoftCover, m_avoidPos, m_softCoverPos);	// left

				// Try to get to hidden point first
				float dist = FLT_MAX;
				SAIStrafeCoverPos* nearestHidden = GetNearestHidden(*m_pRoot, center, dist);
				if (nearestHidden)
				{
					UsePath(pOperand, nearestHidden, navType);
				}
				else
				{
					// If no hidden points available, try to get as far from the target as possible.
					SAIStrafeCoverPos* furthest = 0;
					if (m_uncover)
					{
						dist = FLT_MAX;
						furthest = GetNearestToTarget(*m_pRoot, center, forw, right, dist);
					}
					else
					{
						dist = -FLT_MAX;
						furthest = GetFurthestFromTarget(*m_pRoot, center, forw, right, dist);
					}

					if (furthest)
					{
						UsePath(pOperand, furthest, navType);
					}
					else
					{
						if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
							AIWarning("COPSeekCover::Entity %s could not find path.", pOperand->GetName());
						Reset(pOperand);
						return AIGOR_FAILED;
					}
				}

				m_pTraceDirective = new COPTrace(false, m_endAccuracy);
				m_lastTime = GetAISystem()->GetFrameStartTime();
				// Done calculating
				m_state = -1;
			}
		}
	}
	AIAssert(m_pTraceDirective);
	EGoalOpResult done = m_pTraceDirective->Execute(pOperand);
	if (m_pTraceDirective == NULL)
		return AIGOR_IN_PROGRESS;

	AIAssert(m_pTraceDirective);

	// HACK: The following code tries to put a lost or stuck agent back on track.
	// It works together with a piece of in ExecuteDry which tracks the speed relative
	// to the requested speed and if it drops dramatically for certain time, this code
	// will trigger and try to move the agent back on the path. [Mikko]

	float timeout = 1.5f;
	if (pOperand->GetType() == AIOBJECT_VEHICLE )
		timeout = 7.0f;

	if (m_notMovingTime > timeout)
	{
		// Stuck or lost, move to the nearest point on path.
		AIWarning("COPSeekCover::Entity %s has not been moving fast enough for %.1fs it might be stuck, abort.", pOperand->GetName(), m_notMovingTime);
		Reset(pOperand);
		return AIGOR_FAILED;
	}

	if (done != AIGOR_IN_PROGRESS)
	{
		Reset(pOperand);
		return done;
	}
	if (!m_pTraceDirective)
	{
		Reset(pOperand);
		return AIGOR_FAILED;
	}

	return AIGOR_IN_PROGRESS;
}

//====================================================================
// ExecuteDry
//====================================================================
void COPSeekCover::ExecuteDry(CPipeUser *pOperand) 
{
	CAISystem *pSystem = GetAISystem();

	if (m_pTraceDirective)
	{
		m_pTraceDirective->ExecuteTrace(pOperand, false);

		// HACK: The following code together with some logic in the execute tries to keep track
		// if the agent is not moving for some time (is stuck), and pathfinds back to the path. [Mikko]
		CTimeValue	time(pSystem->GetFrameStartTime());
		float	dt((time - m_lastTime).GetSeconds());
		float	speed = pOperand->GetVelocity().GetLength();
		float	desiredSpeed = pOperand->m_State.fDesiredSpeed;
		if(desiredSpeed > 0.0f)
		{
			float ratio = clamp(speed / desiredSpeed, 0.0f, 1.0f);
			if(ratio < 0.1f)
				m_notMovingTime += dt;
			else
				m_notMovingTime -= dt;
			if(m_notMovingTime < 0.0f)
				m_notMovingTime = 0.0f;
		}
		m_lastTime = time;
	}
}


//===================================================================
// DebugDraw
//===================================================================
void COPSeekCover::DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const
{
	if (m_pTraceDirective)
		m_pTraceDirective->DebugDraw(pOperand, pRenderer);

	Vec3 target;
	if (pOperand->GetAttentionTarget())
		target = pOperand->GetAttentionTarget()->GetPos();
	else
		target = pOperand->GetPos() + pOperand->GetBodyDir() * 10.0f;

	if (m_pRoot)
	{
		pRenderer->GetIRenderAuxGeom()->DrawLine(m_center, ColorB(255,32,32), m_center + m_right*4.0f, ColorB(255,32,32), 4.0f);
		pRenderer->GetIRenderAuxGeom()->DrawLine(m_center, ColorB(32,255,32), m_center + m_forward*4.0f, ColorB(32,255,32), 4.0f);

		DrawStrafeCoverTree(pRenderer, *m_pRoot, target);
	}

	pRenderer->GetIRenderAuxGeom()->DrawCone(m_target+Vec3(0,0,1), Vec3(0,0,-1), 0.15f, 0.6f, ColorB(255,255,255));

	for (unsigned i = 0, ni = m_avoidPos.size(); i < ni; ++i)
		DebugDrawCircleOutline(pRenderer, m_avoidPos[i].first, m_avoidPos[i].second, ColorB(255,0,0));

	for (unsigned i = 0, ni = m_softCoverPos.size(); i < ni; ++i)
		DebugDrawCircleOutline(pRenderer, m_softCoverPos[i].first, m_softCoverPos[i].second, ColorB(196,255,0));
}

//===================================================================
// DrawStrafeCoverTree
//===================================================================
void COPSeekCover::DrawStrafeCoverTree(IRenderer *pRenderer, SAIStrafeCoverPos& pos, const Vec3& target) const
{
	for (unsigned i = 0, ni = pos.child.size(); i < ni; ++i)
	{
		unsigned char a = pos.child[i].valid ? 255: 120;
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos.pos, ColorB(255,255,255), pos.child[i].pos, ColorB(255,255,255));
		pRenderer->GetIRenderAuxGeom()->DrawSphere(pos.child[i].pos, 0.15f, ColorB(255,255,255,a));
		if (!pos.child[i].vis)
			pRenderer->GetIRenderAuxGeom()->DrawSphere(pos.child[i].pos+Vec3(0,0,0.7f), 0.25f, ColorB(255,0,0));
		//		else
		//			pRenderer->GetIRenderAuxGeom()->DrawLine(pos.child[i].pos+Vec3(0,0,0.7f), ColorB(255,255,255), target, ColorB(255,255,255));

		DrawStrafeCoverTree(pRenderer, pos.child[i], target);
	}
}

//===================================================================
// COPProximity
//===================================================================
COPProximity::COPProximity(float radius, const string& signalName, bool signalWhenDisabled, bool visibleTargetOnly) :
m_radius(radius),
m_signalName(signalName),
m_signalWhenDisabled(signalWhenDisabled),
m_visibleTargetOnly(visibleTargetOnly),
m_triggered(false),
m_pProxObject(0)
{
}

//===================================================================
// ~COPProximity
//===================================================================
COPProximity::~COPProximity()
{
}

//===================================================================
// Execute
//===================================================================
EGoalOpResult COPProximity::Execute(CPipeUser *pOperand)
{
	if (m_triggered)
		return AIGOR_SUCCEED;

	CPuppet* pOperandPuppet = pOperand->CastToCPuppet();

	CAIObject* pTarget = (CAIObject*)pOperand->GetAttentionTarget();
	if (!pTarget)
	{
		Reset(pOperand);
		return AIGOR_FAILED;
	}

	if (!m_pProxObject)
	{
		m_pProxObject = (CAIObject*)pOperand->GetLastOpResult();
		if (!m_pProxObject)
		{
			Reset(pOperand);
			return AIGOR_FAILED;
		}
		if (m_radius < 0.0f)
			m_radius = m_pProxObject->GetRadius();
	}

	bool trigger = false;
	CPipeUser* pProxPipeuser = m_pProxObject->CastToCPipeUser();
	CPipeUser* pTargetPipeuser = m_pProxObject->CastToCPipeUser();

	bool b3D = (pTargetPipeuser && pTargetPipeuser->IsUsing3DNavigation() ) || (pProxPipeuser && pProxPipeuser->IsUsing3DNavigation());
	float distSqr = b3D? Distance::Point_PointSq(pTarget->GetPos(), m_pProxObject->GetPos()) : 
		Distance::Point_Point2DSq(pTarget->GetPos(), m_pProxObject->GetPos());

	if (m_signalWhenDisabled && !m_pProxObject->IsEnabled())
		trigger = true;

	if (!m_visibleTargetOnly || (m_visibleTargetOnly && pOperand->GetAttentionTargetType() == AITARGET_VISUAL))
	{
		if (distSqr < sqr(m_radius))
			trigger = true;
	}

	if (trigger)
	{
		AISignalExtraData* pData = new AISignalExtraData;
		pData->fValue = sqrtf(distSqr);
		pOperand->SetSignal(0, m_signalName.c_str(), pOperand->GetEntity(), pData);
		m_triggered = true;
		return AIGOR_SUCCEED;
	}

	return AIGOR_IN_PROGRESS;
}

//===================================================================
// Reset
//===================================================================
void COPProximity::OnObjectRemoved(CAIObject *pObject)
{
	if (m_pProxObject == pObject)
		m_pProxObject = 0;
}

//===================================================================
// Reset
//===================================================================
void COPProximity::Reset(CPipeUser *pOperand)
{
	m_triggered = false;
	m_pProxObject = 0;
}

//===================================================================
// Serialize
//===================================================================
void COPProximity::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup( "AICOPProximity" );
	{
		ser.Value("m_radius",m_radius);
		ser.Value("m_triggered",m_triggered);
		ser.Value("m_signalName",m_signalName);
		ser.Value("m_signalWhenDisabled",m_signalWhenDisabled);
		ser.Value("m_visibleTargetOnly",m_visibleTargetOnly);
		objectTracker.SerializeObjectPointer(ser, "m_pProxObject", m_pProxObject, false);
	}
	ser.EndGroup();
}

//===================================================================
// DebugDraw
//===================================================================
void COPProximity::DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const
{
	if (m_pProxObject)
	{
		DebugDrawWireSphere(pRenderer, m_pProxObject->GetPos(), m_radius, ColorB(255,255,255));

		CPuppet* pOperandPuppet = pOperand->CastToCPuppet();
		if (!pOperandPuppet)
			return;
		CAIObject* pTarget = (CAIObject*)pOperand->GetAttentionTarget();
		if (pTarget)
		{
			Vec3 delta = pTarget->GetPos() - m_pProxObject->GetPos();
			delta.NormalizeSafe();
			delta *= m_radius;
			ColorB col;
			if (pOperand->GetAttentionTargetType() == AITARGET_VISUAL)
				col.Set(255,255,255,255);
			else
				col.Set(255,255,255,128);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(m_pProxObject->GetPos()+delta, 0.25f, col);
			pRenderer->GetIRenderAuxGeom()->DrawLine(m_pProxObject->GetPos()+delta, col, pTarget->GetPos(), col);
		}
	}
}

//===================================================================
// COPMoveTowards
//===================================================================
COPMoveTowards::COPMoveTowards(float distance, float duration) :
m_distance(distance),
m_duration(duration),
m_pPathfindDirective(0),
m_pTraceDirective(0),
m_looseAttentionId(0),
m_moveDist(0),
m_moveSearchRange(0),
m_moveStart(0,0,0),
m_moveEnd(0,0,0)
{
}

//===================================================================
// COPBackoff
//===================================================================
COPMoveTowards::~COPMoveTowards()
{
	Reset(0);
}

//===================================================================
// OnObjectRemoved
//===================================================================
void COPMoveTowards::OnObjectRemoved(CAIObject *pObject)
{
	if (m_pTraceDirective)
		m_pTraceDirective->OnObjectRemoved(pObject);
	if (m_pPathfindDirective)
		m_pPathfindDirective->OnObjectRemoved(pObject);
}


//===================================================================
// Reset
//===================================================================
void COPMoveTowards::Reset(CPipeUser *pOperand)
{
	ResetNavigation(pOperand);
}

//===================================================================
// ResetNavigation
//===================================================================
void COPMoveTowards::ResetNavigation(CPipeUser *pOperand)
{
	if (m_pPathfindDirective)
		delete m_pPathfindDirective;
	m_pPathfindDirective = 0;
	if (m_pTraceDirective)
		delete m_pTraceDirective;
	m_pTraceDirective = 0;

	if (pOperand)
	{
		GetAISystem()->CancelAnyPathsFor(pOperand);
		pOperand->ClearPath("COPBackoff::Reset m_Path");
	}

	if (m_looseAttentionId)
	{
		if (pOperand)
			pOperand->SetLooseAttentionTarget(0,m_looseAttentionId);
		m_looseAttentionId = 0;
	}
}

//===================================================================
// Execute
//===================================================================
void COPMoveTowards::ExecuteDry(CPipeUser *pOperand)
{
	if (m_pTraceDirective)
		m_pTraceDirective->ExecuteTrace(pOperand, false);
}

//===================================================================
// GetEndDistance
//===================================================================
float COPMoveTowards::GetEndDistance(CPipeUser *pOperand) const
{
	if (m_duration > 0.0f)
	{
		float normalSpeed, minSpeed, maxSpeed;
		pOperand->GetMovementSpeedRange(pOperand->m_State.fMovementUrgency, false, normalSpeed, minSpeed, maxSpeed);
		//		float normalSpeed = pOperand->GetNormalMovementSpeed(pOperand->m_State.fMovementUrgency, false);
		if (normalSpeed > 0.0f)
			return normalSpeed * m_duration;
	}
	return m_distance;
}

//===================================================================
// Execute
//===================================================================
EGoalOpResult COPMoveTowards::Execute(CPipeUser *pOperand)
{ 

	if (!m_pPathfindDirective)
	{
		CAIObject* pTarget = pOperand->m_pLastOpResult;
		if (!pTarget)
		{
			Reset(pOperand);
			return AIGOR_FAILED;
		}

		Vec3 operandPos = pOperand->GetPhysicsPos();
		Vec3 targetPos = pTarget->GetPhysicsPos();

		float moveDist = GetEndDistance(pOperand);
		float searchRange = Distance::Point_Point(operandPos, targetPos);
		//		moveDist = min(moveDist, searchRange);
		searchRange = max(searchRange, moveDist);
		searchRange *= 2.0f;

		// start pathfinding
		GetAISystem()->CancelAnyPathsFor(pOperand);
		m_pPathfindDirective = new COPPathFind("MoveTowards", pTarget, std::numeric_limits<float>::max(),
			-moveDist, false, searchRange);
		pOperand->m_nPathDecision = PATHFINDER_STILLFINDING;

		// Store params for debug drawing.
		m_moveStart = operandPos;
		m_moveEnd = targetPos;
		m_moveDist = moveDist;
		m_moveSearchRange = searchRange;

		if (m_pPathfindDirective->Execute(pOperand) != AIGOR_IN_PROGRESS)
		{
			if (pOperand->m_nPathDecision == PATHFINDER_NOPATH)
			{
				ResetNavigation(pOperand);	
				return AIGOR_FAILED;
			}
		}
		return AIGOR_IN_PROGRESS;
	}


	if (pOperand->m_nPathDecision == PATHFINDER_PATHFOUND)
	{
		if (pOperand->m_Path.GetPath().size() == 1)
		{
			AIWarning("COPMoveTowards::Entity %s Path has only one point.", pOperand->GetName());
			return AIGOR_FAILED;
		}

		if (!m_pTraceDirective)
		{
			m_pTraceDirective = new COPTrace(false, defaultTraceEndAccuracy);
			//			pOperand->m_Path.GetParams().precalculatedPath = true; // prevent path regeneration
		}

		// keep tracing
		EGoalOpResult done = m_pTraceDirective->Execute(pOperand);
		if (done != AIGOR_IN_PROGRESS)
		{
			Reset(pOperand);
			return done;
		}
		// If this goal gets reseted during m_pTraceDirective->Execute it means that
		// a smart object was used for navigation which inserts a goal pipe which
		// does Reset on this goal which sets m_pTraceDirective to NULL! In this case
		// we should just report that this goal pipe isn't finished since it will be
		// reexecuted after finishing the inserted goal pipe
		if (!m_pTraceDirective)
			return AIGOR_IN_PROGRESS;
	}
	else if (pOperand->m_nPathDecision == PATHFINDER_NOPATH)
	{
		Reset(pOperand);	
		return AIGOR_FAILED;
	}
	else
	{
		m_pPathfindDirective->Execute(pOperand);
	}

	return AIGOR_IN_PROGRESS;
}

//===================================================================
// Serialize
//===================================================================
void COPMoveTowards::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPMoveTowards");
	{
		ser.Value("m_distance", m_distance);
		ser.Value("m_duration", m_duration);
		ser.Value("m_moveStart", m_moveStart);
		ser.Value("m_moveEnd", m_moveEnd);
		ser.Value("m_moveDist", m_moveDist);
		ser.Value("m_moveSearchRange", m_moveSearchRange);

		if(ser.IsWriting())
		{
			if (ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective != NULL))
			{
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			if (ser.BeginOptionalGroup("PathFindDirective", m_pPathfindDirective != NULL))
			{
				m_pPathfindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}		
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if (ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(true);
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
			SAFE_DELETE(m_pPathfindDirective);
			if (ser.BeginOptionalGroup("PathFindDirective", true))
			{
				m_pPathfindDirective = new COPPathFind("");
				m_pPathfindDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
	}
	ser.EndGroup();
}

//===================================================================
// DebugDraw
//===================================================================
void COPMoveTowards::DebugDraw(CPipeUser* pOperand, IRenderer* pRenderer) const
{
	Vec3	dir(m_moveEnd - m_moveStart);
	dir.Normalize();

	Vec3 start = m_moveStart;
	Vec3 end = m_moveEnd;
	start.z += 0.5f;
	end.z += 0.5f;

	DebugDrawRangeArc(pRenderer, start, dir, DEG2RAD(60.0f), m_moveDist, m_moveDist-0.5f, ColorB(255,255,255,16), ColorB(255,255,255), true);
	DebugDrawRangeArc(pRenderer, start, dir, DEG2RAD(60.0f), m_moveSearchRange, 0.2f, ColorB(255,0,0,128), ColorB(255,0,0), true);

	pRenderer->GetIRenderAuxGeom()->DrawLine(start, ColorB(255,255,255), end, ColorB(255,255,255));
}


//====================================================================
// COPDodge
//====================================================================
COPDodge::COPDodge(float distance, bool useLastOpAsBackup) :
m_distance(distance),
m_useLastOpAsBackup(useLastOpAsBackup),
m_notMovingTime(0.0f)
{
	m_pTraceDirective = 0;
	m_basis.SetIdentity();
	m_targetPos.Set(0,0,0);
	m_targetView.Set(0,0,0);
}

//====================================================================
// ~COPDodge
//====================================================================
COPDodge::~COPDodge()
{
	Reset(0);
}

//====================================================================
// Reset
//====================================================================
void COPDodge::Reset(CPipeUser *pOperand)
{
	delete m_pTraceDirective;
	m_pTraceDirective = 0;
	m_notMovingTime = 0;
	m_basis.SetIdentity();
	m_targetPos.Set(0,0,0);
	m_targetView.Set(0,0,0);
	if (pOperand)
		pOperand->ClearPath("COPDodge::Reset");
}

//====================================================================
// Serialize
//====================================================================
void COPDodge::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("COPDodge");
	{
		ser.Value("m_distance", m_distance);
		ser.Value("m_useLastOpAsBackup", m_useLastOpAsBackup);
		ser.Value("m_lastTime", m_lastTime);
		ser.Value("m_notMovingTime", m_notMovingTime);
		ser.Value("m_endAccuracy", m_endAccuracy);
		if(ser.IsWriting())
		{
			if(ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective!=NULL))
			{
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if(ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(false, m_endAccuracy);
				m_pTraceDirective->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		ser.EndGroup();
	}
}

//====================================================================
// CheckSegmentAgainstAvoidPos
//====================================================================
bool COPDodge::OverlapSegmentAgainstAvoidPos(const Vec3& from, const Vec3& to, float rad, const std::vector<Vec3FloatPair>& avoidPos)
{
	Lineseg	movement(from, to);
	float t;
	for (unsigned i = 0, ni = avoidPos.size(); i < ni; ++i)
	{
		const float radSq = sqr(avoidPos[i].second + rad);
		if (Distance::Point_LinesegSq(avoidPos[i].first, movement, t) < radSq)
			return true;
	}
	return false;
}

//====================================================================
// GetNearestPuppets
//====================================================================
void COPDodge::GetNearestPuppets(CPuppet* pSelf, const Vec3& pos, float radius, std::vector<Vec3FloatPair>& positions)
{
	const float radiusSq = sqr(radius);
	const VectorSet<CPuppet*>& enabledPuppetsSet = GetAISystem()->GetEnabledPuppetSet();
	for (VectorSet<CPuppet*>::const_iterator it = enabledPuppetsSet.begin(), itend = enabledPuppetsSet.end(); it != itend; ++it)
	{
		CPuppet* pPuppet= *it;
		if (pPuppet == pSelf) continue;
		Vec3 delta = pPuppet->GetPhysicsPos() - pos;
		if (delta.GetLengthSquared() < radiusSq)
		{
			const float passRadius = pPuppet->GetParameters().m_fPassRadius;
			positions.push_back(std::make_pair(pPuppet->GetPos(), passRadius));

			if (Distance::Point_Point2DSq(pPuppet->m_Path.GetLastPathPos(), pos) < radiusSq)
				positions.push_back(std::make_pair(pPuppet->m_Path.GetLastPathPos(), passRadius));
		}
	}
}


//====================================================================
// Execute
//====================================================================
EGoalOpResult COPDodge::Execute(CPipeUser *pOperand)
{
	if (!m_pTraceDirective)
	{
		FRAME_PROFILER( "Dodge/CalculatePathTree", gEnv->pSystem, PROFILE_AI);

		IAIObject* pTarget = pOperand->GetAttentionTarget();
		if (!pTarget && m_useLastOpAsBackup)
			pTarget = pOperand->m_pLastOpResult;

		if (!pTarget)
		{
			Reset(pOperand);
			return AIGOR_FAILED;
		}

		Vec3 center = pOperand->GetPhysicsPos();
		m_targetPos = pTarget->GetPos();
		m_targetView = pTarget->GetViewDir();

		Vec3 dir = m_targetPos - center;
		dir.Normalize();
		m_basis.SetRotationVDir(dir);

		IAISystem::tNavCapMask navCap = pOperand->m_movementAbility.pathfindingProperties.navCapMask;
		const float passRadius = pOperand->GetParameters().m_fPassRadius;

		int nBuildingID;
		IVisArea* pVisArea;
		IAISystem::ENavigationType navType = GetAISystem()->CheckNavigationType(center, nBuildingID, pVisArea, navCap);

		m_avoidPos.clear();
		GetNearestPuppets(pOperand->CastToCPuppet(), pOperand->GetPhysicsPos(), m_distance + 2.0f, m_avoidPos);

		// Calc path.
		float angleOffset = 0.0f; // ai_frand() * gf_PI;

		const int iters = 7;
		Vec3	pos[iters];

		for (int i = 0; i < iters; ++i)
		{
			float a = ((float)i / (float)iters) * gf_PI2;
			float x = cosf(a);
			float y = sinf(a);
			float d = m_distance * (0.9f + ai_frand()*0.2f);

			Vec3 delta = x*m_basis.GetColumn0() + y*m_basis.GetColumn2() + 0.5f*m_basis.GetColumn1();
			delta.Normalize();
			pos[i] = center + delta * d;

			Vec3 hitPos;
			float hitDist;
			if (IntersectSweptSphere(&hitPos, hitDist, Lineseg(center, pos[i]), passRadius*1.1f, AICE_ALL))
				pos[i] = center + delta * hitDist;

			m_DEBUG_testSegments.push_back(center);
			m_DEBUG_testSegments.push_back(pos[i]);
		}

		Lineseg	targetLOS(m_targetPos, m_targetPos + m_targetView * Distance::Point_Point(center, m_targetPos)*2.0f);
		float t;
		Distance::Point_Lineseg(center, targetLOS, t);
		Vec3 nearestPointOnTargetView = targetLOS.GetPoint(t);

		float bestDist = 0.0f;
		int bestIdx = -1;
		for (int i = 0; i < iters; ++i)
		{
			float d = Distance::Point_PointSq(center, pos[i]);
			if (d > bestDist)
			{
				if (!OverlapSegmentAgainstAvoidPos(center, pos[i], passRadius, m_avoidPos))
				{
					bestDist = d;
					bestIdx = i;
				}
			}
		}

		if (bestIdx == -1)
		{
			Reset(pOperand);
			return AIGOR_FAILED;
		}

		m_bestPos = pos[bestIdx];

		// Set the path
		GetAISystem()->CancelAnyPathsFor(pOperand);
		pOperand->ClearPath("COPDodge::Execute generate path");

		SNavPathParams params(center, m_bestPos);
		params.precalculatedPath = true;
		pOperand->m_Path.SetParams(params);

		pOperand->m_nPathDecision = PATHFINDER_PATHFOUND;
		pOperand->m_Path.PushBack(PathPointDescriptor(navType, center));
		pOperand->m_Path.PushBack(PathPointDescriptor(navType, m_bestPos));

		pOperand->m_OrigPath = pOperand->m_Path;

		if (CPuppet* pPuppet = pOperand->CastToCPuppet())
		{
			// Update adaptive urgency control before the path gets processed further
			pPuppet->AdjustMovementUrgency(pPuppet->m_State.fMovementUrgency,
				pPuppet->m_Path.GetPathLength(pPuppet->IsUsing3DNavigation()));
		}

		m_pTraceDirective = new COPTrace(false, m_endAccuracy);
		m_lastTime = GetAISystem()->GetFrameStartTime();
	}

	AIAssert(m_pTraceDirective);
	EGoalOpResult done = m_pTraceDirective->Execute(pOperand);
	if (m_pTraceDirective == NULL)
		return AIGOR_IN_PROGRESS;

	AIAssert(m_pTraceDirective);

	// HACK: The following code tries to put a lost or stuck agent back on track.
	// It works together with a piece of in ExecuteDry which tracks the speed relative
	// to the requested speed and if it drops dramatically for certain time, this code
	// will trigger and try to move the agent back on the path. [Mikko]

	float timeout = 1.5f;
	if (pOperand->GetType() == AIOBJECT_VEHICLE )
		timeout = 7.0f;

	if (m_notMovingTime > timeout)
	{
		// Stuck or lost, move to the nearest point on path.
		AIWarning("COPDodge::Entity %s has not been moving fast enough for %.1fs it might be stuck, abort.", pOperand->GetName(), m_notMovingTime);
		Reset(pOperand);
		return AIGOR_FAILED;
	}

	if (done != AIGOR_IN_PROGRESS)
	{
		Reset(pOperand);
		return done;
	}
	if (!m_pTraceDirective)
	{
		Reset(pOperand);
		return AIGOR_FAILED;
	}

	return AIGOR_IN_PROGRESS;
}

//====================================================================
// ExecuteDry
//====================================================================
void COPDodge::ExecuteDry(CPipeUser *pOperand) 
{
	CAISystem *pSystem = GetAISystem();

	if (m_pTraceDirective)
	{
		m_pTraceDirective->ExecuteTrace(pOperand, false);

		// HACK: The following code together with some logic in the execute tries to keep track
		// if the agent is not moving for some time (is stuck), and pathfinds back to the path. [Mikko]
		CTimeValue	time(pSystem->GetFrameStartTime());
		float	dt((time - m_lastTime).GetSeconds());
		float	speed = pOperand->GetVelocity().GetLength();
		float	desiredSpeed = pOperand->m_State.fDesiredSpeed;
		if(desiredSpeed > 0.0f)
		{
			float ratio = clamp(speed / desiredSpeed, 0.0f, 1.0f);
			if(ratio < 0.1f)
				m_notMovingTime += dt;
			else
				m_notMovingTime -= dt;
			if(m_notMovingTime < 0.0f)
				m_notMovingTime = 0.0f;
		}
		m_lastTime = time;
	}
}


//===================================================================
// DebugDraw
//===================================================================
void COPDodge::DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const
{
	if (m_pTraceDirective)
		m_pTraceDirective->DebugDraw(pOperand, pRenderer);

	const float passRadius = pOperand->GetParameters().m_fPassRadius;

	Vec3 center = pOperand->GetPhysicsPos();

	for (unsigned i = 0, ni = m_DEBUG_testSegments.size(); i < ni; i += 2)
	{
		pRenderer->GetIRenderAuxGeom()->DrawLine(m_DEBUG_testSegments[i], ColorB(255,255,255), m_DEBUG_testSegments[i+1], ColorB(255,255,255));
		DebugDrawWireSphere(pRenderer, m_DEBUG_testSegments[i+1], passRadius*1.1f, ColorB(255,255,255));
	}

	pRenderer->GetIRenderAuxGeom()->DrawSphere(m_bestPos, 0.2f, ColorB(255,0,0));

	for (unsigned i = 0, ni = m_avoidPos.size(); i < ni; ++i)
		DebugDrawWireSphere(pRenderer, m_avoidPos[i].first, m_avoidPos[i].second, ColorB(255,0,0));
}


