/*=============================================================================
3DSamplerOctree.h : 
Copyright 2006 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef __3DSAMPLEROCTREE_H__
#define __3DSAMPLEROCTREE_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include <I3DSampler.h>
#include "Cry3DEngineBase.h"

#define OCTREE_3D_SAMPLER_FILE_VERSION	1																	//Revision number of the file format
#define LEVELLM_PAK_NAME "LevelLM.pak"

/*
	Octree based 3d sample interpolator - regular sampling based b-spline interpolation
*/
class C3DSamplerOctree : public I3DSampler, public Cry3DEngineBase
{ 
public:
	struct s3DSamplerOctreeCell
	{
		int16	m_nFlags;														// highest bit: have childrens, if have, no items in the cell at all. lower 8 bit, how much items
		void* m_pDatas[8];												// the childs fixed order or the items pointer
	};
	struct s3DSamplerOctreeCellStack
	{
		s3DSamplerOctreeCell* m_pCell;						// the cell
		AABB									m_AABB;							// the AABB of that cell
	};

	struct sHeader_3DSamplerOctree
	{
		int32		m_nRevisionNumber;																						//Revision number of the file format
		e3DSamplingType m_SamplingType;																				//the type of sampling
		int32		m_nNumberOfCells;																							//the number of cells
		int32		m_nNumberOfPoints;																						//the number of samples
		AABB		m_GlobalBBox;																									//the global bounding box (DefineLocalSpace rebuild the local one)
		Vec3		m_vScale;																											//the scale between the point in world space
		AUTO_STRUCT_INFO
	};

	struct s3DSamplerOctreeCell_Serialize
	{
		int16	m_nFlags;														// highest bit: have childrens, if have, no items in the cell at all. lower 8 bit, how much items
		int32 m_pDatas[8];												// the childs fixed order or the items pointer
		AUTO_STRUCT_INFO
	};

	C3DSamplerOctree():m_pOctree(NULL),m_pStack(NULL),m_nSizeOfTheStack(0),m_nOctreeCellNumber(0)
	{}

	~C3DSamplerOctree()
	{
		SAFE_DELETE_ARRAY( m_pStack );
		SAFE_DELETE_ARRAY( m_pOctree );
	}

	bool						GetInterpolatedData( const Vec3& vPosition, f32* pFloats );																			//Give back the interpolated datas from the octree

	//serialization
	bool						Save( const char* szFilePath );																																	// Save all data
	bool						Load( const char* szFilePath );																																	// Load all data

	//Creation helper functions
	bool						CreateIrregularSampling( const AABB& GlobalBBox, const Vec3& vScale , const int nPointNumber, const f32* pPoints );	//Create the octree for that points

protected:
	bool						SearchNearSamplingPoints( const Vec3& vPositionInLocalSpace );														// Search the near sampling points
	void						DefineLocalSpace( const AABB& GlobalBBox, const Vec3& vScale );														// Define the translation / scale from world space to local space
protected:
	s3DSamplerOctreeCellStack* m_pStack;																																	// The stack for walking in the octree
	s3DSamplerOctreeCell* m_pOctree;																																			// The octree
	AABB				m_LocalBBox;																																							// The bounding box of the octree in local space
	Vec3				m_vInvScale;																																							// The scale value for translate the world space to local space
	AABB				m_GlobalBBox;																																							// The bounding box of the octree in world space
	Vec3				m_vScale;																																									// The scale value for translate the local space to world space
	int					m_nSizeOfTheStack;																																				// The size of the stack
	int					m_nOctreeCellNumber;																																			// The number of the cells.
};

#endif//__3DSAMPLEROCTREE_H__