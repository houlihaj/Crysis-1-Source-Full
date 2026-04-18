#define NO_GDI
#include "StdAfx.h"
#undef NO_GDI
#include "Cry3DEngineBase.h"

#include "VertexScatterSerializationManager.h"

#include "Brush.h"
#include "TypeInfo_impl.h"

#define LEVELVS_PAK_NAME "LevelVS.pak"

#define SEGMENTED_SAVE

struct SVertexScatterLODLevelHeader
{
	enum { MAGIC_NUMBER  = 0xF53210B1 };
	SVertexScatterLODLevelHeader() { m_nMagicNumber = MAGIC_NUMBER; }

	int32 m_nMagicNumber;
	int32 m_nLODLevel;
	int32 m_nNumVertices;

	AUTO_STRUCT_INFO
};

struct SVertexScatterSubObjectHeader
{
	enum { MAGIC_NUMBER  = 0xE683201E };
	SVertexScatterSubObjectHeader() { m_nMagicNumber = MAGIC_NUMBER; }

	int32 m_nMagicNumber;
	int32 m_nLODLevels;

	AUTO_STRUCT_INFO
};

struct SVertexScatterObjectHeader
{
	enum { MAGIC_NUMBER  = 0xAE60D571 };

	SVertexScatterObjectHeader() { m_nMagicNumber = MAGIC_NUMBER; }

	int32 m_nMagicNumber;
	int32 m_nId;
	int32 m_nNumSubObjects;
	Vec4 m_VertexScatterTransformZ;

	AUTO_STRUCT_INFO
};

struct SFileHeader
{
	enum { MAGIC_NUMBER  = 0xB3DB2C68 };
	enum { VERSION = 1 };
	SFileHeader()
	{
		iMagicNumber = MAGIC_NUMBER;
		iVersion = VERSION;
	}

	uint32 iMagicNumber;
	uint32 iVersion;
	uint32 iNumScatterObjects;

	AUTO_STRUCT_INFO
};

struct AutoFileCloser: Cry3DEngineBase
{
	AutoFileCloser(FILE*pFile):m_pFile(pFile){}
	~AutoFileCloser(){if (m_pFile)GetPak()->FClose(m_pFile);}
protected:
	FILE* m_pFile;
};

CVertexScatterSerializationManager::CVertexScatterSerializationManager()
{
	m_ObjectArrayForVertexScatterGeneration = NULL;
}

CVertexScatterSerializationManager::~CVertexScatterSerializationManager() {}

STRUCT_INFO_BEGIN(SVertexScatterLODLevelHeader)
	STRUCT_VAR_INFO(m_nMagicNumber, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(m_nLODLevel, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(m_nNumVertices, TYPE_INFO(uint32))
STRUCT_INFO_END(SVertexScatterLODLevelHeader)

STRUCT_INFO_BEGIN(SVertexScatterSubObjectHeader)
	STRUCT_VAR_INFO(m_nMagicNumber, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(m_nLODLevels, TYPE_INFO(uint32))
STRUCT_INFO_END(SVertexScatterSubObjectHeader)

STRUCT_INFO_BEGIN(SVertexScatterObjectHeader)
	STRUCT_VAR_INFO(m_nMagicNumber, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(m_nId, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(m_nNumSubObjects, TYPE_INFO(uint32))
	//STRUCT_VAR_INFO(m_TransformationForVertexScatter, TYPE_INFO(Vec4))
	STRUCT_VAR_INFO(m_VertexScatterTransformZ, TYPE_ARRAY(4, TYPE_INFO(float)))
STRUCT_INFO_END(SVertexScatterObjectHeader)

STRUCT_INFO_BEGIN(SFileHeader)
	STRUCT_VAR_INFO(iMagicNumber, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(iVersion, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(iNumScatterObjects, TYPE_INFO(uint32))
STRUCT_INFO_END(SFileHeader)

SVertexScatterLODLevel::SVertexScatterLODLevel()
{
	m_nNumVertices = 0;
	m_VertexScatterValues = NULL;
	m_WorldSpacePositions = NULL;
}

SVertexScatterLODLevel::~SVertexScatterLODLevel()
{
	if (m_VertexScatterValues)
	{
		assert(m_nNumVertices);
		delete[] m_VertexScatterValues;
	}
	if (m_WorldSpacePositions)
	{
		assert(m_nNumVertices);
		delete[] m_WorldSpacePositions;
	}
}

SVertexScatterSubObject::SVertexScatterSubObject()
{
	for (int i=0; i<MAX_VERTEX_SCATTER_LODS_NUM; ++i)
		m_VertexScatterLODLevels[i] = NULL;
}


SVertexScatterSubObject::~SVertexScatterSubObject()
{
	for (int i=0; i<MAX_VERTEX_SCATTER_LODS_NUM; ++i)
		if (m_VertexScatterLODLevels[i])
			delete m_VertexScatterLODLevels[i];
}

SScatteringObjectData::SScatteringObjectData()
{
	m_VertexScatterTransformZ = Vec4(0.0f, 0.0f, 1.0f, 0.0f);
	m_VertexScatterSubObjects = NULL;
	m_nNumSubObjects = 0;
}

SScatteringObjectData::~SScatteringObjectData()
{
	if (m_VertexScatterSubObjects)
		delete[] m_VertexScatterSubObjects;
}

inline static void AppendScatterData(void*& dest, const void* src, int size)
{
	memcpy(dest, src, size);
	BINARY(dest) += size;
}

uint CVertexScatterSerializationManager::SaveVertexScatter(const char* pszFileName) const
{
	string strDirName = PathUtil::GetParentDirectory(pszFileName);
	string strPakName = strDirName + "\\" LEVELVS_PAK_NAME;
	GetPak()->ClosePack(strPakName.c_str());
	// make sure the pak file in which this VS data resides is opened
	CrySetFileAttributes(strPakName.c_str(), FILE_ATTRIBUTE_NORMAL);
	ICryArchive_AutoPtr pPak = GetPak()->OpenArchive(strPakName.c_str(), ICryArchive::FLAGS_RELATIVE_PATHS_ONLY);
	if (!pPak)
		return NSAVE_RESULT::EPAK_FILE_OPEN_FAIL;

	// Determine file size
	int nFileSize = sizeof(SFileHeader);
	for (std::map<int32, SScatteringObjectData>::const_iterator itVertexScatterData = m_VertexScatterData.begin(); itVertexScatterData!=m_VertexScatterData.end(); ++itVertexScatterData)
	{
		nFileSize += sizeof(SVertexScatterObjectHeader);
		const SScatteringObjectData& scatteringobjectdata = itVertexScatterData->second;

		int numsubobjects;
		if (scatteringobjectdata.m_nNumSubObjects)
			numsubobjects = scatteringobjectdata.m_nNumSubObjects;
		else
			numsubobjects = 1;

		for (int i=0; i<numsubobjects; ++i)
		{
			const SVertexScatterSubObject& vsso = scatteringobjectdata.m_VertexScatterSubObjects[i];
			nFileSize += sizeof(SVertexScatterSubObjectHeader);
			for (int j=0; j<MAX_VERTEX_SCATTER_LODS_NUM; ++j)
			{
				if (vsso.m_VertexScatterLODLevels[j])
				{
					const SVertexScatterLODLevel& vsll = *vsso.m_VertexScatterLODLevels[j];
					nFileSize += sizeof(SVertexScatterLODLevelHeader) + sizeof(vsll.m_VertexScatterValues[0]) * vsll.m_nNumVertices;
				}
			}
		}
	}

	// serialize
	SFileHeader fileheader;
	fileheader.iNumScatterObjects = m_VertexScatterData.size();
	void* buffer = malloc(nFileSize);
	void* p = buffer;

	AppendScatterData(p, &fileheader, sizeof(SFileHeader));

	for (std::map<int32, SScatteringObjectData>::const_iterator itVertexScatterData = m_VertexScatterData.begin(); itVertexScatterData!=m_VertexScatterData.end(); ++itVertexScatterData)
	{
		SVertexScatterObjectHeader vsoheader;
		vsoheader.m_nId = itVertexScatterData->first;

		const SScatteringObjectData& scatteringobjectdata = itVertexScatterData->second;
		vsoheader.m_nNumSubObjects = scatteringobjectdata.m_nNumSubObjects;
		vsoheader.m_VertexScatterTransformZ = scatteringobjectdata.m_VertexScatterTransformZ;

		AppendScatterData(p, &vsoheader, sizeof(vsoheader));

		int numsubobjects;
		if (scatteringobjectdata.m_nNumSubObjects)
			numsubobjects = scatteringobjectdata.m_nNumSubObjects;
		else
			numsubobjects = 1;

		for (int i=0; i<numsubobjects; ++i)
		{
			SVertexScatterSubObjectHeader vssoheader;
			const SVertexScatterSubObject& vsso = scatteringobjectdata.m_VertexScatterSubObjects[i];

			int lodcount = 0;
			for (int j=0; j<MAX_VERTEX_SCATTER_LODS_NUM; ++j)
				if (vsso.m_VertexScatterLODLevels[j])
					++lodcount;

			vssoheader.m_nLODLevels = lodcount;
			AppendScatterData(p, &vssoheader, sizeof(vssoheader));

			for (int j=0; j<MAX_VERTEX_SCATTER_LODS_NUM; ++j)
			{
				if (vsso.m_VertexScatterLODLevels[j])
				{
					SVertexScatterLODLevelHeader vsllheader;
					const SVertexScatterLODLevel& vsll = *vsso.m_VertexScatterLODLevels[j];
					vsllheader.m_nLODLevel = j;
					vsllheader.m_nNumVertices = vsll.m_nNumVertices;

					AppendScatterData(p, &vsllheader, sizeof(vsllheader));
					AppendScatterData(p, &vsll.m_VertexScatterValues[0], sizeof(vsll.m_VertexScatterValues[0])*vsll.m_nNumVertices);
				}
			}
		}
	}
	pPak->UpdateFile(VS_EXPORT_FILE_NAME, buffer, nFileSize, ICryArchive::METHOD_COMPRESS, ICryArchive::LEVEL_BEST);
	GetPak()->ClosePack(strPakName.c_str());
	free(buffer);

	return NSAVE_RESULT::ESUCCESS;
}

static void InitializeLODSubObject(struct IStatObj* pIStatObj, SVertexScatterSubObject& scatteringsubobject)
{
	for (int nLod=0; nLod<MAX_VERTEX_SCATTER_LODS_NUM; ++nLod)
	{
		IStatObj* lodobject = pIStatObj->GetLodObject(nLod);
		if (NULL == lodobject)
			continue;

		IRenderMesh* pRenderMesh = lodobject->GetRenderMesh();
		if(!pRenderMesh)
			continue;

		int nVertexCount = pRenderMesh->GetVertCount();
		assert(nVertexCount);
		scatteringsubobject.m_VertexScatterLODLevels[nLod] = new SVertexScatterLODLevel;
		SVertexScatterLODLevel& vsll = *scatteringsubobject.m_VertexScatterLODLevels[nLod];
		vsll.m_nNumVertices = nVertexCount;
		vsll.m_VertexScatterValues = new float[nVertexCount];

		for (int i=0; i<nVertexCount; ++i)
			vsll.m_VertexScatterValues[i] = 1.0f;
	}
}

void CVertexScatterSerializationManager::InitializeScattering(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber)
{
	for (int i=0; i<nIRenderNodesNumber; ++i)
	{
		// Tbyte TODO: Editor independent ID (Entity system???)
		int nId = pIRenderNodes[i]->GetEditorObjectId();

		SScatteringObjectData& scatteringobjectdata = m_VertexScatterData[nId];
		assert(NULL == scatteringobjectdata.m_VertexScatterSubObjects);
		scatteringobjectdata.m_VertexScatterTransformZ = Vec4(0.0f, 0.0f, 1.0f, 0.0f);

		IStatObj* pIStatObj = pIRenderNodes[i]->GetEntityStatObj(0);
		assert(pIStatObj);
		int subobjectcount = scatteringobjectdata.m_nNumSubObjects = pIStatObj->GetSubObjectCount();

		if (pIStatObj->GetRenderMesh())
		{
			assert(0 == subobjectcount || 1 == subobjectcount);
			scatteringobjectdata.m_VertexScatterSubObjects = new SVertexScatterSubObject[1];
			InitializeLODSubObject(pIStatObj, scatteringobjectdata.m_VertexScatterSubObjects[0]);
		}
		else
		{
			scatteringobjectdata.m_VertexScatterSubObjects = new SVertexScatterSubObject[subobjectcount];
			for (int j=0; j<subobjectcount; ++j)
			{
				IStatObj::SSubObject* pSubObject = pIStatObj->GetSubObject(j);
				if(pSubObject->nType!=STATIC_SUB_OBJECT_MESH || !pSubObject->pStatObj)
					continue;
				InitializeLODSubObject(pSubObject->pStatObj, scatteringobjectdata.m_VertexScatterSubObjects[j]);
			}
		}
	}
}

bool CVertexScatterSerializationManager::LoadVertexScatteringFile(const char* pszFileName)
{
	assert(0 == m_VertexScatterData.size());

	string strDirName = PathUtil::GetParentDirectory(pszFileName);
	string VertexScatterFilename = (strDirName + "\\" LEVELVS_PAK_NAME);
	{
		// Check if vertex scattering file exist.
		_finddata_t fd;
		intptr_t handle = GetPak()->FindFirst(VertexScatterFilename, &fd);

		if (-1 == handle)
			return false;
		GetPak()->FindClose(handle);
	}

	// ---------------------------------------------------------------------------------------------
	// Reconstruct vertex scattering data from pszFileName
	// ---------------------------------------------------------------------------------------------
	PrintMessage("Loading vertex scatter ...");

	// make sure the pak file in which this vertex scattering data resides is opened
	GetPak()->OpenPack(VertexScatterFilename.c_str());

	FILE* hFile = GetPak()->FOpen(pszFileName, "rb");
	if (hFile == NULL)
	{
		PrintMessage("Could not load vertex scatter file");
		return false;
	}
	AutoFileCloser _AutoClose (hFile);

	SFileHeader fileheader;
	int iNumItemsRead = GetPak()->FRead(&fileheader, 1, hFile);

	if (SFileHeader::MAGIC_NUMBER != fileheader.iMagicNumber)
		return false;

	if (0 == fileheader.iNumScatterObjects)
	{
		PrintMessage("Vertex scatter file corrupted, unsupported file format");
		return false;
	}

	for (int i=0; i<fileheader.iNumScatterObjects; ++i)
	{
		SVertexScatterObjectHeader vsoheader;
		iNumItemsRead = GetPak()->FRead(&vsoheader, 1, hFile);
		if (SVertexScatterObjectHeader::MAGIC_NUMBER != vsoheader.m_nMagicNumber)
		{
			PrintMessage("Vertex scatter file corrupted, invalid object");
			return false;
		}

		int numsubobjects;
		if (vsoheader.m_nNumSubObjects)
			numsubobjects = vsoheader.m_nNumSubObjects;
		else
			numsubobjects = 1;

		SScatteringObjectData& scatteringobjectdata = m_VertexScatterData[vsoheader.m_nId];
		scatteringobjectdata.m_VertexScatterTransformZ = vsoheader.m_VertexScatterTransformZ;
		scatteringobjectdata.m_nNumSubObjects = vsoheader.m_nNumSubObjects;
		assert(NULL == scatteringobjectdata.m_VertexScatterSubObjects);
		scatteringobjectdata.m_VertexScatterSubObjects = new SVertexScatterSubObject[numsubobjects];

		for (int j=0; j<numsubobjects; ++j)
		{
			SVertexScatterSubObjectHeader vssoheader;
			iNumItemsRead = GetPak()->FRead(&vssoheader, 1, hFile);
			if (SVertexScatterSubObjectHeader::MAGIC_NUMBER != vssoheader.m_nMagicNumber)
			{
				PrintMessage("Vertex scatter file corrupted, invalid sub-object");
				return false;
			}

			SVertexScatterSubObject& vsso = scatteringobjectdata.m_VertexScatterSubObjects[j];
			for (int k=0; k<vssoheader.m_nLODLevels; ++k)
			{
				SVertexScatterLODLevelHeader vsllheader;
				iNumItemsRead = GetPak()->FRead(&vsllheader, 1, hFile);
				if (SVertexScatterLODLevelHeader::MAGIC_NUMBER != vsllheader.m_nMagicNumber)
				{
					PrintMessage("Vertex scatter file corrupted, invalid LOD level");
					return false;
				}

				vsso.m_VertexScatterLODLevels[vsllheader.m_nLODLevel] = new SVertexScatterLODLevel;
				SVertexScatterLODLevel& vsll = *vsso.m_VertexScatterLODLevels[vsllheader.m_nLODLevel];

				vsll.m_nNumVertices = vsllheader.m_nNumVertices;
				vsll.m_VertexScatterValues = new float[vsll.m_nNumVertices];
				iNumItemsRead = GetPak()->FRead(vsll.m_VertexScatterValues, vsll.m_nNumVertices, hFile);
			}
		}
	}
	return true;
}

bool CVertexScatterSerializationManager::ApplyVertexScatterFile(const char* pszFileName, struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber)
{
	if (!LoadVertexScatteringFile(pszFileName))
	{
		m_VertexScatterData.clear();
		InitializeScattering(pIRenderNodes, nIRenderNodesNumber);
	}

	for (int i=0; i<nIRenderNodesNumber; ++i)
	{
		std::map<int32, SScatteringObjectData>::const_iterator it = m_VertexScatterData.find(pIRenderNodes[i]->GetEditorObjectId());
		if (it != m_VertexScatterData.end())
			pIRenderNodes[i]->SetVertexScattering(it->second);
	}
	return true;
}

void CVertexScatterSerializationManager::CollectSubObjectDataForVertexScatterGeneration(struct IStatObj* pIStatObj, struct SVertexScatterSubObject& scatteringsubobject, const Matrix34& worldmatrix, float fRefractionAmount)
{
	for (int nLod=0; nLod<MAX_VERTEX_SCATTER_LODS_NUM; ++nLod)
	{
		IStatObj* lodobject = pIStatObj->GetLodObject(nLod);
		if (NULL == lodobject)
			continue;

		IRenderMesh* pRenderMesh = lodobject->GetRenderMesh();
		if(!pRenderMesh)
			continue;

		int nVertexCount = pRenderMesh->GetVertCount();
		assert(nVertexCount);
		scatteringsubobject.m_VertexScatterLODLevels[nLod] = new SVertexScatterLODLevel;
		SVertexScatterLODLevel& vsso = *scatteringsubobject.m_VertexScatterLODLevels[nLod];
		vsso.m_nNumVertices = nVertexCount;

		int nStridePosition;
		int nStrideTangent;
		int nStrideBinormal;
		byte* pVertexPositions = pRenderMesh->GetStridedPosPtr(nStridePosition);
		byte* pVertexTangents = pRenderMesh->GetStridedTangentPtr(nStrideTangent);
		byte* pVertexBinormals = pRenderMesh->GetStridedBinormalPtr(nStrideBinormal);

		vsso.m_VertexScatterValues = new float[nVertexCount];
		vsso.m_WorldSpacePositions = new Vec3[nVertexCount];
		if (fRefractionAmount && pVertexTangents && pVertexBinormals)
		{
			for (int j=0; j<nVertexCount; ++j)
			{
				Vec3 tangent, binormal;
				tangent.x = tPackB2F(reinterpret_cast<int16f*>(pVertexTangents + j*nStrideTangent)[0]);
				tangent.y = tPackB2F(reinterpret_cast<int16f*>(pVertexTangents + j*nStrideTangent)[1]);
				tangent.z = tPackB2F(reinterpret_cast<int16f*>(pVertexTangents + j*nStrideTangent)[2]);
				binormal.x = tPackB2F(reinterpret_cast<int16f*>(pVertexBinormals + j*nStrideBinormal)[0]);
				binormal.y = tPackB2F(reinterpret_cast<int16f*>(pVertexBinormals + j*nStrideBinormal)[1]);
				binormal.z = tPackB2F(reinterpret_cast<int16f*>(pVertexBinormals + j*nStrideBinormal)[2]);
				float orientation = tPackB2F(reinterpret_cast<int16f*>(pVertexTangents + j*nStrideTangent)[3]);
				vsso.m_WorldSpacePositions[j] = worldmatrix.TransformPoint(*reinterpret_cast<Vec3*>(pVertexPositions + j*nStridePosition) - fRefractionAmount*orientation*(tangent.Cross(binormal).GetNormalized()));
			}
		}
		else
			for (int j=0; j<nVertexCount; ++j)
				vsso.m_WorldSpacePositions[j] = worldmatrix.TransformPoint(*reinterpret_cast<Vec3*>(pVertexPositions + j*nStridePosition));
	}
}

//#define FORCE_REFRACTIONAMOUNT_AMOUNT 0.0f

void CVertexScatterSerializationManager::CollectDataForVertexScatterGeneration(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber)
{
	Matrix34 worldmatrix;

	//SObjectForVertexScatterGeneration* objects_for_vertexscattergeneration = new SObjectForVertexScatterGeneration[nIRenderNodesNumber];
	//GetRenderer()->SetDataForVertexScatterGeneration(objects_for_vertexscattergeneration, nIRenderNodesNumber);

	for (int i=0; i<nIRenderNodesNumber; ++i)
	{
		int nId = pIRenderNodes[i]->GetEditorObjectId();
		SScatteringObjectData& scatteringobjectdata = m_VertexScatterData[nId];

		IStatObj* pIStatObj = pIRenderNodes[i]->GetEntityStatObj(0, 0, &worldmatrix);
		int subobjectcount = pIStatObj->GetSubObjectCount();

#ifdef FORCE_REFRACTIONAMOUNT_AMOUNT
		float fRefractionAmount = FORCE_REFRACTIONAMOUNT_AMOUNT;
#else
		IMaterial *pMat = pIRenderNodes[i]->GetMaterial();
		if (!pMat)
			pMat = pIStatObj->GetMaterial();
		assert(pMat);
		float fRefractionAmount = pMat ? pMat->GetPSRefractionAmount() : 0.0f;
#endif

		if (pIStatObj->GetRenderMesh())
		{
			assert(0 == subobjectcount || 1 == subobjectcount);
			scatteringobjectdata.m_nNumSubObjects = 1;
			scatteringobjectdata.m_VertexScatterSubObjects = new SVertexScatterSubObject[1];
			CollectSubObjectDataForVertexScatterGeneration(pIStatObj, scatteringobjectdata.m_VertexScatterSubObjects[0], worldmatrix, fRefractionAmount);
		}
		else
		{
			scatteringobjectdata.m_nNumSubObjects = subobjectcount;
			scatteringobjectdata.m_VertexScatterSubObjects = new SVertexScatterSubObject[subobjectcount];

			for (int j=0; j<subobjectcount; ++j)
			{
				IStatObj::SSubObject* pSubObject = pIStatObj->GetSubObject(j);
				if(pSubObject->nType!=STATIC_SUB_OBJECT_MESH || !pSubObject->pStatObj)
					continue;
				Matrix34 worldmatrix_subobject = worldmatrix * pSubObject->tm;
				CollectSubObjectDataForVertexScatterGeneration(pSubObject->pStatObj, scatteringobjectdata.m_VertexScatterSubObjects[j], worldmatrix_subobject, fRefractionAmount);
			}
		}
	}
}

void CVertexScatterSerializationManager::CollectObjectArrayForVertexScatterGeneration(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber)
{
	IRenderer* pIRenderer = GetRenderer();
	assert(NULL == m_ObjectArrayForVertexScatterGeneration);
	m_ObjectArrayForVertexScatterGeneration = new SScatteringObjectData*[nIRenderNodesNumber];
	for (int i=0; i<nIRenderNodesNumber; ++i)
	{
		std::map<int32, SScatteringObjectData>::iterator it = m_VertexScatterData.find(pIRenderNodes[i]->GetEditorObjectId());
		assert(it != m_VertexScatterData.end());
		m_ObjectArrayForVertexScatterGeneration[i] = &it->second;
	}
	GetRenderer()->SetDataForVertexScatterGeneration(m_ObjectArrayForVertexScatterGeneration, nIRenderNodesNumber);
}

void CVertexScatterSerializationManager::DeleteObjectArray()
{
	assert(NULL != m_ObjectArrayForVertexScatterGeneration);
	GetRenderer()->SetDataForVertexScatterGeneration(NULL, 0);
	delete[] m_ObjectArrayForVertexScatterGeneration;
	m_ObjectArrayForVertexScatterGeneration = NULL;
}

void CVertexScatterSerializationManager::SetVertexScattering(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber)
{
	for (int i=0; i<nIRenderNodesNumber; ++i)
	{
		std::map<int32, SScatteringObjectData>::const_iterator it = m_VertexScatterData.find(pIRenderNodes[i]->GetEditorObjectId());
		assert(it != m_VertexScatterData.end());
		pIRenderNodes[i]->SetVertexScattering(it->second);
	}
}
