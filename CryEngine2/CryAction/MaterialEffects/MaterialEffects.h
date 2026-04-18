////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   MaterialEffects.h
//  Version:     v1.00
//  Created:     28/11/2006 by JohnN/AlexL
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __MATERIALEFFECTS_H__
#define __MATERIALEFFECTS_H__

#pragma once

#include <map>
#include <ISystem.h>
#include <IMaterialEffects.h>
#include "IMFXEffect.h"
#include <SerializeFwd.h>

class CMaterialFGManager;

class CMaterialEffects : public IMaterialEffects
{
public:
	CMaterialEffects();
	virtual ~CMaterialEffects();

	// load spreadsheet and effect libraries
	bool Load(const char* filenameSpreadSheet);

	// load flowgraph based effects 
	bool LoadFlowGraphLibs();

	// serialize
	void Serialize(TSerialize ser);

	// load assets referenced by material effects.
	void PreLoadAssets();

	// IMaterialEffects
	virtual void Reset();
	virtual TMFXEffectId GetEffectIdByName(const char* libName, const char* effectName);
	virtual TMFXEffectId GetEffectId(int surfaceIndex1, int surfaceIndex2);
	virtual TMFXEffectId GetEffectId(const char* customName, int surfaceIndex2);
	virtual TMFXEffectId GetEffectId(IEntityClass* pEntityClass, int surfaceIndex2);
	virtual SMFXResourceListPtr GetResources(TMFXEffectId effectId);
	void NoDelayForMovingTrain(SMFXRunTimeEffectParams& params);
	virtual bool ExecuteEffect(TMFXEffectId effectId, SMFXRunTimeEffectParams& runtimeParams);
	virtual void StopEffect(TMFXEffectId effectId);
	virtual int	GetDefaultSurfaceIndex() { return m_defaultSurfaceId; }
	virtual int	GetDefaultCanopyIndex() { return m_canopySurfaceId; }
	virtual bool PlayBreakageEffect(ISurfaceType* pSurfaceType, const char* breakageType, const SMFXBreakageParams& breakageParams);
	// ~IMaterialEffects

	void GetMemoryStatistics(ICrySizer * s);
	void NotifyFGHudEffectEnd(IFlowGraphPtr pFG);
	void Update(float frameTime);
	void SetUpdateMode(bool bMode);
	CMaterialFGManager* GetFGManager() const { return m_pMaterialFGManager; }

protected:
	static const int MAX_SURFACE_COUNT = 255+1; // from SurfaceTypeManager.h, but will also work with any other number
	typedef void*   TPtrIndex;
	typedef int     TIndex;    // normal index 
	typedef std::pair<TIndex, TIndex> TIndexPair;
	typedef std::map<TMFXNameId, IMFXEffectPtr> TFXLibrary;

	struct SDelayedEffect 
	{
		SDelayedEffect() 
			: m_delay(0.0f)
		{

		}
		SDelayedEffect(IMFXEffectPtr pEffect, SMFXRunTimeEffectParams& runtimeParams)
			: m_pEffect(pEffect), m_effectRuntimeParams(runtimeParams), m_delay(m_pEffect->m_effectParams.delay)
		{
		}

		IMFXEffectPtr m_pEffect;
		SMFXRunTimeEffectParams m_effectRuntimeParams;
		float m_delay;
	};

	template <typename T> struct Array2D
	{
		void Create(size_t rows, size_t cols)
		{
			if (m_pData)
				delete[] m_pData;
			m_nRows = rows;
			m_nCols = cols;
			m_pData = new T[m_nRows*m_nCols];
			memset(m_pData, 0, m_nRows*m_nCols*sizeof(T));
		}

		Array2D() : m_pData(0), m_nRows(0), m_nCols(0) 
		{
		}

		~Array2D()
		{
			if (m_pData)
				delete[] m_pData;
		}

		T* operator[] (const size_t row) const
		{
			assert (m_pData);
			assert (row >= 0 && row < m_nRows);
			return &m_pData[row*m_nCols];
		}

		T& operator() (const size_t row, const size_t col) const
		{
			assert (m_pData);
			assert (row >= 0 && row < m_nRows && col >= 0 && col < m_nCols);
			return m_pData[row*m_nCols+col];
		}

		T GetValue(const size_t row, const size_t col, const T& defaultValue) const
		{
			return (row >= 0 && row < m_nRows && col >= 0 && col < m_nCols) ? m_pData[row*m_nCols+col] : defaultValue;
		}
		void GetMemoryStatistics(ICrySizer * s)
		{
			s->AddObject(m_pData, sizeof(T)*m_nRows*m_nCols);
		}

		T* m_pData;
		size_t m_nRows;
		size_t m_nCols;
	};

	// Refactor remains

	void Load();
	void LoadFXLibraries();
	void LoadFXLibrary(const char *name);

	// schedule effect
	void TimedEffect(IMFXEffectPtr effect, SMFXRunTimeEffectParams& params);

	// index1 x index2 are used to lookup in m_matmat array
	inline TMFXEffectId InternalGetEffectId(int index1, int index2) const
	{
		const TMFXEffectId effectId = m_matmatArray.GetValue(index1, index2, InvalidEffectId);
		return effectId;
	}

	inline IMFXEffectPtr InternalGetEffect(const char* libName, const char* effectName) const
	{
		std::map<TMFXNameId, TFXLibrary>::const_iterator iter = m_mfxLibraries.find(CONST_TEMP_STRING(libName));
		if (iter != m_mfxLibraries.end())
		{
			const TFXLibrary& library = iter->second;
			return stl::find_in_map(library, CONST_TEMP_STRING(effectName), 0);
		}
		return 0;
	}

	inline IMFXEffectPtr InternalGetEffect(TMFXEffectId effectId) const
	{
		assert (effectId < m_effectVec.size());
		if (effectId < m_effectVec.size())
			return m_effectVec[effectId];
		return 0;
	}

	inline TIndex SurfaceIdToMatrixEntry(int surfaceIndex)
	{
		return (surfaceIndex >= 0 && surfaceIndex < m_surfaceIdToMatrixEntry.size()) ? m_surfaceIdToMatrixEntry[surfaceIndex] : 0;
	}

protected:
	ISystem*            m_pSystem;
	string              m_filename;
	int                 m_defaultSurfaceId;
	int                 m_canopySurfaceId;
	bool                m_bUpdateMode;
	CMaterialFGManager* m_pMaterialFGManager;

	// the libraries we have loaded
	std::map<TMFXNameId, TFXLibrary> m_mfxLibraries;

	// this maps a surface type to the corresponding column(==row) in the matrix
	std::vector<TIndex> m_surfaceIdToMatrixEntry;

	// this maps custom pointers (entity classes
	std::map<TPtrIndex, TIndex> m_ptrToMatrixEntryMap;

	// converts custom tags to indices
	std::map<string, TIndex, stl::less_stricmp<string> > m_customConvert;

	// main lookup surface x surface -> effectId
	Array2D<TMFXEffectId> m_matmatArray;
	std::vector<IMFXEffectPtr> m_effectVec; // indexed by TEffectId

	// runtime effects which are delayed
	std::vector<SDelayedEffect> m_delayedEffects;
};

#endif
