// GoalPipe.cpp: implementation of the CGoalPipe class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CAISystem.h"
#include "AILog.h"
#include "GoalPipe.h"
#include "GoalOp.h"
#include "PipeUser.h"
#include "ObjectTracker.h"

#include <ISystem.h>
#include <ISerialize.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

struct SAIGoalOpName
{
	EGoalOperations op;
	const char* szName;
};

// Note, this table needs to be in the same order as the enum!
static SAIGoalOpName g_GoalOpNames[LAST_AIOP] =
{
	{ AIOP_ACQUIRETARGET, "acqtarget" },
	{ AIOP_LOOKAROUND, 		"lookaround" },
	{ AIOP_APPROACH, 			"approach" },
	{ AIOP_FOLLOW, 				"follow" },
	{ AIOP_FOLLOWPATH, 		"followpath" },
	{ AIOP_BACKOFF, 			"backoff" },
	{ AIOP_FIRECMD, 			"firecmd" },
	{ AIOP_STRAFE, 				"strafe" },
	{ AIOP_BODYPOS, 			"bodypos" },
	{ AIOP_RUN, 					"run" },
	{ AIOP_TIMEOUT, 			"timeout" },
	{ AIOP_PATHFIND, 			"pathfind" },
	{ AIOP_LOCATE, 				"locate" },
	{ AIOP_TRACE, 				"trace" },
	{ AIOP_SIGNAL, 				"signal" },
	{ AIOP_IGNOREALL, 		"ignoreall" },
	{ AIOP_DEVALUE, 			"devalue" },
	{ AIOP_FORGET, 				"forget" },
	{ AIOP_HIDE, 				  "hide" },
	{ AIOP_FORM, 					"form" },
	{ AIOP_STICK, 				"stick" },
	{ AIOP_CLEAR, 				"clear" },
	{ AIOP_BRANCH,  			"branch" },
	{ AIOP_RANDOM,  			"random" },
	{ AIOP_LOOKAT,  			"lookat" },
	{ AIOP_CONTINUOUS, 		"continuous" },
	{ AIOP_MOVE, 					"move" },
	{ AIOP_CHARGE, 				"charge" },
	{ AIOP_WAITSIGNAL, 		"waitsignal" },
	{ AIOP_ANIMATION, 		"animation" },
	{ AIOP_ANIMTARGET, 		"animtarget" },
	{ AIOP_USECOVER, 			"usecover" },
	{ AIOP_WAIT, 					"wait" },
	{ AIOP_ADJUSTAIM, 		"adjustaim" },
	{ AIOP_SEEKCOVER, 		"seekcover" },
	{ AIOP_PROXIMITY, 		"proximity" },
	{ AIOP_MOVETOWARDS,		"movetowards" },
	{ AIOP_DODGE,					"dodge" },
};

static EGoalOperations GetGoalOpEnum(const char* szName)
{
	for (unsigned i = 0; i < LAST_AIOP; ++i)
		if (strcmp(g_GoalOpNames[i].szName, szName) == 0)
			return g_GoalOpNames[i].op;
	return LAST_AIOP;
}

static const char* GetGoalOpName(EGoalOperations op)
{
	if (op == LAST_AIOP)
		return 0;
	return g_GoalOpNames[(int)op].szName;
}


CGoalPipe::CGoalPipe(const string &name, CAISystem *pAISystem, bool bDynamic):
m_sName( name ),
m_nPosition( 0 ),
m_pSubPipe( 0 ), 
m_pArgument( 0 ),
m_bLoop( true ),
m_bDynamic( bDynamic ),
m_nEventId( 0 ),
m_bHighPriority( false ),
m_nCurrentBlockCounter(0)
{
}

CGoalPipe::~CGoalPipe()
{
	if (!m_qGoalPipe.empty())
	{
		m_qGoalPipe.clear();
	}

	if (m_pSubPipe)
	{
		delete m_pSubPipe;
		m_pSubPipe = 0;
	}
}

void CGoalPipe::SetDebugName(const char* name)
{
	m_sDebugName = name;
}

void CGoalPipe::PushLabel(const char* label)
{
	LabelsMap::iterator it = m_Labels.find(CONST_TEMP_STRING( label ));
	if (it != m_Labels.end())
		AIWarning("Label %s already exists in goal pipe %s. Overriding previous label!", label, m_sName.c_str());

	m_Labels[label] = m_qGoalPipe.size();
}

EGoalOperations CGoalPipe::GetGoalOpEnum(const char* szName)
{
	return ::GetGoalOpEnum(szName);
}

const char* CGoalPipe::GetGoalOpName(EGoalOperations op)
{
	return ::GetGoalOpName(op);
}

void CGoalPipe::PushGoal(EGoalOperations name, bool bBlocking, EGroupType eGrouping, GoalParameters &params)
{
	QGoal newgoal;

	if (eGrouping == IGoalPipe::GT_GROUPED && name != AIOP_WAIT)
		++m_nCurrentBlockCounter;
	else if (name != AIOP_WAIT) 
		m_nCurrentBlockCounter = 0;	// continuous group counter.

	switch(name)
	{
	case AIOP_ACQUIRETARGET:
		{
			newgoal.pGoalOp = new COPAcqTarget( params.szString);
		}
		break;
	case AIOP_APPROACH:
		{
			float duration, endDistance;
			if (params.nValue & AI_USE_TIME)
			{
				duration = fabsf(params.fValue);
				endDistance = 0.0f;
			}
			else
			{
				duration = 0.0f;
				endDistance = params.fValue;
			}

			// Random variation on the end distance.
			if (params.m_vPosition.x > 0.01f)
			{
				float u = (ai_rand() % 10) / 9.0f;
				if (endDistance > 0.0f)
					endDistance = max(0.0f, endDistance - u * params.m_vPosition.x);
				else
					endDistance = min(0.0f, endDistance + u * params.m_vPosition.x);
			}

			newgoal.pGoalOp = new COPApproach(endDistance, params.fValueAux, duration,
				(params.nValue & AILASTOPRES_USE) != 0, (params.nValue & AILASTOPRES_LOOKAT) != 0,
				(params.nValue & AI_REQUEST_PARTIAL_PATH)!=0, (params.nValue & AI_STOP_ON_ANIMATION_START)!=0, params.szString);
		}
		break;
	case AIOP_FOLLOW:
		{
			newgoal.pGoalOp = new COPFollow(params.fValue, params.bValue);
		}
		break;
	case AIOP_FOLLOWPATH:
		{
			newgoal.pGoalOp = new COPFollowPath((params.nValue & 1) != 0, (params.nValue & 2) != 0, (params.nValue & 4) != 0, params.nValueAux, params.fValueAux, (params.nValue & 8) != 0, (params.nValue & 16) != 0);
		}
		break;
	case AIOP_BACKOFF:
		{
			newgoal.pGoalOp = new COPBackoff(params.fValue, params.fValueAux, params.nValue);
		}
		break;
	case AIOP_MOVETOWARDS:
		{
			float duration, endDistance;
			if (params.nValue & AI_USE_TIME)
			{
				duration = fabsf(params.fValue);
				endDistance = 0.0f;
			}
			else
			{
				duration = 0.0f;
				endDistance = params.fValue;
			}

			// Random variation on the end distance.
			if (params.m_vPosition.x > 0.01f)
			{
				float u = (ai_rand() % 10) / 9.0f;
				if (endDistance > 0.0f)
					endDistance = max(0.0f, endDistance - u * params.m_vPosition.x);
				else
					endDistance = min(0.0f, endDistance + u * params.m_vPosition.x);
			}

/*			newgoal.pGoalOp = new COPApproach(endDistance, params.fValueAux, duration, (params.nValue & AILASTOPRES_USE) != 0, 
				(params.nValue & AILASTOPRES_LOOKAT) != 0, (params.nValue & AI_REQUEST_PARTIAL_PATH)!=0,
				(params.nValue & AI_STOP_ON_ANIMATION_START)!=0, params.szString);*/

			newgoal.pGoalOp = new COPMoveTowards(endDistance, duration); //, params.nValue);
		}
		break;
	case AIOP_FIRECMD:
		{
			newgoal.pGoalOp = new COPFireCmd(params.nValue, params.bValue, params.fValue, params.fValueAux);
		}
		break;
	case AIOP_BODYPOS:
		{
			int thepos = (int) params.fValue;
			newgoal.pGoalOp = new COPBodyCmd(thepos, params.nValue!=0);
		}
		break;
	case AIOP_STRAFE:
		{
			 newgoal.pGoalOp = new COPStrafe(params.fValue, params.fValueAux, params.bValue);
		}
		break;
	case AIOP_TIMEOUT:
		{	
			if (!GetAISystem()->m_pSystem)
				AIError("CGoalPipe::PushGoal Pushing goals without a valid System instance [Code bug]");

			newgoal.pGoalOp = new COPTimeout(params.fValue, GetAISystem(),params.fValueAux);
		}
		break;
	case AIOP_RUN:
		{
			newgoal.pGoalOp = new COPRunCmd(params.fValue, params.fValueAux, params.m_vPosition.x);
		}
		break;
	case AIOP_LOOKAROUND:
		{
			newgoal.pGoalOp = new COPLookAround(params.fValue, params.fValueAux, params.m_vPosition.x, params.m_vPosition.y, (params.nValue & AI_BREAK_ON_LIVE_TARGET)!=0,(params.nValue & AILASTOPRES_USE)!=0);
		}
		break;
	case AIOP_LOCATE:
		{
			newgoal.pGoalOp = new COPLocate(params.szString.c_str(), params.nValue, params.fValue);
		}
		break;
	case AIOP_PATHFIND:
		{
			newgoal.pGoalOp = new COPPathFind(params.szString.c_str(),params.m_pTarget, 0.0f, 0.0f, params.bValue, (params.nValue & AI_REQUEST_PARTIAL_PATH)!=0);
		}
		break;
	case AIOP_TRACE:
		{
			bool bParam = (params.nValue > 0);
			float duration, endDistance;
			if (params.nValue & AI_USE_TIME)
			{
				duration = fabsf(params.fValue);
				endDistance = 0.0f;
			}
			else
			{
				duration = 0.0f;
				endDistance = params.fValue;
			}
			if (duration > 0.0f || endDistance > 0.0f)
				GetAISystem()->LogComment("CGoalPipe::PushGoal", "%s - Specifying distance/duration in 'trace' goal-op is deprecated.", GetName());
			newgoal.pGoalOp = new COPTrace(bParam, params.fValue, params.nValueAux > 0);
		}
		break;
	case AIOP_IGNOREALL:
		{
			bool bParam = (params.fValue > 0);
			newgoal.pGoalOp = new COPIgnoreAll(bParam);
		}
		break;
	case AIOP_SIGNAL:
		{
			int nParam = (int)params.fValue;
			unsigned char cFilter = (unsigned char)params.nValue;
			newgoal.pGoalOp = new COPSignal(nParam,params.szString.c_str(),cFilter,(int)params.fValueAux);
		}
		break;
	case AIOP_DEVALUE:
		{
			newgoal.pGoalOp = new COPDeValue((int)params.fValue,params.bValue);
		}
		break;
	case AIOP_HIDE:
		{
			newgoal.pGoalOp = new COPHide(params.fValue, params.fValueAux, params.nValue,params.bValue,
				(params.nValueAux & AILASTOPRES_LOOKAT) != 0);
		}
		break;
	case AIOP_STICK:
		{
			float duration, endDistance;
			if (params.nValue & AI_USE_TIME)
			{
				duration = fabsf(params.fValue);
				endDistance = 0.0f;
			}
			else
			{
				duration = 0.0f;
				endDistance = params.fValue;
			}
			
			// Random variation on the end distance.
			if (params.m_vPosition.x > 0.01f)
			{
				float u = (ai_rand() % 10) / 9.0f;
				if (endDistance > 0.0f)
					endDistance = max(0.0f, endDistance - u * params.m_vPosition.x);
				else
					endDistance = min(0.0f, endDistance + u * params.m_vPosition.x);
			}

			// Note: the flag 0x01 means break in a goalpipe description but we change it to continuous flag inside stick goalop.
			newgoal.pGoalOp = new COPStick(endDistance, params.fValueAux, duration,params.nValue,params.nValueAux);
/*			(params.nValue & AILASTOPRES_USE) != 0, (params.nValue & AILASTOPRES_LOOKAT) != 0,
				(params.nValueAux & 0x01) == 0, (params.nValueAux & 0x02) != 0, (params.nValue & AI_REQUEST_PARTIAL_PATH)!=0,
				(params.nValue & AI_STOP_ON_ANIMATION_START)!=0, (params.nValue & AI_CONSTANT_SPEED)!=0);*/

		}
		break;
	case AIOP_FORM:
		{
			newgoal.pGoalOp = new COPForm(params.szString.c_str());
		}
		break;
	case AIOP_CLEAR:
		{
			newgoal.pGoalOp = new COPDeValue();
		}
		break;
	case AIOP_BRANCH:
		{
			newgoal.pGoalOp = new COPDeValue();
			bBlocking = false;
		}
		break;
	case AIOP_RANDOM:
		{
			newgoal.pGoalOp = new COPDeValue();
			bBlocking = false;
		}
		break;
	case AIOP_LOOKAT:
		{
			newgoal.pGoalOp = new COPLookAt(params.fValue, params.fValueAux, params.nValue, params.bValue);
		}
		break;
	case AIOP_CONTINUOUS:
		{
			newgoal.pGoalOp = new COPContinuous( params.bValue );
		}
		break;
	case AIOP_MOVE:
		{
			newgoal.pGoalOp = new COPMove(params.fValue, params.fValueAux,
				(params.nValue & AILASTOPRES_USE) != 0, (params.nValue & AILASTOPRES_LOOKAT) != 0, params.bValue );
		}
		break;
	case AIOP_CHARGE:
		{
			newgoal.pGoalOp = new COPCharge(params.fValue, params.fValueAux,
				(params.nValue & AILASTOPRES_USE) != 0, (params.nValue & AILASTOPRES_LOOKAT) != 0 );
		}
		break;
	case AIOP_ANIMATION:
		{
			newgoal.pGoalOp = new COPAnimation( (EAnimationMode) params.nValue, params.szString );
		}
		break;
	case AIOP_ANIMTARGET:
		{
			newgoal.pGoalOp = new COPAnimTarget( params.bValue, params.szString, params.m_vPosition.x, params.m_vPosition.y, params.m_vPosition.z, params.m_vPositionAux );
		}
		break;
	case AIOP_USECOVER:
		{
			newgoal.pGoalOp = new COPUseCover( params.bValue, params.nValue != 0, params.fValue, params.fValueAux, params.nValueAux != 0 );
		}
		break;
	case AIOP_WAITSIGNAL:
		{
			switch ( int(params.fValue) )
			{
			case 0:
				newgoal.pGoalOp = new COPWaitSignal( params.szString, params.fValueAux );
				break;
			case 1:
				newgoal.pGoalOp = new COPWaitSignal( params.szString, params.szString2, params.fValueAux );
				break;
			case 2:
				newgoal.pGoalOp = new COPWaitSignal( params.szString, params.nValue, params.fValueAux );
				break;
			case 3:
				newgoal.pGoalOp = new COPWaitSignal( params.szString, EntityId(params.nValue), params.fValueAux );
				break;
			default:
				return;
			}
		}
		break;
	case AIOP_WAIT:
		{
			newgoal.pGoalOp = new COPWait(params.nValue, m_nCurrentBlockCounter);
			m_nCurrentBlockCounter = 0;
			bBlocking = true;	// wait always has to be blocking
			eGrouping = IGoalPipe::GT_GROUPED; // wait always needs to be grouped
		}
		break;
	case AIOP_ADJUSTAIM:
		{
			newgoal.pGoalOp = new COPAdjustAim((params.nValue & 0x01) != 0, (params.nValue & 0x02) != 0, (params.nValue & 0x04) != 0);
		}
		break;
	case AIOP_SEEKCOVER:
		{
			newgoal.pGoalOp = new COPSeekCover(params.bValue, params.fValue, params.nValue, (params.nValueAux & 1) != 0, (params.nValueAux & 2) != 0);
		}
		break;
	case AIOP_PROXIMITY:
		{
			newgoal.pGoalOp = new COPProximity(params.fValue, params.szString, (params.nValue & 0x1) != 0, (params.nValue & 0x2) != 0);
		}
		break;
	case AIOP_DODGE:
		{
			newgoal.pGoalOp = new COPDodge(params.fValue, (params.nValueAux & 1) != 0);
		}
		break;
	default:
		// Trying to push undefined/unimplemented goalop.
		AIAssert(0);
	}

	newgoal.op = name;
	newgoal.params = params;
	newgoal.bBlocking = bBlocking;
	newgoal.eGrouping = eGrouping;

	m_qGoalPipe.push_back(newgoal);
}

//
//-------------------------------------------------------------------------------------------------------
void CGoalPipe::PushPipe(const char* szName, bool bBlocking, EGroupType eGrouping, GoalParameters &params)
{
	if (eGrouping == IGoalPipe::GT_GROUPED)
		++m_nCurrentBlockCounter;
	else 
		m_nCurrentBlockCounter = 0;	// continuous group counter.

	QGoal newgoal;
	newgoal.op = LAST_AIOP;
	newgoal.pipeName = szName;
	newgoal.params = params;
	newgoal.bBlocking = bBlocking;
	newgoal.eGrouping = eGrouping;

	m_qGoalPipe.push_back(newgoal);
}

//
//-------------------------------------------------------------------------------------------------------
EPopGoalResult CGoalPipe::PopGoal(QGoal& theGoal, CPipeUser *pOperand)
{
	// if we are processing a subpipe
	if (m_pSubPipe)
	{
		EPopGoalResult result = m_pSubPipe->PopGoal(theGoal, pOperand);
		if ( result == ePGR_AtEnd )
		{
			bool dontProcessParentPipe = m_pSubPipe->m_nEventId != 0;

			delete m_pSubPipe;
			m_pSubPipe = 0; // this subpipe is finished

			if (m_pArgument)
				pOperand->SetLastOpResult(m_pArgument);

			pOperand->NotifyListeners( this, ePN_Resumed );
			if (dontProcessParentPipe)
				return ePGR_BreakLoop;
		}
		else
			return result;
	}

	if (m_nPosition < m_qGoalPipe.size() )
	{
		QGoal& current = m_qGoalPipe[m_nPosition++];
		if (current.op == LAST_AIOP)
		{
			if (CGoalPipe* pPipe = GetAISystem()->IsGoalPipe(current.pipeName))
			{
				// this goal is a subpipe of goals, get that one until it is finished
				m_pSubPipe = pPipe;
				if(pOperand->GetAttentionTarget())
					pPipe->m_vAttTargetPosAtStart = pOperand->GetAttentionTarget()->GetPos();
				else
					pPipe->m_vAttTargetPosAtStart.zero();
				return m_pSubPipe->PopGoal(theGoal, pOperand);
			}
		}
		else
		{
			// this is an atomic goal, just return it
			theGoal = current;
			// goal successfully retrieved
			return 0 != theGoal.pGoalOp ? ePGR_Succeed : ePGR_AtEnd;
		}
	}

	// we have reached the end of this goal pipe
	// reset position and let the world know we are done
	pOperand->NotifyListeners( this, ePN_Finished );
	Reset();
	return ePGR_AtEnd;
}

//
//-------------------------------------------------------------------------------------------------------
/*
GoalPointer CGoalPipe::PopGoal(bool &blocking, string &name, GoalParameters &params, CPipeUser *pOperand)
{

	// if we are processing a subpipe
	if (m_pSubPipe)
	{
		GoalPointer anymore = m_pSubPipe->PopGoal(blocking, name,params,pOperand);
		if (!anymore)
		{
			delete m_pSubPipe;
			m_pSubPipe = 0; // this subpipe is finished

			if (m_pArgument)
				pOperand->SetLastOpResult(m_pArgument);

			pOperand->NotifyListeners( this, ePN_Resumed );
		}
		else
			return anymore;
	}

	if (m_nPosition < m_qGoalPipe.size() )
	{
		QGoal current = m_qGoalPipe[m_nPosition++];

		CGoalPipe *pPipe;
		if (pPipe = GetAISystem()->IsGoalPipe(current.name))
		{
			// this goal is a subpipe of goals, get that one until it is finished
			m_pSubPipe = pPipe;
			if(pOperand->GetAttentionTarget())
				pPipe->m_vAttTargetPosAtStart = pOperand->GetAttentionTarget()->GetPos();
			else
				pPipe->m_vAttTargetPosAtStart.zero();
			return m_pSubPipe->PopGoal(blocking,name,params,pOperand);
		}
		else
		{
			// this is an atomic goal, just return it
			blocking = current.bBlocking;
			name = current.name;
			params = current.params;
			// goal succesfuly retrieved

			return current.pGoalOp;
		}
	}

	// we have reached the end of this goal pipe
	// reset position and let the world know we are done
	pOperand->NotifyListeners( this, ePN_Finished );
	Reset();
	return 0;
}
*/

//
//--------------------------------------------------------------------------------------------------------------
CGoalPipe * CGoalPipe::Clone()
{
	CGoalPipe *pClone = new CGoalPipe(m_sName, GetAISystem(), true);

	// copy goal queue
	VectorOGoals::iterator gi;

	// copy the labels 
	if (!m_Labels.empty())
	{
		// convert labels to offsets before cloning
		for ( gi = m_qGoalPipe.begin(); gi != m_qGoalPipe.end(); ++gi )
		{
			if ( !gi->params.szString.empty() && (gi->op == AIOP_BRANCH || gi->op == AIOP_RANDOM) )
			{
				LabelsMap::iterator it = m_Labels.find( gi->params.szString );
				if ( it != m_Labels.end() )
				{
					int absolute = it->second;
					int current = gi - m_qGoalPipe.begin();
					if ( current < absolute )
						gi->params.nValueAux = absolute - current - 1;
					else if ( current > absolute )
						gi->params.nValueAux = absolute - current;
					else
					{
						gi->params.nValueAux = 0;
						AIWarning("Empty loop at label %s ignored in goal pipe %s.", gi->params.szString.c_str(), m_sName.c_str());
					}
				}
				else
				{
					gi->params.nValueAux = 0;
					AIWarning("Label %s not found in goal pipe %s.", gi->params.szString.c_str(), m_sName.c_str());
				}
				gi->params.szString.clear();
			}
		}

		// we don't need the labels anymore
		m_Labels = LabelsMap();
	}

	for (gi=m_qGoalPipe.begin();gi!=m_qGoalPipe.end(); ++gi)
	{
		QGoal &gl = (*gi);
		if (gl.op != LAST_AIOP)
			pClone->PushGoal(gl.op,gl.bBlocking,gl.eGrouping,gl.params);
		else
			pClone->PushPipe(gl.pipeName.c_str(),gl.bBlocking,gl.eGrouping,gl.params);
	}

	return pClone;
}

void CGoalPipe::Reset()
{
	m_nPosition = 0;
	if (m_pSubPipe)
		delete m_pSubPipe;
	m_pSubPipe = 0;
	m_lastResult = AIGOR_NONE;
}

// Makes the IP of this pipe jump to the desired position
bool CGoalPipe::Jump(int position)
{
	// if we are processing a subpipe
	if (m_pSubPipe)
	{
		return m_pSubPipe->Jump( position );
		//return;
	}

	if (position<0)
		position--;
	if (m_nPosition)
		m_nPosition+=position;

	return position<0;
}

// obsolete - string labels are converted to integer relative offsets
// TODO: cut this version of Jump in a few weeks
bool CGoalPipe::Jump(const char* label)
{
	// if we are processing a subpipe
	if (m_pSubPipe)
	{
		return m_pSubPipe->Jump( label );
		//return;
	}
	int step = 0;
	LabelsMap::iterator it = m_Labels.find(CONST_TEMP_STRING( label ));
	if (it != m_Labels.end())
	{
		step = it->second - m_nPosition;
		m_nPosition = it->second;
	}
	else
		AIWarning("Label %s not found in goal pipe %s.", label, m_sName.c_str());

	return step<0;
}

void CGoalPipe::ReExecuteGroup()
{
	// this must be the last inserted goal pipe
	AIAssert(!m_pSubPipe);
/* this is how it was before......
	while (	m_nPosition && m_qGoalPipe[--m_nPosition].eGrouping==IGoalPipe::GT_GROUPWITHPREV)
		; // nothing to do, just keep decreasing m_nPosition until we found the beginning of this group
*/

	if(m_nPosition==0)	// if already in the beginning of the pipe - no need to move the position
		return;
	if(m_qGoalPipe[m_nPosition-1].eGrouping==IGoalPipe::GT_GROUPED)
	{
		if(m_nPosition)
			--m_nPosition;
		while (	m_nPosition 
			&& m_qGoalPipe[m_nPosition].eGrouping==IGoalPipe::GT_GROUPED
			&& m_qGoalPipe[m_nPosition-1].eGrouping==IGoalPipe::GT_GROUPED)
			--m_nPosition; 
	}
	else
	{
		while (	m_nPosition && m_qGoalPipe[--m_nPosition].eGrouping==IGoalPipe::GT_GROUPWITHPREV)
			; // nothing to do, just keep decreasing m_nPosition until we found the beginning of this group
	}
}

void CGoalPipe::SetSubpipe(CGoalPipe * pPipe)
{
	if ( m_pSubPipe && pPipe )
		pPipe->SetSubpipe( m_pSubPipe );
	m_pSubPipe = pPipe;
}

//
//------------------------------------------------------------------------------------------------------
bool CGoalPipe::RemoveSubPipe( int& goalPipeId, bool keepInserted, bool keepHigherPriority )
{
	if ( m_bHighPriority )
		keepHigherPriority = false;
	if ( !m_pSubPipe )
		return false;
	if ( !goalPipeId && !m_pSubPipe->m_pSubPipe )
		goalPipeId = m_pSubPipe->m_nEventId;
	if ( m_pSubPipe->m_nEventId == goalPipeId )
	{
		if ( keepInserted )
		{
			CGoalPipe* temp = m_pSubPipe;
			m_pSubPipe = m_pSubPipe->m_pSubPipe;
			temp->m_pSubPipe = NULL;
			delete temp;
		}
		else
		{
			if ( keepHigherPriority )
			{
				CGoalPipe* temp = m_pSubPipe;
				while ( temp && (!temp->m_pSubPipe || !temp->m_pSubPipe->m_bHighPriority) )
					temp = temp->m_pSubPipe;
				if ( temp )
				{
					CGoalPipe* subPipe = m_pSubPipe;
					m_pSubPipe = temp->m_pSubPipe;
					temp->m_pSubPipe = NULL;
					delete subPipe;
					return true;
				}
			}
			// there's no higher priority pipe
			delete m_pSubPipe;
			m_pSubPipe = NULL;
		}
		return true;
	}
	else
		return m_pSubPipe->RemoveSubPipe( goalPipeId, keepInserted, keepHigherPriority );
}

void CGoalPipe::Serialize(TSerialize ser, class CObjectTracker& objectTracker, VectorOGoals &activeGoals)
{
	ser.Value( "GoalPipePos", m_nPosition);
	ser.Value( "nEventId", m_nEventId);
	ser.Value( "bHighPriority", m_bHighPriority);
	ser.EnumValue("m_lastResult",m_lastResult,AIGOR_NONE,LAST_AIGOR);
	//ser.Value( "m_bDynamic", m_bDynamic);

	objectTracker.SerializeObjectPointer(ser, "m_pArgument", m_pArgument, false);

	// take care of active goals
	ser.BeginGroup("ActiveGoals");
	int counter = 0;
	int goalIdx;
	char groupName[256];
	if(ser.IsWriting())
	{
		for(int activeIdx=0; activeIdx<activeGoals.size(); ++activeIdx)
		{
			VectorOGoals::iterator actGoal = std::find(m_qGoalPipe.begin(),m_qGoalPipe.end(),activeGoals[activeIdx]);
			if(actGoal!=m_qGoalPipe.end())
			{
				sprintf(groupName, "ActiveGoal-%d", counter);
				ser.BeginGroup(groupName);
				goalIdx = actGoal - m_qGoalPipe.begin();
				ser.Value("goalIdx", goalIdx);
				m_qGoalPipe[goalIdx].Serialize(ser, objectTracker);
				ser.EndGroup();
				++counter;
			}
		}
		ser.Value("activeGoalGounter", counter);
	}
	else
	{
		ser.Value("activeGoalGounter", counter);
		activeGoals.resize(counter);
		for (int i=0; i<counter; ++i)
		{
			sprintf(groupName, "ActiveGoal-%d", i);
			ser.BeginGroup(groupName);
			ser.Value("goalIdx", goalIdx);
			activeGoals[i] = m_qGoalPipe[goalIdx];
			m_qGoalPipe[goalIdx].Serialize(ser, objectTracker);
			ser.EndGroup();
		}
	}
	ser.EndGroup();

	if(ser.IsReading())
	{
    AIAssert(!IsInSubpipe()); // Danny not completely sure this is a correct assertion to make
		if(ser.BeginOptionalGroup( "SubPipe", true ))
		{
			string subPipeName;
			ser.Value( "GoalSubPipe", subPipeName);
			CGoalPipe* pSubPipe = new CGoalPipe(subPipeName, GetAISystem(), true);
			ser.BeginGroup( "DynamicPipe" );
				pSubPipe->SerializeDynamic( ser );
			ser.EndGroup();
			SetSubpipe(pSubPipe);
			GetSubpipe()->Serialize(ser, objectTracker, activeGoals);
			ser.EndGroup();
		}
	}
	else
	{
		if (ser.BeginOptionalGroup( "SubPipe", IsInSubpipe() ))
		{
			ser.Value( "GoalSubPipe", GetSubpipe()->m_sName );
			ser.BeginGroup( "DynamicPipe" );
				GetSubpipe()->SerializeDynamic( ser );
			ser.EndGroup();
			GetSubpipe()->Serialize(ser, objectTracker, activeGoals);
			ser.EndGroup();
		}
	}

//	QGoal current = m_qGoalPipe[m_nPosition];
//	current.pGoalOp->Serialize(ser, objectTracker);
}

//
//------------------------------------------------------------------------------------------------------
void QGoal::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.EnumValue("op", op, AIOP_ACQUIRETARGET, LAST_AIOP);
	ser.Value("pipeName", pipeName);
	ser.Value("Blocking", bBlocking);
	ser.EnumValue("Grouping",eGrouping,IGoalPipe::GT_NOGROUP,IGoalPipe::GT_LAST);
	pGoalOp->Serialize(ser, objectTracker);
}

//
//------------------------------------------------------------------------------------------------------
void CGoalPipe::SerializeDynamic(TSerialize ser)
{
	if(!m_bDynamic)
		return;
	uint32	count(m_qGoalPipe.size());
	char	buffer[16];
	ser.Value("count", count);
	for (int curIdx(0); curIdx<count; ++curIdx)
	{
		sprintf(buffer, "goal_%d", curIdx);
		ser.BeginGroup(buffer);
		{
			QGoal gl;
			if(ser.IsWriting())
				gl = m_qGoalPipe[curIdx];
			ser.EnumValue("op", gl.op, AIOP_ACQUIRETARGET, LAST_AIOP);
			ser.Value("pipeName" , gl.pipeName);
			ser.Value("bBlocking" , gl.bBlocking);
			ser.EnumValue("Grouping",gl.eGrouping,IGoalPipe::GT_NOGROUP,IGoalPipe::GT_LAST);
			gl.params.Serialize(ser);
			if(ser.IsReading())
			{
				if (gl.op != LAST_AIOP)
					PushGoal(gl.op,gl.bBlocking,gl.eGrouping,gl.params);
				else
					PushPipe(gl.pipeName,gl.bBlocking,gl.eGrouping,gl.params);
			}
		}
		ser.EndGroup();
	}
	ser.Value("Labels",m_Labels);
}

//
//------------------------------------------------------------------------------------------------------
void GoalParameters::Serialize(TSerialize ser)
{
	ser.Value("m_vPosition", m_vPosition);
	ser.Value("m_vPositionAux", m_vPositionAux);
	//IAIObject *m_pTarget; 
	ser.Value("fValue", fValue);
	ser.Value("fValueAux", fValueAux);
	ser.Value("nValue", nValue);
	ser.Value("nValueAux", nValueAux);
	ser.Value("bValue", bValue);
	ser.Value("szString", szString);
	ser.Value("szString2", szString2);
}
