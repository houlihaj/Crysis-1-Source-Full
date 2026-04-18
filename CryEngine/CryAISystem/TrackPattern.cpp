/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   TrackPattern.cpp
	$Id$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 30/8/2005   : Created by Mikko Mononen

*********************************************************************/

#include "StdAfx.h"
#include "TrackPattern.h"
#include "TrackPatternDescriptor.h"
#include "IAgent.h"
#include "AILog.h"
#include MATH_H
#include "AICollision.h"
#include <ISerialize.h>
#include "ObjectTracker.h"
#include "IRenderer.h"
#include "IRenderAuxGeom.h"


//-----------------------------------------------------------------------------------------------------------
//
// CTrackDummy
//

CTrackDummy::CTrackDummy( CAISystem* pAISystem ) :
	CAIObject( pAISystem ),
	m_pTrackPattern( 0 )
{
	// Empty.
}

void CTrackDummy::SetOwnerTrackPattern( CTrackPattern* pTracker )
{
	m_pTrackPattern = pTracker;
}

const Vec3 & CTrackDummy::GetPos() const
{
	if( m_pTrackPattern )
		m_pTrackPattern->OnNotifyUsage( this );

	return CAIObject::GetPos();
}

const Vec3 & CTrackDummy::GetBodyDir() const
{
	if( m_pTrackPattern )
		m_pTrackPattern->OnNotifyUsage( this );

	return CAIObject::GetBodyDir();
}

const Vec3 & CTrackDummy::GetMoveDir() const
{
  if( m_pTrackPattern )
    m_pTrackPattern->OnNotifyUsage( this );

  return CAIObject::GetMoveDir();
}

const Vec3 & CTrackDummy::GetViewDir() const
{
	if( m_pTrackPattern )
		m_pTrackPattern->OnNotifyUsage( this );

	return CAIObject::GetViewDir();
}


//-----------------------------------------------------------------------------------------------------------
//
// CTrackPattern
//

CTrackPattern::CTrackPattern( CPuppet* pOwner ) :
	m_lastUpdateTime( -1.0f ),
	m_lastUsageTime( -1.0f ),
	m_pTargetDummy( 0 ),
	m_pOwner( pOwner ),
	m_hasAttTarget( false ),
	m_flags( 0 ),
	m_pCurrentNode( 0 ),
	m_currentNodeDeformation( 0 ),
	m_patternDeformation( 0 ),
	m_patternState( AITRACKPAT_STATE_EXPOSED ),
	m_advance( true ),
	m_globalDeformTreshold( 0 ),
	m_localDeformTreshold( 0 ),
	m_stateTresholdMin( 0.35f ),
	m_stateTresholdMax( 0.4f ),
	m_exposureMod( 0 ),
	m_hibernating( false ),
	m_nodeSignalSent( false ),
	m_patternValid( false )
{
	// Empty.
}

CTrackPattern::~CTrackPattern()
{
	if( m_pTargetDummy )
		GetAISystem()->RemoveDummyObject( m_pTargetDummy );
}

void CTrackPattern::WakeUp()
{
	m_patternState = AITRACKPAT_STATE_EXPOSED;
	m_acquireTime = m_lastUsageTime = GetAISystem()->GetFrameStartTime();
	Update();
}

void CTrackPattern::ContinueAdvance( bool advance )
{
	if( !m_advance && advance )
	{
		m_pCurrentNode = ChooseNextTarget( m_pCurrentNode );
		m_currentNodeDeformation = m_pCurrentNode->deformation;
	}

	m_advance = advance;
}

void CTrackPattern::SetPattern( const CTrackPatternDescriptor& pattern )
{
	if( !pattern.m_finalized )
	{
    AIError( "CTrackPattern::SetPattern: Trying to use non-finished pattern '%s'. AI.EndTrackPattern() should be called to when defining the pattern.", pattern.m_name.c_str() );
		return;
	}

	// If the pattern is already setup, do not initalise again.
	if( pattern.m_name == m_curPatternName )
		return;

	// Initialise the nodes.
	m_pattern.resize( pattern.m_nodes.size() );
	for( size_t i = 0; i < m_pattern.size(); i++ )
		m_pattern[i].Reset();

	// Track if the pattern has nodes with start flag, used later when choosing the start node.
	m_hasStartNodes = false;

	// Create the  node the node hierarchy based on the pattern descriptor.
	for( TTrackPatDescNodeMap::const_iterator noIt = pattern.m_nodes.begin(); noIt != pattern.m_nodes.end(); ++noIt )
	{
		const CTrackPatternDescriptorNode&	descNode = noIt->second;
		Node&	node = m_pattern[descNode.m_index];

		node.offset = descNode.m_offset;
		node.flags = descNode.m_flags;
		node.branchMethod = descNode.m_branchMethod;
		node.signalValue = descNode.m_signalValue;
#ifdef TRACKPATTERN_DEBUG
		node.name = noIt->first;
#endif

		if( node.flags & AITRACKPAT_NODE_START )
			m_hasStartNodes = true;

		// Find parent.
		if( descNode.m_parent )
			node.parent = &m_pattern[descNode.m_parent->m_index];

		// The validation radius is decreased the further down to the hierarchy we go.
		// This is so that the further validation checks do not fail initially.
		if( node.parent )
			node.rad = node.parent->rad * 0.8f;
		else
			node.rad = pattern.m_validationRadius;

		// Find branch node pointers.
		if( !descNode.m_branchNodes.empty() )
		{
			node.branchNodes.resize( descNode.m_branchNodes.size() );
			int	j = 0;
			for( std::list<CTrackPatternDescriptorNode*>::const_iterator braIt = descNode.m_branchNodes.begin(); braIt != descNode.m_branchNodes.end(); ++braIt )
			{
				if( (*braIt) )
					node.branchNodes[j] = &m_pattern[(*braIt)->m_index];
				++j;
			}
		}
	}

	// Copy the pattern parameters.
	m_curPatternName = pattern.m_name;
	m_flags = pattern.m_flags;
	m_validationRadius = pattern.m_validationRadius;
	m_advanceRadius = pattern.m_advanceRadius;
	m_globalDeformTreshold = pattern.m_globalDeformTreshold;
	m_localDeformTreshold = pattern.m_localDeformTreshold;
	m_stateTresholdMin = pattern.m_stateTresholdMin;
	m_stateTresholdMax = pattern.m_stateTresholdMax;
	m_exposureMod = pattern.m_exposureMod;
	m_rotAngles = pattern.m_rotAngles;

	// Reset current node.
	m_pCurrentNode = 0;
	m_currentNodeDeformation = 0;
	m_advance = true;
	m_nodeSignalSent = false;
	m_patternValid = false;

	// Create the pattern rotation.
	m_rotMat.SetIdentity();

	if( m_flags & AITRACKPAT_ALIGN_ORIENT_TO_TARGET )
	{
		if( m_pOwner && m_pOwner->GetAttentionTarget() )
		{
			Vec3	dirToTarget = m_pOwner->GetPos() - m_pOwner->GetAttentionTarget()->GetPos();
			dirToTarget.NormalizeSafe();
			Matrix33	lookAtMat;
 			lookAtMat.SetRotationVDir( dirToTarget );
 			lookAtMat.Transpose();
 			m_rotMat = lookAtMat * m_rotMat;
			m_initialDirToTarget = dirToTarget;
		}
	}

	if( m_flags & AITRACKPAT_ALIGN_TARGET_DIR )
	{
		if( m_pOwner && m_pOwner->GetAttentionTarget() )
		{
			Vec3	dirToTarget = m_pOwner->GetAttentionTarget()->GetViewDir();
			dirToTarget.NormalizeSafe();
			if (!dirToTarget.IsZero())
			{
				Matrix33	lookAtMat;
 				lookAtMat.SetRotationVDir( dirToTarget );
 				lookAtMat.Transpose();
 				m_rotMat = lookAtMat * m_rotMat;
				m_initialDirToTarget = dirToTarget;
			}
		}
	}

	if( m_flags & AITRACKPAT_ALIGN_RANDOM )
	{
		Ang3	angles( ai_frand() * DEG2RAD( m_rotAngles.x ), ai_frand() * DEG2RAD( m_rotAngles.y ), ai_frand() * DEG2RAD( m_rotAngles.z ) );
		Matrix33	randRotMat;
		randRotMat.SetRotationXYZ( angles );
		m_rotMat = randRotMat * m_rotMat;
	}
}

const char* CTrackPattern::GetCurrentPatternName() const
{
	return m_curPatternName.c_str();
}

void CTrackPattern::Update()
{
  FUNCTION_PROFILER(GetAISystem()->m_pSystem, PROFILE_AI);
	CTimeValue	curTime = GetAISystem()->GetFrameStartTime();
	float	deltaTime = (curTime - m_lastUpdateTime).GetSeconds();
	if( deltaTime > 1.0f )
		deltaTime = 1.0f;

	// If the system has not been used for a long time, update less often.
	if( (curTime - m_lastUsageTime).GetSeconds() > 2.0f )
	{
		// Update less ofthen if this pattern is not being used.
		if( deltaTime < 0.5f )
		{
			return;
		}
		m_hibernating = true;
	}
	else
	{
		// If previously has been hibernating, and the pattern is being used again
		// and the pattern state is the non-default exposed, send a signal.
		if( m_hibernating && m_patternState == AITRACKPAT_STATE_ENCLOSED )
			m_pOwner->SetSignal( 1, "OnTrackPatternEnclosed", 0, 0, g_crcSignals.m_nOnTrackPatternEnclosed );
		m_hibernating = false;
	}

	UpdateInternal( deltaTime );

	m_lastUpdateTime = curTime;
}

void CTrackPattern::UpdateInternal( float deltaTime )
{
	// Create the AI object to follow on first update.
	if( !m_pTargetDummy )
	{
		m_pTargetDummy = (CTrackDummy*)GetAISystem()->CreateDummyObject( string(m_pOwner->GetName()) + "_targettrack", CAIObject::STP_TRACKDUMMY );
		m_pTargetDummy->SetOwnerTrackPattern( this );
	}

	// Update the pattern.
	// When validating the points test is made from the start position to the end position.
	// Start position is either the pattern origin or in case the parent is provided, the parent position.
	// The end position is either relative offset from the start position or offset from the pattern origin, 
	// this is chosen based on the node flag.
	// The offset is clamped to the physical world based on the test method.

	IAIObject*	pAttTarget = m_pOwner->GetAttentionTarget();

	if( pAttTarget )
	{
		Matrix33			rotMat;
		Matrix33			refFrame;
		rotMat.SetIdentity();
		refFrame.SetIdentity();

/*		if( m_flags & AITRACKPAT_ALIGN_LEVEL_TO_TARGET )
		{
			Vec3	up = pAttTarget->GetUpDir();
			Vec3	forw = pAttTarget->GetViewDir();
			Vec3	right = forw % up;
			refFrame.SetFromVectors( right, up, right % up );
			rotMat = refFrame;
		}

		if( m_flags & AITRACKPAT_ALIGN_ORIENT_TO_TARGET )
		{
			Vec3	dirToTarget = m_initialDirToTarget * refFrame.GetTransposed();
			Matrix33	lookAtMat;
			lookAtMat.SetRotationVDir( dirToTarget );
			lookAtMat.Transpose();
			rotMat = rotMat * lookAtMat;
		}*/

		rotMat = rotMat * m_rotMat;

		Vec3	targetPos = pAttTarget->GetPos();
		Vec3	targetViewDir = pAttTarget->GetViewDir();

		m_patternOrig = targetPos;

		float	maxDeformation = 0;
		float	currentDeformation = 0;

		// Update all nodes, validate them and calculate their exposure.
		for( size_t i = 0; i < m_pattern.size(); i++ )
		{
			Node&	node = m_pattern[i];
			Vec3	startPos;
			Vec3	endPos;

			if( node.parent )
			{
				startPos = node.parent->validatedPosition;
				//FIXME [Kirill] --> Mikko, take a look - is it ever supposed to be 0? trying to prenent this
				// GetEntitiesInBox: too many cells requested by Game (26208, (-0.5,-0.5,-0.5)-(994.1,1654.6,609.0))
				if(startPos.IsZero(1.f))
					startPos = m_patternOrig;
			}
			else
				startPos = m_patternOrig;

			if( node.flags & AITRACKPAT_NODE_ABSOLUTE )
				endPos = m_patternOrig + node.offset * rotMat;
			else
				endPos = startPos + node.offset * rotMat;

			float deformation = 1.0f;

			if( node.offset.GetLengthSquared() > 0.01f )
			{
				if( m_flags & AITRACKPAT_VALIDATE_SWEPTSPHERE )
				{
					float	dist;
					if( IntersectSweptSphere( 0, dist, Lineseg( startPos, endPos ), m_validationRadius, AICE_ALL ) )
					{
						Vec3	delta = endPos - startPos;
						deformation = dist / delta.GetLength();
						endPos = startPos + delta * deformation;
					}
				}
				else if( m_flags & AITRACKPAT_VALIDATE_RAYCAST )
				{
					IPhysicalWorld*	pPhysWorld = GetAISystem()->GetPhysicalWorld();

					Vec3	delta = endPos - startPos;
					float	len = delta.GetLength();
					if(len > 0.00001f)
						delta *= (len + m_validationRadius) / len;
					ray_hit	hit;
					int rwiResult;
TICK_COUNTER_FUNCTION
TICK_COUNTER_FUNCTION_BLOCK_START
					rwiResult = pPhysWorld->RayWorldIntersection( startPos, delta, ent_static, rwi_stop_at_pierceable, &hit, 1 );
TICK_COUNTER_FUNCTION_BLOCK_END
					if( rwiResult )
					{
						float	dist = hit.dist - m_validationRadius;
						if( dist < 0 )
							dist = 0;
						deformation = dist / delta.GetLength();
						endPos = startPos + delta * deformation;
					}
				}

				// At this point the deformation mening is 1 non deformed, and 0 fully deformed,
				// change the range so that 1 means fully deformed.
				node.deformation = 1.0f - deformation;

				// Clamp the bottom range of the deformation.
				if( node.deformation < m_localDeformTreshold ) node.deformation = m_localDeformTreshold;
				if( node.deformation > 1.0f ) node.deformation = 1.0f;
				node.deformation = (node.deformation - m_localDeformTreshold) / (1.0f - m_localDeformTreshold);

				// Accumulate the maximum and current.
				maxDeformation += 1.0f;
				currentDeformation += node.deformation;

				// Add the deformation of the current node to all the parent nodes.
				PropagateDeformation( &node );
			}
			else
			{
				// This point is an alies, that is basically the same point as the parent.
				// Copy the deformation value from the parent.
				if( node.parent )
					node.deformation = node.parent->deformation;
			}

			// Store the validated value.
			node.validatedPosition = endPos;

			// Calculate the node direction.
			Vec3	dir( 0, 0, 0 );
			if( node.flags & AITRACKPAT_NODE_DIRBRANCH )
			{
				for( size_t j = 0; j < node.branchNodes.size(); j++ )
					dir += node.branchNodes[j]->validatedPosition - node.validatedPosition;
			}
			else
			{
				// Default direction is to point towards the center of the pattern.
				dir = m_patternOrig - node.validatedPosition;
			}
			dir.NormalizeSafe();
			node.validatedDirection = dir;

			// Update the exposure. The exposure is how much the target sees the node.
			// The exposure will be higher the more directly and the longer the target is looking towards it.
			Vec3	diff = node.validatedPosition - targetPos;
			diff.NormalizeSafe();

			float	exposure = (float)pow( max( 0.0f, targetViewDir.Dot( diff ) ), 2.0f );

//			node.exposure += (exposure * 1.2f - 0.2f) * deltaTime;
			node.exposure += (exposure - node.exposure) * deltaTime;

			if( node.exposure < 0 )
				node.exposure = 0;
			if( node.exposure > 1.0f )
				node.exposure = 1.0f;

			node.totalExposure = node.exposure;

			PropagateExposure( &node );
		}

		// Normalize the total exposure.
		float	maxExp = 0.0f;
		for( size_t i = 0; i < m_pattern.size(); i++ )
			maxExp = max(maxExp, m_pattern[i].totalExposure);
		if (maxExp > 0.0f)
		{
			for( size_t i = 0; i < m_pattern.size(); i++ )
				m_pattern[i].totalExposure /= maxExp;
		}

		// Calculate the total normalised deformation.
		if( maxDeformation == 0.0f ) maxDeformation = 1.0f;
		float	totalDeformation = currentDeformation / maxDeformation;

		if( totalDeformation < m_globalDeformTreshold ) totalDeformation = m_globalDeformTreshold;
		if( totalDeformation > 1.0f ) totalDeformation = 1.0f;
		totalDeformation = (totalDeformation - m_globalDeformTreshold) / (1.0f - m_globalDeformTreshold);

		m_patternDeformation += (totalDeformation - m_patternDeformation) * deltaTime;

		// Check the pattern state, exposed or enclosed and signal the behavior about the change.
		if( m_patternState == AITRACKPAT_STATE_ENCLOSED )
		{
			// Enclosed, check if it is required to change state.
			if( m_patternDeformation < m_stateTresholdMin )
			{
				// CryLog( "%s enclosed->exposed %f  (%f - %f)", m_pOwner->GetName(), m_patternDeformation, m_stateTresholdMin, m_stateTresholdMax );
				m_pOwner->SetSignal( 1, "OnTrackPatternExposed", 0, 0, g_crcSignals.m_nOnTrackPatternExposed );
				m_patternState = AITRACKPAT_STATE_EXPOSED;
			}
		}
		else if( m_patternState == AITRACKPAT_STATE_EXPOSED )
		{
			// Exposed, check if it is required to change state.
			if( m_patternDeformation > m_stateTresholdMax )
			{
				// CryLog( "%s exposed->enclosed %f  (%f - %f)", m_pOwner->GetName(),m_patternDeformation, m_stateTresholdMin, m_stateTresholdMax );
				m_pOwner->SetSignal( 1, "OnTrackPatternEnclosed", 0,0,g_crcSignals.m_nOnTrackPatternEnclosed );
				m_patternState = AITRACKPAT_STATE_ENCLOSED;
			}
		}

		// If no current node, choose the closest one.
		if( !m_pCurrentNode )
		{
			m_pCurrentNode = ChooseStartNode();
			// The choose start node may return null if the pattern is not validated.
			if( m_pCurrentNode )
			{
				m_currentNodeDeformation = m_pCurrentNode->deformation;
				m_nodeSignalSent = false;
			}
		}
		else
		{
			// If the deformation of the current node has changed more than 50%, re-evaluate the current node.
			if( m_currentNodeDeformation > 0 && fabsf( m_pCurrentNode->deformation - m_currentNodeDeformation ) > m_currentNodeDeformation * 0.5f )
			{
				m_pCurrentNode = ChooseStartNode();
				// The choose start node may return null if the pattern is not validated.
				if( m_pCurrentNode )
				{
					m_currentNodeDeformation = m_pCurrentNode->deformation;
					m_nodeSignalSent = false;
				}
			}
		}

		// Check if it is time to switch the target.
		if( m_pCurrentNode )
		{
 			if( m_advance )
			{
				Vec3	diff = m_pCurrentNode->validatedPosition - m_pOwner->GetPos();
				if( diff.GetLength() < m_advanceRadius ) //m_pOwner->GetParameters().m_fPassRadius )
				{
					Node*	nextNode = 0;

					// If the node that is reached has stpo flag set, stop adnvacing.
					// The current node will be advanced when the advance is enabled again.
					if( m_pCurrentNode->flags & AITRACKPAT_NODE_STOP )
						m_advance = false;
					else
						nextNode = ChooseNextTarget( m_pCurrentNode );

					// Send a signal to allow reacting reaching this node
					if( !m_nodeSignalSent && m_pCurrentNode->flags & AITRACKPAT_NODE_SIGNAL )
					{
						AISignalExtraData* pData = new AISignalExtraData;
						pData->iValue = m_pCurrentNode->signalValue;										// Signal value set in the script
						pData->iValue2 = (nextNode && nextNode != m_pCurrentNode) ? 1 : 0;
						pData->fValue = m_pCurrentNode->totalExposure * m_exposureMod;	// Current exposure
						m_pOwner->SetSignal( 1, "OnReachedTrackPatternNode", 0, pData, g_crcSignals.m_nOnReachedTrackPatternNode );
						m_nodeSignalSent = true;
					}

					if (nextNode && nextNode != m_pCurrentNode)
					{
						m_pCurrentNode = nextNode;
						m_currentNodeDeformation = m_pCurrentNode->deformation;
						m_nodeSignalSent = false;
					}

				}
			}

			// Update target position & direction.
			m_pTargetDummy->SetPos( m_pCurrentNode->validatedPosition );
      m_pTargetDummy->SetBodyDir( m_pCurrentNode->validatedDirection );
      m_pTargetDummy->SetMoveDir( m_pCurrentNode->validatedDirection );
		}

		m_hasAttTarget = true;
	}
	else
		m_hasAttTarget = false;

	// mark the pattern validated.
	m_patternValid = true;
}

CTrackPattern::Node* CTrackPattern::ChooseStartNode()
{
	// The choose start node depends on the validated pattern locations.
	if( !m_patternValid )
		return 0;

	// Choose start node that is closest to the owner.
	// If none of the nodes has start flag set, consider all nodes,
	// othewise use only the nodes marked with start flag.
	Node*	bestNode = 0;
	float	bestDist = FLT_MAX;
	for( size_t i = 0; i < m_pattern.size(); i++ )
	{
		Node&	node = m_pattern[i];
		if( m_hasStartNodes && (node.flags & AITRACKPAT_NODE_START) == 0 )
			continue;
		float	d = (node.validatedPosition - m_pOwner->GetPos()).GetLengthSquared();
		if( d < bestDist )
		{
			bestDist = d;
			bestNode = &node;
		}
	}
	return bestNode;
}

CTrackPattern::Node* CTrackPattern::ChooseNextTarget( Node* curNode )
{
	// The choose next node depends on the validated pattern locations.
	if( !m_patternValid )
		return curNode;

	if( !curNode )
		return 0;
	if( curNode->branchNodes.empty() )
		return curNode;

	if( curNode->branchMethod == AITRACKPAT_CHOOSE_ALWAYS )
	{
		float		maxExp = -FLT_MAX;
		size_t	best = 0;

		// Return the point which is most exposed.
		for( size_t i = 0; i < curNode->branchNodes.size(); i++ )
		{
			size_t idx = (curNode->lastUsedBranch + 1 + i) % curNode->branchNodes.size();
			float	exposure = curNode->branchNodes[idx]->totalExposure * m_exposureMod;
			if( exposure > maxExp )
			{
				best = idx;
				maxExp = exposure;
			}
		}
		curNode->lastUsedBranch = best;
		return curNode->branchNodes[best];
	}
	else if( curNode->branchMethod == AITRACKPAT_CHOOSE_MOST_EXPOSED )
	{
		float		maxExp = -FLT_MAX;
		size_t	best = 0;

		// Return the point which is most exposed.
		for( size_t i = 0; i < curNode->branchNodes.size(); i++ )
		{
			size_t idx = (curNode->lastUsedBranch + 1 + i) % curNode->branchNodes.size();
			float	exposure = curNode->branchNodes[idx]->totalExposure * m_exposureMod;
			if( exposure > maxExp )
			{
				best = idx;
				maxExp = exposure;
			}
		}

/*		bool	minimum = false;
		if (m_exposureMod < 0)
			minimum = curNode->totalExposure > 0.99f && maxExp >= curNode->totalExposure * m_exposureMod;
		else if (m_exposureMod > 0)
			minimum = curNode->totalExposure < 0.01f && maxExp >= curNode->totalExposure * m_exposureMod;*/

		if (maxExp > curNode->totalExposure * m_exposureMod) // || minimum)
		{
			curNode->lastUsedBranch = best;
			return curNode->branchNodes[best];
		}
	}
	else if( curNode->branchMethod == AITRACKPAT_CHOOSE_LESS_DEFORMED )
	{
		// Choose the least deformed branch.
		float		minDef = FLT_MAX;
		size_t	best = 0;
		for( size_t i = 0; i < curNode->branchNodes.size(); i++ )
		{
			size_t idx = (curNode->lastUsedBranch + 1 + i) % curNode->branchNodes.size();
			float	val = curNode->branchNodes[idx]->deformation - curNode->branchNodes[idx]->totalExposure * m_exposureMod;
			if( val < minDef )
			{
				best = idx;
				minDef = val;
			}
		}
		curNode->lastUsedBranch = best;
		return curNode->branchNodes[best];
	}
	else if( curNode->branchMethod == AITRACKPAT_CHOOSE_RANDOM )
	{
		// Return random branch.
		size_t	idx = 0;

		if( curNode->branchNodes.size() > 2 )
		{
			// If there is 3 or more branches, try to choose one that has not bee visited yet.
			size_t	iter = 0;
			do
			{
				idx = ai_rand() % curNode->branchNodes.size();
				iter++;
			}
			while( idx == curNode->lastUsedBranch && iter < 10 );
		}
		else
		{
			// For one or two branches, it is either always the same or ping-pong.
			idx = ai_rand() % curNode->branchNodes.size();
		}
		curNode->lastUsedBranch = idx;
		return curNode->branchNodes[idx];
	}

	return curNode;
}

void CTrackPattern::PropagateDeformation( Node* startNode )
{
	// Propagates the deformation information from the start node to all the parent nodes.
	if( !startNode )
		return;
	Node*	cur = startNode->parent;
	while( cur )
	{
		cur->deformation += startNode->deformation;
		cur = cur->parent;
	}
}

void CTrackPattern::PropagateExposure( Node* startNode )
{
	// Propagates the total exposure of the start node to all the parent nodes.
	if( !startNode )
		return;
	Node*	cur = startNode->parent;
	while( cur )
	{
		cur->totalExposure += startNode->totalExposure;
		cur = cur->parent;
	}
}

void CTrackPattern::OnObjectRemoved( CAIObject* pObject )
{
	// empty.
}

void CTrackPattern::OnNotifyUsage( const CAIObject* pSource )
{
	m_lastUsageTime = GetAISystem()->GetFrameStartTime();
}

CAIObject* CTrackPattern::GetTarget()
{
	if( m_hasAttTarget )
		return m_pTargetDummy;
	return 0;
}

void CTrackPattern::Serialize( TSerialize ser, class CObjectTracker& objectTracker )
{
	size_t	curNode = 0;
	float		curNodeDeformation = m_currentNodeDeformation;
	float		exposureMod = m_exposureMod;

	ser.BeginGroup( "TrackPattern" );
	{
		ser.Value( "m_curPatternName", m_curPatternName );

		if( ser.IsWriting() )
		{
			curNode = 0;
			for( ; curNode < m_pattern.size(); curNode++ )
				if( &m_pattern[curNode] == m_pCurrentNode )
					break;
		}

		ser.Value( "curNode", curNode );
		ser.Value( "curNodeDeformation", curNodeDeformation );

		CTimeValue	curTime = GetAISystem()->GetFrameStartTime();
		float	lastUpdateTime = 0, lastUsageTime = 0, acquireTime = 0;

		if( ser.IsWriting() )
		{
			lastUpdateTime = (m_lastUpdateTime - curTime).GetSeconds();
			lastUsageTime = (m_lastUsageTime - curTime).GetSeconds();
			acquireTime = (m_acquireTime - curTime).GetSeconds();
		}

		ser.Value( "lastUpdateTime", lastUpdateTime );
		ser.Value( "lastUsageTime", lastUsageTime );
		ser.Value( "acquireTime", acquireTime );

		if( ser.IsReading() )
		{
			m_lastUpdateTime = curTime + CTimeValue( lastUpdateTime );
			m_lastUsageTime = curTime + CTimeValue( lastUsageTime );
			m_acquireTime = curTime + CTimeValue( acquireTime );
		}

		ser.Value( "exposureMod", exposureMod );
		ser.Value( "m_advance", m_advance );
		ser.Value( "m_hibernating", m_hibernating );

		ser.Value( "m_patternState", m_patternState );
		ser.Value( "m_patternDeformation", m_patternDeformation );
		ser.Value( "m_patternOrig", m_patternOrig );

		objectTracker.SerializeObjectPointer( ser, "m_pTargetDummy", m_pTargetDummy, false );
		if( ser.IsReading() && m_pTargetDummy )
			m_pTargetDummy->SetOwnerTrackPattern( this );
	}
	ser.EndGroup();

	if( ser.IsReading() )
	{
		CTrackPatternDescriptor*	desc = (CTrackPatternDescriptor*)GetAISystem()->FindTrackPatternDescriptor( m_curPatternName.c_str() );
		if( desc )
		{
			// Force setting the current pattern.
			m_curPatternName = "";

			SetPattern( *desc );
		}

		m_exposureMod = exposureMod;

		if( !m_pattern.empty() && curNode < m_pattern.size() )
			m_pCurrentNode = &m_pattern[curNode];

		UpdateInternal( 1.0f );
	}
}

void CTrackPattern::DebugDraw(IRenderer* pRenderer)
{
	if( m_pCurrentNode )
	{
		ColorB	col( 255, 200, 200 );
		const Vec3&	pos = m_pCurrentNode->validatedPosition;
		const Vec3&	forward = m_pCurrentNode->validatedDirection;
		pRenderer->GetIRenderAuxGeom()->DrawSphere( pos, 0.15f, col );
		pRenderer->GetIRenderAuxGeom()->DrawLine( pos, col, pos + forward * 1.5f, col );
		pRenderer->GetIRenderAuxGeom()->DrawCone( pos + forward * 1.5f, forward, 0.05f, 0.2f, col );
	}

	// Draw the track pattern.
	ColorB	colStart( 255, 50, 50 );
	ColorB	colPat( 255, 200, 50 );
	for( size_t i = 0; i < m_pattern.size(); i++ )
	{
		const CTrackPattern::Node&	node = m_pattern[i];

		Vec3	startPos, endPos;
		float	startExp, endExp;

		if( node.parent )
		{
			startPos = node.parent->validatedPosition;
			startExp = node.parent->exposure;
		}
		else
		{
			startPos = m_patternOrig;
			startExp = 0;
		}

		endPos = node.validatedPosition;
		endExp = node.exposure;

		ColorF	startCol( 1, 1.0f - startExp, 0 );
		ColorF	endCol( 1, 1.0f - endExp, 0 );

		if( node.flags & AITRACKPAT_NODE_START )
			pRenderer->GetIRenderAuxGeom()->DrawCone( node.validatedPosition, Vec3( 0, 0, 1 ), 0.1f, 0.2f, startCol );
		else
			pRenderer->GetIRenderAuxGeom()->DrawSphere( node.validatedPosition, 0.07f, startCol );


		pRenderer->GetIRenderAuxGeom()->DrawLine( startPos, startCol, endPos, endCol );

		pRenderer->DrawLabel( endPos + Vec3( 0, 0, 0.5f ), 1, "%s\nD:%.3f\nE:%.3f", node.name.c_str(), node.deformation, node.totalExposure );
	}

	// Draw pattern state.
	pRenderer->DrawLabel( m_patternOrig + Vec3(0,0,3), 1, "%s\nD:%.3f", m_patternState == AITRACKPAT_STATE_ENCLOSED ? "<ENCLOSED>" : "> EXPOSED <", m_patternDeformation );
}
