#include "StdAfx.h"
#include "GraphStructures.h"

GraphNode::GraphNode(IAISystem::ENavigationType type, const Vec3 &inpos, unsigned int _ID)
: navType(type), pos(inpos)
{
	firstLinkIndex = 0;
	fCostFromStart = fInvalidCost;
	fEstimatedCostToGoal = fInvalidCost;
	prevPathNodeIndex = 0;
	mark = 0;
	nRefCount = 0;
	if (_ID == 0)
	{
		if (freeIDs.empty())
		{
			ID = maxID + 1;
		}
		else
		{
			ID = freeIDs.back();
			freeIDs.pop_back();
		}
	}
	else
	{
		ID = _ID;
	}
	if (ID > maxID)
		maxID = ID;
	tag = 0;

}

GraphNode::~GraphNode()
{
  freeIDs.push_back(ID);
}
