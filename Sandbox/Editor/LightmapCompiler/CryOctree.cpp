/*=============================================================================
  CryOctree.cpp : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#include "stdafx.h"

#include <string.h>

#include "CryOctree.h"

// The size of the initial stack for traversing the tree.
static const int32 INITIAL_OCTREE_STACK_SIZE = 64;
static const f32 BBOX_CUTOFF = 0.125f;
static const f32 BBOX_CUTOFF_2 = BBOX_CUTOFF*2;

// EPSILON for cell creation, needs to be at least half as small as CUTOFF
static const f32 OCT_EPSILON = 0.0001f;

//===================================================================================
// Constructor
//===================================================================================
COctreeCell::COctreeCell (void)
{
   memset (&m_boundingBox, 0, sizeof (OctBoundingBox));
   memset (&m_children, 0, sizeof (COctreeCell*) * 8);
   m_numItems = 0;
	 m_ChildrenNumber = 0;
   m_item = NULL;
}

//===================================================================================
// Destructor
//===================================================================================
COctreeCell::~COctreeCell (void)
{
   for (int32 c = 0; c < m_ChildrenNumber; c++)
   {
       SAFE_DELETE(m_children[c] );
   }
   memset (&m_children, 0, sizeof (COctreeCell*) * 8);
   memset (&m_boundingBox, 0, sizeof (OctBoundingBox));
   m_numItems = 0;
	 m_ChildrenNumber = 0;
   SAFE_DELETE( m_item );
}

//===================================================================================
// Constructor
//===================================================================================
CCryOctree::CCryOctree (void)
{
   m_root = NULL;
	 m_pCellArray = NULL;
	 m_pCellItemArray = NULL;
}

//===================================================================================
// Destructor
//===================================================================================
CCryOctree::~CCryOctree (void)
{
	if( m_pCellArray )
	{
		SAFE_DELETE_ARRAY( m_pCellArray );
		SAFE_DELETE_ARRAY( m_pCellItemArray );
	}
	else
	{
		SAFE_DELETE( m_root );
	}

   m_root = NULL;
}

_inline void KillDenormals( float &f )
{
#ifndef WIN64
	const int nFloat = *reinterpret_cast <const int *> (&f);
	f = ( nFloat & 0x007FFFFF && !(nFloat & 0x7F800000)) ? 0 : f;
#endif
}

//===================================================================================
// Method				:	GetBoundingBox 
// Description	:	Gets the bounding box for a given child.
//===================================================================================
void
CCryOctree::GetBoundingBox (int32 childIndex, OctBoundingBox *bbox, 
                           COctreeCell* child)
{
	Vec3 vHalf( ( bbox->maxX - bbox->minX ) * 0.5f, 
		( bbox->maxY - bbox->minY ) * 0.5f,
		( bbox->maxZ - bbox->minZ ) * 0.5f );
	Vec3 vCenter( vHalf.x + bbox->minX, vHalf.y + bbox->minY, vHalf.z + bbox->minZ);

	// Figure out min/maxs
	switch (childIndex)
	{
	case eOCT_TREE_CHILD_PX_PY_PZ:
		child->m_boundingBox.minX = vCenter.x - OCT_EPSILON;
		child->m_boundingBox.maxX = vCenter.x + vHalf.x + OCT_EPSILON;
		child->m_boundingBox.minY = vCenter.y - OCT_EPSILON;
		child->m_boundingBox.maxY = vCenter.y + vHalf.y + OCT_EPSILON;
		child->m_boundingBox.minZ = vCenter.z - OCT_EPSILON;
		child->m_boundingBox.maxZ = vCenter.z + vHalf.z + OCT_EPSILON;
		break;

	case eOCT_TREE_CHILD_PX_NY_PZ:
		child->m_boundingBox.minX = vCenter.x - OCT_EPSILON;
		child->m_boundingBox.maxX = vCenter.x + vHalf.x + OCT_EPSILON;
		child->m_boundingBox.minY = vCenter.y - vHalf.y - OCT_EPSILON;
		child->m_boundingBox.maxY = vCenter.y + OCT_EPSILON;
		child->m_boundingBox.minZ = vCenter.z - OCT_EPSILON;
		child->m_boundingBox.maxZ = vCenter.z + vHalf.z + OCT_EPSILON;
		break;

	case eOCT_TREE_CHILD_PX_PY_NZ:
		child->m_boundingBox.minX = vCenter.x - OCT_EPSILON;
		child->m_boundingBox.maxX = vCenter.x + vHalf.x + OCT_EPSILON;
		child->m_boundingBox.minY = vCenter.y - OCT_EPSILON;
		child->m_boundingBox.maxY = vCenter.y + vHalf.y + OCT_EPSILON;
		child->m_boundingBox.minZ = vCenter.z - vHalf.z - OCT_EPSILON;
		child->m_boundingBox.maxZ = vCenter.z + OCT_EPSILON;
		break;

	case eOCT_TREE_CHILD_PX_NY_NZ:
		child->m_boundingBox.minX = vCenter.x - OCT_EPSILON;
		child->m_boundingBox.maxX = vCenter.x + vHalf.x + OCT_EPSILON;
		child->m_boundingBox.minY = vCenter.y - vHalf.y - OCT_EPSILON;
		child->m_boundingBox.maxY = vCenter.y + OCT_EPSILON;
		child->m_boundingBox.minZ = vCenter.z - vHalf.z - OCT_EPSILON;
		child->m_boundingBox.maxZ = vCenter.z + OCT_EPSILON;
		break;

	case eOCT_TREE_CHILD_NX_PY_PZ:
		child->m_boundingBox.minX = vCenter.x - vHalf.x - OCT_EPSILON;
		child->m_boundingBox.maxX = vCenter.x + OCT_EPSILON;
		child->m_boundingBox.minY = vCenter.y - OCT_EPSILON;
		child->m_boundingBox.maxY = vCenter.y + vHalf.y + OCT_EPSILON;
		child->m_boundingBox.minZ = vCenter.z - OCT_EPSILON;
		child->m_boundingBox.maxZ = vCenter.z + vHalf.z + OCT_EPSILON;
		break;

	case eOCT_TREE_CHILD_NX_NY_PZ:
		child->m_boundingBox.minX = vCenter.x - vHalf.x - OCT_EPSILON;
		child->m_boundingBox.maxX = vCenter.x + OCT_EPSILON;
		child->m_boundingBox.minY = vCenter.y - vHalf.y - OCT_EPSILON;
		child->m_boundingBox.maxY = vCenter.y + OCT_EPSILON;
		child->m_boundingBox.minZ = vCenter.z - OCT_EPSILON;
		child->m_boundingBox.maxZ = vCenter.z + vHalf.z + OCT_EPSILON;
		break;

	case eOCT_TREE_CHILD_NX_PY_NZ:
		child->m_boundingBox.minX = vCenter.x - vHalf.x - OCT_EPSILON;
		child->m_boundingBox.maxX = vCenter.x + OCT_EPSILON;
		child->m_boundingBox.minY = vCenter.y - OCT_EPSILON;
		child->m_boundingBox.maxY = vCenter.y + vHalf.y + OCT_EPSILON;
		child->m_boundingBox.minZ = vCenter.z - vHalf.z - OCT_EPSILON;
		child->m_boundingBox.maxZ = vCenter.z + OCT_EPSILON;
		break;

	case eOCT_TREE_CHILD_NX_NY_NZ:
		child->m_boundingBox.minX = vCenter.x - vHalf.x - OCT_EPSILON;
		child->m_boundingBox.maxX = vCenter.x + OCT_EPSILON;
		child->m_boundingBox.minY = vCenter.y - vHalf.y - OCT_EPSILON;
		child->m_boundingBox.maxY = vCenter.y + OCT_EPSILON;
		child->m_boundingBox.minZ = vCenter.z - vHalf.z - OCT_EPSILON;
		child->m_boundingBox.maxZ = vCenter.z + OCT_EPSILON;
		break;

	default:
		// badness.
		{int32 i = 0;}
		break;
	}

	KillDenormals(child->m_boundingBox.minX);
	KillDenormals(child->m_boundingBox.minY);
	KillDenormals(child->m_boundingBox.minZ);
	KillDenormals(child->m_boundingBox.maxX);
	KillDenormals(child->m_boundingBox.maxY);
	KillDenormals(child->m_boundingBox.maxZ);
} // end of GetBoundingBox 


//===================================================================================
// Method				:	FillTree
// Description	:	Fill in the octree.
//===================================================================================
bool
CCryOctree::FillTree (int32 numItems, void* itemList,
                     OctBoundingBox& boundingBox,
                     PFNITEMINBOX fnItemInBox, int32 maxInBox,
										 PFNITEMBBOX fnItemBBox,
                     PFNOCTPROGRESS fnProgress)
{
   // First create the root node
   m_root = new COctreeCell;
   memcpy (&(m_root->m_boundingBox), &boundingBox, sizeof (OctBoundingBox));

	 // Expand bounding box a little, just in case there's some floating point
	 // strangeness with planes lying on planes.
	 m_root->m_boundingBox.minX -= OCT_EPSILON;
	 m_root->m_boundingBox.minY -= OCT_EPSILON;
	 m_root->m_boundingBox.minZ -= OCT_EPSILON;
	 m_root->m_boundingBox.maxX += OCT_EPSILON;
	 m_root->m_boundingBox.maxY += OCT_EPSILON;
	 m_root->m_boundingBox.maxZ += OCT_EPSILON;

	 // Allocate items list for root
   m_root->m_numItems = 0;
	 m_root->m_ChildrenNumber = 0;
   m_root->m_item = new int32 [numItems];
   if (m_root->m_item == NULL)
   {
      return false;
   }
   memset (m_root->m_item, 0, sizeof (int32) * numItems);

   // Fill in items for root.
   for (int32 i = 0; i < numItems; i++)
   {
      // By calling this function when filling this array it lets the
      // calling program weed out some items. Useful in ambient occlusion
      // to have some items not block rays.
		 //  this stage i don't add the non opaque triangles - never ever need to care about them in octree..
      if (fnItemInBox (i, itemList, &(m_root->m_boundingBox)) == true)
      {
         m_root->m_item[m_root->m_numItems] = i;
         m_root->m_numItems++;
      }
   }

   // Allocate a stack to hold the list of cells we are working with.
   COctreeCell** cell = new COctreeCell* [INITIAL_OCTREE_STACK_SIZE];
   if (cell == NULL)
   {
      return false;
   }
   int32 maxCells = INITIAL_OCTREE_STACK_SIZE;
   memset (cell, 0, sizeof (COctreeCell*) * maxCells);

   // Now partition
   cell[0] = m_root;
   int32 numCells = 1;
   while (numCells > 0)
   {
      // Show some progress if requested.
      if (fnProgress != NULL)
      {
				if( numCells )
					fnProgress (1.f / (f32)numCells);
				else
					fnProgress (1.f);
      }
      
      // Take the one off the stack.
      numCells--;
      COctreeCell* currCell = cell[numCells];
/*      
			// First see if we even need to create children
			if ( (currCell->m_numItems < maxInBox) ||
				( ( currCell->m_boundingBox.maxX - currCell->m_boundingBox.minX ) <= BBOX_CUTOFF_2) ||
				( ( currCell->m_boundingBox.maxY - currCell->m_boundingBox.minY ) <= BBOX_CUTOFF_2) ||
				( ( currCell->m_boundingBox.maxZ - currCell->m_boundingBox.minZ ) <= BBOX_CUTOFF_2) )
			{
				continue;
			}
			/*
			// If the average bounding box of the triangles is bigger than the half of the cell's bounding box,
			// doesn't worth to cut it more... if the triangles are too big why we want a deeper octree...
			float fAveDim[3];
			fAveDim[0] =
			fAveDim[1] =
			fAveDim[2] = 0.f;
			OctBoundingBox BBox;
			for (int32 i = 0; i < currCell->m_numItems; i++)
			{
				fnItemBBox(currCell->m_item[i], itemList, &BBox);

				//cut with the current cell-s bounding box... 
				BBox.min[0] = BBox.min[0] < currCell->m_boundingBox.minX ? currCell->m_boundingBox.minX : BBox.min[0];
				BBox.min[1] = BBox.min[1] < currCell->m_boundingBox.minY ? currCell->m_boundingBox.minY : BBox.min[1];
				BBox.min[2] = BBox.min[2] < currCell->m_boundingBox.minZ ? currCell->m_boundingBox.minZ : BBox.min[2];
				BBox.max[0] = BBox.max[0] > currCell->m_boundingBox.maxX ? currCell->m_boundingBox.maxX : BBox.max[0];
				BBox.max[1] = BBox.max[1] > currCell->m_boundingBox.maxY ? currCell->m_boundingBox.maxY : BBox.max[1];
				BBox.max[2] = BBox.max[2] > currCell->m_boundingBox.maxZ ? currCell->m_boundingBox.maxZ : BBox.max[2];

				fAveDim[0] += BBox.max[0] - BBox.min[0];
				fAveDim[1] += BBox.max[0] - BBox.min[0];
				fAveDim[2] += BBox.max[0] - BBox.min[0];
			}

			if( currCell->m_numItems )
			{
				float fOnePerNum = 1.f / currCell->m_numItems;
				fAveDim[0] *= 
				fAveDim[1] *= 
				fAveDim[2] *= fOnePerNum;
			}

			float fCellDim[3];
			fCellDim[0] = currCell->m_boundingBox.maxX - currCell->m_boundingBox.minX;
			fCellDim[1] = currCell->m_boundingBox.maxY - currCell->m_boundingBox.minY;
			fCellDim[2] = currCell->m_boundingBox.maxZ - currCell->m_boundingBox.minZ;

			if( ( fCellDim[0]*0.5f < fAveDim[0] ) && ( fCellDim[1]*0.5f < fAveDim[1] ) && ( fCellDim[2]*0.5f < fAveDim[2] ) )
				continue;
			*/
      // Create children as needed and add them to our list.

			int32*	pTemp = new int32 [ currCell->m_numItems ];
			if( NULL == pTemp )
			{
				SAFE_DELETE_ARRAY(cell);
				return false;
			}

      for (int32 cc = 0; cc < 8; cc++)
      {
         // First create a new cell.
         currCell->m_children[currCell->m_ChildrenNumber] = new COctreeCell;
         if (currCell->m_children[currCell->m_ChildrenNumber] == NULL)
         {
						SAFE_DELETE_ARRAY(pTemp);
            SAFE_DELETE_ARRAY(cell);
            return false;
         }
         COctreeCell* child = currCell->m_children[currCell->m_ChildrenNumber];
         memset (child, 0, sizeof (COctreeCell));

				 // Now figure out the bounding box.
				 GetBoundingBox (cc, &currCell->m_boundingBox, child);

				 // Run through the items and see if they belong in this cell.
				 child->m_numItems = 0;
				 for (int32 i = 0; i < currCell->m_numItems; i++)
				 {
					 if (fnItemInBox (currCell->m_item[i], itemList, &child->m_boundingBox) )
					 {
						 pTemp[child->m_numItems++] = currCell->m_item[i];
					 }
				 } // end for number of items in the current cell.

				 // If we didn't find anything in the box, bail
				 if (0 == child->m_numItems )
				 {
					 delete currCell->m_children[currCell->m_ChildrenNumber];
					 currCell->m_children[currCell->m_ChildrenNumber] = NULL;
					 child = NULL;
					 continue;
				 }

				 //we have the real numItems... allocate only the nessesary space
				 child->m_item = new int32 [child->m_numItems];
				 if (child->m_item == NULL)
				 {
					 SAFE_DELETE_ARRAY( cell );
					 SAFE_DELETE_ARRAY( pTemp );
					 return NULL;
				 }
				 memcpy (child->m_item, pTemp, sizeof (int32) * child->m_numItems);

         // If we haven't reached our target item size add this into our list
				 if ( (child->m_numItems > maxInBox) &&
					 (( ( child->m_boundingBox.maxX - child->m_boundingBox.minX ) > BBOX_CUTOFF_2) ||
					 ( ( child->m_boundingBox.maxY - child->m_boundingBox.minY ) > BBOX_CUTOFF_2) ||
					 ( ( child->m_boundingBox.maxZ - child->m_boundingBox.minZ ) > BBOX_CUTOFF_2)) )
         {
            // Make sure we have enough room on our stack.
            if (numCells >= maxCells)
            {
               maxCells += INITIAL_OCTREE_STACK_SIZE;
               COctreeCell** newCells = new COctreeCell* [maxCells];
							 memset (newCells, 0, sizeof (COctreeCell*) * maxCells);
               memcpy (newCells, cell, sizeof (COctreeCell*) * numCells);
               SAFE_DELETE_ARRAY( cell );
               cell = newCells;
            }

            // Add this item into our list.
            cell[numCells] = currCell->m_children[currCell->m_ChildrenNumber];
            numCells++;
         }
         child = NULL;
				 currCell->m_ChildrenNumber++;
      } // for c (number of children);

			SAFE_DELETE_ARRAY( pTemp );

			//free up the items
			if( currCell->m_ChildrenNumber )
			{
					currCell->m_numItems = 0;
					SAFE_DELETE_ARRAY( currCell->m_item );
			}
   } // While we still have cells.

   // Worked!
   SAFE_DELETE_ARRAY( cell );


	 //Rescale the octree leaf bounding boxes -> it will help to skip the free spaces...
	//RescaleCell( m_root, fnItemBBox, itemList );

   return true;
} // end of FillTree


//===================================================================================
// Method				:	RescaleCell
// Description	:	Rescale the bounding box of the cells to help skip the free spaces
//===================================================================================
bool CCryOctree::RescaleCell( COctreeCell *pCell, PFNITEMBBOX fnItemBBox, void* itemList )
{
	int32 i;


	bool bHaveValidBBox = false;

	Vec3 vMin( FLT_MAX,FLT_MAX,FLT_MAX );
	Vec3 vMax( -FLT_MAX,-FLT_MAX,-FLT_MAX );

	//first: recalculate the childs bboxes
	for( i = 0; i < pCell->m_ChildrenNumber; ++i )
	{
		COctreeCell *pChild = pCell->m_children[i];
		if( RescaleCell( pChild, fnItemBBox, itemList ) )
		{
			vMin.x = ( vMin.x < pChild->m_boundingBox.min[0] ) ? vMin.x  : pChild->m_boundingBox.min[0];
			vMin.y = ( vMin.y < pChild->m_boundingBox.min[1] ) ? vMin.y  : pChild->m_boundingBox.min[1];
			vMin.z = ( vMin.z < pChild->m_boundingBox.min[2] ) ? vMin.z  : pChild->m_boundingBox.min[2];
			vMax.x = ( vMax.x > pChild->m_boundingBox.max[0] ) ? vMax.x  : pChild->m_boundingBox.max[0];
			vMax.y = ( vMax.y > pChild->m_boundingBox.max[1] ) ? vMax.y  : pChild->m_boundingBox.max[1];
			vMax.z = ( vMax.z > pChild->m_boundingBox.max[2] ) ? vMax.z  : pChild->m_boundingBox.max[2];
			bHaveValidBBox = true;
		}
	}

	if( pCell->m_numItems )
	{
		OctBoundingBox BBox;
		//second check the geometry itself.
		for( i = 0; i < pCell->m_numItems; ++i )
		{
			if( fnItemBBox( i , itemList, &BBox) )
			{
				BBox.min[0] -= OCT_EPSILON;
				BBox.min[1] -= OCT_EPSILON;
				BBox.min[2] -= OCT_EPSILON;
				BBox.max[0] += OCT_EPSILON;
				BBox.max[1] += OCT_EPSILON;
				BBox.max[2] += OCT_EPSILON;

				vMin.x = ( vMin.x < BBox.min[0] ) ? vMin.x : BBox.min[0];
				vMin.y = ( vMin.y < BBox.min[1] ) ? vMin.y : BBox.min[1];
				vMin.z = ( vMin.z < BBox.min[2] ) ? vMin.z : BBox.min[2];
				vMax.x = ( vMax.x > BBox.max[0] ) ? vMax.x : BBox.max[0];
				vMax.y = ( vMax.y > BBox.max[1] ) ? vMax.y : BBox.max[1];
				vMax.z = ( vMax.z > BBox.max[2] ) ? vMax.z : BBox.max[2];
				bHaveValidBBox = true;
			}
		}
	}

	if( bHaveValidBBox )
	{
		//check which is a smaller (sometimes the geometry can bigger than the original BBox )
		pCell->m_boundingBox.min[0]  = ( vMin.x < pCell->m_boundingBox.min[0] ) ? vMin.x : pCell->m_boundingBox.min[0];
		pCell->m_boundingBox.min[1]  = ( vMin.y < pCell->m_boundingBox.min[1] ) ? vMin.y : pCell->m_boundingBox.min[1];
		pCell->m_boundingBox.min[2]  = ( vMin.z < pCell->m_boundingBox.min[2] ) ? vMin.z : pCell->m_boundingBox.min[2];

		pCell->m_boundingBox.max[0]  = ( vMax.x > pCell->m_boundingBox.max[0] ) ? vMax.x : pCell->m_boundingBox.max[0];
		pCell->m_boundingBox.max[1]  = ( vMax.y > pCell->m_boundingBox.max[1] ) ? vMax.y : pCell->m_boundingBox.max[1];
		pCell->m_boundingBox.max[2]  = ( vMax.z > pCell->m_boundingBox.max[2] ) ? vMax.z : pCell->m_boundingBox.max[2];
	}

	return true;
}

void	CCryOctree::Populate( COctreeCell *pCell, int32 &nCellID, int32 &nItemID )
{
	assert( pCell );


	t_pairOctreeId OctreeId;
	OctreeId.first = nCellID;
	OctreeId.second = nItemID;
	m_PointerMap.insert( t_pairOcreeCellId( pCell, OctreeId ) );

	++nCellID;
	nItemID += pCell->m_numItems;

	for( int32 i = 0; i < pCell->m_ChildrenNumber; ++i )
	{
		assert( pCell->m_children[i] );
		Populate( pCell->m_children[i], nCellID, nItemID );
	}
}

void	CCryOctree::WriteCells( COctreeCell *pCell, FILE *f )
{
	assert( pCell );
	COctreeCell Cell = *pCell;

	//exchange the pointers
	for( int32 i = 0; i < Cell.m_ChildrenNumber; ++i )
	{
		//TODO: FIX IT HAZAD!!!
		*((int32*)&Cell.m_children[i]) = (m_PointerMap[ Cell.m_children[i] ]).first;
	}

	//TODO: FIX IT HAZAD!!!
	*((int32*)&Cell.m_item) = (m_PointerMap[ pCell ]).second;

	//write to disk
	fwrite( &Cell, sizeof(COctreeCell), 1, f );

	//write the childrens too...
	for( int32 i = 0; i < pCell->m_ChildrenNumber; ++i )
	{
		assert( pCell->m_children[i] );
		WriteCells( pCell->m_children[i], f );
	}

	//clear all "fake" memory pointer
	memset( &Cell, 0, sizeof(COctreeCell) );
}

void	CCryOctree::WriteItems( COctreeCell *pCell, FILE *f )
{
	assert( pCell );

	//write to disk
	if( pCell->m_numItems )
		fwrite( &(pCell->m_item[0]), sizeof(int32)*pCell->m_numItems, 1, f );

	//write the childrens too...
	for( int32 i = 0; i < pCell->m_ChildrenNumber; ++i )
	{
		assert( pCell->m_children[i] );
		WriteItems( pCell->m_children[i], f );
	}
}


bool CCryOctree::Save( FILE *f )
{
	assert( NULL != f );
	if( NULL == f )
			return false;

	int32 nCellID = 0;
	int32 nItemID = 0;
	Populate( m_root, nCellID, nItemID );

	fwrite( &nCellID, sizeof(int32), 1, f );
	WriteCells( m_root, f );

	fwrite( &nItemID, sizeof(int32), 1, f );
	WriteItems( m_root, f );

	return true;
}

void CCryOctree::GeneratePointersForCells( COctreeCell *pCell )
{
	//item pointer
	int32 nItemID = *(int32*)&(pCell->m_item);
	pCell->m_item = &m_pCellItemArray[ nItemID ];

	if( pCell->m_item[0] > 1190 || pCell->m_item[0] < 0 )
	{
		int32 i = 11;
		i += 12;
	}

	//update the childrens too...
	for( int32 i = 0; i < pCell->m_ChildrenNumber; ++i )
	{
		pCell->m_children[i] = &m_pCellArray[ *(int32*)&(pCell->m_children[i]) ];
		assert( pCell->m_children[i] );
		GeneratePointersForCells( pCell->m_children[i] );
	}
}

bool CCryOctree::Load( FILE *f )
{
	assert( NULL != f );
	if( NULL == f )
		return false;

	int32 nNumberOfCell, nNumberOfItem;

	fread( &nNumberOfCell, sizeof(int32), 1, f );
	m_pCellArray = new COctreeCell[ nNumberOfCell ];
	if( NULL == m_pCellArray )
		return false;
	fread( m_pCellArray, sizeof(COctreeCell)*nNumberOfCell,1,f );

	fread( &nNumberOfItem, sizeof(int32), 1, f );
	m_pCellItemArray = new int32[ nNumberOfItem ];
	if( NULL == m_pCellItemArray )
		return false;
	fread( m_pCellItemArray, sizeof(int32)*nNumberOfItem,1,f );

	//the first element is the root
	m_root = &(m_pCellArray[0]);
	GeneratePointersForCells( m_root );

	return true;
}
