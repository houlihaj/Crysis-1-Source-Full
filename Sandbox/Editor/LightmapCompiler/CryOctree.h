/*=============================================================================
  CryOctree.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __CRYOCTREE_H__
#define __CRYOCTREE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "platform.h"

#include "Ray.h"
//#define DEBUG_OCTREE

// Enumerations for various children.
// PX = Positive X
// PY = Positive Y
// PZ = Positive Z
// NX = Negative X
// NY = Negative Y
// NZ = Negative Z
enum
{
   eOCT_TREE_CHILD_PX_PY_PZ = 0,
   eOCT_TREE_CHILD_PX_NY_PZ,
   eOCT_TREE_CHILD_PX_PY_NZ,
   eOCT_TREE_CHILD_PX_NY_NZ,
   eOCT_TREE_CHILD_NX_PY_PZ,
   eOCT_TREE_CHILD_NX_NY_PZ,
   eOCT_TREE_CHILD_NX_PY_NZ,
   eOCT_TREE_CHILD_NX_NY_NZ,
};

// A bounding box structure
typedef union
{
   struct
   {
      f32 box[6];
   };
   struct
   {
      f32 min[3];
      f32 max[3];
   };
   struct
   {
      f32 minX, minY, minZ;
      f32 maxX, maxY, maxZ;
   };
} OctBoundingBox;

// A cell within the Octree.
class COctreeCell
{
	public:
		// Con/de-struction
		COctreeCell (void);
		~COctreeCell (void);
		//===================================================================================
		// Method				:	Intersect
		// Description	:	Ray intersection with the octree.
		//===================================================================================
		inline bool Intersect(const CRay* pRay )
		{
			//X plane
			f32 fMinX = (m_boundingBox.box[ 0 + pRay->nSign[0] ] - pRay->vFrom.x) * pRay->vInvDir.x;
			f32 fMaxX = (m_boundingBox.box[ 3 - pRay->nSign[0] ] - pRay->vFrom.x) * pRay->vInvDir.x;

			//Y plane
			f32 fMinY = (m_boundingBox.box[ 1 + pRay->nSign[1] ] - pRay->vFrom.y) * pRay->vInvDir.y;
			f32 fMaxY = (m_boundingBox.box[ 4 - pRay->nSign[1] ] - pRay->vFrom.y) * pRay->vInvDir.y;

			if( fMinX > fMaxY || fMinY >  fMaxX )
				return false;

			f32 fNearDistance = fMinX > fMinY ? fMinX : fMinY;
			f32 fFarDistance = fMaxX < fMaxY ? fMaxX : fMaxY;

			//Z plane
			f32 fMinZ = (m_boundingBox.box[ 2 + pRay->nSign[2] ] - pRay->vFrom.z) * pRay->vInvDir.z;
			f32 fMaxZ = (m_boundingBox.box[ 5 - pRay->nSign[2] ] - pRay->vFrom.z) * pRay->vInvDir.z;

			if( fNearDistance > fMaxZ || fMinZ >  fFarDistance )
				return false;

			fNearDistance = fNearDistance > fMinZ ? fNearDistance : fMinZ;
			fFarDistance = fFarDistance < fMaxZ ? fFarDistance : fMaxZ;

			//test the planes
			return (fFarDistance > pRay->fTmin && fNearDistance < pRay->fT );
		}

	private:
		// Disable copying
		COctreeCell& operator = (COctreeCell& o) { return *this; };


   public:
			 // Number of stuff in this cell
			 int32 m_numItems;
			 int32  m_ChildrenNumber;

			// Bounding box for the cell.
      OctBoundingBox m_boundingBox;

      // The eight children of this cell
      COctreeCell* m_children[8];

			// List of stuff in this cell
      int32* m_item;

			// the last intersection's datas
//			f32 fNearDistance, fFarDistance;
};

// Returns true if the item falls into the given box, false otherwise.
typedef bool (* PFNITEMINBOX)(int32 , void* ,
                                  OctBoundingBox* );

// A callback to show some progress, right now it always passes 0.0.
typedef void (* PFNOCTPROGRESS)(f32 progress);

// Returns axis aligned bounding box of the item.
typedef bool (* PFNITEMBBOX)(int32 , void* ,
															OctBoundingBox* );

typedef std::pair<int32, int32 > t_pairOctreeId;
typedef std::pair<COctreeCell *,t_pairOctreeId> t_pairOcreeCellId;

// Interface into the tree itself.
class CCryOctree
{
   public:
      // Top cell in the octree.
      COctreeCell* m_root;

      // Fill the tree
      bool FillTree (int32 numItems, void* itemList,
                      OctBoundingBox& boundingBox,
                      PFNITEMINBOX fnItemInBox, int32 maxInBox,
											PFNITEMBBOX fnItemBBox,
                      PFNOCTPROGRESS fnProgress = NULL);

      CCryOctree (void);
      virtual ~CCryOctree (void);

			bool				Save( FILE *f );
			bool				Load( FILE *f );

private:
      // No void construction
      // Disable copying
      CCryOctree& operator = (CCryOctree& o) { return *this; };

      // Internal routine to chop up a bounding box.
      void GetBoundingBox (int32 childIndex, OctBoundingBox *bbox,
                           COctreeCell* child);

			// Rescale the bounding box of the cells to help skip the free spaces
			bool RescaleCell( COctreeCell *pCell, PFNITEMBBOX fnItemBBox, void* itemList  );

			void	WriteCells( COctreeCell *pCell, FILE *f );
			void	WriteItems( COctreeCell *pCell, FILE *f );
			void	Populate( COctreeCell *pCell, int32 &nCellID, int32 &nItemID );
			void	GeneratePointersForCells( COctreeCell *pCell );
private:
			int32				*m_pCellItemArray;								///After loading, all cell item are here...
			COctreeCell	*m_pCellArray;										///After loading, all cell are here...
			std::map< COctreeCell *, t_pairOctreeId >	m_PointerMap;	/// serializing helper table
};

#endif
