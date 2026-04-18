/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   TrackPattern.h
	$Id$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 30/8/2005   : Created by Mikko Mononen

*********************************************************************/


#ifndef __TrackPattern_H__
#define __TrackPattern_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include "IRenderer.h"

class CTrackPattern;
class CTrackDummy;

#include "IAgent.h"
#include "Puppet.h"
#include "AIObject.h"
#include "TrackPatternDescriptor.h"
#include <list>
#include <vector>
#include <ISerialize.h>
#include "ObjectTracker.h"

#define		TRACKPATTERN_DEBUG	1


// The TrackDummy is a light wrapper to the CAIObject, which also notifies to the target tracker when it is used.
// Currently only the position and direction queries are notified to the tracker.
class CTrackDummy : public CAIObject
{
public:
	CTrackDummy( CAISystem* pAISystem );

	void	SetOwnerTrackPattern( CTrackPattern* pTracker );

	// When ever therse methods are accessed the parent tracker is notified about the usage.
	const Vec3 &GetPos() const;
  const Vec3 &GetBodyDir() const;
  const Vec3 &GetMoveDir() const;
	const Vec3 &GetViewDir() const;

protected:
	CTrackPattern* m_pTrackPattern;
};


enum EPatternState
{
	AITRACKPAT_STATE_ENCLOSED,
	AITRACKPAT_STATE_EXPOSED,
};


class CTrackPattern
{
public:

	CTrackPattern( CPuppet* pOwner );
	virtual ~CTrackPattern();

	/// Updates the internal state of the tracker. The owner attention target state is checked and
	/// pattern is updated accordingly.
	void					Update();

	/// Makes sure the track pattern is up to date. The internal state is 
	void					WakeUp();

	/// Creates a pattern based on the pattern descriptor. If the current pattern is the same as the
	/// one that is going to be set, the current cofiguration is kept.
	void					SetPattern( const CTrackPatternDescriptor& pattern );

	/// Returns the current active pattern name. Empty string is returned if no pattern is set yet.
	const char*		GetCurrentPatternName() const;

	/// This method should be called when an AI object is removed to notify the pattern.
	void					OnObjectRemoved( CAIObject* pObject );

	/// This method is called by the CTrackDummy to inform the pattern that it is being used.
	void					OnNotifyUsage( const CAIObject* pSource );

	/// Returns the target dummy object.
	CAIObject*		GetTarget();

	// Stops or continues to advance to the next node when a node is reached. 
	void					ContinueAdvance( bool advance );

	// Returns the state of the pattern, see EPatternState.
	int						GetState() const { return m_patternState; }

	// Returns the global deformation of the pattern.
	float					GetGlobalDeformation() const { return m_patternDeformation; };

	// Displayes debug information about the pattern.
	void					DebugDraw(IRenderer* pRenderer);

	// Serializes the track pattern.
	void					Serialize( TSerialize ser, class CObjectTracker& objectTracker );


protected:

	struct Node
	{
		Node() : parent(0), flags(0), branchMethod(0), deformation(0), lastUsedBranch(0), signalValue(0),
			rad(0), exposure(0), totalExposure(0) {};

		void	Reset()
		{
			validatedPosition.Set( 0, 0, 0 );
			validatedDirection.Set( 0, 0, 0 );
			deformation = 0;
			offset.Set( 0, 0, 0 );
			parent = 0;
			flags = 0;
			branchMethod = 0;
			branchNodes.clear();
			signalValue = 0;
			lastUsedBranch = 0;
			rad = 0;
			exposure = 0;
			totalExposure = 0;
		}

		Vec3								validatedPosition;
		Vec3								validatedDirection;
		float								deformation;
		Vec3								offset;
		Node*								parent;
		int									flags;
		int									branchMethod;
		std::vector<Node*>	branchNodes;
		int									signalValue;
		size_t							lastUsedBranch;
		float								rad;
		float								exposure;
		float								totalExposure;
#ifdef TRACKPATTERN_DEBUG
		string							name;
#endif
	};

	// Choose the closest start node.
	Node*				ChooseStartNode();
	// Chooses next node based on the branching method.
	Node*				ChooseNextTarget( Node* curNode );
	// Propagates additively the deformation value to all the nodes further down in the hierarchy.
	void				PropagateDeformation( Node* startNode );
	// Propagates additively the total exposure value to all the nodes further down in the hierarchy.
	void				PropagateExposure( Node* startNode );
	// Internal update.
	void				UpdateInternal( float deltaTime );
	// Returns the target frame of reference.
	Matrix33		GetRefFrame( const Vec3& forw ) const;

	CPuppet*			m_pOwner;										// Owner of the track pattern.
	CTrackDummy*	m_pTargetDummy;							// The track target
	CTimeValue		m_lastUpdateTime;						// Last time the pattern was updated.
	CTimeValue		m_lastUsageTime;						// Last time the track target was used.
	CTimeValue		m_acquireTime;							// Last time the wakeup was called (the track target was acquired).
	bool					m_hasAttTarget;							// Flag indicatingthat the ower has attention target.
	string				m_curPatternName;						// The name of the current pattern.
	int						m_flags;										// Pattern functionality flgs, see ETrackPatternFlags.
	float					m_validationRadius;					// Pattern validation radius.
	float					m_advanceRadius;						// Pattern point advance radius.
	float					m_currentNodeDeformation;		// The deformation of the current node when it was selected.
	bool					m_hasStartNodes;						// Flag indicating that the current pattern has nodes marked with start flag.
	bool					m_hibernating;							// Flag indicating that the pattern has not been used for some time, and is being updated less frequently.
	float					m_globalDeformTreshold;			// The global deformation bottom range treshold.
	float					m_localDeformTreshold;			// The local (per node) deformation bottom range treshold.
	float					m_stateTresholdMin;					// The global deformation treshold to switch to exposed state.
	float					m_stateTresholdMax;					// The global deformation treshold to switch to enclosed state.
	bool					m_advance;									// Flag indicating if the pattern is being advanced.
	float					m_exposureMod;							// Modifer describing how much the exposure of each node affects the node selection.
	bool					m_nodeSignalSent;						// Flag indicating that the requested signal at node has been sent.
	Vec3					m_patternOrig;							// The origin (owner att target position) of the pattern.
	float					m_patternDeformation;				// The current global deformation value of the pattern.
	int						m_patternState;							// The current state of the pattern (exposed, enclosed).
	Node*					m_pCurrentNode;							// Pointer to current node.
	bool					m_patternValid;							// This flag is set when the pattern is validated the first time.
	std::vector<Node>	m_pattern;							// The nodes of the current pattern.
	Matrix33			m_rotMat;										// The rotation of the pattern.
	Ang3					m_rotAngles;								// The pattern random rotation.
	Vec3					m_initialDirToTarget;				// Initial direction to the target.
};

#endif
