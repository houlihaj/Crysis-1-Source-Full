//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File: AnimationManager.cpp
//  Implementation of Animation Manager.cpp
//
//	History:
//	January 12, 2005: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <CryHeaders.h>
#include <StlUtils.h>
#include <CryCompiledFile.h>

#include "LoaderCAF.h"
#include "ModelAnimationSet.h"
#include "LoaderCGA.h"
#include "LoaderDBA.h"
#include "ControllerPQ.h"
#include "ControllerOpt.h"

#include <StringUtils.h>
#include "CryPath.h"

stl::PoolAllocatorNoMT<sizeof(SAnimationSelectionProperties)> *g_Alloc_AnimSelectProps = 0;

GlobalAnimationHeader::~GlobalAnimationHeader() 
{

	ClearControllers();
	if (m_pSelectionProperties != NULL)
	{
		g_Alloc_AnimSelectProps->Deallocate(m_pSelectionProperties);
	}
};

void CAnimationManager::Reset()
{
	Clear();
}

void CAnimationManager::Clear()
{
	for(TLoadersArray::iterator it = m_arrLoaders.begin(), end = m_arrLoaders.end(); it != end; ++it)
	{
		if (*it)
		delete *it;
	}

	m_arrLoaders.clear();
}

/////////////////////////////////////////////////////////////////////
// finds the animation-asset by path-name. 
// Returns -1 if no animation was found.
// Returns the animation ID if it was found.
/////////////////////////////////////////////////////////////////////
int CAnimationManager::GetGlobalIDbyFilePath (const char * sAnimFileName)
{
	size_t id = m_AnimationMap.GetValue(sAnimFileName);
	if ( id != -1 && !stricmp(m_arrGlobalAnimations[id].GetPathName(),sAnimFileName))
		return id;
	else
		return -1;
}

////////////////////////////////////////////////////////////////////////////////////
// loads the animation with the specified name; if the animation is already loaded,
// then just returns its id
// The caller MUST TAKE CARE to bind the animation if it's already loaded before it has registered itself within this manager
// RETURNS:
//   The global animation id.
//   -1 if the animation couldn't be loaded 
// SIDE EFFECT NOTES:
//   This function does not put up a warning in the case the file was not found.
//   The caller must do so. But if the file was found and was corrupted, more detailed
//   error information (like where the error occured etc.) may be put into the log
int CAnimationManager::CreateGlobalMotionHeader (const string& strFilePath)
{
	int nAnimId = GetGlobalIDbyFilePath (strFilePath);
	bool bRecordExists = (nAnimId >= 0);
	if (bRecordExists && m_arrGlobalAnimations[nAnimId].IsAssetLoaded())
	{
		// we have already such file loaded
		return nAnimId;
	}
	else
	{
		selfValidate();
		if (bRecordExists==0)
		{
			// add a new animation structure that will hold the info about the new animation.
			// the new animation id is the index of this structure in the array

			m_arrGlobalAnimations.push_back(GlobalAnimationHeader());

			nAnimId = (int)m_arrGlobalAnimations.size() - 1;
			m_arrGlobalAnimations[nAnimId].SetPathName(strFilePath);

			m_AnimationMap.InsertValue(m_arrGlobalAnimations[nAnimId].GetCRC32(), nAnimId);

/*
nAnimId = (int)m_arrGlobalAnimations.size();
//			GlobalAnimationHeader * header = new GlobalAnimationHeader();
m_arrGlobalAnimations.push_back(GlobalAnimationHeader());
GlobalAnimationHeader * header =&m_arrGlobalAnimations[nAnimId];
header->SetPathName(strFilePath);

*/
		}

		selfValidate();
	}

	return nAnimId;
}


//////////////////////////////////////////////////////////////////////////
// Loads the animation controllers into the existing animation record.
// May be used to load the animation the first time, or reload it upon request.
// Does nothing if there are already controls in the existing animation record.
bool CAnimationManager::LoadAnimation(int nAnimId, eLoadingOptions flags)
{

	GlobalAnimationHeader& Anim = m_arrGlobalAnimations[nAnimId];
	uint32 numCtrl = Anim.GetControllersCount();//m_arrController.size();
	if (numCtrl)
		return true;

	if (strncmp(Anim.GetPathName(), NULL_ANIM_FILE, strlen(NULL_ANIM_FILE)) == 0)
		return LoadAnimationPQLogNull(nAnimId, Anim);
	else
	{
		bool res;

		if (g_pISystem->IsDevMode())
		{
			res = LoadAnimationFromFile(nAnimId, Anim, false, flags);

			if (!res)
				return LoadAnimationFromDBA(nAnimId, Anim, true);
		}
		else
		{
			res = LoadAnimationFromDBA(nAnimId, Anim, false);

			if (!res)
				return LoadAnimationFromFile(nAnimId, Anim, true, flags);

		}

		return res;
	}
}

//////////////////////////////////////////////////////////////////////////
// Loads (from file) the animation controllers into the existing animation record.
// May be used to load the animation the first time, or reload it upon request.
// Does nothing if there are already controls in the existing animation record.
bool CAnimationManager::LoadAnimationFromDBA(int nAnimId, GlobalAnimationHeader& Anim, bool bShowError)
{
	const CCommonSkinningInfo * pSkinningInfo = GetSkinningInfo(Anim.GetPathName());;//pLoader->GetSkinningInfo(Anim.GetPathName());

	if (pSkinningInfo == 0)
	{
		//PS3HACK
#ifndef PS3
		if (bShowError)
			g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,Anim.GetPathName(),	"Failed to load animation CAF file" );
#endif
		return false;
	}
	//----------------------------------------------------------------------------------------------
	GlobalAnimationHeader& rGlobalAnim = m_arrGlobalAnimations[nAnimId];
	rGlobalAnim.m_nTicksPerFrame	= TICKS_PER_FRAME;	//pSkinningInfo->m_nTicksPerFrame;
	rGlobalAnim.m_fSecsPerTick		= SECONDS_PER_TICK;	//pSkinningInfo->m_secsPerTick;
	rGlobalAnim.m_nStartKey				= pSkinningInfo->m_nStart;
	rGlobalAnim.m_nEndKey					= pSkinningInfo->m_nEnd;
	assert(rGlobalAnim.m_nTicksPerFrame==TICKS_PER_FRAME);

	int32 fTicksPerFrame  = rGlobalAnim.m_nTicksPerFrame;
	f32		fSecsPerTick		= rGlobalAnim.m_fSecsPerTick;
	f32		fSecsPerFrame		= fSecsPerTick * fTicksPerFrame;
	rGlobalAnim.m_fStartSec = rGlobalAnim.m_nStartKey * fSecsPerFrame;
	rGlobalAnim.m_fEndSec  = rGlobalAnim.m_nEndKey * fSecsPerFrame;
	if(rGlobalAnim.m_fEndSec<=rGlobalAnim.m_fStartSec)
		rGlobalAnim.m_fEndSec  = rGlobalAnim.m_fStartSec+(1.0f/30.0f);
	assert(rGlobalAnim.m_fStartSec>=0);
	assert(rGlobalAnim.m_fEndSec>=0);

	rGlobalAnim.m_fTotalDuration = rGlobalAnim.m_fEndSec - rGlobalAnim.m_fStartSec;
	assert(rGlobalAnim.m_fTotalDuration > 0);

	//----------------------------------------------------------------
	//if (0)
	{
		//Initialize from chunk. Don't initialize at loading-time
		rGlobalAnim.m_fSpeed		= pSkinningInfo->m_fSpeed;
		rGlobalAnim.m_vVelocity = Vec3(0,pSkinningInfo->m_fSpeed,0);
		rGlobalAnim.m_fDistance = pSkinningInfo->m_fDistance;
		rGlobalAnim.m_fSlope		= pSkinningInfo->m_fSlope;

		rGlobalAnim.m_FootPlantVectors.m_LHeelEnd		= pSkinningInfo->m_LHeelEnd;
		rGlobalAnim.m_FootPlantVectors.m_LHeelStart = pSkinningInfo->m_LHeelStart;
		rGlobalAnim.m_FootPlantVectors.m_LToe0End		= pSkinningInfo->m_LToe0End;
		rGlobalAnim.m_FootPlantVectors.m_LToe0Start = pSkinningInfo->m_LToe0Start;

		rGlobalAnim.m_FootPlantVectors.m_RHeelEnd		= pSkinningInfo->m_RHeelEnd;
		rGlobalAnim.m_FootPlantVectors.m_RHeelStart = pSkinningInfo->m_RHeelStart;
		rGlobalAnim.m_FootPlantVectors.m_RToe0End		= pSkinningInfo->m_RToe0End;
		rGlobalAnim.m_FootPlantVectors.m_RToe0Start = pSkinningInfo->m_RToe0Start;

		rGlobalAnim.m_bFromDatabase = true;

		//rGlobalAnim.m_MoveDirection = pSkinningInfo->m_MoveDirectionInfo;  //will be replace by velocity
		rGlobalAnim.m_StartLocation = pSkinningInfo->m_StartPosition;

		if (pSkinningInfo->m_Looped)
			rGlobalAnim.OnAssetCycle();

		rGlobalAnim.m_FootPlantBits.resize(pSkinningInfo->m_FootPlantBits.size());
		std::copy(pSkinningInfo->m_FootPlantBits.begin(), pSkinningInfo->m_FootPlantBits.end(),rGlobalAnim.m_FootPlantBits.begin());

		//std::copy(pSkinningInfo->m_FootPlantBits.begin(), pSkinningInfo->m_FootPlantBits.end(),rGlobalAnim.m_FootPlantBits.begin());
		//rGlobalAnim.m_FootPlantBits.assign(pSkinningInfo->m_FootPlantBits.begin(), pSkinningInfo->m_FootPlantBits.end());

		//	if (rGlobalAnim.m_fSpeed>=0 && (!rGlobalAnim.m_FootPlantBits.empty()))
		if (rGlobalAnim.m_fSpeed>=0 && rGlobalAnim.m_fDistance>=0)
			rGlobalAnim.OnAssetProcessed();
	}

	uint32 numController = pSkinningInfo->m_pControllers.size();
	rGlobalAnim.m_arrController.resize(numController);

	for(uint32 i=0; i<numController; i++ )
	{
		rGlobalAnim.m_arrController[i] = /*(IController*)*/pSkinningInfo->m_pControllers[i];
	}
	//if (numController != rGlobalAnim.m_arrController.size())
	//{
	//	g_pILog->LogError ("%d controllers (%d expected) loaded from file %s. Please re-export the file. The animations will be discarded.",
	//		numController, rGlobalAnim.m_arrController.size(), rGlobalAnim.GetPathName());
	//}

	std::sort(rGlobalAnim.m_arrController.begin(),	rGlobalAnim.m_arrController.end(), AnimCtrlSortPred()	);

	rGlobalAnim.OnAssetLoaded();

	//pLoader->RemoveSkinningInfo(Anim.GetPathName());
	//RemoveSkinningInfo(Anim.GetPathName());

	return true;
}


//////////////////////////////////////////////////////////////////////////
// Loads (from file) the animation controllers into the existing animation record.
// May be used to load the animation the first time, or reload it upon request.
// Does nothing if there are already controls in the existing animation record.
bool CAnimationManager::LoadAnimationFromFile(int nAnimId, GlobalAnimationHeader& Anim, bool bShowError, eLoadingOptions flags)
{
	//	CContentCGF* pCGF = g_pI3DEngine->LoadChunkFileContent( Anim.GetPathName() );

	CLoaderCAF loader;

	loader.SetLoadOnlyCommon(flags == eLoadOnlyInfo);
	loader.SetLoadOldChunks(Console::GetInst().ca_LoadUncompressedChunks > 0);

	CInternalSkinningInfo* pSkinningInfo  = loader.LoadCAF(Anim.GetPathName(), 0) ;//g_pI3DEngine->LoadChunkFileContent( geomName );

	if (pSkinningInfo==0)
	{
//PS3HACK
#ifndef PS3
		if (bShowError)
			g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,Anim.GetPathName(),	"Failed to load animation CAF file" );
#endif
		return 0;
	}

	if (Console::GetInst().ca_AnimWarningLevel > 1 && loader.GetHasOldControllers())
	{
		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,Anim.GetPathName(),	"Animation file has uncompressed data" );
	}

	if (Console::GetInst().ca_AnimWarningLevel > 0 && !loader.GetHasNewControllers())
	{
		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,Anim.GetPathName(),	"Animation file has no compressed data" );
	}

	//CSkinningInfo* pSkinningInfo = pCGF->GetSkinningInfo();
	//if (pSkinningInfo==0) 
	//	return 0;

	//----------------------------------------------------------------------------------------------

	GlobalAnimationHeader& rGlobalAnim = m_arrGlobalAnimations[nAnimId];
	rGlobalAnim.m_nTicksPerFrame	= TICKS_PER_FRAME;	//pSkinningInfo->m_nTicksPerFrame;
	rGlobalAnim.m_fSecsPerTick		= SECONDS_PER_TICK;	//pSkinningInfo->m_secsPerTick;
	rGlobalAnim.m_nStartKey				= pSkinningInfo->m_nStart;
	rGlobalAnim.m_nEndKey					= pSkinningInfo->m_nEnd;
	assert(rGlobalAnim.m_nTicksPerFrame==TICKS_PER_FRAME);

	int32 fTicksPerFrame  = rGlobalAnim.m_nTicksPerFrame;
	f32		fSecsPerTick		= rGlobalAnim.m_fSecsPerTick;
	f32		fSecsPerFrame		= fSecsPerTick * fTicksPerFrame;
	rGlobalAnim.m_fStartSec = rGlobalAnim.m_nStartKey * fSecsPerFrame;
	rGlobalAnim.m_fEndSec  = rGlobalAnim.m_nEndKey * fSecsPerFrame;
	if(rGlobalAnim.m_fEndSec<=rGlobalAnim.m_fStartSec)
		rGlobalAnim.m_fEndSec  = rGlobalAnim.m_fStartSec+(1.0f/30.0f);
	assert(rGlobalAnim.m_fStartSec>=0);
	assert(rGlobalAnim.m_fEndSec>=0);

	rGlobalAnim.m_fTotalDuration = rGlobalAnim.m_fEndSec - rGlobalAnim.m_fStartSec;
	assert(rGlobalAnim.m_fTotalDuration > 0);

	//----------------------------------------------------------------
	//if (0)
	{
		//Initialize from chunk. Don't initialize at loading-time
		rGlobalAnim.m_fSpeed		= pSkinningInfo->m_fSpeed;
		rGlobalAnim.m_vVelocity = Vec3(0,pSkinningInfo->m_fSpeed,0);
		rGlobalAnim.m_fDistance = pSkinningInfo->m_fDistance;
		rGlobalAnim.m_fSlope		= pSkinningInfo->m_fSlope;

		rGlobalAnim.m_FootPlantVectors.m_LHeelEnd		= pSkinningInfo->m_LHeelEnd;
		rGlobalAnim.m_FootPlantVectors.m_LHeelStart = pSkinningInfo->m_LHeelStart;
		rGlobalAnim.m_FootPlantVectors.m_LToe0End		= pSkinningInfo->m_LToe0End;
		rGlobalAnim.m_FootPlantVectors.m_LToe0Start = pSkinningInfo->m_LToe0Start;

		rGlobalAnim.m_FootPlantVectors.m_RHeelEnd		= pSkinningInfo->m_RHeelEnd;
		rGlobalAnim.m_FootPlantVectors.m_RHeelStart = pSkinningInfo->m_RHeelStart;
		rGlobalAnim.m_FootPlantVectors.m_RToe0End		= pSkinningInfo->m_RToe0End;
		rGlobalAnim.m_FootPlantVectors.m_RToe0Start = pSkinningInfo->m_RToe0Start;

		rGlobalAnim.m_bFromDatabase = false;

		//rGlobalAnim.m_MoveDirection = pSkinningInfo->m_MoveDirectionInfo; //will be replace by velocity
		rGlobalAnim.m_StartLocation = pSkinningInfo->m_StartPosition;


		if (pSkinningInfo->m_Looped)
			rGlobalAnim.OnAssetCycle();

		rGlobalAnim.m_FootPlantBits.resize(pSkinningInfo->m_FootPlantBits.size());
		std::copy(pSkinningInfo->m_FootPlantBits.begin(), pSkinningInfo->m_FootPlantBits.end(),rGlobalAnim.m_FootPlantBits.begin());
		//rGlobalAnim.m_FootPlantBits.assign(pSkinningInfo->m_FootPlantBits.begin(), pSkinningInfo->m_FootPlantBits.end());

		//	if (rGlobalAnim.m_fSpeed>=0 && (!rGlobalAnim.m_FootPlantBits.empty()))
		if (rGlobalAnim.m_fSpeed>=0 && rGlobalAnim.m_fDistance>=0)
			rGlobalAnim.OnAssetProcessed();
	}

	//----------------------------------------------------------------

	uint32 numController = pSkinningInfo->m_pControllers.size();
	rGlobalAnim.m_arrController.resize(numController);
	for(uint32 i=0; i<numController; i++ )
		rGlobalAnim.m_arrController[i] = (IController*)pSkinningInfo->m_pControllers[i];

	if (numController != rGlobalAnim.m_arrController.size())
	{
		g_pILog->LogError ("%d controllers (%d expected) loaded from file %s. Please re-export the file. The animations will be discarded.",
			numController, rGlobalAnim.m_arrController.size(), rGlobalAnim.GetPathName());
	}

	std::sort(rGlobalAnim.m_arrController.begin(),	rGlobalAnim.m_arrController.end(), AnimCtrlSortPred()	);

	rGlobalAnim.OnAssetLoaded();

	return 1;
}

//////////////////////////////////////////////////////////////////////////
// Write NULL animation controllers into the existing animation record.
// May be used to load the animation the first time, or reload it upon request.
// Does nothing if there are already controls in the existing animation record.
bool CAnimationManager::LoadAnimationPQLogNull(int nAnimId, GlobalAnimationHeader& Anim)
{

	//----------------------------------------------------------------------------------------------

	GlobalAnimationHeader& rGlobalAnim = m_arrGlobalAnimations[nAnimId];
	rGlobalAnim.m_nTicksPerFrame	= TICKS_PER_FRAME;
	rGlobalAnim.m_fSecsPerTick		= SECONDS_PER_TICK; //0.00020833334f; // Numbers taken from sample file.
	rGlobalAnim.m_nStartKey				= 0;
	rGlobalAnim.m_nEndKey					= 1;
	assert(rGlobalAnim.m_nTicksPerFrame==TICKS_PER_FRAME);

	int32 fTicksPerFrame  = rGlobalAnim.m_nTicksPerFrame;
	f32		fSecsPerTick		= rGlobalAnim.m_fSecsPerTick;
	f32		fSecsPerFrame		= fSecsPerTick * fTicksPerFrame;
	rGlobalAnim.m_fStartSec = rGlobalAnim.m_nStartKey * fSecsPerFrame;
	rGlobalAnim.m_fEndSec  = rGlobalAnim.m_nEndKey * fSecsPerFrame;
	if(rGlobalAnim.m_fEndSec<=rGlobalAnim.m_fStartSec)
		rGlobalAnim.m_fEndSec  = rGlobalAnim.m_fStartSec+(1.0f/30.0f);
	assert(rGlobalAnim.m_fStartSec>=0);
	assert(rGlobalAnim.m_fEndSec>=0);

	rGlobalAnim.m_fTotalDuration = rGlobalAnim.m_fEndSec - rGlobalAnim.m_fStartSec;
	assert(rGlobalAnim.m_fTotalDuration > 0);

	// At this point we do not know the skeleton. We return this partially constructed animation,
	// and the calling functions must add the controllers using knowledge of the skeleton.
	rGlobalAnim.ClearControllers();//m_arrController.clear();

	rGlobalAnim.OnAssetLoaded();

	return 1;
}


//////////////////////////////////////////////////////////////////////////
// Loads the animation controllers into the existing animation record.
// May be used to load the animation the first time, or reload it upon request.
// Does nothing if there are already controls in the existing animation record.
bool CAnimationManager::LoadAnimationTCB(int nAnimId, std::vector<CControllerTCB>& m_LoadCurrAnimation, CryCGALoader* pCGA, uint32 unique_model_id )
{
//	return false;

	GlobalAnimationHeader& rGlobalAnim = m_arrGlobalAnimations[nAnimId];
	rGlobalAnim.m_nTicksPerFrame	= TICKS_PER_FRAME; //pCGA->m_ticksPerFrame;
	rGlobalAnim.m_fSecsPerTick		= SECONDS_PER_TICK;//pCGA->m_secsPerTick;
	rGlobalAnim.m_nStartKey				= pCGA->m_start;
	rGlobalAnim.m_nEndKey					= pCGA->m_end;

	int32 fTicksPerFrame  = rGlobalAnim.m_nTicksPerFrame;
	f32		fSecsPerTick		= rGlobalAnim.m_fSecsPerTick;
	f32		fSecsPerFrame		= fSecsPerTick * fTicksPerFrame;
	rGlobalAnim.m_fStartSec = rGlobalAnim.m_nStartKey * fSecsPerFrame;
	rGlobalAnim.m_fEndSec  = rGlobalAnim.m_nEndKey * fSecsPerFrame;
	if(rGlobalAnim.m_fEndSec<=rGlobalAnim.m_fStartSec)
		rGlobalAnim.m_fEndSec  = rGlobalAnim.m_fStartSec+(1.0f/30.0f);

	rGlobalAnim.m_fTotalDuration = rGlobalAnim.m_fEndSec - rGlobalAnim.m_fStartSec;
	assert(rGlobalAnim.m_fTotalDuration > 0);

	uint32 numCtrl = rGlobalAnim.m_arrController.size();
	if (numCtrl)
		return true;

	//extern uint32 g_nControllerJID;
	uint32 numController = 0;//m_LoadCurrAnimation.size();

	for(uint32 i=0; i<m_LoadCurrAnimation.size(); i++)
	{
		if (!m_LoadCurrAnimation[i].m_posTrack.empty() || !m_LoadCurrAnimation[i].m_rotTrack.empty() ||	!m_LoadCurrAnimation[i].m_sclTrack.empty()) {
			++numController;
		}
	}
	rGlobalAnim.m_arrController.resize(numController);
	//uint32 nControllerJID=g_nControllerJID-numController;

	for(uint32 i=0, j = 0; i<m_LoadCurrAnimation.size(); i++)
	{
		if (!m_LoadCurrAnimation[i].m_posTrack.empty() || !m_LoadCurrAnimation[i].m_rotTrack.empty() ||	!m_LoadCurrAnimation[i].m_sclTrack.empty()) {
			CControllerTCB* pControllerTCB = new CControllerTCB();
			*pControllerTCB =	m_LoadCurrAnimation[i];
			//	pControllerTCB->m_ID=nControllerJID+i;
			pControllerTCB->m_ID=unique_model_id+i;	//g_nControllerJID+i;
			IController* pController = (IController*)pControllerTCB;
			rGlobalAnim.m_arrController[j] = static_cast<IController*>(pController);
			++j;
		}
	}

	std::sort( rGlobalAnim.m_arrController.begin(), rGlobalAnim.m_arrController.end(), AnimCtrlSortPred() );

	rGlobalAnim.OnAssetLoaded();

	return true;
}














// notifies the controller manager that this client doesn't use the given animation any more.
// these calls must be balanced with AnimationAddRef() calls
void CAnimationManager::AnimationRelease (int nGlobalAnimId, CAnimationSet* pClient)
{

	int32 numGlobalAnims = (int32)m_arrGlobalAnimations.size();
	assert (nGlobalAnimId<numGlobalAnims && nGlobalAnimId>=0);

	if (nGlobalAnimId<0)
		return;
	if (nGlobalAnimId>=numGlobalAnims)
		return;

	int32 RefCount2 = m_arrGlobalAnimations[nGlobalAnimId].m_nRefCount;

	// if the given client is still interested in unload events, it will receive them all anyway,
	// so we don't force anything but pure release. Normally during this call the client doesn't need
	// any information about the animation being released
	m_arrGlobalAnimations[nGlobalAnimId].Release();

	int32 RefCount = m_arrGlobalAnimations[nGlobalAnimId].m_nRefCount;
	if (RefCount==0)
		m_arrGlobalAnimations[nGlobalAnimId].m_nFlags = 0;
}



// returns the structure describing the animation data, given the global anim id
GlobalAnimationHeader& CAnimationManager::GetGlobalAnimationHeader (int nAnimID)
{
	if ((unsigned)nAnimID < m_arrGlobalAnimations.size())
		return m_arrGlobalAnimations[nAnimID];
	else
	{
		assert (0);
		static GlobalAnimationHeader dummy;
		return dummy;
	}
}

GlobalAnimationHeader& CAnimationManager::GlobalAnimationHeaderFromCRC(uint32 crc)
{
	size_t num = m_AnimationMap.GetValueCRC(crc);
	if (num != -1)
	{
		return m_arrGlobalAnimations[num];
	}
	else
	{
		assert (0);
		static GlobalAnimationHeader dummy;
		return dummy;
	}
}

uint32 CAnimationManager::GlobalIDFromCRC(uint32 crc)
{
	size_t num = m_AnimationMap.GetValueCRC(crc);
	return (uint32)num;
}


//void GlobalAnimationHeader::MakePM(int32 localAnimID, int32 globalAnimID)
//{
//	m_pPM = new CParamizedMotion(localAnimID, globalAnimID, *this);
//}

















// puts the size of the whole subsystem into this sizer object, classified,
// according to the flags set in the sizer
void CAnimationManager::GetSize(class ICrySizer* pSizer)
{
#if ENABLE_GET_MEMORY_USAGE
	SIZER_SUBCOMPONENT_NAME(pSizer, "Keys");
	size_t nSize = sizeof(CAnimationManager);
//	if (m_nCachedSizeofThis)
//		nSize = m_nCachedSizeofThis;
//	else
	{
		// approximately calculating the size of the map
		unsigned nMaxNameLength = 0;
		//nSize += sizeofArray (m_arrAnimByFile);
		nSize += m_AnimationMap.GetSize();
		for (std::vector<GlobalAnimationHeader>::const_iterator it = m_arrGlobalAnimations.begin(); it != m_arrGlobalAnimations.end(); ++it)
			nSize += it->SizeOfThis();

		//calculate size of DBAs

		TLoadersArray::iterator it = m_arrLoaders.begin();
		TLoadersArray::iterator end = m_arrLoaders.end();
		for (; it != end; ++it)
		{
			if (*it)
				nSize += (*it)->SizeOfThis();
		}


		//m_nCachedSizeofThis = nSize;
	}
	pSizer->AddObject(this, nSize);

#endif
}

size_t GlobalAnimationHeader::SizeOfThis ()const
{
	size_t nSize = sizeof(GlobalAnimationHeader) + sizeofVector(m_arrController)+ strlen(GetName()) + 1;
	nSize += sizeofVector(m_arrBSAnimations);

	for (int32 i = 0; i < m_arrBSAnimations.size(); ++i)
		nSize += m_arrBSAnimations[i].m_strAnimName.length();


	nSize += sizeofVector(m_arrAimIKPoses);
	nSize += sizeofVector(m_FootPlantBits);
	nSize += sizeofVector(m_AnimEvents);

	for (int32 i = 0; i < m_AnimEvents.size(); ++i)
	{
		nSize += m_AnimEvents[i].m_strEventName.length();
		nSize += m_AnimEvents[i].m_strCustomParameter.length();
		nSize += m_AnimEvents[i].m_strBoneName.length();
	}

	if (!m_bFromDatabase)
	{
		IController_AutoArray::const_iterator it = m_arrController.begin(), itEnd = it + m_arrController.size();
		for (; it != itEnd; ++it)
			nSize += (*it)->SizeOfThis();
	}

//	if (m_pPM)
//		nSize += m_pPM->SizeOfThis();

	return nSize;
}

void CAnimationManager::selfValidate()
{
#ifdef _DEBUG
	//assert (m_arrAnimByFile.size()==m_arrGlobalAnimations.size());
	//for (int i = 0; i < (int)m_arrAnimByFile.size()-1; ++i)
	//	assert (stricmp(m_arrGlobalAnimations[m_arrAnimByFile[i]].GetName(), m_arrGlobalAnimations[m_arrAnimByFile[i+1]].GetName()) < 0);

	//no need to do this. 
#endif
}


int32 CAnimationManager::GetAnimEventsCount(int32 nGlobalID) const
{
	int32 numAnims = (int32)m_arrGlobalAnimations.size();
	if (nGlobalID<0)
		return -1;
	if (nGlobalID >= numAnims)
		return -1;

	uint32 numAnimEvents = m_arrGlobalAnimations[nGlobalID].m_AnimEvents.size();
	return (int)numAnimEvents;
} 
int32 CAnimationManager::GetAnimEventsCount(const char* pFilePath )
{
	//string sAnimFileName = pFilePath;
	int32 nGlobalID = GetGlobalIDbyFilePath( pFilePath );
	return GetAnimEventsCount(nGlobalID);
}

int CAnimationManager::GetGlobalAnimID(const char* pFilePath)
{
	return this->GetGlobalIDbyFilePath(pFilePath);
}

void CAnimationManager::AddAnimEvent(int nGlobalID, const char* pName, const char* pParameter, const char* pBone, float fTime, const Vec3& vOffset, const Vec3& vDir)
{
	if (nGlobalID >= 0 && nGlobalID < int(m_arrGlobalAnimations.size()))
	{
		AnimEvents aevents;
		aevents.m_time=fTime;
		aevents.m_strCustomParameter=pParameter;
		aevents.m_strEventName=pName;
		aevents.m_strBoneName=pBone;
		aevents.m_vOffset=vOffset;
		aevents.m_vDir=vDir;
		m_arrGlobalAnimations[nGlobalID].m_AnimEvents.push_back(aevents);
	}
}

void CAnimationManager::DeleteAllEventsForAnimation(int nGlobalID)
{
	if (nGlobalID >= 0 && nGlobalID < int(m_arrGlobalAnimations.size()))
		m_arrGlobalAnimations[nGlobalID].m_AnimEvents.clear();
}

const CCommonSkinningInfo * CAnimationManager::GetSkinningInfo(const char * filename) const
{
	for (size_t i = 0, end = m_arrLoaders.size(); i < end; ++i)
	{
		const CCommonSkinningInfo * pInfo = m_arrLoaders[i]->GetSkinningInfo(filename);
		if (pInfo)
			return pInfo;
	}

	return 0;
}

//CLoaderDBA * LoadDBA(std::vector<string>& name);
bool CAnimationManager::LoadDBA(std::vector<string>& name, std::vector< CLoaderDBA* > * loaders)
{

	bool res(false);
	
	for (size_t i = 0, end = name.size(); i < end; ++i)
	{
		if (CLoaderDBA * loader = GetLoaderDBA(name[i]))
		{
			res = true;
			if (loaders)
				loaders->push_back(loader);
		}
	}

	return res;
}

void CAnimationManager::RemoveDBA( const string& file)
{
	RemoveLoaderDBA(file);
}

CLoaderDBA * CAnimationManager::GetLoaderDBA(const string& file)
{
	//std::vector<CLoaderDBA*> m_arrLoaders;

	for (TLoadersArray::iterator it = m_arrLoaders.begin(), end = m_arrLoaders.end(); it != end; ++it)
	{
		if (!(*it)->m_Filename.compareNoCase(file))
		{
			(*it)->AddRef();
			return *it;
		}
	}

	CLoaderDBA * pLoader(new CLoaderDBA);
	if (!pLoader->LoadDBA(file))
	{
		delete pLoader;
		pLoader = 0;
		return 0;
	}

	pLoader->AddRef();

	m_arrLoaders.push_back(pLoader);

	return pLoader;
}

void CAnimationManager::RemoveLoaderDBA( const string& file)
{
	for (TLoadersArray::iterator it = m_arrLoaders.begin(), end = m_arrLoaders.end(); it != end; ++it)
	{
		if (!(*it)->m_Filename.compareNoCase(file))
		{
			(*it)->Release();
			if ((*it)->GetRefcounter() <= 0)
			{
				//delete *it;
				//m_arrLoaders.erase(it);
				return;
			}
		}
	}

}

void CAnimationManager::RemoveSkinningInfo(const char * filename)
{
	for (size_t i = 0, end = m_arrLoaders.size(); i < end; ++i)
	{
		if (m_arrLoaders[i]->RemoveSkinningInfo(filename))
			return;
	}
}


void CAnimationManager::ClearChache()
{
	while(!GlobalAnimationHeader::m_arrGAHQueue.empty()) {
		GlobalAnimationHeader::m_arrGAHQueue.back()->ClearCache();
		GlobalAnimationHeader::m_arrGAHQueue.pop_back();
	}

}

void CAnimationManager::ClearStatistics()
{
	for (size_t i = 0, end = m_arrGlobalAnimations.size(); i < end; ++i) {
		m_arrGlobalAnimations[i].m_nPriority = 0;
//		m_arrGlobalAnimations[i].m_nPlayedCount = 0;
	}
}

bool CAnimationManager::GetGlobalAnimStatistics(size_t num, SAnimationStatistics& stat)
{
	if (num >= m_arrGlobalAnimations.size())
		return false;

	stat.count = m_arrGlobalAnimations[num].m_nPriority;
	stat.name = m_arrGlobalAnimations[num].GetPathName();
	return true;
}

// Unloads animation from memory and remove 
void CAnimationManager::UnloadAnimation(int nGLobalAnimID)
{
	for (uint32 i =0, end = m_arrClients.size(); i < end; ++i)
	{
		m_arrClients[i]->OnAnimationGlobalUnLoad(nGLobalAnimID);
	}

	m_arrGlobalAnimations[nGLobalAnimID].ClearControllers();//m_arrController.clear();
	m_arrGlobalAnimations[nGLobalAnimID].m_arrBSAnimations.clear();
	m_arrGlobalAnimations[nGLobalAnimID].m_FootPlantBits.clear();
	m_arrGlobalAnimations[nGLobalAnimID].ClearAssetLoaded();
}

void GlobalAnimationHeader::StartAnimation()
{
	if (!IsAssetOnDemand() && Console::GetInst().ca_UseAnimationsCache)
	{
		if (!m_vCache) {
			if ((int)m_arrGAHQueue.size() > Console::GetInst().ca_UseAnimationsCache) {
				if (!m_arrGAHQueue.empty() &&  this->m_nPriority > m_arrGAHQueue.back()->m_nPriority) {
					if (UnpackAnimation()) {
						m_arrGAHQueue.back()->ClearCache();
						m_arrGAHQueue.pop_back();
						PutInCache(this);
					}
				}
			} else {
				if (UnpackAnimation())
					PutInCache(this);
			}
		}
		//	//remove from cache list
		//	TGAHqueue::iterator it = std::lower_bound(m_arrGAHQueue.begin(), m_arrGAHQueue.end(), this);
		//	if (it != m_arrGAHQueue.end() && *it == this) {
		//		m_arrGAHQueue.erase(it);
		//	}
		//}
		//else {
		//	while (m_arrGAHQueue.size() > MAX_IN_MEMORY) {
		//		m_arrGAHQueue.back()->ClearCache();
		//		m_arrGAHQueue.pop_back();
		//	}
		//	UnpackAnimation();
		//}
	}
//	++m_nPlayedCount;
	++m_nPriority;
}


void GlobalAnimationHeader::StopAnimation()
{
//	--m_nPlayedCount;

// 	assert(m_nPlayedCount>=0);
//	if (!m_nPlayedCount && !IsAssetOnDemand() && m_vCache) {
////		ClearCache();
//		PutInCache(this);
//	}
}

TGAHqueue GlobalAnimationHeader::m_arrGAHQueue;



void GlobalAnimationHeader::PutInCache(GlobalAnimationHeader* ptr)
{
	TGAHqueue::iterator it = std::lower_bound(m_arrGAHQueue.begin(), m_arrGAHQueue.end(), ptr);
	if (it != m_arrGAHQueue.end() && *it == ptr)
		return;

	m_arrGAHQueue.push_back(ptr);
	std::sort(m_arrGAHQueue.begin(), m_arrGAHQueue.end(), GAHless()/*GAHCompare*/);

		//;;/insert(std::lower_bound(m_arrGAHQueue.begin(), m_arrGAHQueue.end(), ptr, GAHless()), ptr);
}

bool GlobalAnimationHeader::UnpackAnimation()
{
	DEFINE_PROFILER_FUNCTION();

	if(m_arrController.empty() || (m_arrController[0]->GetControllerType() != eControllerOpt && m_arrController[0]->GetControllerType() != eCController))
		return false;

	size_t count = m_arrController.size();
	m_arrControllerCache.resize(count);

	size_t size(0);
	for (size_t i = 0; i < count; ++i) {
		size += m_arrController[i]->GetRotationKeysNum();
	}

	assert(!m_vCache);
	m_vCache = new Quat[size]; 

	Quat * vStep = m_vCache;

	if (m_arrController[0]->GetControllerType() == eControllerOpt) {
		for (size_t i = 0; i < count; ++i) {

			IControllerOpt * pOldController = (IControllerOpt *)m_arrController[i].get();

			uint32 rotk = eNoCompressQuat;
			uint32 rotkt = pOldController->GetRotationKeyTimesFormat();
			uint32 posk = pOldController->GetPositionFormat();// eNoFormat;
			uint32 poskt = pOldController->GetPositionKeyTimesFormat() ;//eNoFormat;

			IControllerOpt_AutoPtr pController = 0;

			uint32 ss = m_arrController[i]->GetRotationKeysNum();

			if (rotk != pOldController->GetRotationFormat() && ss)	{

				Quat * vStepStart = vStep;

				for (size_t j = 0; j < ss; ++j, ++vStep) {
					*vStep = m_arrController[i]->GetOrientationByKey(j);
				}
			
				pController =  ControllerHelper::CreateController(rotk, rotkt, posk, poskt);
				
				pController->SetRotationKeyTimes(ss, pOldController->GetRotationKeyTimesData());
				pController->SetRotationKeys(ss, /*pOldController->GetRotationData()*/(char*)vStepStart);
				ss = m_arrController[i]->GetPositionKeysNum();
				pController->SetPositionKeyTimes(ss, pOldController->GetPositionKeyTimesData());
				pController->SetPositionKeys(ss, pOldController->GetPositionKeyData());


				//if (rotkt != eNoFormat)
				//{
				//	//pController->SetRotationKeyTimes(pktSizes[rk], &m_pDatabaseInfo->m_Storage[ktOffsets[rk]]);
				////pController->SetRotationKeys(prSizes[r], &m_pDatabaseInfo->m_Storage[rOffsets[r]]);//m_pDatabaseInfo->m_arrRotationTracks[r]->GetData());
				//}
				//if (posk != eNoFormat && poskt != eNoFormat)
				//{
				//	//pController->SetPositionKeyTimes(pktSizes[pk], &m_pDatabaseInfo->m_Storage[ktOffsets[pk]]);
				//	//pController->SetPositionKeys(ppSizes[p], &m_pDatabaseInfo->m_Storage[pOffsets[p]]);   ;//m_pDatabaseInfo->m_arrPositionTracks[p]->GetData());
				//}
			} else {
				pController = pOldController;//m_arrController[i];
			}

			pController->m_nControllerId = pOldController->m_nControllerId;
			m_arrControllerCache[i] = pController;
		}
	} else // CController 
	{
		for (size_t i = 0; i < count; ++i) {

			CController * pOldController = (CController *)m_arrController[i].get();

			uint32 rotk = eNoCompressQuat;// GetRotationFormat();
			uint32 rotkt = pOldController->GetRotationController()->GetKeyTimesInformation()->GetFormat();//   GetRotationKeyTimesFormat();
			uint32 posk = pOldController->GetPositionController()->GetFormat();//GetPositionFormat();// eNoFormat;
			uint32 poskt = pOldController->GetPositionController()->GetKeyTimesInformation()->GetFormat() ;//GetPositionKeyTimesFormat() ;//eNoFormat;

			IControllerOpt_AutoPtr pController = 0;

			uint32 ss = m_arrController[i]->GetRotationKeysNum();
			if (rotk != pOldController->GetRotationController()->GetFormat() && ss )	{

				Quat * vStepStart = vStep;

				for (size_t j = 0; j < ss; ++j, ++vStep) {
					*vStep = m_arrController[i]->GetOrientationByKey(j);
				}

				pController =  ControllerHelper::CreateController(rotk, rotkt, posk, poskt);
				pController->SetRotationKeyTimes(ss, pOldController->GetRotationController()->GetKeyTimesInformation()->GetData());
				pController->SetRotationKeys(ss, (char*)vStepStart);
				ss = m_arrController[i]->GetPositionKeysNum();
				pController->SetPositionKeyTimes(ss, pOldController->GetPositionController()->GetKeyTimesInformation()->GetData());
				pController->SetPositionKeys(ss, pOldController->GetPositionController()->GetData());
				m_arrControllerCache[i] = pController;
			} else {
				m_arrControllerCache[i] = pOldController;
			}
		}
	}

	std::sort(m_arrControllerCache.begin(),	m_arrControllerCache.end(), AnimCtrlSortPred()	);

	return true;
}

void GlobalAnimationHeader::ClearCache()
{
	m_arrControllerCache.clear();
	delete [] m_vCache;
  m_vCache = 0;
}

IController* GlobalAnimationHeader::GetControllerByJointID(uint32 nControllerID)
{
	/*
	uint32 numController = m_arrController.size();
	for (uint32 i=0; i<numController; i++) 
	{
	if ( m_arrController[i]->GetID()==nControllerID )
	return m_arrController[i];
	}
	return NULL;
	*/

	IController_AutoArray & controllerArray = m_arrControllerCache.empty() ? m_arrController : m_arrControllerCache;

	IController_AutoArray::iterator  it = std::lower_bound(controllerArray.begin(), controllerArray.end(), nControllerID, AnimCtrlSortPred());//, std::equal<uint32>());
	if (it == controllerArray.end() || (*it)->GetID() != nControllerID)
		return NULL;


	return *it;
}
