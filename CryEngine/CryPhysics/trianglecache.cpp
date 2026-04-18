//////////////////////////////////////////////////////////////////////
//
//	Triangle Cache
//	
//	File: trianglecache.cpp
//	Description : Helper class to extract and store triangles extracted from a height field
//
//	History:
//	- 2008.07.15 Created by TSTR
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "raybv.h"
#include "raygeom.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "geoman.h"
#include "physicalworld.h"
#include "rigidentity.h"
#include "trianglecache.h"
#include "geometries.h"
#include "unprojectionchecks.h"


// Class constructor
CTriangleCache::CTriangleCache() : m_numTriangles(0)
{ 
	m_box[0]=m_box[1]=Vec3(0);
	memset(&m_contacts,0,sizeof(m_contacts));
}


// Class destructor
CTriangleCache::~CTriangleCache()
{
}


// Logical check against the cached area
// Return:	true if area is fully covered by the cache
//			false if given box is not covered byt the current cache
bool	CTriangleCache::Contains(const Vec3* box)
{
	return AABB_contains(m_box, box) != 0;
}


// Extract and store triangles within the given box from heightfield
int		CTriangleCache::Cache(const Vec3* cachedArea, CHeightfield* pHeightField)
{
	// Store query box, and clear triangle cache
	m_box[0]=cachedArea[0];m_box[1]=cachedArea[1];
	m_numTriangles=0;
	// Pointer t oprimitive
	primitives::heightfield* phf = &pHeightField->m_hf;

	// Prepare primitives for intersection
	primitives::box box;
	box.bOriented = false;
	box.center = (cachedArea[0] + cachedArea[1]) * 0.5f;
	box.size = (cachedArea[1] - cachedArea[0]) * 0.5f;

	primitives::triangle hftri;
	vector2df sz,ptmin,ptmax;
	int ix,iy,ix0,iy0,ix1,iy1;
	float hmax;

	ptmin.x = (m_box[0].x)*phf->stepr.x; ptmin.y = (m_box[0].y)*phf->stepr.y; 
	ptmax.x = (m_box[1].x)*phf->stepr.x; ptmax.y = (m_box[1].y)*phf->stepr.y;
	ix0 = float2int(ptmin.x-0.5f); iy0 = float2int(ptmin.y-0.5f); ix0 &= ~(ix0>>31); iy0 &= ~(iy0>>31);
	ix1 = min(float2int(ptmax.x+0.5f),phf->size.x); iy1 = min(float2int(ptmax.y+0.5f),phf->size.y);

	// check if all heightfield points are below the lowest box point
	for(ix=ix0,hmax=0;ix<=ix1;ix++) for(iy=iy0;iy<=iy1;iy++) 
		hmax = max(hmax,phf->getheight(ix,iy));
	if (hmax<m_box[0].z)
		return 0;

	// check for intersection with each underlying triangle
	for(ix=ix0;ix<ix1;ix++) for(iy=iy0;iy<iy1;iy++) {
		hftri.pt[0].x=hftri.pt[2].x = ix*phf->step.x; hftri.pt[0].y=hftri.pt[1].y = iy*phf->step.y; 
		hftri.pt[1].x = hftri.pt[0].x+phf->step.x; hftri.pt[2].y = hftri.pt[0].y+phf->step.y;
		hftri.pt[0].z = phf->getheight(ix,iy); hftri.pt[1].z = phf->getheight(ix+1,iy); 
		hftri.pt[2].z = phf->getheight(ix,iy+1);
		hftri.n = hftri.pt[1]-hftri.pt[0] ^ hftri.pt[2]-hftri.pt[0];
		if (box_tri_overlap_check(&box,&hftri))
		{
			hftri.n.Normalize();
			Push( &hftri );
		}
		hftri.pt[0] = hftri.pt[2]; hftri.pt[2].x += phf->step.x; hftri.pt[2].z = phf->getheight(ix+1,iy+1);
		hftri.n = hftri.pt[1]-hftri.pt[0] ^ hftri.pt[2]-hftri.pt[0];
		if (box_tri_overlap_check(&box,&hftri))
		{
			hftri.n.Normalize();
			Push( &hftri );
		}
	}
	// Auxilary data from heightfield
	m_minVtxDist = (phf->step.x+phf->step.y)*1E-3f;
	return m_numTriangles;
}


// Cylinder collision Query against the stored triangles
// Unused: pData2 this is actually referencing our internal, already transformed triangle data. Assumed to be IDENTITY!
int		CTriangleCache::Intersect(CCylinderGeom *pCollider, geom_world_data *pdata1,geom_world_data *pdata2, intersection_params *pparams, geom_contact *&pcontacts)
{
	int numContacts = 0;
	pcontacts = &m_contacts[numContacts];
	pparams->pGlobalContacts = pcontacts;

	float sweepstep = pdata1->v.len();
	unprojection_mode unproj;
	unproj.imode = 0;
	unproj.dir = pdata1->v / sweepstep;
	unproj.tmax = sweepstep * pparams->time_interval;
	unproj.minPtDist = min(m_minVtxDist, pCollider->m_minVtxDist);

	Vec3 unprojDirNeg = -unproj.dir;

	Vec3 startoffs = pdata1->offset;
	pdata1->offset += pdata1->v;

	// Create transformed cylinder
	primitives::cylinder cylinder;
	cylinder.center = pdata1->R*pCollider->m_cyl.center*pdata1->scale + pdata1->offset;
	cylinder.axis = pdata1->R*pCollider->m_cyl.axis;
	cylinder.hh = pCollider->m_cyl.hh*pdata1->scale;
	cylinder.r = pCollider->m_cyl.r*pdata1->scale;

	contact contact_cur;
	for(int j=0; j<m_numTriangles; j++)
	{
		if ( tri_cylinder_lin_unprojection(&unproj, &m_cache[j], -1, &cylinder, -1, &contact_cur, NULL) )
		{
			contact_cur.pt -= unproj.dir*contact_cur.t; contact_cur.n.Flip();
			geom_contact *pcontact = &m_contacts[numContacts];
			pcontact->t = sweepstep-contact_cur.t;
			pcontact->pt = pcontact->center = contact_cur.pt;
			pcontact->n = contact_cur.n.normalized();
			pcontact->dir = unprojDirNeg;
			pcontact->id[0] = (char)-1;//gtest.idbuf[i];
			pcontact->id[1] = (char)-1;//gtest[1].idbuf[j];
			++numContacts;
			if ( numContacts >= _TRIANGLE_CACHE_SIZE)
				break;
		}
	}
	pdata1->offset = startoffs;
	return numContacts;
}

// Render physics representation
void CTriangleCache::DrawHelperInformation(IPhysRenderer *pRenderer, int flags)
{
	// Draw cached ttriangles
	for(int j=0; j < m_numTriangles; j++)
	{
		pRenderer->DrawLine( m_cache[j].pt[0], m_cache[j].pt[1] );
		pRenderer->DrawLine( m_cache[j].pt[1], m_cache[j].pt[2] );
		pRenderer->DrawLine( m_cache[j].pt[2], m_cache[j].pt[0] );
	}
	// Draw cached box
	pRenderer->DrawLine( Vec3(m_box[0].x, m_box[0].y, m_box[0].z) ,  Vec3(m_box[0].x, m_box[0].y, m_box[1].z));
	pRenderer->DrawLine( Vec3(m_box[0].x, m_box[0].y, m_box[0].z) ,  Vec3(m_box[0].x, m_box[1].y, m_box[0].z));
	pRenderer->DrawLine( Vec3(m_box[0].x, m_box[0].y, m_box[0].z) ,  Vec3(m_box[1].x, m_box[0].y, m_box[0].z));

	pRenderer->DrawLine( Vec3(m_box[0].x, m_box[1].y, m_box[1].z) ,  Vec3(m_box[1].x, m_box[1].y, m_box[1].z));
	pRenderer->DrawLine( Vec3(m_box[1].x, m_box[0].y, m_box[1].z) ,  Vec3(m_box[1].x, m_box[1].y, m_box[1].z));
	pRenderer->DrawLine( Vec3(m_box[1].x, m_box[1].y, m_box[0].z) ,  Vec3(m_box[1].x, m_box[1].y, m_box[1].z));

	pRenderer->DrawLine( Vec3(m_box[0].x, m_box[1].y, m_box[1].z) ,  Vec3(m_box[0].x, m_box[0].y, m_box[1].z));
	pRenderer->DrawLine( Vec3(m_box[1].x, m_box[1].y, m_box[0].z) ,  Vec3(m_box[0].x, m_box[1].y, m_box[0].z));
	pRenderer->DrawLine( Vec3(m_box[1].x, m_box[0].y, m_box[1].z) ,  Vec3(m_box[1].x, m_box[0].y, m_box[0].z));

	pRenderer->DrawLine( Vec3(m_box[0].x, m_box[1].y, m_box[0].z) ,  Vec3(m_box[0].x, m_box[1].y, m_box[1].z));
	pRenderer->DrawLine( Vec3(m_box[0].x, m_box[0].y, m_box[1].z) ,  Vec3(m_box[1].x, m_box[0].y, m_box[1].z));
	pRenderer->DrawLine( Vec3(m_box[1].x, m_box[0].y, m_box[0].z) ,  Vec3(m_box[1].x, m_box[1].y, m_box[0].z));
}



