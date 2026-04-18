/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   TrackPatternDescriptor.cpp
	$Id$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 30/8/2005   : Created by Mikko Mononen

*********************************************************************/

#include "StdAfx.h"
#include "TrackPatternDescriptor.h"
#include "AILog.h"
#include MATH_H

CTrackPatternDescriptorNode::CTrackPatternDescriptorNode() :
	m_offset( 0, 0, 0 ), m_flags( 0 ), m_branchMethod( AITRACKPAT_CHOOSE_ALWAYS ),
	m_index(0), m_parent(0), m_signalValue(0)
{
	// Empty
}

size_t CTrackPatternDescriptorNode::MemStats() const
{
	size_t	size = sizeof( CTrackPatternDescriptorNode );
	size += m_parentName.capacity();
	for( std::list<string>::const_iterator iter = m_branchNodeNames.begin(); iter != m_branchNodeNames.end(); ++iter )
		size += (*iter).capacity();

	size += m_branchNodes.size() * sizeof( CTrackPatternDescriptorNode* );
	return size;
}

CTrackPatternDescriptor::CTrackPatternDescriptor( const char* name ) :
	m_name( name ), m_flags( 0 ),
	m_globalDeformTreshold( 0.0f ), m_localDeformTreshold( 0.0f ), 
	m_stateTresholdMin( 0.35f ), m_stateTresholdMax( 0.4f ),
	m_validationRadius( 0 ), m_finalized( false ), m_rotAngles( 0, 0, 0 )
{
	// empty
}

const char*	CTrackPatternDescriptor::GetName() const
{
	return m_name.c_str();
}

void	CTrackPatternDescriptor::SetFlags( int flags )
{
	m_flags = flags;
}

void	CTrackPatternDescriptor::SetValidationRadius( float radius )
{
	m_validationRadius = radius;
}

void	CTrackPatternDescriptor::SetAdvanceRadius( float radius )
{
	m_advanceRadius = radius;
}

void	CTrackPatternDescriptor::SetDeformationTreshold( float globalTreshold, float localTreshold )
{
	m_globalDeformTreshold = globalTreshold;
	m_localDeformTreshold = localTreshold;
}


void	CTrackPatternDescriptor::SetStateTreshold( float stateTresholdMin, float stateTresholdMax )
{
	m_stateTresholdMin = stateTresholdMin;
	m_stateTresholdMax = stateTresholdMax;
}

void CTrackPatternDescriptor::SetExposureMod( float mod )
{
	m_exposureMod = mod;
}

void CTrackPatternDescriptor::SetRandomRotation( const Ang3& angles )
{
	m_rotAngles = angles;
}

void CTrackPatternDescriptor::AddNode( const char* nodeName, const Vec3& offset, int flags, const char* parentName, int signalValue )
{
	CTrackPatternDescriptorNode	node;
	node.m_offset = offset;
	node.m_flags = flags;
	node.m_parentName = parentName;
	node.m_signalValue = signalValue;
	node.m_index = 0; //m_nodes.size();

	TTrackPatDescNodeMap::iterator	nodeIt = m_nodes.find( nodeName );
	if( nodeIt != m_nodes.end() )
		m_nodes.erase( nodeIt );
	// The seq does not exists.
	m_nodes.insert( TTrackPatDescNodeMap::value_type( nodeName, node ) );
}

ITrackPatternDescriptorNode*	CTrackPatternDescriptor::GetNode( const char* nodeName )
{
	TTrackPatDescNodeMap::iterator	nodeIt = m_nodes.find( nodeName );
	if( nodeIt != m_nodes.end() )
		return (ITrackPatternDescriptorNode*)&((*nodeIt).second);
	return 0;
}

const ITrackPatternDescriptorNode*	CTrackPatternDescriptor::GetNode( const char* nodeName ) const
{
	TTrackPatDescNodeMap::const_iterator	nodeIt = m_nodes.find( nodeName );
	if( nodeIt != m_nodes.end() )
		return (const ITrackPatternDescriptorNode*)&((*nodeIt).second);
	return 0;
}

void	CTrackPatternDescriptor::Finalize()
{
	// Update parent and branch pointers.
	int	i = 0;
	for( TTrackPatDescNodeMap::iterator noIt = m_nodes.begin(); noIt != m_nodes.end(); ++noIt )
	{
		CTrackPatternDescriptorNode&	descNode = noIt->second;

		descNode.m_index = i++;

		// Find parent.
		if( !descNode.m_parentName.empty() )
		{
			CTrackPatternDescriptorNode*	branchNode = (CTrackPatternDescriptorNode*)GetNode( descNode.m_parentName );
			if( !branchNode )
				AILogAlways( "<CTrackPatternDescriptor> While building hierarchy for node '%s', could not find parent node '%s'.", (*noIt).first.c_str(), descNode.m_parentName.c_str() );
			descNode.m_parent = branchNode;
		}

		// Find branch nodes.
		for( std::list<string>::const_iterator braIt = descNode.m_branchNodeNames.begin(); braIt != descNode.m_branchNodeNames.end(); ++braIt )
		{
			CTrackPatternDescriptorNode*	branchNode = (CTrackPatternDescriptorNode*)GetNode( (*braIt) );
			if( !branchNode )
				AILogAlways( "<CScriptBind_AI> While building hierarchy for node '%s', could not find branch node '%s'.", (*noIt).first.c_str(), (*braIt).c_str() );
			else
				descNode.m_branchNodes.push_back( branchNode );
		}
	}

	// Finished with this descriptor.
	m_finalized = true;
}

size_t	CTrackPatternDescriptor::MemStats() const
{
	size_t	size = sizeof( CTrackPatternDescriptor );
	size += m_name.capacity();
	for( TTrackPatDescNodeMap::const_iterator iter = m_nodes.begin(); iter != m_nodes.end(); ++iter )
	{
		size += (*iter).first.capacity();
		size += (*iter).second.MemStats();
	}
	return size;
}

