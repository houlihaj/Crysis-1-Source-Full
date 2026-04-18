////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	28/09/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  high-level loading of characters
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <CryHeaders.h>
#include <I3DEngine.h>

#include "CharacterInstance.h"
#include "CharacterManager.h"
#include "CryAnimationScriptCommands.h"
#include "LoaderCHR.h"
#include "LoaderCGA.h"
#include "FacialAnimation/FaceAnimation.h"


//#include "CryMemoryManager_impl.h"

float g_YLine=0.0f;


CCharacterModel* model_thin=0;
CCharacterModel* model_fat=0;


//////////////////////////////////////////////////////////////////////////
// Loads a cgf and the corresponding caf file and creates an animated object,
// or returns an existing object.
ICharacterInstance* CharacterManager::CreateInstance(const char* szCharacterFileName, uint32 IsSkinAtt, IAttachment* pIMasterAttachment)
{
	//g_pILog->LogError ("CryAnimation: creating character instance: %s", szCharacterFileName);

	g_pIRenderer			= g_pISystem->GetIRenderer();
	g_pIPhysicalWorld	= g_pISystem->GetIPhysicalWorld();
	g_pI3DEngine			=	g_pISystem->GetI3DEngine();
	g_pAuxGeom				= g_pIRenderer->GetIRenderAuxGeom();

	if (!szCharacterFileName)
		return (NULL);	// to prevent a crash in the frequent case the designers will mess 
	// around with the entity parameters in the editor

	string strPath = szCharacterFileName;
	CryStringUtils::UnifyFilePath(strPath);

	string fileExt = PathUtil::GetExt(strPath);
	bool IsCGA = (0 == stricmp(fileExt,"cga"));
	bool IsCHR = (0 == stricmp(fileExt,"chr"));
	bool IsCDF = (0 == stricmp(fileExt,"cdf"));

	if (IsCGA)
		return CreateCGAInstance( strPath ); //Loading CGA file.

	if (IsCHR)
		return CreateCHRInstance( strPath, IsSkinAtt,(CAttachment*)pIMasterAttachment ); //Loading CHR file.

	if (IsCDF)
		return LoadCharacterDefinition( strPath ); //Loading CDF file.

	g_pILog->LogError ("CryAnimation: no valid character file-format: %s", szCharacterFileName);
	return 0; //if it ends here, then we have no valid file-format
}

int static single = 0;

int static modelsSize = 0;

ICharacterInstance* CharacterManager::CreateCHRInstance( const string& strPath, uint32 IsSkinAtt, CAttachment* pMasterAttachment  )
{
  LOADING_TIME_PROFILE_SECTION(g_pISystem);

#ifdef MEMORY_REPORT
	//if (single)
	{
		//single = false;
		setLogAllocations(true);
	}
#endif

	uint64 membegin;

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		//char tmp[4096];
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		membegin = info.allocated - info.freed;
	}





	//IVO! this is no good implementation, but for testing it will do  
	model_thin=0;
	model_fat=0;


	CSkinInstance* pCryCharInstance;

	CCharacterModel* pModel = CheckIfModelLoaded(strPath);
	if (pModel==0)
	{
		//there is no model in memory; so lets load one
		string strPath_thin=strPath;
		PathUtil::RemoveExtension(strPath_thin);
		strPath_thin=strPath_thin+"_thin." + CRY_CHARACTER_FILE_EXT;
		if (gEnv->pCryPak->IsFileExist(strPath_thin.c_str()))
			model_thin = FetchModel(strPath_thin,1,0);

		string strPath_fat=strPath;
		PathUtil::RemoveExtension(strPath_fat);
		strPath_fat=strPath_fat+"_fat." + CRY_CHARACTER_FILE_EXT;
		if (gEnv->pCryPak->IsFileExist(strPath_fat.c_str()))
			model_fat  = FetchModel(strPath_fat,1,0);

		// try to find already loaded model, or load a new one
		pModel = FetchModel (strPath,0,1);

		if (model_thin)	model_thin->Release();
		if (model_fat)	model_fat->Release();
		model_thin=0;
		model_fat=0;
		if(pModel==0)
			return NULL; // the model has not been loaded

		if (IsSkinAtt==0xDeadBeef)
		{
			pCryCharInstance = new CSkinInstance (strPath, pModel, IsSkinAtt, pMasterAttachment);
		}
		else
		{
			pCryCharInstance = new CCharInstance (strPath, pModel);
			if (pModel->m_IsProcessed==0)
			{
				pModel->m_AnimationSet.AnalyseAndModifyAnimations(strPath,pModel);
				pModel->m_IsProcessed++;
			}
			SetFootPlantsFlag( (CCharInstance*)pCryCharInstance ); 
		}
		DecideModelLockStatus(pModel, 0);

		for (uint32 i = 0; i < pModel->GetModelMeshCount(); ++i)
		{
			pModel->GetModelMesh(i)->m_arrIntVertices.clear();
			pModel->GetModelMesh(i)->m_arrIntFaces.clear();
		}

		if (Console::GetInst().ca_MemoryUsageLog)
		{
			//char tmp[4096];
			CryModuleMemoryInfo info;
			CryGetMemoryInfoForModule(&info);
			g_AnimStatisticsInfo.m_iModelsSizes += info.allocated - info.freed  - membegin;
			g_pILog->UpdateLoadingScreen("Models size %i",g_AnimStatisticsInfo.GetModelsSize());
		}
	}
	else
	{
		//the model is already loaded
		if (IsSkinAtt==0xDeadBeef)
		{
			pCryCharInstance = new CSkinInstance(strPath, pModel, IsSkinAtt,pMasterAttachment);
		}
		else
		{
			pCryCharInstance = new CCharInstance (strPath, pModel);
			if (pModel->m_IsProcessed==0)
			{
				pModel->m_AnimationSet.AnalyseAndModifyAnimations(strPath,pModel);
				pModel->m_IsProcessed++;
			}
			SetFootPlantsFlag( (CCharInstance*)pCryCharInstance ); 
		}
	}


	if (Console::GetInst().ca_MemoryUsageLog)
	{
		//char tmp[4096];
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("CreateCHRInstance %s. Memstat %i. %i",strPath.c_str(), (int)(info.allocated - info.freed), (int)membegin );
		g_AnimStatisticsInfo.m_iInstancesSizes +=  info.allocated - info.freed  - membegin;
//		modelsSize += info.allocated - info.freed  - membegin;
		g_pILog->UpdateLoadingScreen("Instances memstat %i", g_AnimStatisticsInfo.GetInstancesSize());
//		STLALLOCATOR_CLEANUP;
	}


//#ifdef MEMORY_REPORT
//	setLogAllocations(false);
//	single = 1;
//#endif
	return pCryCharInstance;
}


//////////////////////////////////////////////////////////////////////////
CCharacterModel* CharacterManager::CheckIfModelLoaded (const string& strFileName)
{
	uint32 numModels = m_arrModelCache.size();
	ValidateModelCache();
	CryModelCache::iterator it = std::lower_bound (m_arrModelCache.begin(), m_arrModelCache.end(), strFileName, OrderByFileName());

	if (it != m_arrModelCache.end())
	{
		const string& strBodyFilePath = (*it)->GetFilePath();
		if (strBodyFilePath == strFileName)
			return *it;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// Finds a cached or creates a new CCharacterModel instance and returns it
// returns NULL if the construction failed
CCharacterModel* CharacterManager::FetchModel (const string& strFileName, uint32 sw, uint32 la)
{
  LOADING_TIME_PROFILE_SECTION(g_pISystem);

	// [Alexey] Removed because its double-check. We had the same in CreateCHRInstance
	/*
	CCharacterModel* pModel = CheckIfModelLoaded(strFileName);
	if (pModel)
	return pModel;
	*/

//	uint64 membegin;


	if (sw == 0)
	{
		g_pILog->LogToFile("Loading Character %s", strFileName.c_str());	// to file only so console is cleaner
		g_pILog->UpdateLoadingScreen(0);
	}

	CryCHRLoader CHRLoader;
	CCharacterModel* pModel = CHRLoader.LoadNewCHR(strFileName,this,sw,la);
	if (pModel)
		RegisterModel(pModel);


	return pModel;
}

//////////////////////////////////////////////////////////////////////////
ICharacterInstance* CharacterManager::CreateCGAInstance( const char* strPath )
{
  LOADING_TIME_PROFILE_SECTION(g_pISystem);

	uint64 membegin;
	if (Console::GetInst().ca_MemoryUsageLog)
	{
		//char tmp[4096];
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		membegin = info.allocated - info.freed;
	}


	CCharacterModel* pModel = 0;
	ValidateModelCache();

	uint32 numModels = m_arrModelCache.size();
	for (uint32 i=0; i<numModels; i++)
	{
		CCharacterModel* pModelInCache=m_arrModelCache[i];
		const char* filename = pModelInCache->GetModelFilePath();
		uint32 tt=0;
	}

	CryModelCache::iterator it = std::lower_bound (m_arrModelCache.begin(), m_arrModelCache.end(), strPath, OrderByFileName());
	if (it != m_arrModelCache.end())
	{
		const string& strBodyFilePath = (*it)->GetFilePath();
		if (strBodyFilePath == strPath)
			pModel=*it; 
	}

	if (pModel==0)
	{
		g_pILog->UpdateLoadingScreen("Loading CGA %s", strPath);

		CryCGALoader CGALoader;
		pModel = CGALoader.LoadNewCGA( strPath, this );
		if (pModel)
		{
			RegisterModel(pModel);
//			pModel->m_AnimationSet.AnalyseAndModifyAnimations(strPath,pModel);
			//DecideModelLockStatus(pModel, nFlags);
		}
	}

	if (pModel==0)
		return NULL; // the model has not been loaded

	CCharInstance* pCryCharInstance = new CCharInstance (strPath, pModel);
	pCryCharInstance->m_SkeletonPose.InitCGASkeleton();


	pCryCharInstance->m_SkeletonPose.UpdateBBox(1);

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("CreateCGAInstance %s. Memstat %i",strPath, (int)(info.allocated - info.freed));
		g_AnimStatisticsInfo.m_iInstancesSizes +=  info.allocated - info.freed  - membegin;
		g_pILog->UpdateLoadingScreen("Instances Memstat %i",g_AnimStatisticsInfo.GetInstancesSize());
//		STLALLOCATOR_CLEANUP;
	}

	return pCryCharInstance;
}



// adds the body to the cache from which it can be reused if another instance of that model is to be created
void CharacterManager::RegisterModel (CCharacterModel* pModel)
{

	/// check for identical model skeleton

	bool bIdentical(false);
	_smart_ptr<CModelSkeleton> pSkeleton;
	for (uint32 i = 0, end = m_arrModelCache.size(); i < end; ++i )
	{
		if (m_arrModelCache[i]->m_pModelSkeleton->m_arrModelJoints.size() == pModel->m_pModelSkeleton->m_arrModelJoints.size()) {
			// at least numer of bones is identical
			for (uint32 j = 0, endj = m_arrModelCache[i]->m_pModelSkeleton->m_arrModelJoints.size(); j < endj; ++j) {
				if (m_arrModelCache[i]->m_pModelSkeleton->m_arrModelJoints[j] != pModel->m_pModelSkeleton->m_arrModelJoints[j])
					bIdentical = false;
				break;
			}
			if (bIdentical) {
//				g_pILog->UpdateLoadingScreen("Identical model %s", pModel->GetFile());
				pModel->m_pModelSkeleton = m_arrModelCache[i]->m_pModelSkeleton;

				break;
			}
		}
	}

	if (!bIdentical) {
		uint32 numJoints = pModel->m_pModelSkeleton->m_arrModelJoints.size();
		for (uint32 i=0; i<numJoints; i++)
		{
			//uint32 c0=pModel->m_pModelSkeleton->m_arrModelJoints[i].m_numChildren;
			//uint32 c1=pModel->m_pModelSkeleton->m_arrModelJoints[i].m_numChilds;
			//assert(c0==c1);
			pModel->m_pModelSkeleton->m_arrModelJoints[i].m_idx = i;
			if (!pModel->m_pModelSkeleton->m_JointMap.InsertValue(&pModel->m_pModelSkeleton->m_arrModelJoints[i], i))
			{
				AnimWarning("Model %s has duplicated joints %s", pModel->GetFilePath().c_str(), pModel->m_pModelSkeleton->m_arrModelJoints[i].GetJointName());
			}
		}
	}

	m_arrModelCache.insert (std::lower_bound(m_arrModelCache.begin(), m_arrModelCache.end(), pModel->GetFilePath(), OrderByFileName()), pModel);
}

//////////////////////////////////////////////////////////////////////////
void CharacterManager::LockModel( CCharacterModel* pModel )
{
	m_arrTempLock.push_back( pModel );
}

// Deletes itself
void CharacterManager::Release()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimation* CharacterManager::GetIFacialAnimation()
{
	return m_pFacialAnimation;
}

//////////////////////////////////////////////////////////////////////////
bool IsResourceLocked( const char *filename )
{
	IResourceList *pResList = gEnv->pCryPak->GetRecorderdResourceList(ICryPak::RFOM_NextLevel);
	if (pResList)
	{
		return pResList->IsExist(filename);
	}
	return false;
}


// locks or unlocks the given body if needed, depending on the hint and console variables
void CharacterManager::DecideModelLockStatus(CCharacterModel* pModel, uint32 nHints)
{
	// there's already a character, so we can safely remove the body from the lock pool
	// basic rules:
	//  transient hint takes precedence over everything else
	//  any hint takes precedence over the console settings
	//  if there are no hints, console variable ca_KeepModels is used to determine
	//    if the model should be kept in memory all the time (ca_KeepModels 1) or not (0)
	//  Note: changing ca_KeepModels won't immediately lock or unlock all models. It will only affect
	//  models which are actively used to create new characters or destroy old ones.
	if (nHints & nHintModelTransient) 
		m_setLockedModels.erase (pModel);
	else
		if (nHints & nHintModelPersistent) 
			m_setLockedModels.insert (pModel);
		else
			if (Console::GetInst().ca_KeepModels)
				m_setLockedModels.insert (pModel);
			else
				m_setLockedModels.erase (pModel);
}



// Deletes the given body from the model cache. This is done when there are no instances using this body info.
void CharacterManager::UnregisterModel (CCharacterModel* pModel)
{
	ValidateModelCache();
	uint32 numBodies = m_arrModelCache.size();
	if (numBodies)
	{
		const char* fname = pModel->GetModelFilePath();
		uint32 i=0;
	}

	uint32 counter=0;
	uint32 numModels2 = m_arrModelCache.size();
	for (uint32 i=0; i<numModels2; i++)
	{
		if (pModel==m_arrModelCache[i])
			counter++;
	}
	assert(counter<2);


	CryModelCache::iterator itCache = std::lower_bound (m_arrModelCache.begin(), m_arrModelCache.end(), pModel, OrderByFileName());
	if (itCache != m_arrModelCache.end())
	{
		if (*itCache == pModel)
			m_arrModelCache.erase (itCache);
		else
		{
			assert (false); // there must be no duplicate name pointers here
			while (++itCache != m_arrModelCache.end())
				if (*itCache == pModel)
				{
					m_arrModelCache.erase(itCache);
					break;
				}
		}
	}
	else
		assert (false); // this pointer must always be in the cache
	ValidateModelCache();


	uint32 numModels = m_arrModelCache.size();
	for (uint32 i=0; i<numModels; i++)
	{
		CCharacterModel* pModelInCache=m_arrModelCache[i];
		const char* filename = pModelInCache->GetModelFilePath();
		uint32 tt=0;
	}


}


// returns statistics about this instance of character animation manager
// don't call this too frequently
void CharacterManager::GetStatistics(Statistics& rStats)
{
	memset (&rStats, 0, sizeof(rStats));
	rStats.numCharModels = m_arrModelCache.size();
	for (CryModelCache::const_iterator it = m_arrModelCache.begin(); it != m_arrModelCache.end(); ++it)
		rStats.numCharacters += (*it)->NumInstances();

	//rStats.numAnimObjectModels = rStats.numAnimObjects = m_pAnimObjectManager->NumObjects();
}


// Validates the cache
void CharacterManager::ValidateModelCache()
{
#ifdef _DEBUG
	CryModelCache::iterator itPrev = m_arrModelCache.end();
	for (CryModelCache::iterator it = m_arrModelCache.begin(); it != m_arrModelCache.end(); ++it)
	{
		if (itPrev != m_arrModelCache.end())
			assert ((*itPrev)->GetFilePath() < (*it)->GetFilePath());
	}
#endif
}



//////////////////////////////////////////////////////////////////////////
// Deletes all the cached bodies and their associated character instances
void CharacterManager::CleanupModelCache()
{
	m_setLockedModels.clear();

	while (!m_arrModelCache.empty())
	{
		CCharacterModel* pModel = m_arrModelCache.back();

		// cleaning up the instances referring to this body actually releases it and deletes from the cache
		pModel->CleanupInstances();

		if (m_arrModelCache.empty())
			break;

		// if the body still stays in the cache, it means someone (who?!) still refers to it
		if (m_arrModelCache.back() == pModel)
		{
			assert(0); // actually ,after deleting all instances referring to the body, the body must be auto-deleted and deregistered.
			// if this doesn't happen, something is very wrong
			//delete pBody; // still, we'll delete the body
		}
	}

	m_AnimationManager.Clear();

}


bool CharacterManager::OrderByFileName::operator () (const CCharacterModel* pLeft, const CCharacterModel* pRight)
{
	const char* l = pLeft->GetModelFilePath();
	const char* r =	pRight->GetModelFilePath();
	return pLeft->GetFilePath() < pRight->GetFilePath();
}
bool CharacterManager::OrderByFileName::operator () (const string& strLeft, const CCharacterModel* pRight)
{
	return strLeft < pRight->GetFilePath();
}
bool CharacterManager::OrderByFileName::operator () (const CCharacterModel* pLeft, const string& strRight)
{
	const char* l = pLeft->GetModelFilePath();
	const char* r = strRight.c_str();
	return pLeft->GetFilePath() < strRight;
}

// puts the size of the whole subsystem into this sizer object, classified,
// according to the flags set in the sizer
void CharacterManager::GetMemoryUsage(class ICrySizer* pSizer)const
{
#if ENABLE_GET_MEMORY_USAGE
	if (!pSizer->Add(*this))
		return;
	g_AnimationManager.GetSize (pSizer);
	GetModelCacheSize(pSizer);
	GetCharacterInstancesSize(pSizer);



	//m_pAnimObjectManager->GetMemoryUsage (pSizer);
#endif
}

void CharacterManager::GetModelCacheSize(class ICrySizer* pSizer)const
{
	SIZER_SUBCOMPONENT_NAME(pSizer, "Model cache");

	for (uint32 i =0, end = m_arrModelCache.size(); i < end; ++i)
		m_arrModelCache[i]->SizeOfThis(pSizer);
}

void CharacterManager::GetCharacterInstancesSize(class ICrySizer* pSizer)const
{
	SIZER_SUBCOMPONENT_NAME(pSizer, "Character instances");

	uint32 nSize(0);

	for (uint32 i =0, end = m_arrModelCache.size(); i < end; ++i)
	{

		for (std::set<ICharacterInstance*>::iterator it = m_arrModelCache[i]->m_SetInstances.begin(), end = m_arrModelCache[i]->m_SetInstances.end();
			it != end; ++it)
		{
			nSize += (*it)->SizeOfThis(pSizer);
		}

	}


	pSizer->AddObject(this,0);

}

/*
void CharacterManager::GetCharacterInstancesSize(class ICrySizer* pSizer)const
{
	SIZER_SUBCOMPONENT_NAME(pSizer, "Character instances");

	uint32 nSize=0;

	for (uint32 i =0, end = m_arrModelCache.size(); i < end; ++i) {

		for (std::vector<ICharacterInstance*>::iterator it = m_arrModelCache[i]->m_SetInstances.begin(), end = m_arrModelCache[i]->m_SetInstances.end();
			it != end; ++it) {
				nSize += (*it)->SizeOfThis(pSizer);
		}
	}
	pSizer->AddObject(this, 0);
}*/



//! Cleans up all resources - currently deletes all bodies and characters (even if there are references on them)
void CharacterManager::ClearResources(void)
{
	CleanupModelCache();
}

//------------------------------------------------------------------------
//--  average frame-times to avoid stalls and peaks in framerate
//------------------------------------------------------------------------
f32 CharacterManager::GetAverageFrameTime(f32 sec, f32 FrameTime, f32 fTimeScale, f32 LastAverageFrameTime) 
{ 
	uint32 numFT=m_arrFrameTimes.size();
	for (int32 i=(numFT-2); i>-1; i--)
		m_arrFrameTimes[i+1] = m_arrFrameTimes[i];

	m_arrFrameTimes[0] = FrameTime;

	//get smoothed frame
	uint32 FrameAmount = 1;
	if (LastAverageFrameTime)
	{
		FrameAmount = uint32(sec/LastAverageFrameTime*fTimeScale+0.5f); //average the frame-times for a certain time-period (sec)
		if (FrameAmount>numFT)	FrameAmount=numFT;
		if (FrameAmount<1)	FrameAmount=1;
	}

	f32 AverageFrameTime=0;
	for (uint32 i=0; i<FrameAmount; i++)	
		AverageFrameTime += m_arrFrameTimes[i];
	AverageFrameTime /= FrameAmount;

	//don't smooth if we pase the game
	if (FrameTime<0.0001f)
		AverageFrameTime=FrameTime;

	//	g_YLine+=66.0f;
	//	float fColor[4] = {1,0,1,1};
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"AverageFrameTime:  Frames:%d  FrameTime:%f  AverageTime:%f", FrameAmount, FrameTime, AverageFrameTime);	
	//	g_YLine+=16.0f;

	return AverageFrameTime;
}



// should be called every frame
void CharacterManager::Update()
{
	DEFINE_PROFILER_FUNCTION();
	
	m_nUpdateCounter++;

	//update interfaces every frame
	g_YLine=16.0f;
	g_pIRenderer			= g_pISystem->GetIRenderer();
	g_pIPhysicalWorld	= g_pISystem->GetIPhysicalWorld();
	g_pI3DEngine			=	g_pISystem->GetI3DEngine();
	g_nFrameID				= g_pIRenderer->GetFrameID(false);
	g_bProfilerOn			= g_pISystem->GetIProfileSystem()->IsProfiling();

	g_fCurrTime		= g_pITimer->GetCurrTime();

	f32 fTimeScale	=	g_pITimer->GetTimeScale();
	f32 fFrameTime	=	g_pITimer->GetFrameTime();
	if (fFrameTime>0.2f) fFrameTime = 0.2f;
	if (fFrameTime<0.0f) fFrameTime = 0.0f;
	g_AverageFrameTime = GetAverageFrameTime( 0.25f, fFrameTime, fTimeScale, g_AverageFrameTime ); 






	if (Console::GetInst().ca_DebugModelCache)
	{
		uint32 numModels = m_arrModelCache.size();
		for(uint32 i=0; i<numModels; i++ )
		{
			float fColor[4] = {0,1,0,1};
			CCharacterModel* pModel = m_arrModelCache[i];
			const char* PathName = pModel->GetModelFilePath();
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Model: %x  %s",i,PathName );	
			g_YLine+=16.0f;
		}
	}


	extern uint32 g_RootUpdates;
	extern uint32 g_AnimationUpdates;
	extern uint32 g_SkeletonUpdates;
	
	if (Console::GetInst().ca_DebugAnimUpdates)
	{
		float fColor[4] = {0,1,1,1};
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"nCharInstanceLoaded: %d  nSkinInstanceLoaded: %d  Total:%d",g_nCharInstanceLoaded,g_nSkinInstanceLoaded, g_nCharInstanceLoaded+g_nSkinInstanceLoaded );	
		g_YLine+=16.0f;
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"nCharInstanceDeleted: %d  nSkinInstanceDeleted: %d",g_nCharInstanceDeleted,g_nSkinInstanceDeleted );	
		g_YLine+=32.0f;



		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"RootUpdates: %d",g_RootUpdates );	
		g_YLine+=16.0f; 
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"AnimationUpdates: %d",g_AnimationUpdates );	
		g_YLine+=16.0f; 
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"SkeletonUpdates: %d",g_SkeletonUpdates );	
		g_YLine+=16.0f; 

		uint32 numFSU = m_arrSkeletonUpdates.size();
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Instances with 'Force Skeleton Update': %d",numFSU );	
		g_YLine+=16.0f; 
		for (uint32 i=0; i<numFSU; i++)
		{
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.2f, fColor, false,"ModelPath: %s    Anim:(%d)  Force:(%d)  Visible:(%d)",m_arrSkeletonUpdates[i].c_str(),m_arrAnimPlaying[i],m_arrForceSkeletonUpdates[i],m_arrVisible[i] );	
			g_YLine+=14.0f; 
		}


	}

	g_RootUpdates=0;
	g_AnimationUpdates=0;
	g_SkeletonUpdates=0;

	m_arrSkeletonUpdates.resize(0);
	m_arrAnimPlaying.resize(0);
	m_arrForceSkeletonUpdates.resize(0);
	m_arrVisible.resize(0);


	if (Console::GetInst().ca_DebugAnimUsage)
	{
		float fColor[4] = {1,0,0,1};
		uint32 nNumAnimHeaders = g_AnimationManager.m_arrGlobalAnimations.size();

		uint32 nAnimUsed = 0;
		for (uint32 i=0; i<nNumAnimHeaders; i++)
			nAnimUsed += g_AnimationManager.m_arrGlobalAnimations[i].m_nPlayedCount;


		uint32 nNumLMGs = 0;
		uint32 nNumLMG_Used = 0;
		uint32 nNumAssetLoaded = 0;
		uint32 nNumAssets_Used = 0;
		for (uint32 i=0; i<nNumAnimHeaders; i++)
		{
			uint32 IsAssetLMG    = (g_AnimationManager.m_arrGlobalAnimations[i].m_nFlags&CA_ASSET_LMG)!=0;
			uint32 IsAssetLoaded = (g_AnimationManager.m_arrGlobalAnimations[i].m_nFlags&CA_ASSET_LOADED)!=0;
			uint32 IsAimUnloaded = (g_AnimationManager.m_arrGlobalAnimations[i].m_nFlags&CA_AIMPOSE_UNLOADED)!=0;
			uint32 IsOnDemand    = 0;//(g_AnimationManager.m_arrGlobalAnimations[i].m_nFlags&CA_ASSET_ONDEMAND)!=0;

			nNumLMGs += IsAssetLMG;
			nNumAssetLoaded += ((IsAssetLMG==0) && IsAssetLoaded && (IsAimUnloaded==0) && (IsOnDemand==0));
			nNumLMG_Used += (IsAssetLMG && g_AnimationManager.m_arrGlobalAnimations[i].m_nPlayedCount);
			nNumAssets_Used += ((IsAssetLMG==0) && g_AnimationManager.m_arrGlobalAnimations[i].m_nPlayedCount);
		}

		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.6f, fColor, false,"nNumAnimHeaders: %d",nNumAnimHeaders );	
		g_YLine+=20.0f; 
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 2.0f, fColor, false,"nNumLMGs: %d    nNumLMGs_Used: %d",nNumLMGs,nNumLMG_Used );	
		g_YLine+=16.0f; 
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 2.0f, fColor, false,"nNumAssetLoaded: %d    nNumAssets_Used: %d",nNumAssetLoaded,nNumAssets_Used);	
		g_YLine+=16.0f; 
	}


	if (Console::GetInst().ca_DumpAssetStatistics)
	{
		DumpAssetStatistics();
		Console::GetInst().ca_DumpAssetStatistics=0;
	}

	//IGame* pGame = g_g_pISystem->GetIGame();
	//g_bUpdateBonesAlways = pGame? (pGame->GetModuleState(EGameServer) && pGame->GetModuleState(EGameMultiplayer)) : false;
	g_bUpdateBonesAlways = false;

	CAnimDecal::setGlobalTime(g_pITimer->GetCurrTime());

}



void CharacterManager::DumpAssetStatistics()
{
	char token[500];

	struct WOUT
	{
		static void WriteOut(char* token, FILE* stream ) 
		{
			int x;
			for(x=0; ; x++) { if (token[x]==0) break;  }
			token[x]=0x0d; x++;
			token[x]=0x0a; x++;
			fwrite( token, sizeof(unsigned char), x, stream );
		}
	};
	
	uint32 nNumAnimHeaders = g_AnimationManager.m_arrGlobalAnimations.size();


	{
		FILE* stream = fopen( "AnimStatistics_LMG.txt", "w+b" );
		sprintf(token, "--------------------------------------------------------------");
		WOUT::WriteOut(token,stream );
		sprintf(token, "USED LOCOMOTION GROUPS");
		WOUT::WriteOut(token,stream );
		sprintf(token, "--------------------------------------------------------------");
		WOUT::WriteOut(token,stream );
		for (uint32 i=0; i<nNumAnimHeaders; i++)
		{
			uint32 IsAssetLMG    = (g_AnimationManager.m_arrGlobalAnimations[i].m_nFlags&CA_ASSET_LMG)!=0;
			if (IsAssetLMG && g_AnimationManager.m_arrGlobalAnimations[i].m_nPlayedCount)
			{
				const char* strPathName = g_AnimationManager.m_arrGlobalAnimations[i].GetPathName();
				sprintf(token, strPathName);
				WOUT::WriteOut(token,stream );
			}
		}

		//---------------------------------------------------------------------------

		sprintf(token, "--------------------------------------------------------------");
		WOUT::WriteOut(token,stream );
		sprintf(token, "NOT USED LOCOMOTION GROUPS");
		WOUT::WriteOut(token,stream );
		sprintf(token, "--------------------------------------------------------------");
		WOUT::WriteOut(token,stream );
		for (uint32 i=0; i<nNumAnimHeaders; i++)
		{
			uint32 IsAssetLMG    = (g_AnimationManager.m_arrGlobalAnimations[i].m_nFlags&CA_ASSET_LMG)!=0;
			if (IsAssetLMG && (g_AnimationManager.m_arrGlobalAnimations[i].m_nPlayedCount==0))
			{
				const char* strPathName = g_AnimationManager.m_arrGlobalAnimations[i].GetPathName();
				sprintf(token, strPathName);
				WOUT::WriteOut(token,stream );
			}
		}

		fclose( stream );
	}

	//-----------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------
	{
		FILE* stream = fopen( "AnimStatistics_ASSETS.txt", "w+b" );
		sprintf(token, "--------------------------------------------------------------");
		WOUT::WriteOut(token,stream );
		sprintf(token, "USED ANIMATION ASSETS");
		WOUT::WriteOut(token,stream );
		sprintf(token, "--------------------------------------------------------------");
		WOUT::WriteOut(token,stream );
		for (uint32 i=0; i<nNumAnimHeaders; i++)
		{
			uint32 IsAssetLMG    = (g_AnimationManager.m_arrGlobalAnimations[i].m_nFlags&CA_ASSET_LMG)!=0;
			uint32 IsAssetLoaded = (g_AnimationManager.m_arrGlobalAnimations[i].m_nFlags&CA_ASSET_LOADED)!=0;
			if (IsAssetLoaded && (IsAssetLMG==0) && g_AnimationManager.m_arrGlobalAnimations[i].m_nPlayedCount)
			{
				const char* strPathName = g_AnimationManager.m_arrGlobalAnimations[i].GetPathName();
				sprintf(token, strPathName);
				WOUT::WriteOut(token,stream );
			}
		}

		//---------------------------------------------------------------------------

		sprintf(token, "--------------------------------------------------------------");
		WOUT::WriteOut(token,stream );
		sprintf(token, "NOT USED ANIMATION ASSETS");
		WOUT::WriteOut(token,stream );
		sprintf(token, "--------------------------------------------------------------");
		WOUT::WriteOut(token,stream );
		for (uint32 i=0; i<nNumAnimHeaders; i++)
		{
			uint32 IsAssetLMG    = (g_AnimationManager.m_arrGlobalAnimations[i].m_nFlags&CA_ASSET_LMG)!=0;
			uint32 IsAssetLoaded = (g_AnimationManager.m_arrGlobalAnimations[i].m_nFlags&CA_ASSET_LOADED)!=0;
			if (IsAssetLoaded && (IsAssetLMG==0) && (g_AnimationManager.m_arrGlobalAnimations[i].m_nPlayedCount==0))
			{
				const char* strPathName = g_AnimationManager.m_arrGlobalAnimations[i].GetPathName();
				sprintf(token, strPathName);
				WOUT::WriteOut(token,stream );
			}
		}
		fclose( stream );
	}

}

	// can be called instead of Update() for UI purposes (such as in preview viewports, etc).
void CharacterManager::DummyUpdate()
{
	m_nUpdateCounter++;
}

//! Locks all models in memory
void CharacterManager::LockResources()
{
	m_arrTempLock.clear();
	m_arrTempLock.resize (m_arrModelCache.size());
	for (size_t i = 0, end = m_arrModelCache.size(); i < end; ++i)
		m_arrTempLock[i] = m_arrModelCache[i];
}

//! Unlocks all models in memory
void CharacterManager::UnlockResources()
{
	m_arrTempLock.clear();
}


ICharacterInstance* CharacterManager::LoadCharacterDefinition( const string pathname, uint32 IsSkinAtt, CCharInstance* pSkelInstance  )
{
  LOADING_TIME_PROFILE_SECTION(g_pISystem);

	ICharacterInstance* pCharacter=0;

	//	__sgi_alloc::get_wasted_in_blocks();
	if (Console::GetInst().ca_MemoryUsageLog)
	{ 
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("CDF %s. Start. Memstat %i", pathname.c_str(), (int)(info.allocated - info.freed));
	}

	
	uint32 ind = -1;
	uint32 end = m_cdfFiles.size();
	for (uint32 i=0; i<end; i++)
	{
		if (m_cdfFiles[i].FilePath.compareNoCase(pathname) == 0)
		{
			ind = i;
			break;
		}
	}

	if (ind == -1)
	{
		ind = LoadCDF(pathname);
		if (ind == -1)
		{
			g_pILog->LogError ("CryAnimation: character-definition not found: %s", pathname.c_str());
			return 0;
		}

	}

	m_cdfFiles[ind].AddRef();
	uint32 numRefs = m_cdfFiles[ind].m_nRefCounter;
	CharacterDefinition def = m_cdfFiles[ind];

	if (!def.Model.empty()) 
	{
		string file = def.Model;
		pCharacter = CreateInstance( file );
		if (pCharacter==0)
		{
			assert("!filepath for character not found");
			g_pILog->LogError ("filepath for character not found: %s", file.c_str());
			return 0;
		}

		if (IsSkinAtt==0xDeadBeef)
		{
			uint32 dddd=0;
			((CCharInstance*)(pCharacter))->m_AttachmentManager.m_pSkelInstance=pSkelInstance;
		}

		if (!def.Material.empty())
		{
			_smart_ptr<IMaterial> pMaterial = g_pISystem->GetI3DEngine()->GetMaterialManager()->LoadMaterial(def.Material, false);
			pCharacter->SetMaterial(pMaterial);
		}

		//store the CGF-pathname inside the instance
		((CSkinInstance*)pCharacter)->m_strFilePath=pathname;

		//-----------------------------------------------------------
		//load attachment-list 
		IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();
		uint32 num = def.arrAttachments.size();
		for (uint32 i=0; i<num; i++) 
		{
			CharacterAttachment& attach = def.arrAttachments[i];
			IAttachment* pIAttachment=0;

			if (attach.Type==CA_BONE) 
			{
				pIAttachment = pIAttachmentManager->CreateAttachment(attach.Name,CA_BONE,attach.BoneName);
				if (pIAttachment==0) continue;

				pIAttachment->SetAttAbsoluteDefault(QuatT(attach.WRotation,attach.WPosition));
			//	pIAttachment->SetRMWRotation(attach.WRotation);

				string fileExt = PathUtil::GetExt( attach.BindingPath);

				bool IsCDF = (0 == stricmp(fileExt,"cdf"));
				bool IsCHR = (0 == stricmp(fileExt,"chr"));
				bool IsCGA = (0 == stricmp(fileExt,"cga"));
				bool IsCGF = (0 == stricmp(fileExt,"cgf"));
				if (IsCDF) 
				{
					ICharacterInstance* pIChildCharacter = LoadCharacterDefinition( attach.BindingPath );
					if (pIChildCharacter) 
					{
						CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
						pCharacterAttachment->m_pCharInstance = pIChildCharacter;
						IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
						pIAttachment->AddBinding(pIAttachmentObject);
					}
				}
				if (IsCHR||IsCGA) 
				{
					ICharacterInstance* pIChildCharacter = CreateInstance( attach.BindingPath );
					if (pIChildCharacter==0)
					{
						g_pILog->LogError ("CryAnimation: no character created: %s", pathname.c_str());
					} 
					else
					{
						CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
						pCharacterAttachment->m_pCharInstance  = pIChildCharacter;
						IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
						pIAttachment->AddBinding(pIAttachmentObject);
					}
				}
				if (IsCGF) 
				{
					IStatObj* pIStatObj = g_pISystem->GetI3DEngine()->LoadStatObj( attach.BindingPath );
					if (pIStatObj)
					{
						CCGFAttachment* pStatAttachment = new CCGFAttachment();
						pStatAttachment->pObj  = pIStatObj;
						IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pStatAttachment;
						pIAttachment->AddBinding(pIAttachmentObject);
					}
				}
			} 
			else
			if (attach.Type == CA_FACE) 
			{
				pIAttachment = pIAttachmentManager->CreateAttachment(attach.Name,CA_FACE);
				if (pIAttachment==0) continue;

				pIAttachment->SetAttAbsoluteDefault( QuatT(attach.WRotation,attach.WPosition) );

				string fileExt = PathUtil::GetExt( attach.BindingPath );

				bool IsCDF = (0 == stricmp(fileExt,"cdf"));
				bool IsCGF = (0 == stricmp(fileExt,"cgf"));
				bool IsCHR = (0 == stricmp(fileExt,"chr"));
				if (IsCDF) 
				{
					ICharacterInstance* pIChildCharacter = LoadCharacterDefinition( attach.BindingPath );
					if (pIChildCharacter) 
					{
						CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
						pCharacterAttachment->m_pCharInstance = pIChildCharacter;
						IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
						pIAttachment->AddBinding(pIAttachmentObject);
					}
				}
				if (IsCHR) 
				{
					ICharacterInstance* pIChildCharacter = CreateInstance( attach.BindingPath );
					if (pIChildCharacter==0)
					{
						g_pILog->LogError ("CryAnimation: no character created: %s", pathname.c_str());
					} 
					else
					{
						CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
						pCharacterAttachment->m_pCharInstance  = pIChildCharacter;
						IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
						pIAttachment->AddBinding(pIAttachmentObject);
					}
				}
				if (IsCGF) 
				{
					IStatObj* pIStatObj = g_pISystem->GetI3DEngine()->LoadStatObj( attach.BindingPath );
					if (pIStatObj)
					{
						CCGFAttachment* pStatAttachment = new CCGFAttachment();
						pStatAttachment->pObj  = pIStatObj;
						IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pStatAttachment;
						pIAttachment->AddBinding(pIAttachmentObject);
					}
				}
			}
			else
			if (attach.Type == CA_SKIN) 
			{
				pIAttachment = pIAttachmentManager->CreateAttachment(attach.Name,CA_SKIN);

				if (!pIAttachment)
					continue;

				pIAttachment->SetAttAbsoluteDefault( QuatT(attach.WRotation,attach.WPosition) );

				string fileExt = PathUtil::GetExt(attach.BindingPath);

				bool IsCDF = (0 == stricmp(fileExt,"cdf"));
				bool IsCHR = (0 == stricmp(fileExt,"chr"));
				if (IsCDF) 
				{
					ICharacterInstance* pIChildCharacter = LoadCharacterDefinition( attach.BindingPath, 0xDeadBeef, (CCharInstance*)pCharacter );
					if (pIChildCharacter) 
					{
						CCharInstance* pSkinAttachment = (CCharInstance*)pIChildCharacter;
						CCharInstance* pSkeleton =  pSkinAttachment->m_AttachmentManager.m_pSkelInstance;
						CSkinInstance* pSkin3 = (CCharInstance*)pCharacter;

						pSkinAttachment->m_AttachmentManager.m_pSkelInstance=(CCharInstance*)pCharacter;

						CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
						pCharacterAttachment->m_pCharInstance = pIChildCharacter;
						IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
						pIAttachment->AddBinding(pIAttachmentObject);
					}
				}
				if (IsCHR)
				{
					ICharacterInstance* pIChildCharacter = CreateInstance( attach.BindingPath, 0xDeadBeef, pIAttachment );
					if (pIChildCharacter==0)
					{
						g_pILog->LogError ("CryAnimation: no character created: %s", pathname.c_str());
					} 
					else 
					{
						if (pIChildCharacter->GetFacialInstance())
							pIChildCharacter->GetFacialInstance()->SetMasterCharacter(pCharacter);
						CCHRAttachment* pCharacterAttachment = new CCHRAttachment();
						pCharacterAttachment->m_pCharInstance  = pIChildCharacter;
						IAttachmentObject* pIAttachmentObject = (IAttachmentObject*)pCharacterAttachment;
						pIAttachment->AddBinding(pIAttachmentObject);
					}
				}

			}

			if (!attach.Material.empty() && pIAttachment != 0)
			{
				IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
				if (pIAttachmentObject != 0)
				{
					_smart_ptr<IMaterial> pMaterial = g_pISystem->GetI3DEngine()->GetMaterialManager()->LoadMaterial(attach.Material, false);
					pIAttachmentObject->SetMaterial(pMaterial);
				}
			}
			
			//----------------------------------------------------------------

			pIAttachment->SetFlags(attach.AttFlags);
			pIAttachment->HideAttachment(attach.AttFlags&nHideAttachment);

			pIAttachment->SetHingeParams(attach.Idx,attach.Limit,attach.Damping);

		}

		uint32 count = pIAttachmentManager->GetAttachmentCount();
		assert(count==num); //some attachments are invalid or attachment names are duplicated or whatever 
		pIAttachmentManager->ProjectAllAttachment();

		//-----------------------------------------------------------
		//load Shape-Deformation data
		//-----------------------------------------------------------
		f32* pMorphValues = pCharacter->GetShapeDeformArray();
		for (uint32 kk = 0; kk < 8; ++kk)
		{	
			pMorphValues[kk] = def.Morphing[kk];
		}
	}


	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("CDF. Finish. Memstat %i", (int)(info.allocated - info.freed));
	}

	return pCharacter;
}




int32 CharacterManager::LoadCDF(const char* pathname ) 
{

	XmlNodeRef root		= g_pISystem->LoadXmlFile(pathname);
	if (root==0)
	{
		g_pILog->LogError ("CryAnimation: character-definition creation failed: %s", pathname);
		return -1;
	}

	CharacterDefinition def;
	def.FilePath = pathname;
	//-----------------------------------------------------------
	//load model 
	//-----------------------------------------------------------
	XmlNodeRef nodeModel = root->getChild(0);
	if (nodeModel==0)
	{
		g_pILog->LogError ("CryAnimation: XmlNodeRef is zero: %s", pathname);
		return -1;
	}

	const char* modeltag = nodeModel->getTag();
	if (strcmp(modeltag,"Model")==0) 
	{
		def.Model =nodeModel->getAttr( "File" ); 

		const char* szMaterialName = 0;
		if (nodeModel->haveAttr("Material"))
			def.Material = nodeModel->getAttr("Material");
		//-----------------------------------------------------------
		//load attachment-list 
		//-----------------------------------------------------------
		uint32 ChildNo=1;
		XmlNodeRef nodeAttachList = root->getChild(ChildNo);
		const char* AttachListTag = nodeAttachList->getTag();
		if (strcmp(AttachListTag,"AttachmentList")==0) 
		{
			ChildNo++;

			uint32 num = nodeAttachList->getChildCount();
			for (uint32 i=0; i<num; i++) 
			{
				CharacterAttachment attach;

				XmlNodeRef nodeAttach = nodeAttachList->getChild(i);
				const char* AttachTag = nodeAttach->getTag();
				if (strcmp(AttachTag,"Attachment")==0) 
				{
					Quat WRotation(IDENTITY);
					Vec3 WPosition(ZERO);
					string AName = nodeAttach->getAttr( "AName" );
					CryStringUtils::UnifyFilePath(AName);
					attach.Name = AName;

					string Type  = nodeAttach->getAttr( "Type" );
					nodeAttach->getAttr( "Rotation",attach.WRotation );
					nodeAttach->getAttr( "Position",attach.WPosition );


					attach.BoneName = nodeAttach->getAttr( "BoneName" );
					attach.BindingPath = nodeAttach->getAttr( "Binding" );

					if (nodeAttach->haveAttr("Material"))
						attach.Material = nodeAttach->getAttr("Material");


					if (Type=="CA_BONE") 
					{
						attach.Type = CA_BONE;
					} 
					else
						if (Type=="CA_FACE") 
						{
							attach.Type = CA_FACE;
						}
						else
							if (Type=="CA_SKIN") 
							{
								attach.Type = CA_SKIN;
							}

							uint32 flags;
							if (nodeAttach->getAttr("Flags",flags))
								attach.AttFlags = flags;

							int idx=-1;
							float limit=120.0f,damping=5.0f;
							nodeAttach->getAttr("HingeIdx",attach.Idx);
							nodeAttach->getAttr("HingeLimit",attach.Limit);
							nodeAttach->getAttr("HingeDamping",attach.Damping);
							def.arrAttachments.push_back(attach);
				}
			}
		}

		//-----------------------------------------------------------
		//load Shape-Deformation data
		//-----------------------------------------------------------
		XmlNodeRef ShapeDeformation = root->getChild(ChildNo);
		const char* ShapeDefTag = ShapeDeformation->getTag();
		if (strcmp(ShapeDefTag,"ShapeDeformation")==0) 
		{
			ShapeDeformation->getAttr( "COL0",	def.Morphing[0] );
			ShapeDeformation->getAttr( "COL1",	def.Morphing[1] );
			ShapeDeformation->getAttr( "COL2",	def.Morphing[2] );
			ShapeDeformation->getAttr( "COL3",	def.Morphing[3] );
			ShapeDeformation->getAttr( "COL4",	def.Morphing[4] );
			ShapeDeformation->getAttr( "COL5",	def.Morphing[5] );
			ShapeDeformation->getAttr( "COL6",	def.Morphing[6] );
			ShapeDeformation->getAttr( "COL7",	def.Morphing[7] );
		}
	}

	m_cdfFiles.push_back(def);
	return m_cdfFiles.size() - 1;
}


void CharacterManager::ReleaseCDF(const char* pathname)
{

	uint32 ind = -1;
	uint32 end = m_cdfFiles.size();
	for (uint32 i=0; i<end; i++)
	{
		if (m_cdfFiles[i].FilePath.compareNoCase(pathname) == 0)
		{
			ind = i;
			break;
		}
	}

	if (ind != -1)
	{
		//found CDF-name
		assert(m_cdfFiles[ind].m_nRefCounter);
		m_cdfFiles[ind].m_nRefCounter--;
		if (m_cdfFiles[ind].m_nRefCounter==0)
		{
			if (ind)
			{
				for (uint32 i=ind+1; i<end; i++)
					m_cdfFiles[i-1]=m_cdfFiles[i];
			}
			m_cdfFiles.pop_back();
		}
	}

}









uint32 CharacterManager::SaveCharacterDefinition(ICharacterInstance* pCharacter, const char* pathname ) {

	XmlNodeRef root		= g_pISystem->CreateXmlNode( "CharacterDefinition" );	
	if (root==0)
	{
		g_pILog->LogError ("CryAnimation: character-definition creation failed: %s", pCharacter->GetFilePath());
		return 0;
	}

	//-----------------------------------------------------------
	//save model-filepath 
	//-----------------------------------------------------------
	XmlNodeRef nodeModel = root->newChild( "Model" );
	const char* fn = pCharacter->GetICharacterModel()->GetModelFilePath();
	nodeModel->setAttr("File",fn);
	if (pCharacter->GetMaterialOverride())
		nodeModel->setAttr("Material", pCharacter->GetMaterialOverride()->GetName());


	//-----------------------------------------------------------
	//save attachment-list 
	//-----------------------------------------------------------
	IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();
	uint32 count = pIAttachmentManager->GetAttachmentCount();

	XmlNodeRef nodeAttachements; 
	if (count) 
		nodeAttachements=root->newChild( "AttachmentList" );

	for (uint32 i=0; i<count; i++) 
	{
		IAttachment*  pIAttachment = pIAttachmentManager->GetInterfaceByIndex(i);

		XmlNodeRef nodeAttach = nodeAttachements->newChild( "Attachment" );
		const char* AName = pIAttachment->GetName();
		Quat WRotation = pIAttachment->GetAttAbsoluteDefault().q;
		Vec3 WPosition = pIAttachment->GetAttAbsoluteDefault().t;
		uint32 Type = pIAttachment->GetType();

		uint32 BoneID = pIAttachment->GetBoneID();
		const char* BoneName	= ((CCharInstance*)pCharacter)->m_SkeletonPose.GetJointNameByID(BoneID);	

		const char* BindingFilePath = "";

		if (Type==CA_BONE || Type==CA_FACE) 
		{

			IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
			if (pIAttachmentObject)
			{
				ICharacterInstance* pICharInstance = pIAttachmentObject->GetICharacterInstance();
				if (pICharInstance)
					BindingFilePath = pICharInstance->GetFilePath();

				IStatObj* pStaticObject = pIAttachmentObject->GetIStatObj();
				if (pStaticObject)	
					BindingFilePath = pStaticObject->GetFilePath();
			}
		}

		if (Type==CA_SKIN) {
			IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
			if (pIAttachmentObject)
			{
				ICharacterInstance* pICharacterChild=pIAttachmentObject->GetICharacterInstance();
				if (pICharacterChild) 
					BindingFilePath = pICharacterChild->GetFilePath();
			}
		}

		IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();
		const char* szMaterialName = 0;
		if (pIAttachmentObject)
		{
			IMaterial* pMaterial = pIAttachmentObject->GetMaterialOverride();
			if (pMaterial)
				szMaterialName = pMaterial->GetName();
		}

		nodeAttach->setAttr( "AName",AName );
		if (Type==CA_BONE) nodeAttach->setAttr( "Type", "CA_BONE" );
		if (Type==CA_FACE) nodeAttach->setAttr( "Type", "CA_FACE" );
		if (Type==CA_SKIN) nodeAttach->setAttr( "Type", "CA_SKIN" );
		nodeAttach->setAttr( "Rotation",WRotation );
		nodeAttach->setAttr( "Position",WPosition );
		nodeAttach->setAttr( "BoneName",BoneName );
		nodeAttach->setAttr( "Binding", BindingFilePath );

		uint32 nFlags = pIAttachment->GetFlags();
		if (pIAttachment->IsAttachmentHidden())	nFlags|=nHideAttachment;
		nodeAttach->setAttr( "Flags", nFlags );
		if (szMaterialName)
			nodeAttach->setAttr( "Material", szMaterialName );

		int idx; float limit,damping;
		pIAttachment->GetHingeParams(idx,limit,damping);
		if (idx>=0) {
			nodeAttach->setAttr( "HingeIdx",idx );
			nodeAttach->setAttr( "HingeLimit",limit );
			nodeAttach->setAttr( "HingeDamping",damping );
		}
	}

	//-----------------------------------------------------------
	//save Shape-Deformation data
	//-----------------------------------------------------------
	XmlNodeRef ShapeDeformation =	root->newChild( "ShapeDeformation" );
	f32* pMorphValues = pCharacter->GetShapeDeformArray();
	ShapeDeformation->setAttr( "COL0", pMorphValues[0] );
	ShapeDeformation->setAttr( "COL1", pMorphValues[1] );
	ShapeDeformation->setAttr( "COL2", pMorphValues[2] );
	ShapeDeformation->setAttr( "COL3", pMorphValues[3] );
	ShapeDeformation->setAttr( "COL4", pMorphValues[4] );
	ShapeDeformation->setAttr( "COL5", pMorphValues[5] );
	ShapeDeformation->setAttr( "COL6", pMorphValues[6] );
	ShapeDeformation->setAttr( "COL7", pMorphValues[7] );


	root->saveToFile(pathname);

	return 1;
}










///////////////////////////////////////////////////////////////////////////////////////////////////////
// CharacterManager
///////////////////////////////////////////////////////////////////////////////////////////////////////

CharacterManager::CharacterManager ()
{
	//m_OldAbsTime = 0;
	//m_NewAbsTime = 0;
	m_IsDedicatedServer=0;
	m_arrFrameTimes.resize(200);
	for (uint32 i=0; i<200; i++)
		m_arrFrameTimes[i]=0.014f;

	// default to no scaling of animation speeds, unless game requests it
	m_scalingLimits = Vec2(1.0f, 1.0f);

	m_pFacialAnimation = new CFacialAnimation;
	m_nUpdateCounter = 0;
}


CharacterManager::~CharacterManager()
{
	m_setLockedModels.clear();
	uint32 numModels = m_arrModelCache.size();
	if (numModels)
	{
		g_pILog->LogToFile("*ERROR* CharacterManager: %u body instances still not deleted. Forcing deletion (though some instances may still refer to them)", m_arrModelCache.size());
		CleanupModelCache();
	}

	delete m_pFacialAnimation;
	g_DeleteInterfaces();
}







//////////////////////////////////////////////////////////////////////////
void CharacterManager::SetFootPlantsFlag(CCharInstance* pCryCharInstance) 
{
	if (pCryCharInstance->m_pModel->m_pModelSkeleton->IsHuman())
		pCryCharInstance->m_SkeletonPose.SetFootPlants(1);
}


//////////////////////////////////////////////////////////////////////////
void CharacterManager::GetLoadedModels( ICharacterModel** pCharacterModels,int &nCount )
{
	if (!pCharacterModels)
	{
		nCount = (int)m_arrModelCache.size();
		return;
	}
	int num = min( (int)m_arrModelCache.size(),nCount );
	for (int i = 0; i < nCount; i++)
	{
		pCharacterModels[i] = m_arrModelCache[i];
	}
}
