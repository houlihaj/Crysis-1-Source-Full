////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   selecttrack.cpp
//  Version:     v1.00
//  Created:     20/8/2002 by Lennert Schneider.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SoundTrack.h"

//////////////////////////////////////////////////////////////////////////
void CSoundTrack::SerializeKey( ISoundKey &key,XmlNodeRef &keyNode,bool bLoading )
{
	if (bLoading)
	{
		const char *desc;
		desc = keyNode->getAttr( "filename");
		strncpy(key.pszFilename,desc,sizeof(key.pszFilename));
		key.pszFilename[sizeof(key.pszFilename)-1] = 0;
		keyNode->getAttr( "volume",key.fVolume );
		keyNode->getAttr( "pan",key.nPan );
		keyNode->getAttr( "duration",key.fDuration );
		keyNode->getAttr( "InRadius",key.inRadius );
		keyNode->getAttr( "OutRadius",key.outRadius );
		keyNode->getAttr( "Stream",key.bStream );
		keyNode->getAttr( "Is3D",key.b3DSound );
		keyNode->getAttr( "Loop",key.bLoop );
		keyNode->getAttr( "Voice",key.bVoice );
		keyNode->getAttr( "LipSync",key.bLipSync );
		desc = keyNode->getAttr( "desc" );
		strncpy( key.description,desc,sizeof(key.description) );
		key.description[sizeof(key.description)-1] = 0;

		// Register the sound for level stats
		if (gEnv->pSoundSystem)
			gEnv->pSoundSystem->Precache(key.pszFilename, 0, key.bVoice?FLAG_SOUND_PRECACHE_DIALOG_DEFAULT:FLAG_SOUND_PRECACHE_EVENT_DEFAULT);
	}
	else
	{
		keyNode->setAttr( "filename", key.pszFilename);
		keyNode->setAttr( "volume",key.fVolume );
		keyNode->setAttr( "pan",key.nPan );
		keyNode->setAttr( "duration",key.fDuration );
		keyNode->setAttr( "desc",key.description );
		keyNode->setAttr( "InRadius",key.inRadius );
		keyNode->setAttr( "OutRadius",key.outRadius );
		keyNode->setAttr( "Stream",key.bStream );
		keyNode->setAttr( "Is3D",key.b3DSound );
		keyNode->setAttr( "Loop",key.bLoop );
		keyNode->setAttr( "Voice",key.bVoice );
		keyNode->setAttr( "LipSync",key.bLipSync );
	}
}

//////////////////////////////////////////////////////////////////////////
void CSoundTrack::GetKeyInfo( int key,const char* &description,float &duration )
{
	assert( key >= 0 && key < (int)m_keys.size() );
	CheckValid();
	description = 0;
	duration = m_keys[key].fDuration;

	//if (strlen(m_keys[key].animation) > 0)
	{
		//description = m_keys[key].animation;
		if (m_keys[key].bLoop)
		{
			float lastTime = m_timeRange.end;
			if (key+1 < (int)m_keys.size())
			{
				lastTime = m_keys[key+1].time;
			}
			// duration is unlimited but cannot last past end of track or time of next key on track.
			duration = lastTime - m_keys[key].time;
		}
		else
			duration = (m_keys[key].fDuration);// - m_keys[key].startTime) / m_keys[key].speed;
	}

	//if (strlen(m_keys[key].description) > 0)
	//	description = m_keys[key].description;
	if (strlen(m_keys[key].pszFilename) > 0)
		description = m_keys[key].pszFilename;
}
