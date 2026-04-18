#ifndef ASTARSOLVER_H
#define ASTARSOLVER_H

#if _MSC_VER > 1000
#pragma once
#endif

#include "IAgent.h"
#include "AStarOpenList.h"
#include "SerializeFwd.h"
#include "AILog.h"

class CGraph;

enum EAStarResult
{
	ASTAR_NOPATH,
	ASTAR_STILLSOLVING,
	ASTAR_PATHFOUND,
};

//====================================================================
// SNavigationBlocker
// Represents an object that might be blocking a link. Each blocker is
// assumed to be spherical, with the position centred around the floor 
// so that links can intersect it.
//====================================================================
struct SNavigationBlocker
{
	/// pos and radius define the sphere. 
  /// costAddMod is a fixed cost (in m) associated with the blocker obscuring a link - a value of 0 has
  /// no effect - a value of 10 would make the link effectively 10m longer than it is.
  /// costMultMod is the cost modification factor - a value of 0 has no effect - a value of 10 would make the link 
	/// 10x more costly. -ve disables the link
  /// radialDecay indicates if the cost modifiers should decay linearly to 0 over the radius of the sphere
  /// directional indicates if the cost should be unaffected for motion in a radial direction
	SNavigationBlocker(const Vec3& pos, float radius, float costAddMod, float costMultMod, bool radialDecay, bool directional) 
    : sphere(pos, radius), costAddMod(costAddMod), costMultMod(costMultMod), restrictedLocation(false), 
    radialDecay(radialDecay), directional(directional) {}

	/// Just to allow std::vector::resize(0)
	SNavigationBlocker() : sphere(Vec3(0, 0, 0), 0.0f), costAddMod(0), costMultMod(0), radialDecay(false) {AIAssert("Should never get called");}

  Sphere sphere;
  bool radialDecay;
  bool directional;

  /// absolute cost added to any link going through this blocker (useful for small blockers)
	float costAddMod;
  /// multiplier for link costs going through this blocker (0 means no extra cost, 1 means to double etc)
  float costMultMod;

  // info to speed up the intersection checks
  /// If this is true then the blocker is small enough that it only affects the
  /// nav type it resides in. If false then it affects everything.
  bool restrictedLocation;
  IAISystem::ENavigationType navType;
  union Location
  {
    struct // similar for other nav types when there's info to go in them
    {
      // no node because the node "areas" can overlap so it's not useful
      int nBuildingID;
    } waypoint;
  };
  /// Only gets used if restrictedLocation = true
  Location location;
};

typedef std::vector<SNavigationBlocker> NavigationBlockers;

class CHeuristic  
{
public:
	CHeuristic() {};
	virtual ~CHeuristic() {};

	/// Estimate the cost from one node to another - this estimate must be a minimum
	/// bound on the cost in order for AStar to work correctly (though it may run faster
	/// if the estimate more a best-guess than a lower-bound)
	virtual float EstimateCost(const GraphNode & node0, const GraphNode & node1) const = 0;

	/// Calculate the cost from one node to another via a specific link. This calculation
	/// should include anything specific like terrain costs, stealth vs speed etc
	/// A return value < 0 means that this link is absolutely impassable.
	virtual float CalculateCost(const CGraph * graph,
															const GraphNode & node0, unsigned linkIndex0,
															const GraphNode & node1,
															const NavigationBlockers& navigationBlockers) const = 0;

	/// Can be overridden to bypass expensive calculations in CalculateCost
	virtual bool IsLinkPassable(const CGraph * graph,
															const GraphNode & node0, unsigned linkIndex0,
															const GraphNode & node1,
															const NavigationBlockers& navigationBlockers) const {
		return CalculateCost(graph, node0, linkIndex0, node1, navigationBlockers) >= 0.0f;}

	virtual const struct CPathfinderRequesterProperties* GetRequesterProperties() const {return 0;}

};

class CCalculationStopper;

//====================================================================
// CAStarSolver
// Implements A*, supporting splitting the solving over multiple calls. 
//====================================================================
class CAStarSolver
{
public:
	CAStarSolver(CGraphNodeManager& nodeManager);

	/// Setup the path search. A warning will be generated if a path is already in progress - if so
	/// that path will be aborted and the new one started.
	/// Also pass in a collection of navigation blockers - this collection will be copied (to make
	/// sure it doesn't change as the search proceeds)
	EAStarResult SetupAStar(
		CCalculationStopper& stopper,
		const CGraph* pGraph, 
		const CHeuristic* pHeuristic,
		unsigned startIndex, unsigned endIndex,
		const NavigationBlockers& navigationBlockers,
		bool bDebugDrawOpenList);
	/// Continue the path search - all pointers passed in to SetupAStar must still be valid
	EAStarResult ContinueAStar(CCalculationStopper& stopper);

	/// Cancel any existing path request
	void AbortAStar();

	/// returns the current heuristic
	const CHeuristic* GetHeuristic() const {return m_request.m_pHeuristic;}

	typedef std::vector<unsigned> tPathNodes;

	/// Returns the path - only valid once pathfinding has returned ASTAR_PATHFOUND, and before
	/// the next path is requested
	const tPathNodes& GetPathNodes() {return m_pathNodes;}

	/// If the last result was ASTAR_NOPATH then this calculates and returns a partial path that ends 
	/// up as close as possible (according to the heuristic) to the requested destination
	const tPathNodes& CalculateAndGetPartialPath();

	/// Serialise the current request
	virtual void Serialize(TSerialize ser, class CObjectTracker& objectTracker);

	/// returns the memory usage in bytes
	size_t MemStats();

private:
	/// If continue/start found a path then this will actually ratify it - again all pointers
	/// must still be valid
	EAStarResult WalkBack(const CCalculationStopper& stopper);

	void ASTARStep();

	/// Everything we need to know to continue/recreate this request
	struct CRequest
	{
		// Cached state - i.e. state set up when a path is requested and maintained across
		// subsequent calls to ContinueAStar

		/// Pointer to the graph currently being used. If 0 means we're not currently solving.
		const CGraph* m_pGraph;
		/// A* heuristic
		const CHeuristic *m_pHeuristic;
		/// Pointer to the start graph node
		unsigned m_startIndex;

		/// Pointer to the end graph node
		unsigned m_endIndex;
	};

	/// Our request
	CRequest m_request;

	/// Final path we store - can be picked up using GetPathNodes()
	tPathNodes m_pathNodes;

	/// current node we're working on
	unsigned m_pathfinderCurrentIndex;

	/// The open list (nodes that still need investigating). Rather than using a closed
	/// list, "closed" nodes will be tagged but not on the open list 
	CAStarOpenList m_openList;

	/// Searching for the path, or walking back to identify it after it's found?
	bool m_bWalkingBack;

	/// Debug draw open list
	bool m_bDebugDrawOpenList;

	/// collection of blockers used to dynamically modify the links
	NavigationBlockers m_navigationBlockers;
};

#endif