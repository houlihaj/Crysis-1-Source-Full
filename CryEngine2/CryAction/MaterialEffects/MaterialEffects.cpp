////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   MaterialEffects.cpp
//  Version:     v1.00
//  Created:     28/11/2006 by JohnN/AlexL
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include <limits.h>
#include <CryPath.h>
#include "MaterialEffectsCVars.h"
#include "MaterialEffects.h"
#include "MFXEffect.h"
#include "MFXRandomEffect.h"
#include "MFXSoundEffect.h"
#include "MFXParticleEffect.h"
#include "MFXDecalEffect.h"
#include "MaterialFGManager.h"
#include "PoolAllocator.h"

#if defined(USER_alexll)
	#define MFX_CHECK_COLUMN_ROW_ORDER     // check if column and row order of Excel sheet match (matching is a MUST!, but #ifdefed for now, until Sean fixes Excel sheet)
	#define MFX_CHECK_IDENTITY_MISMATCH    // check if mat1->mat2 effect matches mat2->mat1 (no must)
#endif

#define IMPL_POOL_RETURNING(type, rtype) \
	static stl::PoolAllocatorNoMT<sizeof(type)> m_pool_##type; \
	rtype type::Create() \
	{ \
		return new (m_pool_##type.Allocate()) type(); \
	} \
	void type::Destroy() \
	{ \
		this->~type(); \
		m_pool_##type.Deallocate(this); \
	}
#define IMPL_POOL(type) IMPL_POOL_RETURNING(type, type*)

IMPL_POOL_RETURNING(SMFXResourceList, SMFXResourceListPtr);
IMPL_POOL(SMFXFlowGraphListNode);
IMPL_POOL(SMFXDecalListNode);
IMPL_POOL(SMFXParticleListNode);
IMPL_POOL(SMFXSoundListNode);


CMaterialEffects::CMaterialEffects()
{
	m_pSystem = gEnv->pSystem;
	m_bUpdateMode = false;
	m_defaultSurfaceId = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceTypeByName("mat_default")->GetId();
	m_canopySurfaceId = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceTypeByName("mat_canopy")->GetId();
	m_pMaterialFGManager = new CMaterialFGManager();
}

CMaterialEffects::~CMaterialEffects()
{
	// smart pointers will automatically release.
	// the clears will immediately delete all effects while CMaterialEffects is still exisiting
	m_mfxLibraries.clear();
	m_delayedEffects.clear();
	m_effectVec.clear();
	SAFE_DELETE(m_pMaterialFGManager);
}

void CMaterialEffects::LoadFXLibraries()
{
	ICryPak *pak = m_pSystem->GetIPak();
	_finddata_t fd;

	intptr_t handle = pak->FindFirst("Libs/MaterialEffects/FXLibs/*.xml", &fd);
	int res = 0;
	if (handle != -1)
	{
		do 
		{
			LoadFXLibrary(fd.name);
			res = pak->FindNext(handle, &fd);
		} while(res >= 0);
		pak->FindClose(handle);
	}
}

void CMaterialEffects::LoadFXLibrary(const char *name)
{
	string path = PathUtil::Make("Libs/MaterialEffects/FXLibs", name);
	string fileName = name;
	
	int period = fileName.find(".");
	string libName = fileName.substr(0, period);

	XmlNodeRef fxlib = m_pSystem->LoadXmlFile(path);
	if (fxlib != 0)
  {
		// we can store plain ptrs here, because both CMFXEffect* and const char* are still valid
		typedef std::pair<CMFXEffect*, const char*> ChildEffectParentNamePair;
		std::vector<ChildEffectParentNamePair> delayedChildEffects;
		
		// create library
		std::map<TMFXNameId, TFXLibrary>::iterator libIter = m_mfxLibraries.find(libName);
		if (libIter == m_mfxLibraries.end())
		{
			std::pair<std::map<TMFXNameId, TFXLibrary>::iterator, bool> iterPair = 
				m_mfxLibraries.insert(std::map<TMFXNameId, TFXLibrary>::value_type(libName, TFXLibrary()));
			assert (iterPair.second == true);
			libIter = iterPair.first;
			assert (libIter != m_mfxLibraries.end());
		}
		const TMFXNameId& libNameId = libIter->first; // uses CryString's ref-count feature
		TFXLibrary& library = libIter->second;

    for (int i = 0; i < fxlib->getChildCount(); ++i)
    {
			XmlNodeRef curEffectNode = fxlib->getChild(i);
			if (!curEffectNode)
				continue;

			// create base effect
			CMFXEffect *pEffect = new CMFXEffect();

			// build from XML description
      pEffect->Build(curEffectNode);

			// assign library name to effect (mosty for debugging)
			// uses CryString's ref-count feature
			pEffect->m_effectParams.libName = libNameId;

			// get effect name
			const TMFXNameId& effectName = pEffect->m_effectParams.name;

			// check if effect is derived
      const char *parentName = curEffectNode->getAttr("parent");
      if (parentName && *parentName)
      {
				IMFXEffectPtr pParentEffect = stl::find_in_map(library, parentName, 0);
				// does the parent effect already exist?
        if (pParentEffect)
				{
          pEffect->MakeDerivative(pParentEffect);
				}
				else // does not exist, so mark up for later
				{
					// CryCryComment("[MFX] Parent effect '%s:%s' for Effect '%s:%s' not yet defined.", libName.c_str(), parentName, libName.c_str(), effectName.c_str());
					delayedChildEffects.push_back(ChildEffectParentNamePair(pEffect, parentName));
				}
      }
			TFXLibrary::iterator found = library.find(effectName);
			if (found == library.end())
			{
				pEffect->m_effectParams.effectId = m_effectVec.size();	
				library.insert (TFXLibrary::value_type(effectName, pEffect));
				m_effectVec.push_back(pEffect);
			}
			else
			{
				GameWarning("[MFX] Effect '%s:%s' already present. Overriding.", libName.c_str(), effectName.c_str());
				pEffect->m_effectParams.effectId = found->second->m_effectParams.effectId;
	      found->second = pEffect;
				assert (pEffect->m_effectParams.effectId < m_effectVec.size());
				m_effectVec[pEffect->m_effectParams.effectId] = pEffect;
			}
    }
		
		// now derive delayed effects from their parents
		std::vector<ChildEffectParentNamePair>::iterator iter = delayedChildEffects.begin();
		std::vector<ChildEffectParentNamePair>::iterator iterEnd = delayedChildEffects.end();
		while (iter != iterEnd)
		{
			CMFXEffect* pEffect = iter->first;
			const char* parentName = iter->second;
			IMFXEffectPtr pParentEffect = stl::find_in_map(library, parentName, 0);
			if (pParentEffect)
				pEffect->MakeDerivative(pParentEffect);
			else
				GameWarning("[MFX] Parent effect '%s:%s' for Effect '%s:%s' does not exist!", libName.c_str(), parentName, libName.c_str(), pEffect->m_effectParams.name.c_str());
			++iter;
		}
  }
  else
  {
    CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "[MatFX]: Failed to load library %s", path.c_str());
  }
 
}

void CMaterialEffects::NoDelayForMovingTrain(SMFXRunTimeEffectParams& params)
{
	if(params.src && params.trg)
	{
		IEntity *pEntTrg = gEnv->pEntitySystem->GetEntity(params.trg);
		if(pEntTrg && !strnicmp("train", pEntTrg->GetName(),5))
		{
			IEntity *pEntSrc = gEnv->pEntitySystem->GetEntity(params.src);
			if(pEntSrc && !strcmpi(pEntSrc->GetName(), "ammo"))
			{				
				IPhysicalEntity *pPE=pEntTrg->GetPhysics();
				if(pPE)
				{
					pe_status_dynamics sv;
					pPE->GetStatus(&sv);
					if(sv.v.GetLengthSquared()>0.01f)
						params.playflags |= MFX_DISABLE_DELAY;
				}
			}	
		}
	}
}

bool CMaterialEffects::ExecuteEffect(TMFXEffectId effectId, SMFXRunTimeEffectParams& params)
{
  FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);
	
  if (!CMaterialEffectsCVars::Get().mfx_Enable)
		return false;

	bool success = false;
	IMFXEffectPtr pEffect = InternalGetEffect(effectId);
	if (pEffect)
	{
		NoDelayForMovingTrain(params);
		const float delay = pEffect->m_effectParams.delay;
		if (delay > 0.0f && !(params.playflags & MFX_DISABLE_DELAY))
			TimedEffect(pEffect, params);
		else
			pEffect->Execute(params);
		success = true;
	}
	return success;
}

void CMaterialEffects::StopEffect(TMFXEffectId effectId)
{
	IMFXEffectPtr pEffect = InternalGetEffect(effectId);
	if (pEffect)
	{
		SMFXResourceListPtr resources = SMFXResourceList::Create();
		pEffect->GetResources(*resources);

		SMFXFlowGraphListNode *pNext=resources->m_flowGraphList;
		while (pNext)
		{
			GetFGManager()->EndFGEffect(pNext->m_flowGraphParams.name);
			pNext=pNext->pNext;
		}
	}
}

void ToEffectString(const string& effectString, string& libName, string& effectName)
{
	size_t colon = effectString.find(':');
	if (colon != string::npos)
	{
		libName    = effectString.substr(0, colon);
		effectName = effectString.substr(colon + 1, effectString.length() - colon - 1);
	}
	else
	{
		libName = effectString;
		effectName = effectString;
	}
}

bool CMaterialEffects::Load(const char* filenameSpreadsheet)
{
	// reloading with ""
	if (filenameSpreadsheet && *filenameSpreadsheet == 0 && m_filename.empty() == false)
		filenameSpreadsheet = m_filename.c_str();

	Reset();

	m_mfxLibraries.clear();
	m_effectVec.clear();
	m_effectVec.push_back(0); // 0 -> invalid effect id

	LoadFXLibraries();

	// Create a valid path to the XML file.
	string sPath = "Libs/MaterialEffects/";
	sPath+=filenameSpreadsheet;

	// Init messages
	CryComment("[MFX] Init");

	// The root node. 
	XmlNodeRef root = m_pSystem->LoadXmlFile( sPath );
	if (!root)
	{
		// The file wasn't found, or the wrong file format was used
		GameWarning("[MFX] File not found or wrong file type: %s", sPath.c_str());
		return false;
	}
	else
	{
		CryComment("[MFX] Loaded: %s", sPath.c_str());
	}

	m_filename = filenameSpreadsheet;

	// Load the worksheet
	XmlNodeRef workSheet = root->findChild("Worksheet");
	if (!workSheet)
	{
		GameWarning("[MFX] Worksheet not found");
		return false;
	}
	// Load the table
	XmlNodeRef table = workSheet->findChild("Table");
	if (!table)
	{
		GameWarning("[MFX] Table not found");
		return false;
	}

	// temporary multimap: we store effectstring -> [TIndexPairs]+ there
	typedef std::vector<TIndexPair> TIndexPairVec;
	typedef std::map<const char*,TIndexPairVec, stl::less_stricmp<const char*> > TEffectStringToIndexPairVecMap;
	TEffectStringToIndexPairVecMap tmpEffectStringToIndexPairVecMap;

	// rowCount and colCount will be more accurate than i and j because
	// some phantom nodes shouldn't count towards adding rows and columns.
	int rowCount = 0;
	int colCount = 0;
	int warningCount = 0;

	// When we've gone down more rows than we have columns, we've entered special object space
	int maxCol = 0;
	string tmpString; // temporary string
	std::vector<const char*> colOrder;
	std::vector<const char*> rowOrder;

	m_surfaceIdToMatrixEntry.resize(0);
	m_surfaceIdToMatrixEntry.resize(MAX_SURFACE_COUNT, TIndex(0));
	m_ptrToMatrixEntryMap.clear();
	m_customConvert.clear();


	// Iterate through the table's children (rows)
	for (int i = 0; i < table->getChildCount(); i++)
	{
		// Row node
		XmlNodeRef curRow = table->getChild(i);
		// Possible to have a phantom node here, just get the next one.
		if (!curRow->isTag("Row"))
			continue;
		// Iterate through the row's children (columns)
		for (int j = 0; j < curRow->getChildCount(); j++)
		{
			// Get the current cell from this row
			XmlNodeRef curCell = curRow->getChild(j);
			// Some nodes aren't really cells
			if (!curCell->isTag("Cell"))
				continue;

			// Excel compresses file size by skipping blank cells, and adding an "index" field onto the first
			// cell that shows up. We have to account for this to keep colCount accurate.
			int indexVal = 0;
			if (curCell->getAttr("ss:Index", indexVal))
				colCount = indexVal - 1;

			// If it really is a cell, it has a child called Data that holds the actual cell data.
			XmlNodeRef curCellData = curCell->findChild("Data");
			if (!curCellData)
			{
				// If no data, continue
				++colCount;
				continue;
			}

			// The first row has a complete list of materials.
			const char* cellString = curCellData->getContent();

			if (rowCount == 0 && colCount > 0)
			{
				const int matId = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceTypeByName(cellString, "MFX", true)->GetId();
				if (matId != 0 || /* matId == 0 && */ stricmp(cellString, "mat_default") == 0) // if matId != 0 or it's the mat_default name
				{
					// CryLogAlways("[MFX] Material found: %s [ID=%d] [mapping to row/col=%d]", cellString, matId, colCount);
					if (m_surfaceIdToMatrixEntry.size() < matId)
					{
						m_surfaceIdToMatrixEntry.resize(matId+1);
						if (matId >= MAX_SURFACE_COUNT)
						{
							assert (false && "MaterialEffects.cpp: SurfaceTypes exceeds 256. Reconsider implementation.");
							CryLogAlways("MaterialEffects.cpp: SurfaceTypes exceeds %d. Reconsider implementation.", MAX_SURFACE_COUNT);
						}
					}
					m_surfaceIdToMatrixEntry[matId] = colCount;
				}
				else
				{
					GameWarning("MFX WARNING: Material not found: %s", cellString);
					++warningCount;
				}
				colOrder.push_back(cellString);
			} 
			else if (rowCount > 0 && colCount > 0)
			{
				//CryLog("[MFX] Event found: %s", cellString);
				tmpEffectStringToIndexPairVecMap[cellString].push_back(TIndexPair(rowCount, colCount));
			} 
			else if (rowCount > maxCol && colCount == 0)
			{	
				IEntityClass *pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(cellString);
				//CryLog("[MFX] Object class ID: %d", (int)pEntityClass);
				// Add the special object to the string table.
				//m_stringTable[cellString] = rowCount;
				if (pEntityClass)
				{
					// CryComment("[MFX] Found EntityClass based entry: %s [mapping to row/col=%d]", cellString, rowCount);
					m_ptrToMatrixEntryMap[pEntityClass] = rowCount;
				}
				else
				{
          // CryComment("[MFX] Found Custom entry: %s [mapping to row/col=%d]", cellString, rowCount);
					tmpString = cellString;
					tmpString.MakeLower();
					m_customConvert[tmpString] = rowCount;
					++warningCount;
				}
			}
			else if (rowCount > 0 && colCount == 0)
			{
				rowOrder.push_back(cellString);
			}
			// Heavy-duty debug info
			//CryLog("[MFX] celldata = %s at (%d, %d) rowCount=%d colCount=%d maxCol=%d", curCellData->getContent(), i, j, rowCount, colCount, maxCol);
			
			// Check if this is the furthest column we've seen thus far
			if (colCount > maxCol)
				maxCol = colCount;
			
			// Increment column counter
			++colCount;
		}
		// Increment row counter
		++rowCount;
		// Reset column counter
		colCount = 0;
	}

	// now postprocess the tmpEffectStringIndexPairVecMap and generate the m_matmatArray
	{
		// create the matmat array.
		// +1, because index pairs are in range [1..rowCount][1..maxCol]
		m_matmatArray.Create(rowCount+1,maxCol+1);
		TEffectStringToIndexPairVecMap::const_iterator iter = tmpEffectStringToIndexPairVecMap.begin();
		TEffectStringToIndexPairVecMap::const_iterator iterEnd = tmpEffectStringToIndexPairVecMap.end();
		string libName;
		string effectName;
		while (iter != iterEnd)
		{
			// lookup effect
			ToEffectString(iter->first, libName, effectName);
			IMFXEffectPtr pEffect = InternalGetEffect(libName, effectName);
			TMFXEffectId effectId = pEffect ? pEffect->m_effectParams.effectId : InvalidEffectId;
			TIndexPairVec::const_iterator vecIter = iter->second.begin();
			TIndexPairVec::const_iterator vecIterEnd = iter->second.end();
			while (vecIter != vecIterEnd)
			{
				const TIndexPair& indexPair = *vecIter;
				// CryLogAlways("[%d,%d]->%d '%s'", indexPair.first, indexPair.second, effectId, iter->first.c_str());
				m_matmatArray(indexPair.first,indexPair.second) = effectId;
				++vecIter;
			}
			++iter;
		}
	}


#ifdef MFX_CHECK_IDENTITY_MISMATCH
	{
		CryLogAlways("[MFX] RowCount=%d MaxCol=%d (*=%d)", rowCount, maxCol,rowCount*maxCol);
		for (int y=0; y<m_matmatArray.m_nRows; ++y)
		{
			for (int x=0; x<m_matmatArray.m_nCols; ++x)
			{
				TMFXEffectId idRowCol = m_matmatArray.GetValue(y,x, USHRT_MAX);
				assert (idRowCol != USHRT_MAX);
				IMFXEffectPtr pEffectRowCol = InternalGetEffect(idRowCol);
				if (pEffectRowCol)
				{
					CryLogAlways("[%d,%d] -> %d '%s:%s'", y, x, idRowCol, pEffectRowCol->m_effectParams.libName.c_str(), pEffectRowCol->m_effectParams.name.c_str());
					if (y<m_matmatArray.m_nCols)
					{
						TMFXEffectId idColRow = m_matmatArray.GetValue(x,y, USHRT_MAX);
						assert (idColRow != USHRT_MAX);
						IMFXEffectPtr pEffectColRow = InternalGetEffect(idColRow);
						if (idRowCol != idColRow)
						{
							if (pEffectColRow)
							{
								GameWarning("[MFX] Identity mismatch: ExcelRowCol %d:%d: %s:%s != %s:%s", y+1, x+1, 
									pEffectRowCol->m_effectParams.libName.c_str(), pEffectRowCol->m_effectParams.name.c_str(),
									pEffectColRow->m_effectParams.libName.c_str(), pEffectColRow->m_effectParams.name.c_str());
							}
							else
							{
								GameWarning("[MFX] Identity mismatch: ExcelRowCol %d:%d: %s:%s != [not found]", y+1, x+1, 
									pEffectRowCol->m_effectParams.libName.c_str(), pEffectRowCol->m_effectParams.name.c_str());
							}
						}
					}
				}
			}
		}
	}
#endif

	// check that we have the same number of columns and rows
	if (colOrder.size() > rowOrder.size())
	{
		GameWarning("[MFX] Found %d Columns, but not enough rows specified (%d)", colOrder.size(), rowOrder.size());
	}

#ifdef MFX_CHECK_COLUMN_ROW_ORDER
	// check that column order matches row order
	for (int i=0;i<colOrder.size(); ++i)
	{
		const char* colName = colOrder[i];
		const char* rowName = i < rowOrder.size() ? rowOrder[i] : "";
		// CryLogAlways("ExcelColRow=%d col=%s row=%s", i+2, colName.c_str(), rowName.c_str());
		if (strcmp(colName,rowName) != 0)
		{
			GameWarning("ExcelColRow=%d: %s != %s", i+2, colName, rowName);
		}
	}
#endif

	return true;
}

void CMaterialEffects::PreLoadAssets()
{
	for (TMFXEffectId id = 0; id < m_effectVec.size(); ++id)
		if (m_effectVec[id])
			m_effectVec[id]->PreLoadAssets();
}

bool CMaterialEffects::LoadFlowGraphLibs()
{
	if (m_pMaterialFGManager)
		return m_pMaterialFGManager->LoadLibs();
	return false;
}

TMFXEffectId CMaterialEffects::GetEffectIdByName(const char* libName, const char* effectName)
{
	if (!CMaterialEffectsCVars::Get().mfx_Enable)
		return InvalidEffectId;

	// FIXME
	const IMFXEffectPtr pEffectPtr = InternalGetEffect(libName, effectName);
	if (pEffectPtr)
		return pEffectPtr->m_effectParams.effectId;
	return InvalidEffectId;
}

TMFXEffectId CMaterialEffects::GetEffectId(int surfaceIndex1, int surfaceIndex2)
{
	if (!CMaterialEffectsCVars::Get().mfx_Enable)
		return InvalidEffectId;

	// Map surface IDs to internal matmat indices
	const TIndex idx1 = SurfaceIdToMatrixEntry(surfaceIndex1);
	const TIndex idx2 = SurfaceIdToMatrixEntry(surfaceIndex2);
	return InternalGetEffectId(idx1, idx2);
}

TMFXEffectId CMaterialEffects::GetEffectId(const char *customName, int surfaceIndex2)
{
	if (!CMaterialEffectsCVars::Get().mfx_Enable)
		return InvalidEffectId;

	const TIndex idx1 = stl::find_in_map(m_customConvert, CONST_TEMP_STRING(customName), 0);
	const TIndex idx2 = SurfaceIdToMatrixEntry(surfaceIndex2);
	return InternalGetEffectId(idx1, idx2);
}

// Get the cell contents that these parameters equate to
TMFXEffectId CMaterialEffects::GetEffectId(IEntityClass* pEntityClass, int surfaceIndex2)
{
	if (!CMaterialEffectsCVars::Get().mfx_Enable)
		return InvalidEffectId;

	// Map material IDs to effect indexes
	const TIndex idx1 = stl::find_in_map(m_ptrToMatrixEntryMap, pEntityClass, 0);
	const TIndex idx2 = SurfaceIdToMatrixEntry(surfaceIndex2);
	return InternalGetEffectId(idx1, idx2);
}

SMFXResourceListPtr CMaterialEffects::GetResources(TMFXEffectId effectId)
{
	SMFXResourceListPtr pResourceList = SMFXResourceList::Create();
	IMFXEffectPtr mfx = InternalGetEffect(effectId);
	if (mfx)
		mfx->GetResources(*pResourceList);
  return pResourceList;
}

void CMaterialEffects::TimedEffect(IMFXEffectPtr effect, SMFXRunTimeEffectParams &params)
{
	if (!m_bUpdateMode)
		return;
	m_delayedEffects.push_back(SDelayedEffect(effect, params));
}

void CMaterialEffects::SetUpdateMode(bool bUpdate)
{
	if (!bUpdate)
	{
		m_delayedEffects.clear();
		m_pMaterialFGManager->Reset();
	}
//	if (bUpdate && !m_bUpdateMode)
//		PreLoadAssets();
	m_bUpdateMode = bUpdate;
}

void CMaterialEffects::Update(float frameTime)
{
	SetUpdateMode(true);
	std::vector< SDelayedEffect >::iterator it = m_delayedEffects.begin();
	std::vector< SDelayedEffect >::iterator next = it;
	while (it != m_delayedEffects.end())
	{
		++next;
		SDelayedEffect& cur = *it;
		cur.m_delay -= frameTime;
		if (cur.m_delay <= 0.0f)
		{
			cur.m_pEffect->Execute(cur.m_effectRuntimeParams);
			next = m_delayedEffects.erase(it);
		}
		it = next;
	}
}

void CMaterialEffects::NotifyFGHudEffectEnd(IFlowGraphPtr pFG)
{
	if (m_pMaterialFGManager)
		m_pMaterialFGManager->EndFGEffect(pFG);
}

void CMaterialEffects::Reset()
{
	if (m_pMaterialFGManager)
		m_pMaterialFGManager->Reset();
	m_delayedEffects.resize(0);
}

void CMaterialEffects::Serialize(TSerialize ser)
{
	if (m_pMaterialFGManager && CMaterialEffectsCVars::Get().mfx_SerializeFGEffects != 0)
		m_pMaterialFGManager->Serialize(ser);
}

void CMaterialEffects::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s,"MaterialEffects");
	s->Add(*this);
	m_pMaterialFGManager->GetMemoryStatistics(s);

	{
		SIZER_SUBCOMPONENT_NAME(s, "libs");
		s->AddContainer(m_mfxLibraries);
		for (std::map<TMFXNameId,TFXLibrary>::iterator iter = m_mfxLibraries.begin(); iter != m_mfxLibraries.end(); ++iter)
		{
			s->Add(iter->first);
			s->AddContainer(iter->second);
			for (TFXLibrary::iterator iter2 = iter->second.begin(); iter2 != iter->second.end(); ++iter2)
			{
				s->Add(iter2->first);
				if (iter2->second)
					iter2->second->GetMemoryStatistics(s);
			}
		}
	}
	{
		SIZER_SUBCOMPONENT_NAME(s, "convert");
		for (std::map<string, TIndex, stl::less_stricmp<string> >::iterator iter = m_customConvert.begin(); iter != m_customConvert.end(); ++iter)
		{
			s->Add(iter->first);
		}
		s->AddContainer(m_customConvert);
		s->AddContainer(m_surfaceIdToMatrixEntry);
		s->AddContainer(m_ptrToMatrixEntryMap);
	}
	{
		SIZER_SUBCOMPONENT_NAME(s, "lookup");
		s->AddContainer(m_effectVec); // the effects themselves already accounted in "libs"
		m_matmatArray.GetMemoryStatistics(s);
	}
	{
		SIZER_SUBCOMPONENT_NAME(s, "playing"); 
		s->AddContainer(m_delayedEffects); // the effects themselves already accounted in "libs"
		s->AddObject((const void*)&CMFXRandomEffect::GetMemoryUsage, CMFXRandomEffect::GetMemoryUsage());
	}
}

static float NormalizeMass(float fMass)
{
	float massMin = 0.0f;
	float massMax = 500.0f;
	float paramMin = 0.0f;
	float paramMax = 1.0f/3.0f;

	// tiny - bullets
	if (fMass <= 0.1f)
	{
		// small
		massMin = 0.0f;
		massMax = 0.1f;
		paramMin = 0.0f;
		paramMax = 1.0f;
	}
	else if (fMass < 20.0f)
	{
		// small
		massMin = 0.0f;
		massMax = 20.0f;
		paramMin = 0.0f;
		paramMax = 1.0f/3.0f;
	}
	else if (fMass < 200.0f)
	{
		// medium
		massMin = 20.0f;
		massMax = 200.0f;
		paramMin = 1.0f/3.0f;
		paramMax = 2.0f/3.0f;
	}
	else
	{
		// ultra large
		massMin = 200.0f;
		massMax = 2000.0f;
		paramMin = 2.0f/3.0f;
		paramMax = 1.0f;
	}

	const float p = min(1.0f, (fMass - massMin)/(massMax - massMin));
	const float finalParam = paramMin + (p * (paramMax - paramMin));
	return finalParam;
}

bool CMaterialEffects::PlayBreakageEffect(ISurfaceType* pSurfaceType, const char* breakageType, const SMFXBreakageParams& breakageParams)
{
	if (pSurfaceType == 0)
		return false;

	CryFixedStringT<128> fxName ("Breakage:");
	fxName+=breakageType;
	TMFXEffectId effectId = this->GetEffectId(fxName.c_str(), pSurfaceType->GetId());
	if (effectId == InvalidEffectId)
		return false;

	// only play sound at the moment
	SMFXRunTimeEffectParams params;
	params.playflags = MFX_PLAY_SOUND;

	// if hitpos is set, use it
	// otherwise use matrix (hopefully been set or 0,0,0)
	if (breakageParams.CheckFlag(SMFXBreakageParams::eBRF_HitPos) && breakageParams.GetHitPos().IsZero() == false)
		params.pos = breakageParams.GetHitPos();
	else
		params.pos = breakageParams.GetMatrix().GetTranslation();

	params.soundSemantic = eSoundSemantic_Physics_General;

	const Vec3& hitImpulse = breakageParams.GetHitImpulse();
	const float strength = hitImpulse.GetLengthFast();
	params.AddSoundParam("strength", strength);
	const float mass = NormalizeMass(breakageParams.GetMass());
	params.AddSoundParam("mass", mass);

	if (CMaterialEffectsCVars::Get().mfx_Debug & 2)
	{
		IMFXEffectPtr pEffect = InternalGetEffect(effectId);
		if (pEffect != 0)
		{
			CryLogAlways("[MFX]: %s:%s FX=%s:%s Pos=%f,%f,%f NormMass=%f  F=%f Imp=%f,%f,%f  RealMass=%f Vel=%f,%f,%f",
				breakageType, pSurfaceType->GetName(), pEffect->m_effectParams.libName.c_str(), pEffect->m_effectParams.name.c_str(),
				params.pos[0],params.pos[1],params.pos[2], 
				mass,
				strength, 
				breakageParams.GetHitImpulse()[0],
				breakageParams.GetHitImpulse()[1],
				breakageParams.GetHitImpulse()[2],
				breakageParams.GetMass(),
				breakageParams.GetVelocity()[0],
				breakageParams.GetVelocity()[1],
				breakageParams.GetVelocity()[2]
				);
		}
	}

	/*
	if (breakageParams.GetMass() == 0.0f)
	{
		int a = 0;
	}
	*/


	const bool bSuccess = ExecuteEffect(effectId, params);

	return bSuccess;
}
