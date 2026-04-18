// Graph.h: interface for the CGraph class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_GRAPH_H__6D059D2E_5A74_4352_B3BF_2C88D446A2E1__INCLUDED_)
#define AFX_GRAPH_H__6D059D2E_5A74_4352_B3BF_2C88D446A2E1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "IAISystem.h"
#include "IAgent.h"
#include "Heuristic.h"
#include "NavPath.h"
#include "AILog.h"
#include "ISerialize.h"
#include "AllNodesContainer.h"
#include "AutoTypeStructs.h"

#include <list>
#include <map>
#include <set>
#include <vector>
#include <CryArray.h>
#include <VectorMap.h>
#include "STLPoolAllocator.h"

struct IRenderer;
class CCryFile;
class CGraphLinkManager;

enum EPathfinderResult
{
  PATHFINDER_STILLFINDING,
  PATHFINDER_BEAUTIFYINGPATH,
  PATHFINDER_POPNEWREQUEST,
  PATHFINDER_PATHFOUND,
  PATHFINDER_NOPATH,
  PATHFINDER_ABORT,
  PATHFINDER_MAXVALUE
};
class CAISystem;
struct IStatObj;
class ICrySizer;
class CAIObject;
class CVolumeNavRegion;
class CFlightNavRegion;
class CGraphNodeManager;

class CSmartObject;
struct CCondition;

struct IVisArea;

inline int TypeIndexFromType(IAISystem::ENavigationType type)
{
	int typeIndex;
	for (typeIndex = IAISystem::NAV_TYPE_COUNT - 1; typeIndex >= 0 && ((1 << typeIndex) & type) == 0; --typeIndex);
	return typeIndex;
}

inline const char* StringFromTypeIndex(int typeIndex)
{
	static const char* navTypeStrings[IAISystem::NAV_TYPE_COUNT] = {
		"NAV_UNSET",
		"NAV_TRIANGULAR",
		"NAV_WAYPOINT_HUMAN",
		"NAV_WAYPOINT_3DSURFACE",
		"NAV_FLIGHT",
		"NAV_VOLUME",
		"NAV_ROAD",
		"NAV_SMARTOBJECT",
		"NAV_FREE_2D"
	};
	enum {STRING_COUNT = sizeof(navTypeStrings) / sizeof(navTypeStrings[0])};

	if (typeIndex	< 0)
		return "<Invalid Nav Type>";
	else if (typeIndex >= STRING_COUNT)
		return "<Nav Type Added - Update StringFromType()>";
	else
		return navTypeStrings[typeIndex];
}

inline const char* StringFromType(IAISystem::ENavigationType type)
{
	return StringFromTypeIndex(TypeIndexFromType(type));
}

//====================================================================
// CObstacleRef
//====================================================================
class CObstacleRef
{
protected:
  IAIObject*	m_pAnchor;		// pointer to designer defined hiding point
  int			m_vertexIndex;	// index of vertex
  unsigned	m_nodeIndex;		// for indoors nodes could be hide points
	GraphNode* m_pNode;
  CSmartObject*	m_pSmartObject;	// pointer to smart object to be used for hiding
  CCondition*	m_pRule;		// pointer to smart object rule to be used for hiding

public:
  IAIObject* GetAnchor() const { return m_pAnchor; }
  int GetVertex() const { return m_vertexIndex; }
  const unsigned GetNodeIndex() const { return m_nodeIndex; }
	const GraphNode* GetNode() const {return m_pNode;}
  CSmartObject* GetSmartObject() const { return m_pSmartObject; }
  const CCondition* GetRule() const { return m_pRule; }

  CObstacleRef() : m_pAnchor(NULL), m_vertexIndex(-1), m_nodeIndex(0), m_pNode(0), m_pSmartObject(NULL), m_pRule(NULL) {}
  CObstacleRef(const CObstacleRef& other) : m_pAnchor(other.m_pAnchor), m_vertexIndex(other.m_vertexIndex), m_nodeIndex(other.m_nodeIndex), m_pNode(other.m_pNode),
	  m_pSmartObject(other.m_pSmartObject), m_pRule(other.m_pRule) {}
  CObstacleRef(IAIObject* pAnchor) : m_pAnchor(pAnchor), m_vertexIndex(-1), m_nodeIndex(0), m_pNode(0), m_pSmartObject(NULL), m_pRule(NULL) {}
  CObstacleRef(int vertexIndex) : m_pAnchor(NULL), m_vertexIndex(vertexIndex), m_nodeIndex(0), m_pSmartObject(NULL), m_pRule(NULL) {}
  CObstacleRef(unsigned nodeIndex, GraphNode* pNode) : m_pAnchor(NULL), m_vertexIndex(-1), m_nodeIndex(nodeIndex), m_pNode(pNode), m_pSmartObject(NULL), m_pRule(NULL) {}
  CObstacleRef(CSmartObject* pSmartObject, CCondition* pRule) : m_pAnchor(NULL), m_vertexIndex(-1), m_nodeIndex(0), m_pNode(0)
	  , m_pSmartObject(pSmartObject), m_pRule(pRule) {}

  void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
  Vec3 GetPos() const;
	float GetApproxRadius() const;
  const CObstacleRef& operator = (const CObstacleRef& other)
  {
    m_pAnchor = other.m_pAnchor;
    m_vertexIndex = other.m_vertexIndex;
    m_nodeIndex = other.m_nodeIndex;
		m_pNode = other.m_pNode;
    m_pSmartObject = other.m_pSmartObject;
	m_pRule = other.m_pRule;
    return *this;
  }

  bool operator == (const CObstacleRef& other) const
  {
    return m_pAnchor == other.m_pAnchor && m_vertexIndex == other.m_vertexIndex && m_nodeIndex == other.m_nodeIndex
      && m_pNode == other.m_pNode && m_pSmartObject == other.m_pSmartObject && m_pRule == other.m_pRule;
  }
  bool operator != (const CObstacleRef& other) const
  {
    return m_pAnchor != other.m_pAnchor || m_vertexIndex != other.m_vertexIndex || m_nodeIndex != other.m_nodeIndex
      || m_pNode != other.m_pNode || m_pSmartObject != other.m_pSmartObject || m_pRule != other.m_pRule;
  }
  bool operator < (const CObstacleRef& other) const
  {
    return
      m_nodeIndex < other.m_nodeIndex || m_nodeIndex == other.m_nodeIndex &&
      (	m_pAnchor < other.m_pAnchor || m_pAnchor == other.m_pAnchor &&
      ( m_vertexIndex < other.m_vertexIndex || m_vertexIndex == other.m_vertexIndex &&
	  ( m_pSmartObject < other.m_pSmartObject || m_pSmartObject < other.m_pSmartObject && 
	    m_pRule < other.m_pRule ) ) );
  }
  operator bool () const
  {
    return m_pAnchor || m_vertexIndex >= 0 || m_nodeIndex || m_pSmartObject && m_pRule;
  }
  bool operator ! () const
  {
	  return !m_pAnchor && m_vertexIndex < 0 && !m_nodeIndex && (!m_pSmartObject || !m_pRule);
  }

private:
  operator int () const
  {
    // it is illegal to cast CObstacleRef to an int!!!
    // are you still using old code?
    AIAssert(0);
    return 0;
  }
};

// NOTE: int64 here avoids a tiny performance impact on 32-bit platform
// for the cost of loss of full compatibility: 64-bit generated BAI files
// can't be used on 32-bit platform safely. Change the key to int64 to 
// make it fully compatible. The code that uses this map will be recompiled
// to use the full 64-bit key on both 32-bit and 64-bit platforms.
typedef std::multimap<int64,unsigned> EntranceMap;
typedef std::list<Vec3> ListPositions;
typedef std::list<ObstacleData> ListObstacles;
typedef std::multimap<float, ObstacleData> MultimapRangeObstacles;
typedef std::vector<NodeDescriptor> NodeDescBuffer;
typedef std::vector<LinkDescriptor> LinkDescBuffer;
typedef std::list<GraphNode *> ListNodes;
typedef std::list<unsigned> ListNodeIds;
typedef std::set<GraphNode*> SetNodes;
typedef std::set<const GraphNode*> SetConstNodes;
typedef std::list<const GraphNode *> ListConstNodes;
typedef std::vector<const GraphNode *> VectorConstNodes;
typedef std::vector<unsigned> VectorConstNodeIndices;
typedef std::multimap<float,GraphNode*> CandidateMap;
typedef std::multimap<float,unsigned> CandidateIdMap;
typedef std::set< CObstacleRef > SetObstacleRefs;
typedef VectorMap<unsigned, SCachedPassabilityResult> PassabilityCache;

// [Mikko] Note: The Vector map is faster when traversing, and the normal map with pool allocator seems
// to be a little faster in CGraph.GetNodesInRange. Both are faster than normal std::map.
//typedef stl::STLPoolAllocator< std::pair<const GraphNode*, float> > NodeMapAllocator;
//typedef std::map<const GraphNode*, float, std::less<const GraphNode*>, NodeMapAllocator> MapConstNodesDistance;
typedef VectorMap<const GraphNode*, float> MapConstNodesDistance;


//#define USE_GRAPH_NODE_POOL

// static memory pool 
//struct GraphNodesPool 
//{
//	GraphNodesPool()
//	{
//		for (int i = 0; i < IAISystem::NAV_TYPE_COUNT; ++i)
//			m_currentPools[i] = -1;
//		m_typeSizes[0] = sizeof(GraphNode_Unset);
//		m_typeSizes[1] = sizeof(GraphNode_Triangular);
//		m_typeSizes[2] = sizeof(GraphNode_WaypointHuman);
//		m_typeSizes[3] = sizeof(GraphNode_Waypoint3DSurface);
//		m_typeSizes[4] = sizeof(GraphNode_Flight);
//		m_typeSizes[5] = sizeof(GraphNode_Volume);
//		m_typeSizes[6] = sizeof(GraphNode_Road);
//		m_typeSizes[7] = sizeof(GraphNode_SmartObject);
//		m_typeSizes[8] = sizeof(GraphNode_Free2D);
//	}
//
//	~GraphNodesPool()
//	{
//		Clear();
//	}
//
//	size_t MemStats();
//
//	inline bool AddPool(IAISystem::ENavigationType type, size_t num)
//	{
//#ifdef USE_GRAPH_NODE_POOL
//		int typeIndex = TypeIndexFromType(type);
//		if (typeIndex < 0 || num == 0)
//			return true;
//
//		SPoolInfo pool;
//
//		pool.m_nCapacity = num;
//		pool.m_pNodes = (char*)malloc(pool.m_nCapacity * m_typeSizes[typeIndex]);
//
//		m_currentPools[typeIndex] = m_Pool.size();
//		m_Pool.push_back(pool);
//#endif
//		return true;
//	}
//
//	inline void Clear()
//	{
//		for (std::size_t i =0, end = m_Pool.size(); i < end; ++i  )
//		{
//			ClearPool(&m_Pool[i]);
//		}
//	}
//	/*inline */GraphNode* AddGraphNode(IAISystem::ENavigationType type, const Vec3 &inpos, unsigned int _ID)
//	{
//		int typeIndex = TypeIndexFromType(type);
//		if (typeIndex < 0)
//			return 0;
//
//#ifdef USE_GRAPH_NODE_POOL
//		SPoolInfo* pool = (m_currentPools[typeIndex] >= 0 ? &m_Pool[m_currentPools[typeIndex]] : 0);
//		char* memory = 0;
//		if (!pool || pool->m_nNodes >= pool->m_nCapacity)
//			memory = new char [m_typeSizes[typeIndex]];
//		else
//			memory = pool->m_pNodes + m_typeSizes[typeIndex] * pool->m_nNodes++;
//#else //USE_GRAPH_NODE_POOL
//		char* memory = new char [m_typeSizes[typeIndex]];
//#endif //USE_GRAPH_NODE_POOL
//
//		// inplace constructor
//		GraphNode* pNode = 0;
//		switch (type)
//		{
//		case IAISystem::NAV_UNSET: pNode = new(memory) GraphNode_Unset(type, inpos, _ID); break;
//		case IAISystem::NAV_TRIANGULAR: pNode = new(memory) GraphNode_Triangular(type, inpos, _ID); break;
//		case IAISystem::NAV_WAYPOINT_HUMAN: pNode = new(memory) GraphNode_WaypointHuman(type, inpos, _ID); break;
//		case IAISystem::NAV_WAYPOINT_3DSURFACE: pNode = new(memory) GraphNode_Waypoint3DSurface(type, inpos, _ID); break;
//		case IAISystem::NAV_FLIGHT: pNode = new(memory) GraphNode_Flight(type, inpos, _ID); break;
//		case IAISystem::NAV_VOLUME: pNode = new(memory) GraphNode_Volume(type, inpos, _ID); break;
//		case IAISystem::NAV_ROAD: pNode = new(memory) GraphNode_Road(type, inpos, _ID); break;
//		case IAISystem::NAV_SMARTOBJECT: pNode = new(memory) GraphNode_SmartObject(type, inpos, _ID); break;
//		case IAISystem::NAV_FREE_2D: pNode = new(memory) GraphNode_Free2D(type, inpos, _ID); break;
//		default: break;
//		}
//
//		return pNode;
//	}
//
//	/*inline */void DeleteGraphNode(GraphNode* pNode)
//	{
//		assert(pNode->navType != 0);
//		int typeIndex = TypeIndexFromType(pNode->navType);
//		assert(typeIndex >= 0);
//		if (typeIndex < 0)
//			return;
//
//		switch (pNode->navType)
//		{
//		case IAISystem::NAV_UNSET: static_cast<GraphNode_Unset*>(pNode)->~GraphNode_Unset(); break;
//		case IAISystem::NAV_TRIANGULAR: static_cast<GraphNode_Triangular*>(pNode)->~GraphNode_Triangular(); break;
//		case IAISystem::NAV_WAYPOINT_HUMAN: static_cast<GraphNode_WaypointHuman*>(pNode)->~GraphNode_WaypointHuman(); break;
//		case IAISystem::NAV_WAYPOINT_3DSURFACE: static_cast<GraphNode_Waypoint3DSurface*>(pNode)->~GraphNode_Waypoint3DSurface(); break;
//		case IAISystem::NAV_FLIGHT: static_cast<GraphNode_Flight*>(pNode)->~GraphNode_Flight(); break;
//		case IAISystem::NAV_VOLUME: static_cast<GraphNode_Volume*>(pNode)->~GraphNode_Volume(); break;
//		case IAISystem::NAV_ROAD: static_cast<GraphNode_Road*>(pNode)->~GraphNode_Road(); break;
//		case IAISystem::NAV_SMARTOBJECT: static_cast<GraphNode_SmartObject*>(pNode)->~GraphNode_SmartObject(); break;
//		case IAISystem::NAV_FREE_2D: static_cast<GraphNode_Free2D*>(pNode)->~GraphNode_Free2D(); break;
//		default: break;
//		}
//
//#ifdef USE_GRAPH_NODE_POOL
//		for (std::size_t i =0, end = m_Pool.size(); i < end; ++i)
//		{
//			SPoolInfo& pool = m_Pool[i];
//			if (pNode->navType == pool.type && (char*)pNode >= pool.m_pNodes && (char*)pNode < (pool.m_pNodes + pool.m_nCapacity * m_typeSizes[typeIndex]))
//				return;
//		}
//#endif
//
//		// seems this graphnode not stored in pool. just delete it (but only the memory, since we have already destructed it)
//		delete (char*)pNode;
//	}
//
//	//inline uint32 GetCapacity() const 
//	//{
//	//	return m_pCurrentPool->m_nCapacity;
//	//}
//
//	//inline uint32 GetCount() const 
//	//{
//	//	return m_pCurrentPool->m_nNodes;
//	//}
//
//
//private:
//
//	struct SPoolInfo
//	{
//		IAISystem::ENavigationType type;
//
//		char* m_pNodes;
//		uint32 m_nNodes;
//		uint32 m_nCapacity;
//		SPoolInfo() : m_pNodes(0), m_nNodes(0), m_nCapacity(0) {};
//	};
//
//	inline void ClearPool(SPoolInfo * pool)
//	{
//		if (pool->m_pNodes)
//		{
//			for (std::size_t i = 0; i < pool->m_nNodes; ++i)
//			{
//				if (GraphNode* pNode = (GraphNode*)&(pool->m_pNodes[i]))
//				{
//					pNode->~GraphNode();
//				}
//			}
//			free(pool->m_pNodes);
//		}
//		pool->m_nCapacity = 0;
//		pool->m_pNodes = 0;
//		pool->m_nNodes = 0;
//	}
//
//	DynArray<SPoolInfo> m_Pool;
//	int m_currentPools[IAISystem::NAV_TYPE_COUNT];
//	int m_typeSizes[IAISystem::NAV_TYPE_COUNT];
//};

//====================================================================
// CGraph 
//====================================================================
class CGraph : public IGraph 
{
public:
  CGraph(CAISystem* pAISystem);
  virtual ~CGraph();

	CGraphLinkManager& GetLinkManager() {return *m_pGraphLinkManager;}
	const CGraphLinkManager& GetLinkManager() const {return *m_pGraphLinkManager;}

	CGraphNodeManager& GetNodeManager() {return *m_pGraphNodeManager;}
	const CGraphNodeManager& GetNodeManager() const {return *m_pGraphNodeManager;}

	/// Restores the graph to the initial state (i.e. restores pass radii etc). 
  void Reset();

  /// removes all nodes and stuff associated with navTypes matching the bitmask
  void Clear(unsigned navTypeMask);

  /// Connects (two-way) two nodes, optionally returning pointers to the new links
  virtual void Connect(unsigned oneIndex, unsigned twoIndex, float radiusOneToTwo = 100.0f, float radiusTwoToOne = 100.0f,
    unsigned* pLinkOneTwo = 0, unsigned* pLinkTwoOne = 0);

  /// Disconnects a node from its neighbours. if bDelete then pNode will be deleted. Note that 
  /// the previously connected nodes will not be deleted, even if they
  /// end up with no nodes.
  virtual void Disconnect(unsigned nodeIndex, bool bDelete = true);

  /// Removes an individual link from a node (and removes the reciprocal link) - 
  /// doesn't delete it.
  virtual void Disconnect(unsigned nodeIndex, unsigned linkId);

  /// Checks the graph is OK (as far as possible). Asserts if not, and then
  /// returns true/false to indicate if it's OK
  /// msg should indicate where this is being called from (for writing error msgs)
  virtual bool Validate(const char * msg, bool checkPassable) const;

  /// Checks that a node exists (should be quick). If fullCheck is true it will do some further 
  /// checks which will be slower
  virtual bool ValidateNode(unsigned nodeIndex, bool fullCheck) const;
	bool ValidateHashSpace() { return m_allNodes.ValidateHashSpace(); }

  // fns used by the editor to get info about nodes
  virtual int GetBuildingIDFromWaypointNode(const GraphNode *pNode);
  virtual void SetBuildingIDInWaypointNode(GraphNode *pNode, unsigned nBuildingID);
  virtual void SetVisAreaInWaypointNode(GraphNode *pNode, IVisArea *pArea);
  virtual EWaypointNodeType GetWaypointNodeTypeFromWaypointNode(const GraphNode *pNode);
  virtual void SetWaypointNodeTypeInWaypointNode(GraphNode *pNode, EWaypointNodeType type);
  virtual EWaypointLinkType GetOrigWaypointLinkTypeFromLink(unsigned link);
  virtual float GetRadiusFromLink(unsigned link);
  virtual float GetOrigRadiusFromLink(unsigned link);
  virtual IAISystem::ENavigationType GetNavType(const GraphNode *pNode);
  virtual unsigned GetNumNodeLinks(const GraphNode *pNode);
  virtual unsigned GetGraphLink(const GraphNode *pNode, unsigned iLink);
  virtual Vec3 GetWaypointNodeUpDir(const GraphNode *pNode);
  virtual void SetWaypointNodeUpDir(GraphNode *pNode, const Vec3 &up);
  virtual Vec3 GetWaypointNodeDir(const GraphNode *pNode);
  virtual void SetWaypointNodeDir(GraphNode *pNode, const Vec3 &dir);
  virtual Vec3 GetNodePos(const GraphNode *pNode);
  virtual const GraphNode *GetNextNode(unsigned ink) const;
  virtual GraphNode *GetNextNode(unsigned ink);
	unsigned int LinkId (unsigned link) const;

  /// General function to get the graph node enclosing a position - the type depends
  /// on the navigation modifiers etc and the navigation capabilities. If you know what 
  /// navigation type you want go via the specific NavRegion class. If there are a number of nodes
  /// that could be returned, only nodes that have at least one link with radius > passRadius will 
  /// be returned
  virtual unsigned GetEnclosing(const Vec3 &pos, IAISystem::tNavCapMask navCapMask, float passRadius = 0.0f,
    unsigned startIndex = 0, float range = 0.0f, Vec3 * closestValid = 0, bool returnSuspect = false, const char *requesterName = "");

  /// Restores all node/links
  virtual void RestoreAllNavigation( );

  /// adds an entrance for easy traversing later
  virtual void MakeNodeRemovable(int nBuildingID, unsigned nodeIndex, bool bRemovable = true);
  /// adds an entrance for easy traversing later
  virtual void AddIndoorEntrance(int nBuildingID, unsigned nodeIndex, bool bExitOnly = false);
  /// returns false if the entrace can't be found
  virtual bool RemoveEntrance(int nBuildingID, unsigned nodeIndex);

  /// Reads the AI graph from a specified file
  bool ReadFromFile(const char * szName);
  /// Writes the AI graph to file
  void WriteToFile(const char *pname);

  static bool AllocObstacle(CObstacleRef obstacle) {return static_AllocatedObstacles.insert(obstacle).second;}
  static void FreeObstacle(CObstacleRef obstacle)	{static_AllocatedObstacles.erase(obstacle);}

  /// Returns all nodes that are in the graph - not all nodes will be
  /// connected (even indirectly) to each other
  CAllNodesContainer& GetAllNodes() {return m_allNodes;}
  const CAllNodesContainer& GetAllNodes() const {return m_allNodes;}

  /// Checks that the graph is empty. Pass in a bitmask of IAISystem::ENavigationType to 
  /// specify the types to check
  bool CheckForEmpty(IAISystem::tNavCapMask navTypeMask = IAISystem::NAVMASK_ALL) const;

  /// Does a search for a partial path from startPos up to a maximum cost as calculated by the
  /// heuristic and returns the position on the path closest (in terms of cost, determined by the
  /// heuristic) to the end position. This uses a brute-force approach, so is only meant for 
  /// fairly small values of maxCost (maybe up to a few tens of m). 
  /// If there's a problem (e.g. start position is bad) then the position returned is startPos.
  /// The maxCost value is only approximate - if possible this function will return endPos, even 
  /// if the actual cost exceeds maxCost. Currently, if endPos is too far away, only graph node
  /// locations get returned.
  Vec3 GetBestPosition(const CHeuristic& heuristic, float maxCost, 
    const Vec3& startPos, const Vec3& endPos, unsigned startHintIndex, IAISystem::tNavCapMask navCapMask);

  //====================================================================
  // 	functions for modifying the graph at run-time
  //====================================================================
  struct CRegion
  {
    CRegion(const std::list<Vec3>& outline, const AABB& aabb) : m_outline(outline), m_AABB(aabb) {}
    std::list<Vec3> m_outline;
    AABB m_AABB;
  };
  struct CRegions
  {
    std::list<CRegion> m_regions;
    AABB m_AABB;
  };
  // Disable navigation due to terrain breakage in the regions
  void DisableNavigationInBrokenRegions(const CRegions & regions);
  /// Restores just links that have been disabled due to breakable terrain
  void RestoreBrokenLinks();
  // remove all removable nodes withing this building - simulate environment changes - explosions
  void BreakNodes(int nBuildingID);

  // Check whether a position is within a node's triangle
  bool PointInTriangle(const Vec3 & pos, GraphNode * pNode);

  /// uses mark for internal graph operation without disturbing the pathfinder
  void MarkNode(unsigned nodeIndex) const;
  /// clears the marked nodes
  void ClearMarks() const;

  /// Tagging of nodes - must be used ONLY by the pathfinder (apart from when triangulation is generated)
  void TagNode(unsigned nodeIndex) const;
  /// Clears the tagged nodes
  void ClearTags() const;
  /// for debug drawing - nodes get tagged during A*
  const VectorConstNodeIndices & GetTaggedNodes() const {return m_taggedNodes;}

  // defines bounding rectangle of this graph
  void SetBBox(const Vec3 & min, const Vec3 & max);
  // how is that for descriptive naming of functions ??
  bool InsideOfBBox(const Vec3 & pos) const; // returns true if pos is inside of bbox (but not on boundaries)

  /// Creates a new node of the specified type (which can't be subsequently changed). Note that
  /// to delete the node use the disconnect function. 
  unsigned CreateNewNode(IAISystem::ENavigationType type, const Vec3 &pos, unsigned ID = 0);
	GraphNode* GetNode(unsigned index);

  /// Moves a node, updating spatial structures
  void MoveNode(unsigned nodeIndex, const Vec3 &newPos);

  /// finds all nodes within range of startPos and their distance from vStart. 
  /// pStart is just used as a hint.
  /// returns a reference to the input/output so it's easy to use in a test.
  /// traverseForbiddenHideLink should be true if you want to pass through 
  /// links between hide waypoints that have been marked as impassable
  /// SmartObjects will only be considered if pRequester != 0
	MapConstNodesDistance &GetNodesInRange(MapConstNodesDistance &result, const Vec3 & startPos, float maxDist, 
		IAISystem::tNavCapMask navCapMask, float passRadius, unsigned startNodeIndex = 0, const class CAIObject *pRequester = 0);

  GraphNode * GetThirdNode(const Vec3 & vFirst, const Vec3 & vSecond, const Vec3 & vThird);

	SCachedPassabilityResult* GetCachedPassabilityResult(unsigned linkId, bool bCreate);

  //====================================================================
  // The following are used during serialisation
  //====================================================================
  typedef std::map<unsigned, unsigned> TGraphNodeIDs;

  /// Get an ID that is guaranteed to be the same everytime the game is
  /// run for this particular level.
  /// if graphNodeIDs is non-zero then the ID gets added to it
  unsigned GetIDFromNode(unsigned nodeIndex, TGraphNodeIDs *graphNodeIDs = 0) const;

  /// Get a node from a previously calculated ID. Returns 0 if the ID cannot be found
  /// Note that if graphNodeIDs is 0 then this is SLOW - only use it like that for a few nodes
  unsigned GetNodeFromID(unsigned ID, TGraphNodeIDs *graphNodeIDs = 0) const;

  /// Serialise the _modifications_ to the graph
  void Serialize(TSerialize ser, class CObjectTracker& objectTracker, const char* graphName);

  /// Tell the object tracker about objects we've created/own
  void PopulateObjectTracker(class CObjectTracker& objectTracker);

  /// Serialise a _pointer_ to a node. Name is the caller's name for this node
  void SerializeNodePointer(TSerialize ser, const char* name, unsigned& pNode, TGraphNodeIDs *graphNodeIDs = 0) const;

  /// Serialise a container of GraphNode pointers (only certain containers are supported)
  template<typename Container> void SerializeNodePointerContainer(TSerialize ser, const char* name, Container& container, TGraphNodeIDs *graphNodeIDs = 0) const;

  //====================================================================
  // 	The following functions are used during graph generation/triangulation
  //====================================================================
  void ResolveLinkData(GraphNode* pOne, GraphNode* pTwo);
  void ConnectNodes(ListNodeIds & lstNodes);
  void FillGraphNodeData(unsigned nodeIndex);
  typedef std::list<int>			ObstacleIndexList;
  typedef std::multimap< float, GraphNode* >	NodesList;
  int			Rearrange(const string & areaName, ListNodeIds& nodesList, const Vec3r& cutStart, const Vec3r& cutEnd );
  int			ProcessMegaMerge(const string & areaName, ListNodeIds& nodesList, const Vec3r& cutStart, const Vec3r& cutEnd );
  bool		CreateOutline(const string & areaName, ListNodeIds& insideNodes, ListNodeIds& nodesList, ObstacleIndexList&	outline);
  void		TriangulateOutline(const string & areaName, ListNodeIds& nodesList, ObstacleIndexList&	outline, bool orientation );
  bool		ProcessMerge(const string & areaName, GraphNode	*curNode, CGraph::NodesList& ndList );

  // Returns memory usage not including nodes
  size_t MemStats();
  // Returns the memory usage for nodes of the type passed in (bitmask)
  size_t NodeMemStats(unsigned navTypeMask);

  struct SBadGraphData
  {
    enum EType {BAD_PASSABLE, BAD_IMPASSABLE};
    EType mType;
    Vec3 mPos1, mPos2;
  };
  /// List of bad stuff we found during the last validation. mutable because it's
  /// debug - Validate(...) should really be a const method, since it wouldn't change
  /// any "real" data
  mutable std::vector<SBadGraphData> mBadGraphData;

  GraphNode *m_pSafeFirst;
	unsigned m_safeFirstIndex;

	CGraphNodeManager* m_pGraphNodeManager;

private:

  // maps IDs to node pointers. On Writing writes the IDs of nodes that are
  // being serialised. On reading just sets up the map for the serialised nodes
  void SerializeGraphNodeIDs(TSerialize ser, CGraph::TGraphNodeIDs &graphNodeIDs);

  /// Finds nodes within a distance of pNode that can be accessed by something with
  /// passRadius. If pRequester != 0 then smart object links will be checked as well.
  void FindNodesWithinRange(MapConstNodesDistance &result, float curDist, float maxDist, 
    const GraphNode *pNode, float passRadius, const class CAIObject *pRequester) const ;

#if 0
  void SelectTriangularNodes(SSelectNodesResult &result, const Vec3 & vCenter, float fRadius, const GraphNode* pStart);
  void SelectWaypointNodes(SSelectNodesResult &result, const Vec3 & vCenter, float fRadius, IAISystem::tNavCapMask navCapMask, const GraphNode *pStart);
#endif
  void DisableInSphere(const Vec3 &pos,float fRadius);
  void EnableInSphere(const Vec3 &pos,float fRadius);

  bool DbgCheckList( ListNodeIds& nodesList )	const;
  int GetNodesInSphere(const Vec3 &pos, float fRadius);

public:
  /// deletes (disconnects too) all nodes with a type matching the bitmask
  void DeleteGraph(IAISystem::tNavCapMask navTypeMask);

private:
  GraphNode * GetEntrance(int nBuildingID,const Vec3 &pos);
  bool GetEntrances(int nBuildingID, const Vec3& pos, std::vector<unsigned>& nodes);
  void WriteLine(unsigned nodeIndex);
  // reads all the nodes in a map
  bool ReadNodes( CCryFile &file );
  /// Deletes the node, which should have been disconnected first (warning if not)
  void DeleteNode(unsigned nodeIndex);

  // helper called from ValidateNode only (to get ValidateNode to be inlined inside
  // Graph.cpp)
  bool ValidateNodeFullCheck(const GraphNode* pNode) const;

	unsigned m_currentIndex;
  GraphNode *m_pCurrent;
	unsigned m_firstIndex;
  GraphNode *m_pFirst;

  ListNodeIds	m_lstNodesInsideSphere;

  /// All the nodes we've marked
  mutable VectorConstNodeIndices m_markedNodes;
  /// All the nodes we've tagged
  mutable VectorConstNodeIndices m_taggedNodes;

  /// nodes are allocated/deleted via a single interface, so keep track of them
  /// all - for memory tracking and to allow quick iteration
  CAllNodesContainer m_allNodes;

	PassabilityCache m_passabilityCache;

  /// Used to save/load nodes - should be empty otherwise
  NodeDescBuffer m_vNodeDescs;
  /// Used to save/load links - should be empty otherwise
  LinkDescBuffer m_vLinkDescs;

	CGraphLinkManager* m_pGraphLinkManager;

  /// Bounding box of the triangular area
  AABB m_triangularBBox;

  EntranceMap m_mapEntrances;
  EntranceMap m_mapExits;
  EntranceMap m_mapRemovables;

  static SetObstacleRefs static_AllocatedObstacles;

  friend class CTriangularNavRegion;
  friend class CWaypointHumanNavRegion;
  friend class CFlightNavRegion;
  friend class CVolumeNavRegion;
};

//====================================================================
// SMarkClearer
// Helper - the constructor and destructor clear marks
//====================================================================
struct SMarkClearer
{
  SMarkClearer(const CGraph* pGraph) : m_pGraph(pGraph) {m_pGraph->ClearMarks();}
  ~SMarkClearer() {m_pGraph->ClearMarks();}
private:
  const CGraph* m_pGraph;
};

//====================================================================
// Inline implementations
//====================================================================

//====================================================================
// SerializeNodePointerContainer
//====================================================================
template<typename Container>
inline void CGraph::SerializeNodePointerContainer(TSerialize ser, const char* name, Container& container, TGraphNodeIDs *graphNodeIDs) const
{
  unsigned containerSize = container.size();
  ser.BeginGroup(name);
  ser.Value("size", containerSize);
  if (containerSize > 0)
  {
    if (ser.IsReading())
    {
      container.clear();
      for (unsigned i = 0 ; i < containerSize ; ++i)
      {
				ser.BeginGroup("n");
        unsigned nodeIndex;
        SerializeNodePointer(ser, "n", nodeIndex, graphNodeIDs);
        if (nodeIndex)
          container.push_back(nodeIndex);
        else
          AIWarning("got zero node pointer reading %s", name);
				ser.EndGroup();
      }
    }
    else
    {
      // writing
      for (typename Container::const_iterator it = container.begin() ; it != container.end() ; ++it)
      {
				ser.BeginGroup("n");
        unsigned nodeIndex = *it;
        if (nodeIndex)
          SerializeNodePointer(ser, "n", nodeIndex, graphNodeIDs);
        else
          AIWarning("got zero node pointer writing %s", name);
				ser.EndGroup();
      }
    }
  }
  else if (ser.IsReading())
  {
    container.clear();
  }
  ser.EndGroup();
}

#include "AIHash.h"

inline unsigned int CGraph::LinkId (unsigned link) const
{
	unsigned int prev = GetLinkManager().GetPrevNode (link);
	unsigned int next = GetLinkManager().GetNextNode (link);
	const GraphNode * prevNode = GetNodeManager().GetNode(prev);
	const GraphNode * nextNode = GetNodeManager().GetNode(next);
	return HashFromVec3 (prevNode->GetPos (), 0.0f) + HashFromVec3 (nextNode->GetPos (), 0.0f);
}

#endif // !defined(AFX_GRAPH_H__6D059D2E_5A74_4352_B3BF_2C88D446A2E1__INCLUDED_)
