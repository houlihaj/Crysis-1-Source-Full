#ifndef __LM_SERIALIZATION_MANAGER_2_H__
#define __LM_SERIALIZATION_MANAGER_2_H__

#include <ILMSerializationManager.h>
#include "LMCompStructures.h"
#include "I3DEngine.h"

#include <map>

#ifdef WIN64
#pragma warning( push )									//AMD Port
#pragma warning( disable : 4267 )
#endif

class CLMTextureToObjID
{
	int32												m_ID;
	int32												m_SubObjIdx;
public:
														CLMTextureToObjID():
														m_ID(-1),
														m_SubObjIdx(-1)
														{
														}
														CLMTextureToObjID(const int32 ID,const int32 SubObjIdx):
														m_ID(ID),
														m_SubObjIdx(SubObjIdx)
														{
														}

	int32											ID()const{return m_ID;}
	int32											SubObjIdx()const{return m_SubObjIdx;}
	const CLMTextureToObjID&	operator=(const CLMTextureToObjID& rObj){assert(rObj.m_ID!=-1);assert(rObj.m_SubObjIdx!=-1);m_ID=rObj.m_ID;m_SubObjIdx=rObj.m_SubObjIdx;return *this;}
	const CLMTextureToObjID&	operator=(const std::pair<int32,int32>& rObj){m_ID=rObj.first;m_SubObjIdx=rObj.second;return *this;}

	bool											operator==(const CLMTextureToObjID& rObj)const{return m_ID==rObj.m_ID&&m_SubObjIdx==rObj.m_SubObjIdx;}
	bool											operator==(const std::pair<int32,int32>& rObj)const{return m_ID==rObj.first&&m_SubObjIdx==rObj.second;}
	bool											operator<(const CLMTextureToObjID& rObj)const{return m_ID<rObj.m_ID	||	(m_ID==rObj.m_ID&&m_SubObjIdx<rObj.m_SubObjIdx);}
	bool											operator<(const std::pair<int32,int32>& rObj)const{return m_ID<rObj.first	||	(m_ID==rObj.first&&m_SubObjIdx<rObj.second);}

  AUTO_STRUCT_INFO
};


class CTempFile
{
public:
	CTempFile(unsigned nReserve = 0)
	{
		m_arrData.reserve (nReserve);
	}

	void WriteData (const void* pData, unsigned nSize)
	{
		if (nSize)
		{
			unsigned nOffset = m_arrData.size();
			m_arrData.resize(nOffset + nSize);
			memcpy (&m_arrData[nOffset], pData, nSize);
		}
	}

	template <typename T>
	void Write (const T& t)
	{
		WriteData (&t, sizeof(T));
	}

	template <typename T>
	void Write (const T* pT, unsigned numElements)
	{
		WriteData (pT, numElements*sizeof(T));
	}

	unsigned GetSize() const {return m_arrData.size();}
	void SetSize (unsigned n){m_arrData.resize (n);}
	char* GetData() {return &m_arrData[0];}

	void Clear() {m_arrData.clear();}
	void Reserve (unsigned n) {m_arrData.reserve (n);}
	void Init (unsigned nSize)
	{
		m_arrData.clear();
		m_arrData.resize (nSize);
	}
protected:
	std::vector<char> m_arrData;
};

#ifdef WIN64
#pragma warning( pop )									//AMD Port
#endif

class CLMSerializationManager : public Cry3DEngineBase, public ILMSerializationManager
{	
public:

	CLMSerializationManager( );
	virtual ~CLMSerializationManager();

	// interface ILMSerializationManager ------------------------------------

	virtual void Release() { delete this; };
	virtual bool ApplyLightmapfile( const char *pszFileName, struct IRenderNode ** pIGLMs, int32 nIGLMNumber );
	virtual bool Load(const char *pszFileName, const bool cbLoadTextures = true);
	virtual bool LoadFalseLight(const char *pszFileName, struct IRenderNode ** pIGLMs, int32 nIGLMNumber);
	virtual unsigned int SaveFalseLight(const char *pszFileName, const int nObjectNumber,const struct sFalseLightSerializationStructure* pFalseLightList);
	virtual unsigned int Save(const char *pszFileName, LMGenParam rParam, const bool cbAppend = false);
	virtual unsigned int InitLMUpdate(const char *pszFilePath, const bool bAppend );
	virtual RenderLMData * UpdateLMData( 
		const uint32 indwWidth, const uint32 indwHeight, const std::pair<int32,int32>* pGLM_IDs, const int32 nGLM_IDNumber,
		BYTE *_pColorLerp4, BYTE *_pHDRColorLerp4, BYTE *_pDomDirection3, BYTE *_pOcclusion, BYTE *_pRAE, const char* strDirPath, int32* pTextureID );
	virtual void AddTexCoordData(const struct TexCoord2Comp* pTexCoords, const int32 nTexCoordNumber, const int iGLM_ID_UsingTexCoord,const int iGLM_Idx_SubObj, const uint32 indwHashValue, const EntityId* pOcclIDsFirst,const EntityId* pOcclIDsSecond, const int32 nOcclIDNumber, const int8 nFirstOcclusionChannel);
	virtual void RescaleTexCoordData(const int iGLM_ID_UsingTexCoord,const int iGLM_Idx_SubObj, const f32 fScaleU, const f32 fScaleV );
	virtual uint32 GetHashValue(const std::pair<int32,int32> iniGLM_ID_UsingTexCoord) const;
	virtual bool ExportDLights(const char *pszFileName, const CDLight **ppLights, UINT iNumLights, bool bNewZip = true) const;
	virtual bool LoadDLights(const char *pszFileName, CDLight **&ppLightsOut, UINT &iNumLightsOut) const;
	RenderLMData * CreateLightmap(const string& strDirPath, int nItem,UINT iWidth, UINT iHeight, const bool cbLoadHDRMaps, const bool cbLoadOcclMaps = false);
// ----------------------------------------------
protected:
	struct FileHeader
	{
		enum {MAGIC_NUMBER  = 0x8F23123E};
		enum {VERSION = 8};
		FileHeader() { iMagicNumber = MAGIC_NUMBER;iVersion = VERSION; };

		UINT iMagicNumber;
		UINT iVersion;
		UINT iNumLM_Pairs;
		UINT iNumTexCoordSets;

		UINT reserved[4];
		AUTO_STRUCT_INFO
	};

	struct LMHeader
	{
		// the dimensions of LM
		UINT iWidth;
		UINT iHeight;
		// number of GLMs using this LM
		UINT numGLMs; 
		AUTO_STRUCT_INFO
	};

	struct UVSetHeader
	{
		UINT nIdGLM;
		UINT nHashGLM;
		UINT numUVs;
		UVSetHeader() : nIdGLM(0), nHashGLM(0), numUVs(0){}
		AUTO_STRUCT_INFO
	};
	
	//new version available from ver 3 on
	struct UVSetHeader3 : UVSetHeader
	{
		EntityId OcclIds[4*2];	//new: the occlusion map colour channel light id's, this corresponds to the std::pair<EntityId, EntityId>
		unsigned char ucOcclCount/*1..4*/;
		UVSetHeader3():ucOcclCount(0)
		{
			UVSetHeader::UVSetHeader();
			OcclIds[0] = OcclIds[1] = OcclIds[2] = OcclIds[3] = 0;
		}
		AUTO_STRUCT_INFO
	};

	//new version available from ver 4 on
	struct UVSetHeader4 : UVSetHeader3
	{
		EntityId OcclIds[16*2];				//new: the Occlusion map colour channel light id's, this corresponds to the std::pair<EntityId, EntityId>
		unsigned char ucOcclCount/*1..16*/;
		UVSetHeader4():ucOcclCount(0)
		{
			UVSetHeader3::UVSetHeader3();
		}
		AUTO_STRUCT_INFO
	};

	//new version available from ver 5 on
	struct UVSetHeader5 : UVSetHeader3
	{
		struct sOcclID
		{
			EntityGUID	m_nGuid;
			EntityId	m_nLightId;
  		AUTO_STRUCT_INFO
		};
		sOcclID OcclIds[16];				//new: the Ambient Occlusion map colour channel light id's, this corresponds to the std::pair<EntityId, EntityId>
		unsigned char ucOcclCount/*1..16*/;
		UVSetHeader5():ucOcclCount(0)
		{
			UVSetHeader3::UVSetHeader3();
		}
		AUTO_STRUCT_INFO
	};

	//new version available from ver 6 on
	struct UVSetHeader6 : UVSetHeader5
	{
		int8 m_nFirstOcclusionChannel;			/*1..16*/
		UVSetHeader6():m_nFirstOcclusionChannel(0)
		{
			UVSetHeader5::UVSetHeader5();
		}
		AUTO_STRUCT_INFO
	};

	//I dunno what happend to version 7, but the Version ID is already set to 7
	struct UVSetHeader8 : UVSetHeader6
	{
		int8 m_nSubObjIdx;			/*1..16*/
		UVSetHeader8():
		UVSetHeader6(),
		m_nSubObjIdx(0)
		{
		}
		AUTO_STRUCT_INFO
	};
	struct RawLMData
	{
		//! /param _pColorLerp4 if !=0 this memory is copied
		//! /param _pDomDirection3 if !=0 this memory is copied
		template<class T>
		RawLMData(const uint32 indwWidth, const uint32 indwHeight, const T& _vGLM_IDs_UsingPatch )
		{
//			vGLM_IDs_UsingPatch = _vGLM_IDs_UsingPatch;
			const uint	Size	=	_vGLM_IDs_UsingPatch.size();
			vGLM_IDs_UsingPatch.resize(Size);
			for(uint a=0;a<Size;a++)
				vGLM_IDs_UsingPatch[a]	=	_vGLM_IDs_UsingPatch[a];

			m_dwWidth=indwWidth;
			m_dwHeight=indwHeight;
			m_bUseLightMaps =
			m_bUseHDRMaps =
			m_bUseDot3Maps =
			m_bUseOcclusionMaps =
			m_bUseRAE = false;
		};

		enum BitmapEnum
		{
			TEX_COLOR,
			TEX_DOMDIR,
			TEX_HDR,
			TEX_IRRAD_3VEC,
			TEX_IRRAD_4VEC,
			TEX_OCCLUSION,
			TEX_RAE
		};

		// initializes from raw bitmaps
		void initFromBMP (BitmapEnum t, const void* pSource);

		// initializes from files
		bool initFromDDS (BitmapEnum t, ICryPak* pPak, const string& szFileName);


		std::vector<CLMTextureToObjID >						vGLM_IDs_UsingPatch;				//!< vector of object ids that use this lightmap

		// the color DDS, as is in the file
		CTempFile m_ColorLerp4;
		// the color DDS, as is in the file
		CTempFile m_HDRColorLerp4;
		// the dominant direction DDS, as is in the file
		CTempFile m_DomDirection3;
		// the occlusion map DDS, as is in the file
		CTempFile m_Occlusion;
		// the RAE reflection map DDS, as is in the file
		CTempFile m_RAE;

		uint32										  m_dwWidth;									//!<
	  uint32											m_dwHeight;									//!<
		bool												m_bUseLightMaps;
		bool												m_bUseHDRMaps;
		bool												m_bUseDot3Maps;
		bool												m_bUseOcclusionMaps;
		bool												m_bUseRAE;

	private:

		//! copy constructor (forbidden)
		RawLMData( const RawLMData &a )	{}

		//! assignment operator (forbidden)
		RawLMData &operator=( const RawLMData &a ) {	return(*this); }
	};

	struct RawTexCoordData
	{
		//! default constructor (needed for std::map)
		RawTexCoordData()	{}

		RawTexCoordData( const std::vector<TexCoord2Comp>& _vTexCoords, const uint32 indwHashValue, const std::vector<std::pair<EntityId, EntityId> >& rOcclIDs, const int8 nFirstOcclusionChannel )
		{
			vTexCoords = _vTexCoords;
			m_dwHashValue=indwHashValue;
			vOcclIDs = rOcclIDs;
			m_nFirstOcclusionChannel = nFirstOcclusionChannel;
		};

		RawTexCoordData( const std::vector<TexCoord2Comp>& _vTexCoords, const uint32 indwHashValue )
		{
			vTexCoords = _vTexCoords;
			m_dwHashValue=indwHashValue;
			vOcclIDs.clear();
			m_nFirstOcclusionChannel = 0;
		};

		std::vector<TexCoord2Comp>									vTexCoords;									//!< the coordinates for lightmaps
		uint32																			m_dwHashValue;							//!< to detect changes in the lighting (for incremental recompile)
		std::vector<std::pair<EntityId, EntityId> >	vOcclIDs;										//!< specular occlusion indices corresponding to the 0..4 colour channels
		int8																				m_nFirstOcclusionChannel;			///< the first occupied channel in occulsion map
	};

	std::vector<RawLMData *>			m_vLightPatches;							//!< class is responsible for deleteing this 
	std::map<CLMTextureToObjID,RawTexCoordData>		m_vTexCoords;								//!<

	// \param inpIGLMs pointer to the objects we want to assign the data(instance is not touched), 0 if we want to load it to the instance
	bool _Load( const char *pszFileName, struct IRenderNode ** pIGLMs, int32 nIGLMNumber, const bool cbLoadTextures = true);

	void WriteString(const char *pszStr, CTempFile& f) const
	{
		UINT iStrLen = (UINT) strlen(pszStr);
		f.Write (iStrLen);
		f.WriteData(pszStr, iStrLen);
	};

	// Generate DDS files from RawLMData
	unsigned int SaveDDSFiles(const char *pszFilePath, RawLMData *pCurRawLMData, const int32 nID );

};
#endif // __LM_SERIALIZATION_MANAGER_H__