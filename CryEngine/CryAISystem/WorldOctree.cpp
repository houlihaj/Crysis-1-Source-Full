#include "StdAfx.h"
#include "AILog.h"
#include <ISystem.h>
#include "WorldOctree.h"
#include <limits>
#include <IRenderer.h>
#include <IRenderAuxGeom.h>

//====================================================================
// CWorldOctree
//====================================================================
CWorldOctree::CWorldOctree()
:
m_testCounter(0)
{
}

//====================================================================
// CWorldOctree
//====================================================================
CWorldOctree::~CWorldOctree()
{
}

//====================================================================
// Clear
//====================================================================
void CWorldOctree::Clear(bool freeMemory)
{
	if( freeMemory )
	{
    ClearVectorMemory(m_cells);
    ClearVectorMemory(m_triangles);
	}
	else
	{
		m_cells.resize(0);
		m_triangles.resize(0);
	}
}


//====================================================================
// AddTriangle
//====================================================================
void CWorldOctree::AddTriangle(const CTriangle & triangle)
{
	m_triangles.push_back(triangle);
}

//====================================================================
// Clear
//====================================================================
void CWorldOctree::COctreeCell::Clear()
{
	for (unsigned i = 0 ; i < NUM_CHILDREN ; ++i)
		m_childCellIndices[i] = -1;
	m_triangleIndices.clear();
}

//====================================================================
// COctreeCell
//====================================================================
CWorldOctree::COctreeCell::COctreeCell()
{
	Clear();
}

//====================================================================
// COctreeCell
//====================================================================
CWorldOctree::COctreeCell::COctreeCell(const AABB & aabb)
:
m_aabb(aabb)
{
	Clear();
}

//====================================================================
// IncrementTestCounter
//====================================================================
inline void CWorldOctree::IncrementTestCounter() const
{
	++m_testCounter;
	if (m_testCounter == 0)
	{
		// wrap around - clear all the triangle counters
		const unsigned numTriangles = m_triangles.size();
		for (unsigned i = 0 ; i != numTriangles ; ++i)
			m_triangles[i].m_counter = 0;
		m_testCounter = 1;
	}
}


//====================================================================
// DoesTriangleIntersectCell
//====================================================================
bool CWorldOctree::DoesTriangleIntersectCell(const CTriangle & triangle, 
																						 const COctreeCell & cell) const
{
	// quick test
	if (cell.m_aabb.IsContainPoint(triangle.m_v0) ||
			cell.m_aabb.IsContainPoint(triangle.m_v1) ||
			cell.m_aabb.IsContainPoint(triangle.m_v2))
			return true;
	return Overlap::AABB_Triangle(cell.m_aabb, triangle.m_v0, triangle.m_v1, triangle.m_v2);
}

//====================================================================
// CreateAABB
//====================================================================
AABB CWorldOctree::CreateAABB(const AABB & aabb, COctreeCell::EChild child) const
{
	Vec3 dims = 0.5f * (aabb.max - aabb.min);
	Vec3 offset;
	switch (child)
	{
	case COctreeCell::PPP: offset.Set(1, 1, 1); break;
	case COctreeCell::PPM: offset.Set(1, 1, 0); break;
	case COctreeCell::PMP: offset.Set(1, 0, 1); break;
	case COctreeCell::PMM: offset.Set(1, 0, 0); break;
	case COctreeCell::MPP: offset.Set(0, 1, 1); break;
	case COctreeCell::MPM: offset.Set(0, 1, 0); break;
	case COctreeCell::MMP: offset.Set(0, 0, 1); break;
	case COctreeCell::MMM: offset.Set(0, 0, 0); break;
	default:
		AIWarning("CWorldOctree::CreateAABB Got impossible child: %d", child);
		offset.Set(0, 0, 0);
		break;
	}

	AABB result;
	result.min = aabb.min + Vec3(offset.x * dims.x, offset.y * dims.y, offset.z * dims.z);
	result.max = result.min + dims;
	// expand it just a tiny bit just to be safe!
	float extra = 0.00001f;
	result.min -= extra * dims;
	result.max += extra * dims;
	return result;
}

struct TriOutsideAABB
{
	TriOutsideAABB(const AABB& aabb) : m_aabb(aabb) {}

	bool operator() (const CWorldOctree::CTriangle& tri)
	{
		if (m_aabb.IsContainPoint(tri.m_v0)) return false;
		if (m_aabb.IsContainPoint(tri.m_v1)) return false;
		if (m_aabb.IsContainPoint(tri.m_v2)) return false;
		return !Overlap::AABB_Triangle(m_aabb, tri.m_v0, tri.m_v1, tri.m_v2);
	}

	const AABB m_aabb;
};

//====================================================================
// BuildOctree
//====================================================================
void CWorldOctree::BuildOctree(int maxTrianglesPerCell, float minCellSize, const AABB & bbox)
{
	m_boundingBox = bbox;

	// clear any existing cells
	m_cells.clear();

	// remove any triangles that aren't in the bbox
	std::vector<CTriangle>::iterator it = std::remove_if(m_triangles.begin(), m_triangles.end(), TriOutsideAABB(m_boundingBox));
	m_triangles.erase(it, m_triangles.end());

	// set up the root
	m_cells.push_back(COctreeCell(m_boundingBox));
	unsigned numTriangles = m_triangles.size();
	m_cells.back().m_triangleIndices.resize(numTriangles);
	for (unsigned i = 0 ; i < numTriangles ; ++i)
		m_cells.back().m_triangleIndices[i] = i;

	// rather than doing things recursively, use a stack of cells that need
	// to be processed - for each cell if it contains too many triangles we 
	// create child cells and move the triangles down into them (then we
	// clear the parent triangles).
	std::vector<int> cellsToProcess;
	cellsToProcess.push_back(0);

	// bear in mind during this that any time a new cell gets created any pointer
	// or reference to an existing cell may get invalidated - so use indexing.
	while (!cellsToProcess.empty())
	{
		int cellIndex = cellsToProcess.back();
		cellsToProcess.pop_back();

		if ( (m_cells[cellIndex].m_triangleIndices.size() <= maxTrianglesPerCell) ||
			   (m_cells[cellIndex].m_aabb.GetRadius() < minCellSize) )
			continue;

		// we need to put these triangles into the children
		for (unsigned iChild = 0 ; iChild < COctreeCell::NUM_CHILDREN ; ++iChild)
		{
			m_cells[cellIndex].m_childCellIndices[iChild] = (int) m_cells.size();
			cellsToProcess.push_back((int) m_cells.size());
			m_cells.push_back(COctreeCell(CreateAABB(m_cells[cellIndex].m_aabb, (COctreeCell::EChild) iChild)));

			COctreeCell & childCell = m_cells.back();
			unsigned numCellTriangles = m_cells[cellIndex].m_triangleIndices.size();
			for (unsigned i = 0 ; i < numCellTriangles ; ++i)
			{
				int iTri = m_cells[cellIndex].m_triangleIndices[i];
				const CTriangle & tri = m_triangles[iTri];
				if (DoesTriangleIntersectCell(tri, childCell))
				{
					childCell.m_triangleIndices.push_back(iTri);
				}
			}
		}
		// the children handle all the triangles now - we no longer need them
		m_cells[cellIndex].m_triangleIndices.clear();
	}
}

//====================================================================
// IntersectRay
// Note that the intersection functions assume a left-handed winding of triangles!
//====================================================================
bool CWorldOctree::IntersectLineseg(CIntersectionData * data, Lineseg lineseg, bool twoSided, int startingCell) const
{
	m_cellsToTest.resize(0);
	bool gotHit = false;

	IncrementTestCounter();

	/// prepare the output
	if (data)
		data->t = 1.0f;

	// doesn't have to be normalised
	Vec3 lineDir = lineseg.end - lineseg.start;
	CCellSorter cellSorter(&lineDir, &m_cells);

	m_cellsToTest.push_back(startingCell);
	while (!m_cellsToTest.empty())
	{
		int cellIndex = m_cellsToTest.back();
		m_cellsToTest.pop_back();

		const COctreeCell & cell = m_cells[cellIndex];

		if (!Overlap::Lineseg_AABB(lineseg, cell.m_aabb))
			continue;

		if (cell.IsLeaf())
		{
			// if leaf test the triangles
			unsigned numTriangles = cell.m_triangleIndices.size();
			for (unsigned i = 0 ; i < numTriangles ; ++i)
			{
				int triangleIndex = cell.m_triangleIndices[i];
				AIAssert(triangleIndex >= 0);
				AIAssert(triangleIndex < m_triangles.size());
				const CTriangle & triangle = m_triangles[triangleIndex];
				// see if we've already done this triangle
				if (triangle.m_counter == m_testCounter)
					continue;
				triangle.m_counter = m_testCounter;
				if (data)
				{
					Vec3 output;
					float t;
					if (Intersect::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v2, triangle.m_v1, output, &t))
					{
						data->t *= t;
						data->pt = output;
						data->normal = (triangle.m_v1 - triangle.m_v0) ^ (triangle.m_v2 - triangle.m_v0);
						data->normal.NormalizeSafe();
						gotHit = true;
						lineseg.end = lineseg.start + t * (lineseg.end - lineseg.start);
					}
					else if (twoSided && 
									 Intersect::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v1, triangle.m_v2, output, &t))
					{
						data->t *= t;
						data->pt = output;
						data->normal = (triangle.m_v1 - triangle.m_v0) ^ (triangle.m_v2 - triangle.m_v0);
						data->normal.NormalizeSafe();
						gotHit = true;
						lineseg.end = lineseg.start + t * (lineseg.end - lineseg.start);
					}
				}
				else
				{
					// cheaper overlap test
					if (Overlap::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v2, triangle.m_v1))
						return true;
					if (twoSided)
						if (Overlap::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v1, triangle.m_v2))
							return true;
				}
			}

		}
		else
		{
			// if non-leaf, just add the children, 
			for (unsigned iChild = 0 ; iChild < COctreeCell::NUM_CHILDREN ; ++iChild)
			{
				int childIndex = cell.m_childCellIndices[iChild];
				m_cellsToTest.push_back(childIndex);
			}
			// sort the children so that we start working on the cells nearest the line
			// start first
			std::sort(m_cellsToTest.end() - COctreeCell::NUM_CHILDREN, m_cellsToTest.end(), cellSorter);
		}
	}
	return gotHit;
}

//====================================================================
// IntersectLinesegFromCache
//====================================================================
bool CWorldOctree::IntersectLinesegFromCache(CIntersectionData * data, 
																						 Lineseg lineseg, 
																						 bool twoSided) const
{
	bool gotHit = false;
	IncrementTestCounter();

	/// prepare the output
	if (data)
		data->t = 1.0f;

	// doesn't have to be normalised
	Vec3 lineDir = lineseg.end - lineseg.start;
	CCellSorter cellSorter(&lineDir, &m_cells);

	unsigned numCells = m_cachedCells.size();
	for (unsigned iCell = 0 ; iCell < numCells ; ++iCell)
	{
		int cellIndex = m_cachedCells[iCell];
		const COctreeCell & cell = m_cells[cellIndex];

		if (!Overlap::Lineseg_AABB(lineseg, cell.m_aabb))
			continue;

		AIAssert(cell.IsLeaf());

		// if leaf test the triangles
		unsigned numTriangles = cell.m_triangleIndices.size();
		for (unsigned i = 0 ; i < numTriangles ; ++i)
		{
			int triangleIndex = cell.m_triangleIndices[i];
			AIAssert(triangleIndex >= 0);
			AIAssert(triangleIndex < m_triangles.size());
			const CTriangle & triangle = m_triangles[triangleIndex];
			// see if we've already done this triangle
			if (triangle.m_counter == m_testCounter)
				continue;
			triangle.m_counter = m_testCounter;
			if (data)
			{
				Vec3 output;
				float t;
				if (Intersect::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v2, triangle.m_v1, output, &t))
				{
					data->t *= t;
					data->pt = output;
					data->normal = (triangle.m_v1 - triangle.m_v0) ^ (triangle.m_v2 - triangle.m_v0);
					data->normal.NormalizeSafe();
					gotHit = true;
					lineseg.end = lineseg.start + t * (lineseg.end - lineseg.start);
				}
				else if (twoSided && 
									Intersect::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v1, triangle.m_v2, output, &t))
				{
					data->t *= t;
					data->pt = output;
					data->normal = (triangle.m_v1 - triangle.m_v0) ^ (triangle.m_v2 - triangle.m_v0);
					data->normal.NormalizeSafe();
					gotHit = true;
					lineseg.end = lineseg.start + t * (lineseg.end - lineseg.start);
				}
			}
			else
			{
				// cheaper overlap test
				if (Overlap::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v2, triangle.m_v1))
					return true;
				if (twoSided)
					if (Overlap::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v1, triangle.m_v2))
						return true;
			}
		}

	}
	return gotHit;
}

//====================================================================
// IntersectLinesegFromCache2
//====================================================================
bool CWorldOctree::IntersectLinesegFromCache2(CIntersectionData * data, 
																						 Lineseg lineseg, 
																						 bool twoSided) const
{
	bool gotHit = false;
	IncrementTestCounter();

	/// prepare the output
	if (data)
		data->t = 1.0f;

	// doesn't have to be normalised
	Vec3 lineDir = lineseg.end - lineseg.start;
	CCellSorter cellSorter(&lineDir, &m_cells);

	// at some point work out how to avoid the copy here?
	m_cellsToTest = m_cachedCells2;
	while (!m_cellsToTest.empty())
	{
		int cellIndex = m_cellsToTest.back();
		m_cellsToTest.pop_back();

		const COctreeCell & cell = m_cells[cellIndex];

		if (!Overlap::Lineseg_AABB(lineseg, cell.m_aabb))
			continue;

		if (cell.IsLeaf())
		{
			// if leaf test the triangles
			unsigned numTriangles = cell.m_triangleIndices.size();
			for (unsigned i = 0 ; i < numTriangles ; ++i)
			{
				int triangleIndex = cell.m_triangleIndices[i];
				AIAssert(triangleIndex >= 0);
				AIAssert(triangleIndex < m_triangles.size());
				const CTriangle & triangle = m_triangles[triangleIndex];
				// see if we've already done this triangle
				if (triangle.m_counter == m_testCounter)
					continue;
				triangle.m_counter = m_testCounter;
				if (data)
				{
					Vec3 output;
					float t;
					if (Intersect::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v2, triangle.m_v1, output, &t))
					{
						data->t *= t;
						data->pt = output;
						data->normal = (triangle.m_v1 - triangle.m_v0) ^ (triangle.m_v2 - triangle.m_v0);
						data->normal.NormalizeSafe();
						gotHit = true;
						lineseg.end = lineseg.start + t * (lineseg.end - lineseg.start);
					}
					else if (twoSided && 
									 Intersect::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v1, triangle.m_v2, output, &t))
					{
						data->t *= t;
						data->pt = output;
						data->normal = (triangle.m_v1 - triangle.m_v0) ^ (triangle.m_v2 - triangle.m_v0);
						data->normal.NormalizeSafe();
						gotHit = true;
						lineseg.end = lineseg.start + t * (lineseg.end - lineseg.start);
					}
				}
				else
				{
					// cheaper overlap test
					if (Overlap::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v2, triangle.m_v1))
						return true;
					if (twoSided)
						if (Overlap::Lineseg_Triangle(lineseg, triangle.m_v0, triangle.m_v1, triangle.m_v2))
							return true;
				}
			}

		}
		else
		{
			// if non-leaf, just add the children, 
			for (unsigned iChild = 0 ; iChild < COctreeCell::NUM_CHILDREN ; ++iChild)
			{
				int childIndex = cell.m_childCellIndices[iChild];
				m_cellsToTest.push_back(childIndex);
			}
			// sort the children so that we start working on the cells nearest the line
			// start first
			std::sort(m_cellsToTest.end() - COctreeCell::NUM_CHILDREN, m_cellsToTest.end(), cellSorter);
		}
	}
	return gotHit;
}

//====================================================================
// CacheCellsOverlappingAABB
//====================================================================
void CWorldOctree::CacheCellsOverlappingAABB(const AABB & aabb)
{
	m_cachedCells.resize(0);

	m_cellsToTest.resize(0);
	m_cellsToTest.push_back(0);

	while (!m_cellsToTest.empty())
	{
		int cellIndex = m_cellsToTest.back();
		m_cellsToTest.pop_back();
		const COctreeCell & cell = m_cells[cellIndex];

		if (!Overlap::AABB_AABB(aabb, cell.m_aabb))
			continue;

		if (cell.IsLeaf())
		{
			if (!cell.m_triangleIndices.empty())
				m_cachedCells.push_back(cellIndex);
		}
		else
		{
			// if non-leaf, just add the children to check
			for (unsigned iChild = 0 ; iChild < COctreeCell::NUM_CHILDREN ; ++iChild)
			{
				int childIndex = cell.m_childCellIndices[iChild];
				m_cellsToTest.push_back(childIndex);
			}
		}
	}
}

//====================================================================
// CacheCellsOverlappingSphere
//====================================================================
void CWorldOctree::CacheCellsOverlappingSphere(const Vec3 & pos, float radius)
{
	m_cachedCells.resize(0);

	m_cellsToTest.resize(0);
	m_cellsToTest.push_back(0);

	Sphere sphere(pos, radius);

	while (!m_cellsToTest.empty())
	{
		int cellIndex = m_cellsToTest.back();
		m_cellsToTest.pop_back();
		const COctreeCell & cell = m_cells[cellIndex];

		if (!Overlap::Sphere_AABB(sphere, cell.m_aabb))
			continue;

		if (cell.IsLeaf())
		{
			if (!cell.m_triangleIndices.empty())
				m_cachedCells.push_back(cellIndex);
		}
		else
		{
			// if non-leaf, just add the children to check
			for (unsigned iChild = 0 ; iChild < COctreeCell::NUM_CHILDREN ; ++iChild)
			{
				int childIndex = cell.m_childCellIndices[iChild];
				m_cellsToTest.push_back(childIndex);
			}
		}
	}
}

//====================================================================
// CacheCellsOverlappingAABB2
//====================================================================
void CWorldOctree::CacheCellsOverlappingAABB2(const AABB & aabb)
{
	m_cachedCells2.resize(0);

	m_cellsToTest.resize(0);
	m_cellsToTest.push_back(0);

	while (!m_cellsToTest.empty())
	{
		int cellIndex = m_cellsToTest.back();
		m_cellsToTest.pop_back();
		const COctreeCell & cell = m_cells[cellIndex];

		if (!Overlap::AABB_AABB(aabb, cell.m_aabb))
			continue;

		if (cell.IsLeaf())
		{
			if (!cell.m_triangleIndices.empty())
				m_cachedCells2.push_back(cellIndex);
		}
		else
		{
			// if non-leaf, just add the children to check
			for (unsigned iChild = 0 ; iChild < COctreeCell::NUM_CHILDREN ; ++iChild)
			{
				int childIndex = cell.m_childCellIndices[iChild];
				m_cellsToTest.push_back(childIndex);
			}
		}
	}
}

enum EAABBSphereResult {AABB_INSIDE, SPHERE_INSIDE, SPHERE_AABB_INTERSECT, SPHERE_AABB_SEPARATE};
//====================================================================
// AABBSphereRelationship
// Indicates if the AABB is completely inside, outside, intersecting with, 
// or separate from the sphere
//====================================================================
EAABBSphereResult AABBSphereRelationship(const Sphere & sphere, const AABB & aabb)
{
	char res = Overlap::Sphere_AABB_Inside(sphere, aabb);
	if (res == 0)
		return SPHERE_AABB_SEPARATE;
	else if (res == 2)
		return SPHERE_INSIDE;

	// there is overlap - just need to see if the aabb is inside - this is the 
	// case only if all 8 corners of the aab are less than sphere.radius from 
	// sphere.center
	float r2 = sphere.radius * sphere.radius;
	if      ( (Vec3(aabb.min.x, aabb.min.y, aabb.min.z) - sphere.center).GetLengthSquared() > r2)
		return SPHERE_AABB_INTERSECT;
	else if ( (Vec3(aabb.min.x, aabb.min.y, aabb.max.z) - sphere.center).GetLengthSquared() > r2)
		return SPHERE_AABB_INTERSECT;
	else if ( (Vec3(aabb.min.x, aabb.max.y, aabb.min.z) - sphere.center).GetLengthSquared() > r2)
		return SPHERE_AABB_INTERSECT;
	else if ( (Vec3(aabb.min.x, aabb.max.y, aabb.max.z) - sphere.center).GetLengthSquared() > r2)
		return SPHERE_AABB_INTERSECT;
	else if ( (Vec3(aabb.max.x, aabb.min.y, aabb.min.z) - sphere.center).GetLengthSquared() > r2)
		return SPHERE_AABB_INTERSECT;
	else if ( (Vec3(aabb.max.x, aabb.min.y, aabb.max.z) - sphere.center).GetLengthSquared() > r2)
		return SPHERE_AABB_INTERSECT;
	else if ( (Vec3(aabb.max.x, aabb.max.y, aabb.min.z) - sphere.center).GetLengthSquared() > r2)
		return SPHERE_AABB_INTERSECT;
	else if ( (Vec3(aabb.max.x, aabb.max.y, aabb.max.z) - sphere.center).GetLengthSquared() > r2)
		return SPHERE_AABB_INTERSECT;

	return AABB_INSIDE;
}

//====================================================================
// CacheCellsOverlappingSphere
//====================================================================
void CWorldOctree::CacheCellsOverlappingSphere2(const Vec3 & pos, float radius)
{
	m_cachedCells2.resize(0);

	m_cellsToTest.resize(0);
	m_cellsToTest.push_back(0);

	Sphere sphere(pos, radius);

	while (!m_cellsToTest.empty())
	{
		int cellIndex = m_cellsToTest.back();
		m_cellsToTest.pop_back();
		const COctreeCell & cell = m_cells[cellIndex];

		EAABBSphereResult result = AABBSphereRelationship(sphere, cell.m_aabb);

		switch (result)
		{
		case SPHERE_AABB_SEPARATE:
			// nothing to do
			break;

		case SPHERE_INSIDE:
		case SPHERE_AABB_INTERSECT:
			// descend further if possible
			if (cell.IsLeaf())
			{
				if (!cell.m_triangleIndices.empty())
					m_cachedCells2.push_back(cellIndex);
			}
			else
			{
				// if non-leaf, just add the children to check
				for (unsigned iChild = 0 ; iChild < COctreeCell::NUM_CHILDREN ; ++iChild)
				{
					int childIndex = cell.m_childCellIndices[iChild];
					m_cellsToTest.push_back(childIndex);
				}
			}
			break;

		case AABB_INSIDE:
			// add this, unless it's an empty leaf
			if (cell.IsLeaf())
			{
				if (!cell.m_triangleIndices.empty())
					m_cachedCells2.push_back(cellIndex);
			}
			else
			{
				m_cachedCells2.push_back(cellIndex);
			}
		}
	}
}

//====================================================================
// FindCellContainingAABB
//====================================================================
int CWorldOctree::FindCellContainingAABB(const AABB & aabb) const
{
	if (m_cells.empty())
		return 0;

	int cellIndex = 0;
	while (true)
	{
		const COctreeCell & cell = m_cells[cellIndex];
		if (cell.IsLeaf())
			return cellIndex;

		bool foundChild = false;
		for (unsigned iChild = 0 ; iChild < COctreeCell::NUM_CHILDREN ; ++iChild)
		{
			int childIndex = cell.m_childCellIndices[iChild];
			const COctreeCell & childCell = m_cells[childIndex];

			const Vec3 & min = childCell.m_aabb.min;
			const Vec3 & max = childCell.m_aabb.max;

			if (
				min.x < aabb.min.x &&
				min.y < aabb.min.y &&
				min.z < aabb.min.z &&
				max.x > aabb.max.x &&
				max.y > aabb.max.y &&
				max.z > aabb.max.z
				)
			{
				// this child contains the aab - use it
				cellIndex = childIndex;
				foundChild = true;
				break;
			}
		}
		// no child completely contains the aabb so return this one
		if (!foundChild)
			return cellIndex;
	}
	return 0;
}

//====================================================================
// OverlapCapsule
//====================================================================
bool CWorldOctree::OverlapCapsule(const Lineseg& lineseg, float radius)
{
  AABB box(AABB::RESET);
	box.Add(lineseg.start + Vec3(radius, radius, radius));
	box.Add(lineseg.start - Vec3(radius, radius, radius));
	box.Add(lineseg.end + Vec3(radius, radius, radius));
	box.Add(lineseg.end - Vec3(radius, radius, radius));

	m_cellsToTest.resize(0);

	IncrementTestCounter();

	float radiusSq = radius * radius;
	int startingCell = 0;
	m_cellsToTest.push_back(startingCell);
	while (!m_cellsToTest.empty())
	{
		int cellIndex = m_cellsToTest.back();
		m_cellsToTest.pop_back();

		const COctreeCell & cell = m_cells[cellIndex];

		if (!Overlap::AABB_AABB(box, cell.m_aabb))
			continue;

		if (cell.IsLeaf())
		{
			// if leaf test the triangles
			unsigned numTriangles = cell.m_triangleIndices.size();
			for (unsigned i = 0 ; i < numTriangles ; ++i)
			{
				int triangleIndex = cell.m_triangleIndices[i];
				AIAssert(triangleIndex >= 0);
				AIAssert(triangleIndex < m_triangles.size());
				const CTriangle & triangle = m_triangles[triangleIndex];
				// see if we've already done this triangle
				if (triangle.m_counter == m_testCounter)
					continue;
				triangle.m_counter = m_testCounter;

				float distSq = Distance::Lineseg_TriangleSq(lineseg, Triangle(triangle.m_v0, triangle.m_v2, triangle.m_v1), (float*) 0, (float*) 0, (float*) 0);

				if (distSq < radiusSq)
					return true;
			}
		}
		else
		{
			// if non-leaf, just add the children, 
			for (unsigned iChild = 0 ; iChild < COctreeCell::NUM_CHILDREN ; ++iChild)
			{
				int childIndex = cell.m_childCellIndices[iChild];
				m_cellsToTest.push_back(childIndex);
			}
		}
	}
	return false;
}

//====================================================================
// DumpStats
//====================================================================
void CWorldOctree::DumpStats() const
{
	AILogProgress("CWorldOctree::DumpStats:");
	AILogProgress("Num tris = %d, num cells = %d", m_triangles.size(), m_cells.size());

	unsigned maxTris = 0;

	std::vector<int> cellsToProcess;
	cellsToProcess.push_back(0);
	while (!cellsToProcess.empty())
	{
		int cellIndex = cellsToProcess.back();
		cellsToProcess.pop_back();
		const COctreeCell & cell = m_cells[cellIndex];

		if (cell.IsLeaf())
		{
			// check this hasn't got any children
			for (unsigned i = 0 ; i < COctreeCell::NUM_CHILDREN ; ++i)
			{
				if (cell.m_childCellIndices[i] != -1)
					AIWarning("Found unexpected child cell index");
			}
			// record if this cell has the most found
			unsigned numTris = cell.m_triangleIndices.size();
			if (numTris > maxTris)
				maxTris = numTris;
		}
		else
		{
			// non-leaf so check and descend
			if (!cell.m_triangleIndices.empty())
				AIWarning("Found %d triangles in supposedly non-leaf cell", cell.m_triangleIndices.size());
			for (unsigned i = 0 ; i < COctreeCell::NUM_CHILDREN ; ++i)
			{
				if ( (cell.m_childCellIndices[i] >= 0) &&
						 (cell.m_childCellIndices[i] < (int) m_cells.size()) )
					cellsToProcess.push_back(cell.m_childCellIndices[i]);
				else
					AIWarning("Found invalid child cell index %d in non-leaf cell", cell.m_childCellIndices[i]);
			}
		}
	}
	AILogProgress("Max triangles in a cell = %d", maxTris);
}

//====================================================================
// DebugDraw
//====================================================================
void CWorldOctree::DebugDraw(IRenderer *pRenderer) const
{
	unsigned numTriangles = m_triangles.size();
	unsigned numCells = m_cells.size();

	ColorF col(1.0,1.0,0,1.0);
	for (unsigned i = 0 ; i < numTriangles ; ++i)
	{
		const CTriangle & tri = m_triangles[i];
		pRenderer->GetIRenderAuxGeom()->DrawLine(tri.m_v0, col, tri.m_v1, col);
		pRenderer->GetIRenderAuxGeom()->DrawLine(tri.m_v1, col, tri.m_v2, col);
		pRenderer->GetIRenderAuxGeom()->DrawLine(tri.m_v2, col, tri.m_v0, col);

		Vec3 mid = (tri.m_v0 + tri.m_v1 + tri.m_v2) / 3.0f;
		float len = 0.1f;
		Vec3 normal = (tri.m_v1 - tri.m_v0) ^ (tri.m_v2 - tri.m_v0);
		normal.NormalizeSafe();
		pRenderer->GetIRenderAuxGeom()->DrawLine(mid, ColorF(1, 1, 0, 1), mid + len * normal, ColorF(1, 1, 1, 1));
	}
}
