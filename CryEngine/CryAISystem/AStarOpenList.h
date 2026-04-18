#ifndef ASTAROPENLIST_H
#define ASTAROPENLIST_H

#if _MSC_VER > 1000
#pragma once
#endif

#include "IAgent.h"
#include "GraphStructures.h"
#include <vector>
#include <set>
#include <algorithm>

// Choose between using a std::set and a heap
//#define USE_SET

// If this isn't on the OpenListMonitor isn't even mentioned in AStarOpenList.
//#define MONITOR_OPEN_LIST
/**
 * Useful for gathering info about how A* open list operates.
 *
 * Receives an event for every node that's pushed to the open list and every
 * node that popped from there.  Based on that it computes various statistics.
 *
 * TODO Mai 22, 2007: <pvl> no reasonable way of outputting the gathered stats
 * at the moment.  In fact, mainly intended to be used "manually" under
 * debugger ... :)
 */
class OpenListMonitor
{
	/// Holds any info we care to hold per every node currently on the open list.
	struct NodeInfo {
		CTimeValue m_timestamp;
		int m_frame;
		NodeInfo (const CTimeValue & time, int frame) : m_timestamp(time), m_frame(frame)
		{ }
	};
	/// Uses graph node index as a unique node ID and maps that to corresponding node info.
	typedef std::map<unsigned int, NodeInfo> NodeInfoMap;
	NodeInfoMap m_nodeInfoMap;

	/// The actual open list statistics.
	float sMin;
	float sMax;
	float sAvg;
	int sNumSamples;
	float sMinFrames;
	float sMaxFrames;
	float sAvgFrames;
	/// For each frame that's leaving the open list, this is called with the time
	/// and a number of frames that the node spent on the open list.
	void UpdateStats (float t, int frames)
	{
		if (t < sMin) sMin = t;
		if (t > sMax) sMax = t;
		sAvg = (sAvg * sNumSamples + t) / (sNumSamples + 1);

		if (t < sMinFrames) sMinFrames = frames;
		if (t > sMaxFrames) sMaxFrames = frames;
		sAvgFrames = (sAvgFrames * sNumSamples + frames) / (sNumSamples + 1);

		++sNumSamples;
	}
public:
	void NodePushed (unsigned int nodeIndex)
	{
		// NOTE Mai 22, 2007: <pvl> we could filter incoming nodes here if we're
		// only interested in stats for a certain node class
/*
		GraphNode* nodeptr = nodeManager.GetNode(node);
		if (nodeptr->navType != IAISystem::NAV_WAYPOINT_HUMAN)
			return;
*/
		CTimeValue now = gEnv->pTimer->GetAsyncTime();
		int currFrame = gEnv->pRenderer->GetFrameID();
		m_nodeInfoMap.insert (std::make_pair (nodeIndex, NodeInfo (now, currFrame)));
	}
	void NodePopped (unsigned int nodeIndex)
	{
		NodeInfoMap::iterator infoIt = m_nodeInfoMap.find (nodeIndex);
		if (infoIt == m_nodeInfoMap.end ()) {
			// NOTE Mai 22, 2007: <pvl> can happen if we filter nodes in NodePushed()
			return;
		}

		CTimeValue timeWhenPushed = infoIt->second.m_timestamp;
		int frameWhenPushed = infoIt->second.m_frame;

		CTimeValue now = gEnv->pTimer->GetAsyncTime();
		int currFrame = gEnv->pRenderer->GetFrameID();

		float timeSpentInList = (now - timeWhenPushed).GetMilliSeconds();
		int framesSpentInList = currFrame - frameWhenPushed;
		UpdateStats (timeSpentInList, framesSpentInList);
	}
};

/// Helper class to sort the node lists
struct NodeCompare
{
	NodeCompare(CGraphNodeManager& nodeManager)
		: nodeManager(nodeManager)
	{
	}

  bool operator()(unsigned nodeIndex1, unsigned nodeIndex2) const 
	{
		GraphNode* node1 = nodeManager.GetNode(nodeIndex1);
		GraphNode* node2 = nodeManager.GetNode(nodeIndex2);

#ifdef USE_SET
		return ( (node1->fCostFromStart + node1->fEstimatedCostToGoal) < (node2->fCostFromStart + node2->fEstimatedCostToGoal) );
#else
		return ( (node1->fCostFromStart + node1->fEstimatedCostToGoal) > (node2->fCostFromStart + node2->fEstimatedCostToGoal) );
#endif
	}

	CGraphNodeManager& nodeManager;
};

//====================================================================
// CAStarOpenList
//====================================================================
class CAStarOpenList
{
public:
	CAStarOpenList(CGraphNodeManager& nodeManager)
		:	nodeManager(nodeManager)
#ifdef USE_SET
		, m_list(NodeCompare(nodeManager)
#endif //USE_SET

	{
	}

	/// Gets the best node and removes it from the list. Returns 0 if
	/// the list is empty
	unsigned PopBestNode();

	/// Adds a node to the list (shouldn't already be in the list)
	void AddNode(unsigned);

	/// If the node is in the list then its position in the list gets updated.
	/// If not the list isn't changed. Either way the node itself gets
	/// modified
	void UpdateNode(unsigned node, float newCost, float newEstimatedCost);

	/// Indicates if the list is empty
	bool IsEmpty() const;

	/// Empties the list
	void Clear();

	// returns memory usage in bytes
	size_t MemStats();

	/// Reserves memory based on an estimated max list size
	void ReserveMemory(size_t estimatedMaxSize);

private:
#ifdef USE_SET
	typedef std::set<unsigned, NodeCompare> tOpenList;
#else
	typedef std::vector<unsigned> tOpenList;
#endif
	/// the open list
	tOpenList m_list;
	CGraphNodeManager& nodeManager;

#ifdef MONITOR_OPEN_LIST
	OpenListMonitor m_monitor;
#endif // MONITOR_OPEN_LIST
};

//====================================================================
// Don't look below here - inline implementation
//====================================================================

//====================================================================
// ReserveMemory
//====================================================================
inline void CAStarOpenList::ReserveMemory(size_t estimatedMaxSize)
{
#ifndef USE_SET
	m_list.reserve(estimatedMaxSize);
#endif
}


//====================================================================
// MemStats
//====================================================================
inline size_t CAStarOpenList::MemStats()
{
#ifdef USE_SET
	return m_list.size() * sizeof(unsigned); // TODO: Obviously wrong
#else
	return m_list.capacity() * sizeof(unsigned);
#endif
}


//====================================================================
// IsEmpty
//====================================================================
inline bool CAStarOpenList::IsEmpty() const
{
	return m_list.empty();
}

//====================================================================
// PopBestNode
//====================================================================
inline unsigned CAStarOpenList::PopBestNode()
{
	if (IsEmpty())
		return 0;
#ifdef USE_SET
	unsigned node = *m_list.begin();
	m_list.erase(m_list.begin());
#else
	unsigned node = m_list.front();
	// This "forgets about" the last node, and (partially) sorts the rest
	std::pop_heap(m_list.begin(), m_list.end(), NodeCompare(nodeManager));
	// remove the redundant element
	m_list.pop_back();
#endif

#ifdef MONITOR_OPEN_LIST
	m_monitor.NodePopped (node);
#endif // MONITOR_OPEN_LIST

	return node;
}

//====================================================================
// AddNode
//====================================================================
inline void CAStarOpenList::AddNode(unsigned nodeIndex)
{
#ifdef USE_SET
	m_list.insert(nodeIndex);
#else
	m_list.push_back(nodeIndex);
	std::push_heap(m_list.begin(), m_list.end(), NodeCompare(nodeManager));
#endif

#ifdef MONITOR_OPEN_LIST
	m_monitor.NodePushed (nodeIndex);
#endif // MONITOR_OPEN_LIST
}

//====================================================================
// UpdateNode
//====================================================================
inline void CAStarOpenList::UpdateNode(unsigned nodeIndex, float newCost, float newEstimatedCost)
{
	GraphNode* node = nodeManager.GetNode(nodeIndex);

#ifdef USE_SET
	tOpenList::iterator it = m_list.find(nodeIndex);
	if (it != m_list.end())
	{
		m_list.erase(it);
		node->fCostFromStart = newCost;
		node->fEstimatedCostToGoal = newEstimatedCost;
		m_list.insert(nodeIndex);
	}
	else
	{
		node->fCostFromStart = newCost;
		node->fEstimatedCostToGoal = newEstimatedCost;
	}
#else
	const tOpenList::const_iterator end = m_list.end();
	for (tOpenList::iterator it = m_list.begin() ; it != end ; ++it)
	{
		if (*it == nodeIndex)
		{
			node->fCostFromStart = newCost;
			node->fEstimatedCostToGoal = newEstimatedCost;
			std::push_heap(m_list.begin(), it+1, NodeCompare(nodeManager));
			return;
		}
	}
	node->fCostFromStart = newCost;
	node->fEstimatedCostToGoal = newEstimatedCost;
#endif
}

//====================================================================
// Clear
//====================================================================
inline void CAStarOpenList::Clear()
{
#ifdef USE_SET
	m_list.clear();
#else
	m_list.resize(0);
#endif
}


#endif
