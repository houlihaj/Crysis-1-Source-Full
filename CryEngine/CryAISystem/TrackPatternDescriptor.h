/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   TrackPatternDescriptor.h
	$Id$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 30/8/2005   : Created by Mikko Mononen

*********************************************************************/


#ifndef __TrackPatternDescriptor_H__
#define __TrackPatternDescriptor_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include "AITrackPatternDescriptor.h"
#include <list>
#include <map>

/// Implementation of the track pattern descriptor node,
/// see ITrackPatternDescriptorNode for details.
class CTrackPatternDescriptorNode : public ITrackPatternDescriptorNode
{
public:
	CTrackPatternDescriptorNode();

	virtual void	SetOffset( const Vec3& pos ) { m_offset = pos; }
	virtual void	SetParentName( const char* name ) { m_parentName = name; }
	virtual void	SetBranchMethod( int method ) { m_branchMethod = method; }
	virtual void	AddBranchNodeName( const char* name ) { m_branchNodeNames.push_back( name ); }
	virtual void	SetFlags( int val ) { m_flags = val; }
	virtual void	SetSignalValue( int val ) { m_signalValue = val; }

	size_t	MemStats() const;

	Vec3							m_offset;
	string						m_parentName;
	CTrackPatternDescriptorNode*	m_parent;
	int								m_flags;
	int								m_branchMethod;
	int								m_index;
	int								m_signalValue;
	std::list<string>												m_branchNodeNames;
	std::list<CTrackPatternDescriptorNode*>	m_branchNodes;
};


typedef	std::map<string, CTrackPatternDescriptorNode>	TTrackPatDescNodeMap;


/// Implementation of the track pattern descriptor,
/// see ITrackPatternDescriptor for details.
class CTrackPatternDescriptor : public ITrackPatternDescriptor
{
public:
	CTrackPatternDescriptor( const char* name );

	virtual const char* GetName() const;
	virtual void	SetFlags( int flags );
	virtual void	SetAdvanceRadius( float radius );
	virtual void	SetValidationRadius( float radius );
	virtual void	SetDeformationTreshold( float globalTreshold, float localTreshold );
	virtual void	SetStateTreshold( float stateTresholdMin, float stateTresholdMax );
	virtual void	SetExposureMod( float mod );
	virtual void	SetRandomRotation( const Ang3& angles );
	virtual void	AddNode( const char* nodeName, const Vec3& offset, int flags, const char* parentName, int signalValue );
	virtual ITrackPatternDescriptorNode*				GetNode( const char* nodeName );
	virtual const ITrackPatternDescriptorNode*	GetNode( const char* nodeName ) const;
	virtual void	Finalize();

	size_t	MemStats() const;

	string								m_name;
	TTrackPatDescNodeMap	m_nodes;
	int										m_flags;
	float									m_validationRadius;
	float									m_advanceRadius;
	bool									m_finalized;
	float									m_globalDeformTreshold;
	float									m_localDeformTreshold;
	float									m_stateTresholdMin;
	float									m_stateTresholdMax;
	float									m_exposureMod;
	Ang3									m_rotAngles;
};

#endif