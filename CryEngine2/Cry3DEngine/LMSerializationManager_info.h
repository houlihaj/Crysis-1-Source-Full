#include "LMSerializationManager.h"

STRUCT_INFO_EMPTY(CLMTextureToObjID)

STRUCT_INFO_BEGIN(CLMSerializationManager::FileHeader)
	STRUCT_VAR_INFO(iMagicNumber, TYPE_INFO(UINT))
	STRUCT_VAR_INFO(iVersion, TYPE_INFO(UINT))
	STRUCT_VAR_INFO(iNumLM_Pairs, TYPE_INFO(UINT))
	STRUCT_VAR_INFO(iNumTexCoordSets, TYPE_INFO(UINT))
	STRUCT_VAR_INFO(reserved, TYPE_ARRAY(4, TYPE_INFO(UINT)))
STRUCT_INFO_END(CLMSerializationManager::FileHeader)

STRUCT_INFO_BEGIN(CLMSerializationManager::LMHeader)
	STRUCT_VAR_INFO(iWidth, TYPE_INFO(UINT))
	STRUCT_VAR_INFO(iHeight, TYPE_INFO(UINT))
	STRUCT_VAR_INFO(numGLMs, TYPE_INFO(UINT))
STRUCT_INFO_END(CLMSerializationManager::LMHeader)

STRUCT_INFO_BEGIN(CLMSerializationManager::UVSetHeader)
	STRUCT_VAR_INFO(nIdGLM, TYPE_INFO(UINT))
	STRUCT_VAR_INFO(nHashGLM, TYPE_INFO(UINT))
	STRUCT_VAR_INFO(numUVs, TYPE_INFO(UINT))
STRUCT_INFO_END(CLMSerializationManager::UVSetHeader)

STRUCT_INFO_BEGIN(CLMSerializationManager::UVSetHeader3)
	STRUCT_BASE_INFO(CLMSerializationManager::UVSetHeader)
	STRUCT_VAR_INFO(OcclIds, TYPE_ARRAY(4*2, TYPE_INFO(EntityId)))
	STRUCT_VAR_INFO(ucOcclCount, TYPE_INFO(unsigned char))
STRUCT_INFO_END(CLMSerializationManager::UVSetHeader3)

STRUCT_INFO_BEGIN(CLMSerializationManager::UVSetHeader4)
	STRUCT_BASE_INFO(CLMSerializationManager::UVSetHeader3)
	STRUCT_VAR_INFO(OcclIds, TYPE_ARRAY(16*2, TYPE_INFO(EntityId)))
	STRUCT_VAR_INFO(ucOcclCount, TYPE_INFO(unsigned char))
STRUCT_INFO_END(CLMSerializationManager::UVSetHeader4)

STRUCT_INFO_BEGIN(CLMSerializationManager::UVSetHeader5::sOcclID)
	STRUCT_VAR_INFO(m_nGuid, TYPE_INFO(EntityGUID))
	STRUCT_VAR_INFO(m_nLightId, TYPE_INFO(EntityId))
STRUCT_INFO_END(CLMSerializationManager::UVSetHeader5::sOcclID)

STRUCT_INFO_BEGIN(CLMSerializationManager::UVSetHeader5)
	STRUCT_BASE_INFO(CLMSerializationManager::UVSetHeader3)
	STRUCT_VAR_INFO(OcclIds, TYPE_ARRAY(16, TYPE_INFO(CLMSerializationManager::UVSetHeader5::sOcclID)))
	STRUCT_VAR_INFO(ucOcclCount, TYPE_INFO(unsigned char))
STRUCT_INFO_END(CLMSerializationManager::UVSetHeader5)

STRUCT_INFO_BEGIN(CLMSerializationManager::UVSetHeader6)
	STRUCT_BASE_INFO(CLMSerializationManager::UVSetHeader5)
	STRUCT_VAR_INFO(m_nFirstOcclusionChannel, TYPE_INFO(int8))
STRUCT_INFO_END(CLMSerializationManager::UVSetHeader6)

STRUCT_INFO_BEGIN(CLMSerializationManager::UVSetHeader8)
	STRUCT_BASE_INFO(CLMSerializationManager::UVSetHeader6)
	STRUCT_VAR_INFO(m_nSubObjIdx, TYPE_INFO(int8))
STRUCT_INFO_END(CLMSerializationManager::UVSetHeader8)

