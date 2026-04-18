////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MusicManager.cpp
//  Version:     v1.00
//  Created:     22/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MusicManager.h"

#include "MusicThemeLibrary.h"
#include "MusicThemeLibItem.h"

#include <IMusicSystem.h>
#include <IScriptSystem.h>

#define MUSIC_LIBS_PATH "/Music/"

//////////////////////////////////////////////////////////////////////////
// CMusicManager implementation.
//////////////////////////////////////////////////////////////////////////
CMusicManager::CMusicManager()
{
	m_pMusicData = new SMusicData();	

	m_pLevelLibrary = (CBaseLibrary*)AddLibrary( "Level" );
	m_pLevelLibrary->SetLevelLibrary( true );
}

//////////////////////////////////////////////////////////////////////////
CMusicManager::~CMusicManager()
{
	ReleaseMusicData();
	delete m_pMusicData;
}

//////////////////////////////////////////////////////////////////////////
void CMusicManager::ClearAll()
{
	CBaseLibraryManager::ClearAll();

	ReleaseMusicData();

	m_pLevelLibrary = (CBaseLibrary*)AddLibrary( "Level" );
	m_pLevelLibrary->SetLevelLibrary( true );
}

//////////////////////////////////////////////////////////////////////////
void CMusicManager::Export( XmlNodeRef &node )
{
	XmlNodeRef libs = node->newChild( "MusicLibrary" );
	for (int i = 0; i < GetLibraryCount(); i++)
	{
		IDataBaseLibrary* pLib = GetLibrary(i);
		// Skip level library.
		if (pLib->IsLevelLibrary())
			continue;
		// Level libraries are saved in in level.
		XmlNodeRef libNode = libs->newChild( "Library" );

		// Export library.
		libNode->setAttr( "Name",pLib->GetName() );
		libNode->setAttr( "File",Path::GetFile(pLib->GetFilename()) );
	}
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem* CMusicManager::MakeNewItem()
{
	return new CMusicThemeLibItem;
}

//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CMusicManager::MakeNewLibrary()
{
	return new CMusicThemeLibrary(this);
}

//////////////////////////////////////////////////////////////////////////
CString CMusicManager::GetRootNodeName()
{
	return "MusicLibs";
}
//////////////////////////////////////////////////////////////////////////
CString CMusicManager::GetLibsPath()
{
	if (m_libsPath.IsEmpty())
		m_libsPath = Path::GetGameFolder()+MUSIC_LIBS_PATH;
	return m_libsPath;
}

//////////////////////////////////////////////////////////////////////////
void CMusicManager::ReleaseMusicData()
{
	// Delete all sub items of music data.
	SMusicData *pData = m_pMusicData;
	if (!pData)
		return;

	//////////////////////////////////////////////////////////////////////////
	// Stop music system from playing.
	if(GetIEditor()->GetSystem())
	if (GetIEditor()->GetSystem()->GetIMusicSystem())
		GetIEditor()->GetSystem()->GetIMusicSystem()->SetData( NULL ); // Clear music system data.

	// release pattern-defs
	for (TPatternDefVecIt PatternIt=pData->vecPatternDef.begin();PatternIt!=pData->vecPatternDef.end();++PatternIt)
	{
		SPatternDef *pPattern=(*PatternIt);
		delete pPattern;
	}
	// release themes/moods
	for (TThemeMapIt ThemeIt=pData->mapThemes.begin();ThemeIt!=pData->mapThemes.end();++ThemeIt)
	{
		SMusicTheme *pTheme=ThemeIt->second;
		for (TMoodMapIt MoodIt=pTheme->mapMoods.begin();MoodIt!=pTheme->mapMoods.end();++MoodIt)
		{
			SMusicMood *pMood=MoodIt->second;
			for (TPatternSetVecIt PatternSetIt=pMood->vecPatternSets.begin();PatternSetIt!=pMood->vecPatternSets.end();++PatternSetIt)
			{
				SMusicPatternSet *pPatternSet=(*PatternSetIt);
				delete pPatternSet;
			}
			delete pMood;
		}
		delete pTheme;
	}
	m_pMusicData->mapThemes.clear();
	m_pMusicData->vecPatternDef.clear();
}

//////////////////////////////////////////////////////////////////////////
//! Serialize property manager.
void CMusicManager::Serialize( XmlNodeRef &node,bool bLoading )
{
	// TODO if stl debug mode is activated in ProjectDefines.h then a deadlock is reached
	// in this function, indicating that (probably) memory is being allocated in a thread-unsafe 
	// manner.
	if (bLoading)
	{
		ReleaseMusicData();
	}
	CBaseLibraryManager::Serialize( node,bLoading );
	if (bLoading)
	{
		UpdateMusicSystemData();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMusicManager::UpdateMusicSystemData()
{
	if (!GetIEditor()->GetSystem()->GetIMusicSystem())
		return;

	std::vector<SMusicInfo::Theme> themes;
	std::vector<SMusicInfo::Mood> moods;
	std::vector<SMusicInfo::Pattern> patterns;
	
	int nTheme = 0;
	int nMood = 0;
	themes.resize( m_pMusicData->mapThemes.size() );
	for (TThemeMap::iterator themeIt = m_pMusicData->mapThemes.begin(); themeIt != m_pMusicData->mapThemes.end(); ++themeIt)
	{
		SMusicInfo::Theme &themeInfo = themes[nTheme++];
		SMusicTheme &theme = *themeIt->second;
		themeInfo.fDefaultMoodTimeout = theme.fDefaultMoodTimeout;
		themeInfo.sDefaultMood = theme.sDefaultMood;
		themeInfo.sName = theme.sName;
		themeInfo.nBridgeCount = 0;
		themeInfo.pBridges = 0;
		//themeInfo.nBridgeCount = theme.mapBridges.size();

		moods.resize( moods.size() + theme.mapMoods.size() );
		for (TMoodMap::iterator moodIt = theme.mapMoods.begin(); moodIt != theme.mapMoods.end(); ++moodIt)
		{
			SMusicInfo::Mood &moodInfo = moods[nMood++];
			SMusicMood &mood = *moodIt->second;

			moodInfo.bPlaySingle = mood.bPlaySingle;
			moodInfo.fFadeOutTime = mood.fFadeOutTime;
			moodInfo.nPriority = mood.nPriority;
			moodInfo.sName = mood.sName;
			moodInfo.sTheme = theme.sName;
			moodInfo.nPatternSetsCount = (int)mood.vecPatternSets.size();
			moodInfo.pPatternSets = new SMusicInfo::PatternSet[moodInfo.nPatternSetsCount];

			for (int nPatternSet = 0; nPatternSet < moodInfo.nPatternSetsCount; nPatternSet++)
			{
				SMusicInfo::PatternSet &patternSetInfo = moodInfo.pPatternSets[nPatternSet];
				SMusicPatternSet &patternSet = *(mood.vecPatternSets[nPatternSet]);
				patternSetInfo.fIncidentalLayerProbability = patternSet.fIncidentalLayerProbability;
				patternSetInfo.fMaxTimeout = patternSet.fMaxTimeout;
				patternSetInfo.fMinTimeout = patternSet.fMinTimeout;
				patternSetInfo.fRhythmicLayerProbability = patternSet.fRhythmicLayerProbability;
				
				patternSetInfo.fTotalIncidentalPatternProbability = patternSet.fTotalIncidentalPatternProbability;
				patternSetInfo.fTotalMainPatternProbability = patternSet.fTotalMainPatternProbability;
				patternSetInfo.fTotalRhythmicPatternProbability = patternSet.fTotalRhythmicPatternProbability;
				patternSetInfo.fTotalStingerPatternProbability = patternSet.fTotalStingerPatternProbability;
				
				patternSetInfo.nMaxSimultaneousIncidentalPatterns = patternSet.nMaxSimultaneousIncidentalPatterns;
				patternSetInfo.nMaxSimultaneousRhythmicPatterns = patternSet.nMaxSimultaneousRhythmicPatterns;
				
				for (unsigned int nLayerFlag = MUSICLAYER_MAIN; nLayerFlag <= MUSICLAYER_END; nLayerFlag*=2) // ignore ANY too
				{
					TPatternDefVec *pVecPatternDefs = NULL;
					switch (nLayerFlag)
					{
					case MUSICLAYER_MAIN:
						pVecPatternDefs = &patternSet.vecMainPatterns;
						break;
					case MUSICLAYER_RHYTHMIC:
						pVecPatternDefs = &patternSet.vecRhythmicPatterns;
						break;
					case MUSICLAYER_INCIDENTAL:
						pVecPatternDefs = &patternSet.vecIncidentalPatterns;
						break;
					case MUSICLAYER_START:
						pVecPatternDefs = &patternSet.vecStartPatterns;
						break;
					case MUSICLAYER_END:
						pVecPatternDefs = &patternSet.vecEndPatterns;
						break;
					case MUSICLAYER_STINGER:
						pVecPatternDefs = &patternSet.vecStingerPatterns;
						break;
					default:
						assert(0);
					}
					int i;
					for (i = 0; i < (int)pVecPatternDefs->size(); i++)
					{
						patterns.resize( patterns.size()+1 );
						SMusicInfo::Pattern &patternInfo = patterns[patterns.size()-1];
						SPatternDef &pattern = *((*pVecPatternDefs)[i]);
							
						patternInfo.fProbability = pattern.fProbability;
						patternInfo.fLayeringVolume = pattern.fLayeringVolume;
						patternInfo.sFilename = pattern.sFilename;
						patternInfo.sName = pattern.sName;
						patternInfo.nPreFadeIn = pattern.nPreFadeIn;

						patternInfo.sMood = mood.sName;
						patternInfo.sTheme = theme.sName;
						patternInfo.nPatternType = nLayerFlag;
						patternInfo.nPatternSetIndex = nPatternSet;

						patternInfo.nFadePointsCount = (int)pattern.vecFadePoints.size();
						patternInfo.pFadePoints = 0;
						if (patternInfo.nFadePointsCount > 0)
							patternInfo.pFadePoints = new int[patternInfo.nFadePointsCount];
						for (int nFadePoint = 0; nFadePoint < patternInfo.nFadePointsCount; nFadePoint++)
						{
							patternInfo.pFadePoints[nFadePoint] = pattern.vecFadePoints[nFadePoint];
						}
					}
				}
			}
		}
	}

	if (themes.size() > 0 && moods.size() > 0 && patterns.size() > 0)
	{
		SMusicInfo::Data musicData;
		musicData.pThemes = &themes[0];
		musicData.nThemes = (int)themes.size();
		musicData.pMoods = &moods[0];
		musicData.nMoods = (int)moods.size();
		musicData.pPatterns = &patterns[0];
		musicData.nPatterns = (int)patterns.size();
		
		GetIEditor()->GetSystem()->GetIMusicSystem()->SetData( &musicData );
	}
	else
	{
		GetIEditor()->GetSystem()->GetIMusicSystem()->SetData( NULL );
	}

	// Release temporary allocs.
	int nMoodsCount = (int)moods.size();
	for (int i = 0; i < nMoodsCount; i++)
	{
		delete [](moods[i].pPatternSets);
	}
	int nPatternsCount = (int)patterns.size();
	for (int i = 0; i < nPatternsCount; i++)
	{
		delete []patterns[i].pFadePoints;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMusicManager::UpdatePattern( SPatternDef *pPatternDef )
{
	if (!GetIEditor()->GetSystem()->GetIMusicSystem())
		return;

	SMusicInfo::Pattern patternInfo;
	SPatternDef &pattern = *pPatternDef;

	patternInfo.fProbability = pattern.fProbability;
	patternInfo.fLayeringVolume = min(1.0f, pattern.fLayeringVolume);
	patternInfo.sFilename = pattern.sFilename;
	patternInfo.sName = pattern.sName;
	patternInfo.nPreFadeIn = pattern.nPreFadeIn;

	patternInfo.sMood = "";
	patternInfo.sTheme = "";
	patternInfo.nPatternType = 0;
	patternInfo.nPatternSetIndex = 0;

	patternInfo.nFadePointsCount = (int)pattern.vecFadePoints.size();
	patternInfo.pFadePoints = 0;
	if (patternInfo.nFadePointsCount > 0)
		patternInfo.pFadePoints = new int[patternInfo.nFadePointsCount];
	for (int nFadePoint = 0; nFadePoint < patternInfo.nFadePointsCount; nFadePoint++)
	{
		patternInfo.pFadePoints[nFadePoint] = pattern.vecFadePoints[nFadePoint];
	}

	//GetIEditor()->GetSystem()->GetIMusicSystem()->UpdatePattern( patternInfo );

	delete []patternInfo.pFadePoints;
}
