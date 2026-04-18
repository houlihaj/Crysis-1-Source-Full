/*=============================================================================
	GeomCore.h :
	Copyright 2004 Crytek Studios. All Rights Reserved.

	Revision history:
	* Created by Nick Kasyan
=============================================================================*/

#include "stdafx.h"

#include <limits>
#include "GeomCore.h"

//epsilon&inf constants definitions
const f32 CGeomCore::m_fEps = std::numeric_limits<f32>::epsilon();
const f32 CGeomCore::m_fInf = std::numeric_limits<f32>::max();

const f64 CGeomCore::m_dEps = std::numeric_limits<f64>::epsilon();
const f64 CGeomCore::m_dInf = std::numeric_limits<f64>::max();


//fix
const int32 CGeomCore::m_nMaxInBox = 32;
const int32 CGeomCore::m_MaxTreeNodes = 64;

CRLWaitProgress* CGeomCore::m_pWaitProgress = NULL;


//===================================================================================
// Method					:	CGeomCore
// Description		:	Constructor
CGeomCore::CGeomCore() :	m_ppStackCell(NULL), m_bOctreeBuilt (false), m_nMaxCells(0),
													m_nMaxTrisTested(0), m_nMaxTrisHit(0), m_nMaxCellsTested(0),
													m_pOctree(NULL),m_ppStackItemCell(NULL), m_nMaxItemCells(0), m_nNumItemCells(0),m_nSessionID(0)

{
	m_WorldPrims.clear();
	m_BBox.Reset();
}

//===================================================================================
// Method					:	CGeomCore
// Description		:	Destructor
CGeomCore::~CGeomCore()
{
	SAFE_DELETE( m_pOctree );
	SAFE_DELETE_ARRAY( m_ppStackCell );
}

typedef bool (* PFNITEMBBOX)(int32 , void* ,
														 OctBoundingBox* );
//===================================================================================
// Global func for general Octree - create a BBox for an item
bool PrimBox (int32 itemIndex, void* itemList, OctBoundingBox* bbox )
{
	assert(itemIndex >= 0); 
	assert(itemList != NULL);
	assert(bbox != NULL);

	CGeomCore::t_vectTracable* pArrPrim = (CGeomCore::t_vectTracable*)itemList;
	CRLTriangleSmall  *pPrim = &(pArrPrim->at(itemIndex));

	//If the triangle hase nonopaque material - don't allowed to put into octree
//	if( false == pPrim->IsOpaque() )
//		return false;

	Vec3 vPos[3];
	pPrim->GetPositions( vPos );

	//vert. 1
	bbox->max[0] = bbox->min[0] = vPos[0].x;
	bbox->max[1] = bbox->min[1] = vPos[0].y;
	bbox->max[2] = bbox->min[2] = vPos[0].z;

	//vert. 2
	bbox->min[0] = ( bbox->min[0] < vPos[1].x ) ? bbox->min[0] : vPos[1].x;
	bbox->min[1] = ( bbox->min[1] < vPos[1].y ) ? bbox->min[1] : vPos[1].y;
	bbox->min[2] = ( bbox->min[2] < vPos[1].z ) ? bbox->min[2] : vPos[1].z;

	bbox->max[0] = ( bbox->max[0] > vPos[1].x )  ? bbox->max[0] : vPos[1].x;
	bbox->max[1] = ( bbox->max[1] > vPos[1].y )  ? bbox->max[1] : vPos[1].y;
	bbox->max[2] = ( bbox->max[2] > vPos[1].z )  ? bbox->max[2] : vPos[1].z;

	//vert. 3
	bbox->min[0] = ( bbox->min[0] < vPos[2].x ) ? bbox->min[0] : vPos[2].x;
	bbox->min[1] = ( bbox->min[1] < vPos[2].y ) ? bbox->min[1] : vPos[2].y;
	bbox->min[2] = ( bbox->min[2] < vPos[2].z ) ? bbox->min[2] : vPos[2].z;

	bbox->max[0] = ( bbox->max[0] > vPos[2].x )  ? bbox->max[0] : vPos[2].x;
	bbox->max[1] = ( bbox->max[1] > vPos[2].y )  ? bbox->max[1] : vPos[2].y;
	bbox->max[2] = ( bbox->max[2] > vPos[2].z )  ? bbox->max[2] : vPos[2].z;

	return true;
}

//===================================================================================
// Global func for general Octree
bool PrimInBox (int32 itemIndex, void* itemList, OctBoundingBox* bbox)
{
	assert(itemIndex >= 0); 
	assert(itemList != NULL);
	assert(bbox != NULL);

	CGeomCore::t_vectTracable* pArrPrim = (CGeomCore::t_vectTracable*)itemList;
	CRLTriangleSmall  *pPrim = &(pArrPrim->at(itemIndex));

	//If the triangle hase nonopaque material - don't allowed to put into octree
//	if( false == pPrim->IsOpaque() )
//													return false;

	//erly check - much faster than TriBoxIntersect (if the BBox of the triangle is in the big bbox or completly outside
	//we can go on our road, just if there are an intersection need a precise test... )

	Vec3 vPos[3];
	pPrim->GetPositions( vPos );

	Vec3 vTriBBoxMin( vPos[0] );
	Vec3 vTriBBoxMax( vPos[0] );

	//roll out the code...
	vTriBBoxMin.x = ( vTriBBoxMin.x < vPos[1].x ) ? vTriBBoxMin.x : vPos[1].x;
	vTriBBoxMin.y = ( vTriBBoxMin.y < vPos[1].y ) ? vTriBBoxMin.y : vPos[1].y;
	vTriBBoxMin.z = ( vTriBBoxMin.z < vPos[1].z ) ? vTriBBoxMin.z : vPos[1].z;

	vTriBBoxMin.x = ( vTriBBoxMin.x < vPos[2].x ) ? vTriBBoxMin.x : vPos[2].x;
	vTriBBoxMin.y = ( vTriBBoxMin.y < vPos[2].y ) ? vTriBBoxMin.y : vPos[2].y;
	vTriBBoxMin.z = ( vTriBBoxMin.z < vPos[2].z ) ? vTriBBoxMin.z : vPos[2].z;

	vTriBBoxMax.x = ( vTriBBoxMax.x > vPos[1].x ) ? vTriBBoxMax.x : vPos[1].x;
	vTriBBoxMax.y = ( vTriBBoxMax.y > vPos[1].y ) ? vTriBBoxMax.y : vPos[1].y;
	vTriBBoxMax.z = ( vTriBBoxMax.z > vPos[1].z ) ? vTriBBoxMax.z : vPos[1].z;

	vTriBBoxMax.x = ( vTriBBoxMax.x > vPos[2].x ) ? vTriBBoxMax.x : vPos[2].x;
	vTriBBoxMax.y = ( vTriBBoxMax.y > vPos[2].y ) ? vTriBBoxMax.y : vPos[2].y;
	vTriBBoxMax.z = ( vTriBBoxMax.z > vPos[2].z ) ? vTriBBoxMax.z : vPos[2].z;

	//completly outside...
	if( ( bbox->minX > vTriBBoxMax.x && bbox->minY > vTriBBoxMax.y && bbox->minZ > vTriBBoxMax.z ) ||
		  ( bbox->maxX < vTriBBoxMin.x && bbox->maxY < vTriBBoxMin.y && bbox->maxZ < vTriBBoxMin.z ) )
																																													return false;

	//completly inside...
	if( ( bbox->minX <= vTriBBoxMin.x && bbox->minY <= vTriBBoxMin.y && bbox->minZ <= vTriBBoxMax.z ) &&
		  ( bbox->maxX >= vTriBBoxMax.x && bbox->maxY >= vTriBBoxMax.y && bbox->maxZ >= vTriBBoxMax.z ) )
																																															return true;

	//intersection... 
	Vec3 vHalf( ( bbox->maxX - bbox->minX ) * 0.5f, 
							( bbox->maxY - bbox->minY ) * 0.5f,
							( bbox->maxZ - bbox->minZ ) * 0.5f );
	Vec3 vCenter( vHalf.x + bbox->minX, vHalf.y + bbox->minY, vHalf.z + bbox->minZ);
	return 
		pPrim->TriBoxIntersect( vCenter, vHalf, vPos );
}

void GeomCoreProgress(f32 progress)
{
	if( CGeomCore::m_pWaitProgress )
		CGeomCore::m_pWaitProgress->SetProgress( progress );
}

bool CGeomCore::BuildAccel(CRLWaitProgress* pWaitProgress)
{
//	pWaitProgress->Caption("Building Octree ...");

	// Figure out the bounding box for the high res model
	OctBoundingBox bbox;
	bbox.minX = m_BBox.min.x;
	bbox.minY = m_BBox.min.y;
	bbox.minZ = m_BBox.min.z;
	bbox.maxX = m_BBox.max.x;
	bbox.maxY = m_BBox.max.y;
	bbox.maxZ = m_BBox.max.z;
	// Create and fill the octree.
	m_pOctree = new CCryOctree;
	if (m_pOctree == NULL)
	{
		return false;
	}

	m_pWaitProgress = pWaitProgress;

	//PFNITEMINBOX ae = PrimInBox;
	if (m_pOctree->FillTree (m_WorldPrims.size(), (void*)(&m_WorldPrims), bbox, PrimInBox,
		m_nMaxInBox, PrimBox, GeomCoreProgress) == false)
	{
		return false;
	}

	m_bOctreeBuilt = true;

	return true;
}

//===================================================================================
// Method					:	Add
// Description		:	Add ITracable primitive to the GeomCore
//uint32 CGeomCore::Add(ITracable* pPrim)
uint32 CGeomCore::Add(CRLTriangleSmall* pPrim)
{
	if (m_WorldPrims.size() == m_WorldPrims.capacity() )
	{
		CLogFile::FormatLine("Add error: %d size, %d capacity", m_WorldPrims.size(), m_WorldPrims.capacity() );
	}

  m_WorldPrims.push_back(*pPrim);

	//extend scene bound box
	pPrim->Bound(m_BBox);

	return
		(m_WorldPrims.size() - 1) ;
}

/*bool CGeomCore::FillCore(std::vector<std::pair<IRenderNode*, CBrushObject*> >& vNodes)
{
	for (t_vectNode::iterator itNodes = vNodes.begin(); itNodes!=vNodes.end(); itNodes++)
	{
		IRenderNode* pRendNode = itNodes->first;
		CRLMesh* = new CRLMesh(pRendNode, this);
    
		vec		
	}

	return true;
}*/

//===================================================================================
// Method					:	PrintOctreeStats
// Description		:	statistics of Octree's using
CString CGeomCore::GetAccelStats()
{
	// Bail if we don't have an octree
	if (m_pOctree == NULL)
	{
		return NULL;
	}

	// Initialize counter.
	int32 nTotalCells = 0;
	int32 nLeafCells = 0;
	int32 nNonLeafCells = 0;

	// Walk the tree.
	int numCells = 0;
	AddCell (m_pOctree->m_root, &numCells);
	while (numCells > 0)
	{
		// Take the cell from the list.
		numCells--;
		COctreeCell* currCell = m_ppStackCell[numCells];

		// See if this is a leaf node
		bool leaf = true;
		for (int c = 0; c < 8; c++)
		{
			if (currCell->m_children[c] != NULL)
			{
				leaf = false;
				break;
			}
		}

		// If we are a leaf check the triangles
		if (leaf)
		{
//FIX:: rewrite log messages 
#ifdef DEBUG_OCTREE
			{
				OctBoundingBox* bbox = &(currCell->m_boundingBox);

				CLogFile::WriteLine("-------------- leaf --------------\n");
				CLogFile::FormatLine("%d triangles\n", currCell->m_numItems);
				CLogFile::FormatLine("min:    %7.3f %7.3f %7.3f\n", bbox->minX, bbox->minY, bbox->minZ);
				CLogFile::FormatLine("max:    %7.3f %7.3f %7.3f\n", bbox->maxX, bbox->maxY, bbox->maxZ);
				CLogFile::FormatLine("center: %7.3f %7.3f %7.3f\n", bbox->centerX, bbox->centerY, bbox->centerZ);
				CLogFile::FormatLine("half:   %7.3f %7.3f %7.3f\n", bbox->halfX, bbox->halfY, bbox->halfZ);
			}
#endif
			nLeafCells++;
			nTotalCells++;
		} // end if leaf
		else
		{ 
			// Non-leaf
#ifdef DEBUG_OCTREE
			{
				OctBoundingBox* bbox = &(currCell->m_boundingBox);
	
				CLogFile::WriteLine("************ non-leaf ************\n");
				CLogFile::FormatLine("%d triangles\n", currCell->m_numItems);
				CLogFile::FormatLine("min:    %7.3f %7.3f %7.3f\n", bbox->minX, bbox->minY, bbox->minZ);
				CLogFile::FormatLine("max:    %7.3f %7.3f %7.3f\n", bbox->maxX, bbox->maxY, bbox->maxZ);
				CLogFile::FormatLine("center: %7.3f %7.3f %7.3f\n", bbox->centerX, bbox->centerY, bbox->centerZ);
				CLogFile::FormatLine("half:   %7.3f %7.3f %7.3f\n", bbox->halfX, bbox->halfY, bbox->halfZ);
			}
#endif

			// Handle children
			nTotalCells++;
			nNonLeafCells++;
			int childCount = 0;
			for (int c = 0; c < 8; c++)
			{
				if (currCell->m_children[c] != NULL)
				{
					childCount++;
					COctreeCell* child = currCell->m_children[c];
					AddCell (child, &numCells);
				} // end if we have a cell
			} // end for c (8 children)
#ifdef DEBUG_OCTREE
			CLogFile::FormatLine("%d children\n", childCount);
#endif
		} // end else non-leaf node.
	} // end while we still have cells

	// Print stats.
	CLogFile::FormatLine("%d Octree cells (%d leaf %d non-leaf)\n", nTotalCells, nLeafCells,
		nNonLeafCells);

	//fix:: return stat string also
	return NULL;
}

//===================================================================================
// Method					:	Intersect
// Description		:	Ray's intersection test
// Fix::leave tangens space in order to do tracing with normals & rewrite desc
bool CGeomCore::Intersect(CRay* pRay)
{

	bool bFound = false;

	// Some stats.
	int nCellCountStat = 0;
	int nHitTriCountStat = 0;
	int nTriCountStat = 0;

  //fix
	//double* displacement;

	// Walk the octree looking for intersections.
	int numCells = 0;
	AddCell (m_pOctree->m_root, &numCells);
	while (numCells > 0)
	{
		// Take the cell from the list.
		nCellCountStat++;
		numCells--;
		COctreeCell* currCell = m_ppStackCell[numCells];

		// See if this is a leaf node
		bool leaf = true;
		for (int c = 0; c < 8; c++)
		{
			if (currCell->m_children[c] != NULL)
			{
				leaf = false;
				break;
			}
		}

		// If we are a leaf check the triangles
		if (leaf)
		{
			// Run through the triangles seeing if the ray intersects.
			for (int t = 0; t < currCell->m_numItems; t++)
			{
				// Save off current triangle.
				CRLTriangleSmall*  pPrim = (CRLTriangleSmall*)&( m_WorldPrims.at(currCell->m_item[t]) );
				// See if it intersects.
				nTriCountStat++;

				if ( pPrim->IntersectFrontClosest(pRay) ) //fix: check here for different sort of intersections
				{
					bFound = true;
					// Stats.
					nHitTriCountStat++;

	/*#ifdef DEBUG_INTERSECTION
					if (gDbgIntersection)
					{
						CLogFile::FormatLine("      High Triangle %d:\n", currCell->m_item[t]);
						CLogFile::FormatLine("         bcc:  %8.4f %8.4f %8.4f   dist: %8.4f\n",
							b0, intersect.y, intersect.z, intersect.x);
						CLogFile::FormatLine("         nPos: %8.4f %8.4f %8.4f\n", np[0], np[1], np[2]);
						CLogFile::FormatLine("         nNrm: %8.4f %8.4f %8.4f\n", nn[0], nn[1], nn[2]);
					}
#endif
					// See if this should be the normal for the map.
					// Perturb by bump map //fix make research about quality increasing by normal maps
						if (bumpMap != NULL)
						{
							GetPerturbedNormal (hTri, b0, intersect.y,
								intersect.z, bumpMap,
								bumpWidth, bumpHeight,
								hTangentSpace[currCell->m_item[t]].m,
								nn);
						}
*/
						// Copy over values

/*#ifdef DEBUG_INTERSECTION
					else if (gDbgIntersection)
					{
						CLogFile::WriteString("      <<< Intersection is worse!\n");
					}
#endif*/
				} // end if we have an intersection
			} // end for t (num triangles in this cell)
		} // end if leaf
		else
		{  // Non-leaf, add the children to the list if their bounding
			// box intersects the ray.
			for (int c = 0; c < 8; c++)
			{
				if (currCell->m_children[c] != NULL)
				{
					// Save off current child.
					COctreeCell* child = currCell->m_children[c];

					// If the ray intersects the box
					if (child->Intersect(pRay))
					{
						AddCell(child, &numCells);
					} // end if the ray intersects this bounding box.
					//fix: do we need back ray tracing ?
					/*else if (RayIntersectsBox (&pos, &negNorm, &child->m_boundingBox))
					{
						AddCell (child, &numCells);
					} // end if the ray intersects this bounding box.*/
					// else do nothing, we'll never intersect any triangles
					// for it's children.
				} // end if we have a cell
			} // end for c (8 children)
		} // end else non-leaf node.
	} // end while cells

	// Accamulate statistics
	if (nTriCountStat > m_nMaxTrisTested)
	{
		m_nMaxTrisTested = nTriCountStat;
	}
	if (nHitTriCountStat > m_nMaxTrisHit)
	{
		m_nMaxTrisHit = nHitTriCountStat;
	}
	if (nCellCountStat > m_nMaxCellsTested)
	{
		m_nMaxCellsTested = nCellCountStat;
	}

	// Test if we found an intersection.
	if ( bFound )
	{
		//(*displacement) = pRay.fT;
		return true;
	}

	return false;
}


//===================================================================================
// Method					:	IntersectAll
// Description		:	Ray's intersection test - All face
bool CGeomCore::IntersectAll(CRay* pRay)
{
	bool bFound = false;
	// Some stats.
	int nCellCountStat = 0;
	int nHitTriCountStat = 0;
	int nTriCountStat = 0;

	// Walk the octree looking for intersections.
	int numCells = 0;
	AddCell (m_pOctree->m_root, &numCells);
	while (numCells > 0)
	{
		// Take the cell from the list.
		nCellCountStat++;
		numCells--;
		COctreeCell* currCell = m_ppStackCell[numCells];

		// See if this is a leaf node
		bool leaf = true;
		for (int c = 0; c < 8; c++)
		{
			if (currCell->m_children[c] != NULL)
			{
				leaf = false;
				break;
			}
		}

		// If we are a leaf check the triangles
		if (leaf)
		{
			// Run through the triangles seeing if the ray intersects.
			for (int t = 0; t < currCell->m_numItems; t++)
			{
				// Save off current triangle.
				CRLTriangleSmall*  pPrim = (CRLTriangleSmall*)&( m_WorldPrims.at(currCell->m_item[t]) );
				// See if it intersects.
				nTriCountStat++;

				if ( pPrim->IntersectClosest(pRay) ) //fix: check here for different sort of intersections
				{
					bFound = true;
				} // end if we have an intersection
			} // end for t (num triangles in this cell)
		} // end if leaf
		else
		{  // Non-leaf, add the children to the list if their bounding
			// box intersects the ray.
			for (int c = 0; c < 8; c++)
			{
				if (currCell->m_children[c] != NULL)
				{
					// Save off current child.
					COctreeCell* child = currCell->m_children[c];

					// If the ray intersects the box
					if (child->Intersect(pRay))
					{
						AddCell(child, &numCells);
					} // end if the ray intersects this bounding box.
					//fix: do we need back ray tracing ?
					/*else if (RayIntersectsBox (&pos, &negNorm, &child->m_boundingBox))
					{
					AddCell (child, &numCells);
					} // end if the ray intersects this bounding box.*/
					// else do nothing, we'll never intersect any triangles
					// for it's children.
				} // end if we have a cell
			} // end for c (8 children)
		} // end else non-leaf node.
	} // end while cells

	// Accamulate statistics
	if (nTriCountStat > m_nMaxTrisTested)
	{
		m_nMaxTrisTested = nTriCountStat;
	}
	if (nHitTriCountStat > m_nMaxTrisHit)
	{
		m_nMaxTrisHit = nHitTriCountStat;
	}
	if (nCellCountStat > m_nMaxCellsTested)
	{
		m_nMaxCellsTested = nCellCountStat;
	}

	// Test if we found an intersection.
	if ( bFound )
	{
		//(*displacement) = pRay.fT;
		return true;
	}

	return false;
}

//===================================================================================
// Method					:	ReserveTracable
// Description		:	Reserve space for tracable objects
void CGeomCore::ReserveTracable( int32 nNumberOfTracables )
{
	m_WorldPrims.reserve( nNumberOfTracables );
}


#define OCTREE_FILE_VERSION	1
bool	CGeomCore::Save( const char* szFileName )
{
	assert( NULL != szFileName );
	FILE *f = fopen( szFileName, "wb" );
	if( NULL == f )
		return false;

	uint32 nData = OCTREE_FILE_VERSION;
	fwrite( &nData, sizeof(uint32),1, f );

	uint32 nTriangleNumber = m_WorldPrims.size();
	fwrite( &nTriangleNumber, sizeof(uint32),1, f );

//	fwrite( &(m_WorldPrims[0]), sizeof(CRLTriangleSmall)*nTriangleNumber, 1, f );
	float fTriangleData[ 6*3 ];
	Vec3 vPos[3];
	Vec3 vNormals[3];
	for( uint32 i = 0; i < nTriangleNumber; ++i )
	{
		m_WorldPrims[i].GetPositions( vPos );
		m_WorldPrims[i].GetNormals( vNormals );

		fTriangleData[0] = vPos[0].x;
		fTriangleData[1] = vPos[0].y;
		fTriangleData[2] = vPos[0].z;
		fTriangleData[3] = vPos[1].x;
		fTriangleData[4] = vPos[1].y;
		fTriangleData[5] = vPos[1].z;
		fTriangleData[6] = vPos[2].x;
		fTriangleData[7] = vPos[2].y;
		fTriangleData[8] = vPos[2].z;

		fTriangleData[9]  = vNormals[0].x;
		fTriangleData[10] = vNormals[0].y;
		fTriangleData[11] = vNormals[0].z;
		fTriangleData[12] = vNormals[1].x;
		fTriangleData[13] = vNormals[1].y;
		fTriangleData[14] = vNormals[1].z;
		fTriangleData[15] = vNormals[2].x;
		fTriangleData[16] = vNormals[2].y;
		fTriangleData[17] = vNormals[2].z;

		fwrite( fTriangleData, sizeof(float)*18, 1, f );
	}

	if (m_pOctree != NULL)
		m_pOctree->Save( f );
	else
	{
		//generate a "null" octree
		nTriangleNumber = 0;
		fwrite( &nTriangleNumber, sizeof(int32),1, f );
		fwrite( &nTriangleNumber, sizeof(int32),1, f );
	}

	fclose(f);
	return true;
}

bool	CGeomCore::Load( const char* szFileName )
{
	///BROKEN PATH
	return false;
	assert( NULL != szFileName );
	FILE *f = fopen( szFileName, "rb" );
	if( NULL == f )
		return false;

	int32 nTriangleNumber;
	fread( &nTriangleNumber, sizeof(int32),1, f );

	m_WorldPrims.resize( nTriangleNumber );

	fread( &(m_WorldPrims[0]), sizeof(CRLTriangleSmall)*nTriangleNumber, 1, f );

	m_pOctree = new CCryOctree;
	if (m_pOctree == NULL)
		return false;

	m_pOctree->Load( f );

	fclose(f);
	return true;
}

//===================================================================================
// Method				:	IntersectBox
// Description	:	Return true if a given non axis aligned box whose
//							corners are given in the boundingVertices array
//							intersects the given axis aligned box.
// Fix:: Implement for fast culing
//HierarchyIntersectBox(const float *bmin,const float *bmax,const float *normal) ;
//HierarchyIntersectBox(const float *bmin,const float *bmax,const float *obmin,const float *obmax,const float *boundingPlanes) ;
//HierarchyIntersectBox(const float *bmin,const float *bmax,const float *F,const float *T,float &tmin,float &tmax) ;
