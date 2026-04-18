////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2007.
// -------------------------------------------------------------------------
//  File name:   IVertexScatterSerializationManager.h
//  Version:     v1.00
//  Created:     30/10/2007 by Tamas Kezdi
//  Compilers:   Visual Studio 2005
//  Description: Vertex Scatter Serialization interface
// -------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////

#ifndef _CRY_COMMON_VERTEX_SCATTER_SERIALIZATION_MANAGER_HDR_
#define _CRY_COMMON_VERTEX_SCATTER_SERIALIZATION_MANAGER_HDR_

#include <IEntitySystem.h>

//! \brief Interface for vertex scattering serialization
struct IVertexScatterSerializationManager
{
	virtual void Release() = 0;

	//!
	virtual bool ApplyVertexScatterFile(const char* pszFileName, struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber) = 0;
	//!

	//!
	virtual uint SaveVertexScatter(const char* pszFileName) const = 0;
	//!

	//!
	virtual void CollectDataForVertexScatterGeneration(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber) = 0;
	//!

	//!
	virtual void CollectObjectArrayForVertexScatterGeneration(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber) = 0;
	//!

	//!
	virtual void DeleteObjectArray() = 0;
	//!

	//!
	virtual void SetVertexScattering(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber) = 0;
	//!
};

#endif //_CRY_COMMON_VERTEX_SCATTER_SERIALIZATION_MANAGER_HDR_
