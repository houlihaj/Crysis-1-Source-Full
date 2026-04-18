/********************************************************************
CryGame Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   AIHandler.cpp
Version:     v1.00
Description: 

-------------------------------------------------------------------------
History:
- 8:10:2004   12:05 : Created by Kirill Bulatsev

*********************************************************************/



#include "StdAfx.h"
#include <ISound.h>
#include <IAISystem.h>
#include <ICryAnimation.h>
#include <ISerialize.h>
#include <IActorSystem.h>
#include <IAnimationGraph.h>
#include "ScriptBind_AI.h"
#include "AIHandler.h"
#include "AIReadabilityManager.h"


CAIReadabilityManager	CAIHandler::s_ReadabilityManager;


//
//----------------------------------------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------------------------------------
CAIReadabilityManager::CAIReadabilityManager( ) :
m_reloadID(0)
{

}


//
//----------------------------------------------------------------------------------------------------------
CAIReadabilityManager::~CAIReadabilityManager( )
{
}



//
//----------------------------------------------------------------------------------------------------------
void CAIReadabilityManager::Reload( )
{
	m_SoundPacks.clear();
	//	Load("korean_01.xml");
	string path = "Libs/Readability/Sound";
	ICryPak * pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	string fileName;

	string searchPath(path + "/*.xml");
	intptr_t handle = pCryPak->FindFirst( searchPath.c_str(), &fd );
	if (handle != -1)
	{
		do
		{
			fileName = path;
			fileName += "/" ;
			fileName += fd.name;
			Load(fileName);
		} while ( pCryPak->FindNext( handle, &fd ) >= 0 );
		pCryPak->FindClose( handle );
	}

	m_reloadID++;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIReadabilityManager::Load(const char* szPackName)
{
	XmlNodeRef root = GetISystem()->LoadXmlFile(szPackName);
	if (!root)
		return false;

	XmlNodeRef nodeWorksheet = root->findChild("Worksheet");
	if (!nodeWorksheet)
		return false;

	XmlNodeRef nodeTable = nodeWorksheet->findChild("Table");
	if (!nodeTable)
		return false;

	char packName[_MAX_PATH];
	int i = strlen(szPackName);
	while (--i >= 0 && szPackName[i]!='/')
		;
	int j(0);
	while (++i < _MAX_PATH-1 && szPackName[i] != '.')
		packName[j++] = szPackName[i];
	packName[j] = '\0';
	string lowerCasePackName(packName);
	lowerCasePackName.MakeLower();
	//	toLowerInplace(lowerCasePackName);
	m_SoundPacks[lowerCasePackName] = SoundReadabilityPack();
	SoundPacksMap::iterator iCurEntry(m_SoundPacks.find(lowerCasePackName));
	if (iCurEntry == m_SoundPacks.end())
	{
		return false;
	}
	SoundReadabilityPack& currentPack = iCurEntry->second;

	float	blockingTimePerPack;
	string basePath("");
	ReadabilitySoundGroup*	pCurReadabilitySignalTable = 0;
	string	curReadabilitySignal("");

	for (int rowCntr = 0, childN = 0; childN < nodeTable->getChildCount(); ++childN)
	{
		XmlNodeRef nodeRow = nodeTable->getChild(childN);
		if (!nodeRow->isTag("Row"))
			continue;
		++rowCntr;
		if (rowCntr == 1) // first row first cell should contain base path for the sounds
		{
			for(int childrenCntr(0),cellIndex(0); childrenCntr<nodeRow->getChildCount(); ++childrenCntr,++cellIndex)
			{
				XmlNodeRef nodeCell = nodeRow->getChild(childrenCntr);
				if (!nodeCell->isTag("Cell"))
					continue;
				XmlNodeRef nodeCellData = nodeCell->findChild("Data");
				if (!nodeCellData)
					continue;
				basePath = nodeCellData->getContent();
				//isBaseWave = basePath[basePath.size()-1] != ':';
			}
			continue;
		}
		if (rowCntr == 2) // the second row contains group delay for pack
		{
			for(int childrenCntr(0),cellIndex(0); childrenCntr<nodeRow->getChildCount(); ++childrenCntr,++cellIndex)
			{
				XmlNodeRef nodeCell = nodeRow->getChild(childrenCntr);
				if (!nodeCell->isTag("Cell"))
					continue;
				XmlNodeRef nodeCellData = nodeCell->findChild("Data");
				if (!nodeCellData)
					continue;
				const char* sTimeStr(nodeCellData->getContent());
				if(sTimeStr)
					sscanf(sTimeStr, "%f", &blockingTimePerPack);
				break;
			}
			continue;
		}
		if (rowCntr == 3) // Skip the second row, it only have description.
			continue;
		const char* szSignal = 0;
		const char* szSoundName = 0;
		float	blockingTime = -1.0f;
		int		blockingId = 9;
		int		probability = 0;
		bool	isGroup = false;
		bool	isResponse = false;

		for (int childrenCntr = 0,cellIndex = 1; childrenCntr < nodeRow->getChildCount(); ++childrenCntr,++cellIndex)
		{
			XmlNodeRef nodeCell = nodeRow->getChild(childrenCntr);
			if (!nodeCell->isTag("Cell"))
				continue;

			if (nodeCell->haveAttr("ss:Index"))
			{
				const char *pStrIdx=nodeCell->getAttr("ss:Index");
				sscanf(pStrIdx, "%d", &cellIndex);
			}
			XmlNodeRef nodeCellData = nodeCell->findChild("Data");
			if (!nodeCellData)
				continue;

			switch (cellIndex)
			{
			case 1:		// Readability Signal name
				szSignal = nodeCellData->getContent();
				break;
			case 2:		// The sound name
				szSoundName = nodeCellData->getContent();
				break;
			case 3:		// Probability (optional)
				{
					const char* item(nodeCellData->getContent());
					if(item)
						sscanf(item, "%d", &probability);
				}
				break;
			case 4:		// Blocking Time (optional)
				{
					const char* item(nodeCellData->getContent());
					if (item)
						sscanf(item, "%f", &blockingTime);
				}
				break;
			case 5:		// Blocking ID (optional)
				{
					const char* item(nodeCellData->getContent());
					if (item)
						sscanf(item, "%d", &blockingId);
				}
				break;
			case 6:		// Sound Type
				{
					const char* item(nodeCellData->getContent());
					if (item)
					{
						if (stricmp(item, "group") == 0)
							isGroup = true;
						else if (stricmp(item, "response") == 0)
							isResponse = true;
					}
				}
				break;
			}
		}

		// If the signal name is specified, create new readability signal. Otherwise use the previously defined one.
		if (szSignal != NULL)
		{
			const std::pair<SoundReadabilityPack::iterator, bool> insertIter = currentPack.insert(SoundReadabilityPack::value_type(szSignal, ReadabilitySoundGroup()));
			SoundReadabilityPack::iterator iCurEntry = insertIter.first;
			assert(iCurEntry != currentPack.end());
			if (insertIter.second == false)
			{
				gEnv->pAISystem->Warning("<CAIReadabilityManager::Load> ", "Duplicated Readability Signal '%s' in file '%s'.", szSignal, szPackName);		
				iCurEntry->second = ReadabilitySoundGroup(); // override former
			}

			pCurReadabilitySignalTable = &(iCurEntry->second);
			if (blockingTime < 0.f)
				pCurReadabilitySignalTable->m_blockingTime = blockingTimePerPack;
			else
				pCurReadabilitySignalTable->m_blockingTime = blockingTime;
			pCurReadabilitySignalTable->m_blockingID = blockingId;
		}

		// Signal name not yet specified or the row did not specify the sound name.
		if (!pCurReadabilitySignalTable || !szSoundName)
			continue;

		bool voice = false;
		string fullFilename = basePath + szSoundName;
		if (fullFilename.find_first_of(':') == basePath.npos)
		{
			fullFilename += ".wav";
			voice = true;
		}

		probability = max(1, probability);
		if (isGroup)
			pCurReadabilitySignalTable->m_groupSounds.push_back(ReadabilitySoundEntry(fullFilename, probability, voice));
		else if (isResponse)
			pCurReadabilitySignalTable->m_responseSounds.push_back(ReadabilitySoundEntry(fullFilename, probability, voice));
		else
			pCurReadabilitySignalTable->m_aloneSounds.push_back(ReadabilitySoundEntry(fullFilename, probability, voice));

	}
	return true;
}


//
//----------------------------------------------------------------------------------------------------------
CAIReadabilityManager::SoundReadabilityPack* CAIReadabilityManager::FindSoundPack(const char* name)
{
	if (!name || name[0] == '\0')
		return 0;

	string lowerCasePackName(name);
	lowerCasePackName.MakeLower();
	//	toLowerInplace(lowerCasePackName);
	SoundPacksMap::iterator it(m_SoundPacks.find(lowerCasePackName));

	if (it == m_SoundPacks.end())
	{
		gEnv->pAISystem->Warning("<CAIReadabilityManager::FindPack> ", "Can not find sound pack '%s'.", lowerCasePackName.c_str());		
		return NULL;
	}
	return &(it->second);
}

//
//----------------------------------------------------------------------------------------------------------
const char* CAIReadabilityManager::GetSoundPackName(CAIReadabilityManager::SoundReadabilityPack* pPack)
{
	for (SoundPacksMap::iterator it = m_SoundPacks.begin(), end = m_SoundPacks.end(); it != end; ++it)
	{
		SoundReadabilityPack* pCurPack = &(it->second);
		if (pCurPack == pPack)
		{
			return it->first.c_str();
		}
	}
	return "<not found>";
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIReadabilityManager::HasResponseSound(CAIHandler* pActor, const char* text)
{
	if (!pActor->GetSoundPack())
		return false; // no sound pack

	SoundReadabilityPack::iterator itrCurGroup(pActor->GetSoundPack()->find(CONST_TEMP_STRING(text)));
	if (itrCurGroup == pActor->GetSoundPack()->end())
		return false;

	ReadabilitySoundGroup& curGroup(itrCurGroup->second);
	return !curGroup.m_responseSounds.empty();
}

//
//----------------------------------------------------------------------------------------------------------
void CAIReadabilityManager::GetReadabilityBlockingParams(CAIHandler* pActor, const char* text, float& time, int& id)
{
	time = 0.0f;
	id = 0;

	IEntity* pEntity = pActor->m_pEntity;
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return;

	CAIReadabilityManager::SoundReadabilityPack* soundPack = pActor->GetSoundPack();
	if (!soundPack)
		return; // no sound pack

	SoundReadabilityPack::iterator itrCurGroup(soundPack->find(CONST_TEMP_STRING(text)));
	if (itrCurGroup == soundPack->end())
		return;

	time = itrCurGroup->second.m_blockingTime;
	id = itrCurGroup->second.m_blockingID;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIReadabilityManager::PlayReadabilitySound(CAIHandler* pActor, const char* text, int readabilityType, bool stopPreviousSound)
{
	bool playSoundAtActorTarget = readabilityType == SIGNALFILTER_READIBILITYAT;
	IEntity* pEntity = pActor->m_pEntity;
	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (!pAIActor)
		return false;

	CAIReadabilityManager::SoundReadabilityPack* soundPack = pActor->GetSoundPack();
	if (!soundPack)
		return false; // no sound pack

	SoundReadabilityPack::iterator itrCurGroup(soundPack->find(CONST_TEMP_STRING(text)));

	if (itrCurGroup == soundPack->end())
	{
		gEnv->pAISystem->Warning("<CAIReadabilityManager> ", "Entity:'%s' Pack:'%s' Readability:'%s' - Cannot find sound table entry.", pEntity->GetName(), GetSoundPackName(soundPack), text);		
		return false;
	}
	ReadabilitySoundGroup& curGroup = itrCurGroup->second;
	ReadabilitySoundGroup::ESoundType	type = ReadabilitySoundGroup::SND_ALONE;
	if (readabilityType != SIGNALFILTER_READIBILITYRESPONSE)
	{
		// Check if the sound should be played as group sound.
		int groupId = pAI->GetGroupId();
		int count = gEnv->pAISystem->GetGroupCount(groupId, IAISystem::GROUP_ENABLED, AIOBJECT_PUPPET) + 
			gEnv->pAISystem->GetGroupCount(groupId, IAISystem::GROUP_ENABLED, AIOBJECT_PLAYER);
		if (count > 1)
			type = ReadabilitySoundGroup::SND_GROUP;
	}
	else
	{
		// Responses are sent only once per successful normal readability, no need to filter them.
		type = ReadabilitySoundGroup::SND_RESPONSE;
	}

	ReadabilitySoundEntry* curSoundEntry = curGroup.GetMostLikelySound(type);
	if (!curSoundEntry)
	{
		gEnv->pAISystem->Warning("<CAIReadabilityManager> ", "Entity:'%s' Pack:'%s' Readability:'%s' - Cannot find sound in group for specified signal. Sound count - alone:%d group:%d response:%d).", pEntity->GetName(), GetSoundPackName(soundPack), text, curGroup.m_aloneSounds.size(), curGroup.m_groupSounds.size(), curGroup.m_responseSounds.size());
		return false;
	}

	gEnv->pAISystem->LogComment("<CAIReadabilityManager> ", "Entity:'%s' Pack:'%s' Readability:'%s' - Play sound:'%s'", pEntity->GetName(), GetSoundPackName(soundPack), text, curSoundEntry->m_fileName.c_str() );
	return PlaySound(pActor, curSoundEntry, playSoundAtActorTarget, stopPreviousSound);
}

bool CAIReadabilityManager::PlaySound(CAIHandler* pActor, ReadabilitySoundEntry* curSoundEntry, bool playSoundAtActorTarget, bool stopPreviousSound)
{
	ISoundSystem *pSoundSystem=gEnv->pSoundSystem;
	if (!pSoundSystem) // || !m_pGame->m_p3DEngine)
		return false; // no sound can be played anyway

	IEntity	 *pEntity(pActor->m_pEntity);
	IAIObject* pAI = pEntity->GetAI();
	if(!pAI )//|| !pAI->IsEnabled())
		return false;

	bool hasSound(false);

	int nSoundLength = 0; // on sound event this will only be a rough guess
	if (!playSoundAtActorTarget)
	{
		IEntitySoundProxy* pSoundProxy = (IEntitySoundProxy*) pEntity->GetProxy( ENTITY_PROXY_SOUND );
		if(!pSoundProxy)
			if (pEntity->CreateProxy(ENTITY_PROXY_SOUND ))
				pSoundProxy = (IEntitySoundProxy*)pEntity->GetProxy(ENTITY_PROXY_SOUND);

		if (pSoundProxy)
		{
			// Stop the previously playing sound.
			if (stopPreviousSound && pActor->m_ReadibilitySoundID != INVALID_SOUNDID)
			{
				pSoundProxy->StopSound(pActor->m_ReadibilitySoundID);
				pActor->m_ReadibilitySoundID = INVALID_SOUNDID;
			}

			// sound proxy uses head pos on dialog sounds
			ISound *pSound = gEnv->pSoundSystem->CreateSound(curSoundEntry->m_fileName.c_str(),FLAG_SOUND_DEFAULT_3D | (curSoundEntry->voice ? FLAG_SOUND_VOICE : FLAG_SOUND_EVENT));

			if (pSound)
			{
				pActor->m_ReadibilitySoundID = pSound->GetId();
				pSound->AddEventListener( pActor, "AIReadibilityManager" );
				pSound->SetSemantic(eSoundSemantic_AI_Readability);
				nSoundLength = pSound->GetLengthMs();
				pActor->m_bSoundFinished = false;
				hasSound = true;
				pSoundProxy->PlaySound(pSound, Vec3(ZERO), FORWARD_DIRECTION);
			}
			else
			{
				// failed to play the sound.
				gEnv->pAISystem->Warning("<CAIReadabilityManager> ", "Entity:'%s' Pack:'%s' -  Cannot play sound:'%s'", pEntity->GetName(), GetSoundPackName(pActor->GetSoundPack()), curSoundEntry->m_fileName.c_str());
				pActor->m_bSoundFinished = true;
			}
		}
	}
	else
	{
		// Sound Proxy should always be available; This else branch could be removed completely.
		_smart_ptr<ISound> pReadibilitySound = pSoundSystem->CreateSound(curSoundEntry->m_fileName.c_str(),FLAG_SOUND_3D);
		if (pReadibilitySound)
		{
			IPipeUser* pPipeUser = pAI->CastToIPipeUser();
			if (pPipeUser)
			{
				IAIObject* pTarget = pPipeUser->GetAttentionTarget();
				if(pTarget)
					pReadibilitySound->SetPosition(pTarget->GetPos());
				else
					pReadibilitySound->SetPosition(pEntity->GetWorldPos()); // backup
			}
			else
				pReadibilitySound->SetPosition(pEntity->GetPos());
			
			pReadibilitySound->SetSemantic(eSoundSemantic_AI_Readability);
			pReadibilitySound->AddEventListener( pActor, "AIReadibilityManager" );
			nSoundLength = pReadibilitySound->GetLengthMs();
			pReadibilitySound->Play();
			pActor->m_ReadibilitySoundID = pReadibilitySound->GetId();
			if( pActor->m_ReadibilitySoundID != INVALID_SOUNDID )
				hasSound = true;
		}
		else
		{
			// failed to play the sound.
			gEnv->pAISystem->Warning("<CAIReadabilityManager> ", "Entity:'%s' - Cannot play sound at target:'%s'", pEntity->GetName(), curSoundEntry->m_fileName.c_str());
		}
	}
	/*
	if (pActor->m_ReadibilitySoundID != INVALID_SOUNDID) // loading successful ?
	{
	if(m_pScriptObject)
	{
	bool bCheckReadabilityLength = false;
	if( m_pScriptObject->GetValue("bCheckReadabilityLength",bCheckReadabilityLength) && bCheckReadabilityLength)
	m_pScriptObject->SetValue("speakingTime",nSoundLength);
	}
	}
	*/
	return hasSound;
}

//
//----------------------------------------------------------------------------------------------------------
/*unsigned int CAIReadabilityManager::ReadabilitySoundGroup::GetPlayListIndex(t_PlayList& list, unsigned int n)
{
	if(list.empty())
	{
		list.reserve(n);
		for(unsigned int i = 0; i < n; ++i)
			list.push_back(i);
		std::random_shuffle(list.begin(), list.end());
	}
	unsigned int res = list.back();
	list.pop_back();
	return res;
}*/

CAIReadabilityManager::ReadabilitySoundEntry* CAIReadabilityManager::ReadabilitySoundGroup::GetRandomSound(CAIReadabilityManager::ReadabilitySoundGroup::SoundVector& sounds, CAIReadabilityManager::ReadabilitySoundGroup::SoundHistory& hist)
{
	unsigned numSounds = sounds.size();
	if (!numSounds)
		return 0;

	int totalProb = 0;
	for (unsigned i = 0; i < numSounds; ++i)
		totalProb += sounds[i].m_probability;

	if (totalProb == 0)
	{
		gEnv->pAISystem->Warning("<GetRandomSound>", "Probabilities of all sounds in a group are zero!");
		return &sounds[0];
	}

	int idx = -1;
	do
	{
		// Choose random item based on the probability.
		int r = Random(totalProb);
		int rangeMin = 0;
		int rangeMax = 0;
		for (unsigned i = 0; i < numSounds; ++i)
		{
			rangeMin = rangeMax;
			rangeMax += sounds[i].m_probability;
			if (r >= rangeMin && r < rangeMax)
			{
				idx = (int)i;
				break;
			}
		}

		// If the sound was played recently, try again.
		if (numSounds > hist.MAX_SIZE)
		{
			if (hist.Find(idx))
				idx = -1;
		}
	}
	while (idx == -1);

	// Sound found, mark it being used recently.
	hist.Add(idx);

	return &sounds[idx];
}

//
//----------------------------------------------------------------------------------------------------------
CAIReadabilityManager::ReadabilitySoundEntry* CAIReadabilityManager::ReadabilitySoundGroup::GetMostLikelySound(ESoundType type)
{
	if (type == SND_ALONE)
	{
		// Allow to fall back to group sounds if alone sounds are not specified for this sound group.
		if (!m_aloneSounds.empty())
			return GetRandomSound(m_aloneSounds, m_lastAloneSounds);
		else if (!m_groupSounds.empty())
			return GetRandomSound(m_groupSounds, m_lastGroupSounds);
	}
	else if (type == SND_GROUP)
	{
		// Allow to fall back to alone sounds if group sounds are not specified for this sound group.
		if (!m_groupSounds.empty())
			return GetRandomSound(m_groupSounds, m_lastGroupSounds);
		else if (!m_aloneSounds.empty())
			return GetRandomSound(m_aloneSounds, m_lastAloneSounds);
	}
	else if (type == SND_RESPONSE)
	{
		if (!m_responseSounds.empty())
			return GetRandomSound(m_responseSounds, m_lastResponseSounds);
	}

	return 0;
}

bool CAIReadabilityManager::PrepareReadabilityPackTest(CAIHandler* pActor, ReadabilityTestIter& iter)
{
	IEntity* pEntity(pActor->m_pEntity);

	CAIReadabilityManager::SoundReadabilityPack* soundPack = pActor->GetSoundPack();
	if (!soundPack)
	{
		gEnv->pAISystem->Warning("<TestReadabilityPack>", "Entity '%s' does not have AI readability.", pEntity->GetName());
		return false;
	}

	iter.sounds.clear();
	iter.idx = 0;

	// Collect all sounds to be played.
	SoundReadabilityPack::iterator gend(soundPack->end());
	SoundReadabilityPack::iterator git(soundPack->begin());
	for ( ; git != gend; ++git)
	{
		ReadabilitySoundGroup& group(git->second);

		// Alone sounds
		for (unsigned i = 0, ni = group.m_aloneSounds.size(); i < ni; ++i)
			iter.sounds.push_back(&group.m_aloneSounds[i]);

		// Group sounds
		for (unsigned i = 0, ni = group.m_groupSounds.size(); i < ni; ++i)
			iter.sounds.push_back(&group.m_groupSounds[i]);

		// Response sounds
		for (unsigned i = 0, ni = group.m_responseSounds.size(); i < ni; ++i)
			iter.sounds.push_back(&group.m_responseSounds[i]);
	}

	if (iter.sounds.empty())
	{
		gEnv->pAISystem->Warning("<TestReadabilityPack> ", "Entity %s - No readability sounds available.", pEntity->GetName());
		return false;
	}

	return true;
}

bool CAIReadabilityManager::PrepareReadabilityPackTest(CAIHandler* pActor, ReadabilityTestIter& iter, const char* szReadability)
{
	IEntity* pEntity(pActor->m_pEntity);

	CAIReadabilityManager::SoundReadabilityPack* soundPack = pActor->GetSoundPack();
	if (!soundPack)
	{
		gEnv->pAISystem->Warning("<TestReadabilityPack>", "Entity '%s' does not have AI readability.", pEntity->GetName());
		return false;
	}

	iter.sounds.clear();
	iter.idx = 0;

	// Collect all sounds to be played.
	SoundReadabilityPack::iterator itrCurGroup(soundPack->find(CONST_TEMP_STRING(szReadability)));
	if (itrCurGroup == soundPack->end())
	{
		gEnv->pAISystem->Warning("<TestReadabilityPack> ", "Entity:%s Pack:%s Readability:%s - Cannot find sound table entry.", pEntity->GetName(), GetSoundPackName(soundPack), szReadability);
		return false;
	}


	ReadabilitySoundGroup& group = itrCurGroup->second;

	// Alone sounds
	for (unsigned i = 0, ni = group.m_aloneSounds.size(); i < ni; ++i)
		iter.sounds.push_back(&group.m_aloneSounds[i]);

	// Group sounds
	for (unsigned i = 0, ni = group.m_groupSounds.size(); i < ni; ++i)
		iter.sounds.push_back(&group.m_groupSounds[i]);

	// Response sounds
	for (unsigned i = 0, ni = group.m_responseSounds.size(); i < ni; ++i)
		iter.sounds.push_back(&group.m_responseSounds[i]);

	if (iter.sounds.empty())
	{
		gEnv->pAISystem->Warning("<TestReadabilityPack> ", "Entity: %s Pack:%s Readability:%s - No readability sounds available.", pEntity->GetName(), GetSoundPackName(soundPack), szReadability);
		return false;
	}

	return true;
}

CAIReadabilityManager::ETestResult CAIReadabilityManager::TestReadabilityPack(CAIHandler* pActor, CAIReadabilityManager::ReadabilityTestIter& iter)
{
	IEntity* pEntity(pActor->m_pEntity);

	if (!pActor->GetSoundPack())
	{
//		gEnv->pAISystem->Warning("<TestReadabilityPack>", "Entity '%s' does not have AI readability.", pEntity->GetName());
		return TEST_DONE;
	}

	if(iter.idx < 0)
		iter.idx = 0;

/*	if(iter.sounds.empty() || iter.id != m_reloadID)
	{
		iter.sounds.clear();
		iter.idx = 0;
		iter.id = m_reloadID;
		// Collect all sounds to be played.
		SoundReadabilityPack::iterator gend(pActor->m_pMysoundPack->end());
		SoundReadabilityPack::iterator git(pActor->m_pMysoundPack->begin());
		for( ; git != gend; ++git)
		{
			ReadabilitySoundGroup& group(git->second);

			ReadabilitySoundGroup::t_Sounds::iterator send;
			ReadabilitySoundGroup::t_Sounds::iterator sit;

			// Alone sounds
			send = group.m_aloneSounds.end();
			for(sit = group.m_aloneSounds.begin(); sit != send; ++sit)
				iter.sounds.push_back(&(*sit));

			// Group sounds
			send = group.m_groupSounds.end();
			for(sit = group.m_groupSounds.begin(); sit != send; ++sit)
				iter.sounds.push_back(&(*sit));

			// Response sounds
			send = group.m_responseSounds.end();
			for(sit = group.m_responseSounds.begin(); sit != send; ++sit)
				iter.sounds.push_back(&(*sit));
		}
	}*/

	if(iter.sounds.empty())
	{
//		gEnv->pAISystem->Warning("<TestReadabilityPack> ", "Entity %s: No readability sounds available.", pEntity->GetName());
		return TEST_DONE;
	}

	int	maxSounds((int)iter.sounds.size());
	if(iter.idx >= maxSounds)
		return TEST_DONE;

	ETestResult	res(TEST_SUCCEED);

	if(PlaySound(pActor, iter.sounds[iter.idx], false, true))
	{
		gEnv->pAISystem->LogComment("<TestReadabilityPack> ", "%d/%d - Playing readability sound '%s'", iter.idx + 1, maxSounds, iter.sounds[iter.idx]->m_fileName.c_str());		
		res = TEST_SUCCEED;
	}
	else
	{
		gEnv->pAISystem->LogComment("<TestReadabilityPack> ", "%d/%d - FAILED to play readability sound '%s'", iter.idx + 1, maxSounds, iter.sounds[iter.idx]->m_fileName.c_str());
		res = TEST_FAILED;
	}
	iter.idx++;
	return res;
}

void CAIReadabilityManager::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s,"CAIReadabilityManager");
	s->Add(*this);
	s->AddContainer(m_SoundPacks);
	for (SoundPacksMap::iterator iter = m_SoundPacks.begin(); iter != m_SoundPacks.end(); ++iter)
	{
		s->Add(iter->first);
		// add the pack
		SoundReadabilityPack& soundPack = iter->second;
		s->AddContainer(soundPack);
		for (SoundReadabilityPack::iterator iter2 = soundPack.begin(); iter2 != soundPack.end(); ++iter2)
		{
			s->Add(iter2->first);
			ReadabilitySoundGroup& soundGroup = iter2->second;
			soundGroup.GetMemoryStatistics(s);
		}
	}
}
