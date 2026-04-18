#ifndef __VERTEX_SCATTER_SERIALIZATION_MANAGER_H__
#define __VERTEX_SCATTER_SERIALIZATION_MANAGER_H__

#pragma once

#include <IVertexScatterSerializationManager.h>
#include "I3DEngine.h"

class CVertexScatterSerializationManager : public Cry3DEngineBase, public IVertexScatterSerializationManager
{	
public:

	CVertexScatterSerializationManager( );
	virtual ~CVertexScatterSerializationManager();

	// interface CVertexScatterSerializationManager ---------------------------
	virtual void Release() { delete this; };
	virtual bool ApplyVertexScatterFile(const char* pszFileName, struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber);
	virtual uint SaveVertexScatter(const char* pszFileName) const;
	virtual void CollectDataForVertexScatterGeneration(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber);
	virtual void CollectObjectArrayForVertexScatterGeneration(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber);
	virtual void DeleteObjectArray();
	virtual void SetVertexScattering(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber);

	// ------------------------------------------------------------------------

protected:
	std::map<int32, SScatteringObjectData> m_VertexScatterData;
	SScatteringObjectData** m_ObjectArrayForVertexScatterGeneration;

	void CollectSubObjectDataForVertexScatterGeneration(struct IStatObj* pIStatObj, struct SVertexScatterSubObject& scatteringsubobject, const Matrix34& worldmatrix, float fRefractionAmount);
	void InitializeScattering(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber);
	bool LoadVertexScatteringFile(const char* pszFileName);
};
#endif // __VERTEX_SCATTER_SERIALIZATION_MANAGER_H__
