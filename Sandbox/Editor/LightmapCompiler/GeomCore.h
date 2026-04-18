/*=============================================================================
	GeomCore.h :
	Copyright 2004 Crytek Studios. All Rights Reserved.

	Revision history:
	* Created by Nick Kasyan
=============================================================================*/
#ifndef __GEOMCORE_H__
#define __GEOMCORE_H__

#if _MSC_VER > 1000
# pragma once
#endif

//dep
#include <cry_math.h>
#include <cry_geo.h>
#include "GeomPrimitives.h"
#include "CryOctree.h"
#include "RLWaitProgress.h"
#include "IntersectionCacheCluster.h"


//===================================================================================
// Class				:	CGeomCore
class	CGeomCore 
{
public:
	typedef	std::vector<SMeshInformations> t_vectMeshInfo;
	typedef	std::vector<CRLTriangleSmall> t_vectTracable;			//TODO: need an abstract class ?

public:
	CGeomCore();
	virtual ~CGeomCore();

	bool	Save( const char* szFileName );
	bool	Load( const char* szFileName );

	void		ReserveTracable( int32 nNumberOfTracables );
	int32		AddMeshInfos( 	IRenderMesh* pRenderMesh, Matrix34* pMatrix, 	struct IRenderShaderResources* pMaterial  )
	{
		assert( pMatrix );
		SMeshInformations MeshInfo;

		MeshInfo.m_pRenderMesh = pRenderMesh;
		MeshInfo.m_mMatrix = *pMatrix;
		MeshInfo.m_pMaterial  = pMaterial;

		//mostly used datas
		MeshInfo.iVtxStride = 0;
		MeshInfo.pVertices = reinterpret_cast<Vec3 *> (pRenderMesh->GetStridedPosPtr(MeshInfo.iVtxStride));

		m_MeshInfos.push_back( MeshInfo );

		return m_MeshInfos.size() - 1;
	}

	inline const SMeshInformations *GetMeshInfo( int32 nId ) const
	{
		return &(m_MeshInfos[nId]);
	}

	uint32		Add(CRLTriangleSmall *);
	CRLTriangleSmall* GetPrimitive(uint32 nInd)
	{
		return &(m_WorldPrims[nInd]);
	}

	bool		Intersect(CRay *pRay);
	bool		IntersectAll(CRay *pRay);

	//===================================================================================
	// Method					:	SearchRoot
	// Description		:	Search the deepest cell which completly include the sphere
	inline void SearchRoot( COctreeCell** pRoot, Sphere& Sph )
	{
		assert(pRoot);
		if( NULL == pRoot )
			return;

		if( NULL == *pRoot )
			*pRoot = m_pOctree->m_root;

		//search is there a children which absoulte cover the sphere
		COctreeCell** pChildrens = (*pRoot)->m_children;
		int32 nChildrenNum = (*pRoot)->m_ChildrenNumber;
		for( int32 i = 0; i < nChildrenNum; ++i )
		{
			AABB bbox( Vec3(pChildrens[i]->m_boundingBox.min[0],pChildrens[i]->m_boundingBox.min[1],pChildrens[i]->m_boundingBox.min[2]), Vec3(pChildrens[i]->m_boundingBox.max[0],pChildrens[i]->m_boundingBox.max[1],pChildrens[i]->m_boundingBox.max[2]) );
			if( 2 == Overlap::Sphere_AABB_Inside( Sph, bbox ) )
			{
				*pRoot = pChildrens[i];
				SearchRoot( pRoot, Sph );
				return;
			}
		}
	}

	//===================================================================================
	// Method					:	FirstIntersect
	// Description		:	Ray's intersection test
	inline bool FirstIntersect(CRay* pRay, CIntersectionCacheCluster* pCacheCluster, COctreeCell* pRootCell = NULL)
	{
		++m_nSessionID;
		pRay->CalcInvDir();
		float fOriginalDistance = pRay->fT;
		pRay->fTmin = 0;
		//last node check... -> cacheing
		int32 nCacheIndex = pCacheCluster->GetIndex(pRay->vDir);
		COctreeCell* pLastCell = pCacheCluster->GetCell(nCacheIndex);

		//automatic max distance setup :)
		if( NULL != pLastCell )
			IntersectWithACell_Fast( pLastCell, pRay );

		// Walk the octree looking for intersections.
		int numCells = 0;
		if( pRootCell )
			AddCell (pRootCell, &numCells);
		else
			AddCell (m_pOctree->m_root, &numCells);
		while (numCells > 0)
		{
			// Take the cell from the list.
			numCells--;
			COctreeCell* currCell = m_ppStackCell[numCells];
			//the m_numItems exacly show it is a leaf or not.
			if( currCell->m_numItems )
			{
				//needed because the ray max distance is dynamic...
				if ( currCell->Intersect(pRay) )
				{
					f32 fT = pRay->fT;
					IntersectWithACell_Fast( currCell, pRay );

					if( pRay->fT < fT )
						pCacheCluster->SetCell(nCacheIndex, currCell);
				}
			} // end if leaf
			else
			{  // Non-leaf, add the children to the list if their bounding
				// box intersects the ray.
				//roll out the code

				COctreeCell** pChildrens = currCell->m_children;

				switch( currCell->m_ChildrenNumber )
				{
				case 8:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 7:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 6:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 5:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 4:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 3:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 2:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 1:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				}//switch
			} // end else non-leaf node.
		} // end while cells

		if( pRay->fT < fOriginalDistance )
			return true;

		pCacheCluster->SetCell(nCacheIndex,NULL);
		return false;
	}

	//===================================================================================
	// Method					:	ShadowIntersect
	// Description		:	Ray's shadow intersection test
	inline bool CGeomCore::ShadowIntersect(CRay* pRay, CIntersectionCacheCluster* pCacheCluster)
	{
		++m_nSessionID;
		pRay->CalcInvDir();
		pRay->fTmin = 0;
		//last node check... -> cacheing
		int32 nCacheIndex = pCacheCluster->GetIndex(pRay->vDir);
		COctreeCell* pLastCell = pCacheCluster->GetCell(nCacheIndex);
		if( NULL != pLastCell && IntersectWithACell_Shadow( pLastCell, pRay) )
			return true;

		//fix
		//double* displacement;

		// Walk the octree looking for intersections.
		int numCells = 0;
		AddCell (m_pOctree->m_root, &numCells);
		while (numCells > 0)
		{
			// Take the cell from the list.
			numCells--;
			COctreeCell* currCell = m_ppStackCell[numCells];

			//the m_numItems exacly show it is a leaf or not.
			if( currCell->m_numItems )
			{
				if( IntersectWithACell_Shadow( currCell, pRay ) )
				{
					pCacheCluster->SetCell(nCacheIndex,currCell);
					return true;
				}
			} // end if leaf
			else
			{  // Non-leaf, add the children to the list if their bounding
				// box intersects the ray.
				//roll out the code

				COctreeCell** pChildrens = currCell->m_children;

				switch( currCell->m_ChildrenNumber )
				{
				case 8:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 7:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 6:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 5:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 4:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 3:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 2:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				case 1:
					if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
						AddCell(*pChildrens, &numCells);
					pChildrens++;
				}//switch
			} // end else non-leaf node.
		} // end while cells

		pCacheCluster->SetCell(nCacheIndex,NULL);
		return false;
	}


	//===================================================================================
	// Method					:	FirstIntersect_Improved
	// Description		:	Ray's intersection test
	inline bool FirstIntersect_Improved(CRay* pRay, CIntersectionCacheCluster* pCacheCluster)
	{
		++m_nSessionID;
		pRay->CalcInvDir();
		float fOriginalDistance = pRay->fT;
		pRay->fTmin = 0;
		//last node check... -> cacheing
//		int32 nCacheIndex = pCacheCluster->GetIndex(pRay->vDir);
//		COctreeCell* pLastCell = pCacheCluster->GetCell(nCacheIndex);

		//automatic max distance setup :)
//		if( NULL != pLastCell )
//			IntersectWithACell_Fast( pLastCell, pRay );


		//first setup the list of cells with items
		m_nNumItemCells = 0;
		int numCells = 0;
		AddCell_Improved(m_pOctree->m_root, &numCells);
		while (numCells > 0)
		{
			// Take the cell from the list.
			numCells--;
			COctreeCell* currCell = m_ppStackCell[numCells];
			//roll out the code
			COctreeCell** pChildrens = currCell->m_children;

			switch( currCell->m_ChildrenNumber )
			{
			case 8:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 7:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 6:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 5:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 4:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 3:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 2:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 1:
				assert(pChildrens);
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
//				pChildrens++;
			}//switch
		}//while


		//because of the qsort we will have the right from-to list - don't need to go over others..
		//qsort( m_ppStackItemCell, sizeof(COctreeCell*), m_nNumItemCells, );


//		pCacheCluster->SetCell(nCacheIndex,NULL);

		// Walk the octree looking for intersections.
		for( int i = 0; i < m_nNumItemCells; ++i )
		{
			COctreeCell* currCell = m_ppStackItemCell[i];
			if(currCell->Intersect(pRay))
			{
//				f32 OldT	=	pRay->fT;
				IntersectWithACell_Fast( currCell, pRay );
//				if(OldT>pRay->fT)
				{
//					pCacheCluster->SetCell(nCacheIndex,currCell);
//					return true;
				}
			}
		}

		return pRay->fT < fOriginalDistance;
	}

	//===================================================================================
	// Method					:	ShadowIntersect_Improved
	// Description		:	Ray's shadow intersection test
	inline bool CGeomCore::ShadowIntersect_Improved(CRay* pRay, CIntersectionCacheCluster* pCacheCluster)
	{
		++m_nSessionID;
		pRay->CalcInvDir();
		pRay->fTmin = 0;
		//last node check... -> cacheing
		int32 nCacheIndex = pCacheCluster->GetIndex(pRay->vDir);
		COctreeCell* pLastCell = pCacheCluster->GetCell(nCacheIndex);
		if( NULL != pLastCell && IntersectWithACell_Shadow( pLastCell, pRay) )
			return true;

		//first setup the list of cells with items
		m_nNumItemCells = 0;
		int numCells = 0;
		AddCell_Improved(m_pOctree->m_root, &numCells);
		while (numCells > 0)
		{
			// Take the cell from the list.
			numCells--;
			COctreeCell* currCell = m_ppStackCell[numCells];
			//roll out the code
			COctreeCell** pChildrens = currCell->m_children;

			switch( currCell->m_ChildrenNumber )
			{
			case 8:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 7:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 6:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 5:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 4:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 3:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 2:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			case 1:
				if ((*pChildrens) != NULL && (*pChildrens)->Intersect(pRay))
					AddCell_Improved(*pChildrens, &numCells);
				pChildrens++;
			}//switch
		}//while


//		qsort( m_ppStackItemCell, sizeof(COctreeCell*), m_nNumItemCells, );


		// Walk the octree looking for intersections.
		for( int i = 0; i < m_nNumItemCells; ++i )
		{
			COctreeCell* currCell = m_ppStackItemCell[i];
			if( IntersectWithACell_Shadow( currCell, pRay ) )
			{
				pCacheCluster->SetCell(nCacheIndex,currCell);
				return true;
			}
		}

		pCacheCluster->SetCell(nCacheIndex,NULL);
		return false;
	}

	bool		BuildAccel(CRLWaitProgress* pWaitProgress);
	CString GetAccelStats();

  CCryOctree* GetOctree()
	{
		return m_pOctree;
	}

	AABB* GetBBox()
	{
		return &m_BBox;
	}


private:
	//===================================================================================
	// Method					:	AddCell
	// Description		:	Add a cell into list.
	inline bool AddCell (COctreeCell* cell, int32* numCells)
	{
		// See if we have enough space first.
		if ((*numCells) >= m_nMaxCells)
		{
			m_nMaxCells += m_MaxTreeNodes;
			COctreeCell** tmp = new COctreeCell* [m_nMaxCells];
			if (tmp == NULL)
			{
				return false;
			}
			memset (tmp, 0, sizeof (COctreeCell*) * m_nMaxCells);
			memcpy (tmp, m_ppStackCell, sizeof (COctreeCell*) * (*numCells));
			delete [] m_ppStackCell;
			m_ppStackCell = tmp;
			tmp = NULL;
		}

		// Place the pointer in the list
		m_ppStackCell[(*numCells)] = cell;
		(*numCells)++;

		return true;
	}

	//===================================================================================
	// Method					:	AddCell_Improved
	// Description		:	Add a cell into list.
	inline bool AddCell_Improved (COctreeCell* cell, int32* numCells)
	{
		if( 0 == cell->m_numItems )
		{
			// See if we have enough space first.
			if ((*numCells) >= m_nMaxCells)
			{
				m_nMaxCells += m_MaxTreeNodes;
				COctreeCell** tmp = new COctreeCell* [m_nMaxCells];
				if (tmp == NULL)
				{
					return false;
				}
				memset (tmp, 0, sizeof (COctreeCell*) * m_nMaxCells);
				memcpy (tmp, m_ppStackCell, sizeof (COctreeCell*) * (*numCells));
				delete [] m_ppStackCell;
				m_ppStackCell = tmp;
				tmp = NULL;
			}

			// Place the pointer in the list
			m_ppStackCell[(*numCells)] = cell;
			(*numCells)++;
		}
		else
		{
			// See if we have enough space first.
			if( m_nNumItemCells >= m_nMaxItemCells)
			{
				m_nMaxItemCells += m_MaxTreeNodes;
				COctreeCell** tmp = new COctreeCell* [m_nMaxItemCells];
				if (tmp == NULL)
					return false;

				memset (tmp, 0, sizeof (COctreeCell*) * m_nMaxItemCells);
				memcpy (tmp, m_ppStackItemCell, sizeof (COctreeCell*) * (m_nNumItemCells));	//fix
				delete [] m_ppStackItemCell;
				m_ppStackItemCell = tmp;
				tmp = NULL;
			}

			// Place the pointer in the list
			m_ppStackItemCell[m_nNumItemCells] = cell;
			m_nNumItemCells++;
		}

		return true;
	}

	__inline bool IntersectWithACell( const COctreeCell *cell, CRay *pRay ) const
	{
		//Hand optimized version
		int nLoopNum = (cell->m_numItems+15) / 16;
		int t = 0;
		switch( cell->m_numItems % 16 )
		{
			do
			{
			case 0:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 15:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 14:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 13:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 12:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 11:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 10:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 9:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 8:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 7:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 6:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 5:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 4:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 3:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 2:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			case 1:
				if ( m_WorldPrims[cell->m_item[t++]].IntersectFrontClosest(pRay) )
					break;
			} while( --nLoopNum > 0 );
		}

		//eary out == found an intersection
		return ( 0 != nLoopNum );
	}

	///////////////////////////////////////////////////////////////////////////////////
	// Go over the triangles and give back is there any INTERSECTION at all..
	///////////////////////////////////////////////////////////////////////////////////
	__inline bool IntersectWithACell_Shadow( const COctreeCell *cell, CRay *pRay )
	{
		//Hand optimized version
		int32 nLoopNum = (cell->m_numItems+15) / 16;
		CRLTriangleSmall* pTri;
		int32* pItem = cell->m_item;
		switch( cell->m_numItems % 16 )
		{
			do
			{
			case 0:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 15:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 14:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 13:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 12:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 11:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 10:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 9:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 8:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 7:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 6:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 5:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 4:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 3:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 2:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			case 1:
				pTri = &(m_WorldPrims[*pItem++]);
				//if( pTri->m_nSessionID != m_nSessionID )
				{
					//pTri->m_nSessionID = m_nSessionID;
					if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
						break;
				}			
			} while( --nLoopNum > 0 );
		}

		//eary out == found an intersection
		return ( nLoopNum > 0 );
	}

	__inline bool IntersectWithATri_Fast( const CRLTriangleSmall *tri, CRay *pRay ) const
	{
		return tri->TriIntersect_Fast(pRay, &(m_MeshInfos[tri->m_nMeshInfo]) );
	}

	///////////////////////////////////////////////////////////////////////////////////
	//Run over all of the triangles give back the CLOSEST DISTANCE!
	///////////////////////////////////////////////////////////////////////////////////
	__inline void IntersectWithACell_Fast( const COctreeCell *cell, CRay *pRay )
	{
		//Hand optimized version
		int nLoopNum = (cell->m_numItems+15) / 16;
		int32* pItem = cell->m_item;
		CRLTriangleSmall* pTri;
		switch( cell->m_numItems % 16 )
		{
			do
			{
		case 0:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 15:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 14:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 13:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 12:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 11:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 10:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 9:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 8:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 7:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 6:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 5:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 4:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 3:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 2:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
		case 1:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
				{
					pRay->fT = pRay->fTmin;
					pRay->fTmin = 0.f;
				}
			}
			} while( --nLoopNum > 0 );
		}
	}
	__inline bool FirstIntersectWithACell_Fast( const COctreeCell *cell, CRay *pRay )
	{
		//Hand optimized version
		int nLoopNum = (cell->m_numItems+15) / 16;
		int32* pItem = cell->m_item;
		CRLTriangleSmall* pTri;
		switch( cell->m_numItems % 16 )
		{
			do
			{
		case 0:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 15:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 14:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 13:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 12:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 11:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 10:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 9:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 8:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 7:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 6:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 5:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 4:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 3:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 2:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
		case 1:
			pTri = &(m_WorldPrims[*pItem++]);
			//if( pTri->m_nSessionID != m_nSessionID )
			{
				//pTri->m_nSessionID = m_nSessionID;
				if( pTri->TriIntersect_Fast(pRay, &(m_MeshInfos[pTri->m_nMeshInfo]) ) )
					return true;
			}
			} while( --nLoopNum > 0 );
		}
		return false;
	}

private:

	t_vectTracable m_WorldPrims;
	t_vectMeshInfo m_MeshInfos;

	// Octree stack - avoid recursion
	bool m_bOctreeBuilt;
  CCryOctree* m_pOctree;
	COctreeCell** m_ppStackCell;
	int m_nMaxCells;

	COctreeCell** m_ppStackItemCell;
	int32 m_nMaxItemCells, m_nNumItemCells;


	AABB	m_BBox;			// The bound of all the objects in the tree

	//int					currentRayID;		// The global ray counter used for mailboxing

	int32		m_nSessionID;
public:
	//epsilon&inf constants
	static const f32 m_fEps;
	static const f32 m_fInf;
	
	static const f64 m_dEps;
	static const f64 m_dInf;

  //octree  constants
	static const int32 m_MaxTreeNodes;
	static const int32 m_nMaxInBox;

	static CRLWaitProgress* m_pWaitProgress;
private:
	// Some statistic counters.
	int32 m_nMaxTrisTested;
	int32 m_nMaxTrisHit ;
	int32 m_nMaxCellsTested;

};


#endif

