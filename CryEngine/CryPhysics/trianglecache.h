//////////////////////////////////////////////////////////////////////
//
//	Triangle Cache header
//	
//	File: trianglecache.h
//	Description : Helper class to extract and store triangles extracted from a height field
//
//	History:
//	- 2008.07.15 Created by TSTR
//
//////////////////////////////////////////////////////////////////////

#ifndef triangleCache_h
#define triangleCache_h
#pragma once

// Forwards
class CHeightfield;
class CCylinderGeom;

// Considering the size of a wheel and the size of a heightfield cell this should be more than enough
#define _TRIANGLE_CACHE_SIZE 8


// Structure to store the cached triangles around an area  (I plan to reuse this elsewhere)
class CTriangleCache
{
public:
	CTriangleCache();
	~CTriangleCache();
	bool					Contains(const Vec3* box);
	int						Cache(const Vec3* box, CHeightfield* hField);
	int						Intersect(CCylinderGeom *pCollider, geom_world_data *pdata1,geom_world_data *pdata2, intersection_params *pparams, geom_contact *&pcontacts);
	void					DrawHelperInformation(IPhysRenderer *pRenderer, int flags);

protected:
	inline int						Push(primitives::triangle* triangle);
	Vec3									m_box[2];
	int										m_numTriangles;
	primitives::triangle	m_cache[_TRIANGLE_CACHE_SIZE];
	geom_contact					m_contacts[_TRIANGLE_CACHE_SIZE];
	float									m_minVtxDist;
};

// Inlines
// Internal function to store one triangle in cache
inline int		CTriangleCache::Push(primitives::triangle* triangle)
{
	if ( m_numTriangles < _TRIANGLE_CACHE_SIZE)
	{
		m_cache[m_numTriangles] = *triangle; ++m_numTriangles;
	} else assert(false);
	return m_numTriangles;
}

#endif		// triangleCache_h

