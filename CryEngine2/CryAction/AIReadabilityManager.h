/********************************************************************
CryGame Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   AIReadabilityManager.h
Version:     v1.00
Description: 

-------------------------------------------------------------------------
History:
- 8:10:2004   12:04 : Created by Kirill Bulatsev

*********************************************************************/


#ifndef __AIReadabilityManager_H__
#define __AIReadabilityManager_H__


#pragma once

#include <IAgent.h>
#include <ISound.h>

class CAIHandler;
class ICrySizer;

//
//---------------------------------------------------------------------------------------------------
class CAIReadabilityManager
{
public:

	struct ReadabilitySoundEntry
	{
		string	m_fileName;
		bool		voice;
		int			m_probability;
		ReadabilitySoundEntry(string& fileName, int probability, bool voice)
			: m_fileName(fileName), m_probability(probability), voice(voice) {}
	};

	template <unsigned int S>
	class SIntHistory
	{
	public:
		static const unsigned int MAX_SIZE = S;

		SIntHistory()
		{
			Reset();
		}

		void Reset()
		{
			for (unsigned i = 0; i < MAX_SIZE; ++i)
				vals[i] = UNDEF;
			head = 0;
		}

		bool Find(int v)
		{
			for (unsigned i = 0; i < MAX_SIZE; ++i)
				if (vals[i] == v) return true;
			return false;
		}

		void Add(int v)
		{
			vals[head] = v;
			++head;
			if (head >= MAX_SIZE) head = 0;
		}

	private:
		static const unsigned UNDEF = -1;
		int vals[MAX_SIZE];
		unsigned head;
	};

	struct ReadabilitySoundGroup
	{
		ReadabilitySoundGroup() :
			m_blockingTime(5.0f),
			m_blockingID(0)
		{
			// Empty
		}

		enum ESoundType
		{
			SND_ALONE,
			SND_GROUP,
			SND_RESPONSE,
		};

		typedef std::vector<ReadabilitySoundEntry> SoundVector;
		typedef SIntHistory<2> SoundHistory;

		SoundVector m_aloneSounds;
		SoundVector m_groupSounds;
		SoundVector m_responseSounds;
		SoundHistory m_lastAloneSounds;
		SoundHistory m_lastGroupSounds;
		SoundHistory m_lastResponseSounds;
		float m_blockingTime;
		int m_blockingID;

		ReadabilitySoundEntry* GetRandomSound(SoundVector& sounds, SoundHistory& hist);

		ReadabilitySoundEntry* GetMostLikelySound(ESoundType type);

		void AddSoundVector(ICrySizer* s, SoundVector& sounds)
		{
			for (unsigned i = 0, ni = sounds.size(); i < ni; ++i)
				s->Add(sounds[i].m_fileName);
		}

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(*this);
			s->AddContainer(m_aloneSounds);
			AddSoundVector(s, m_aloneSounds);
			s->AddContainer(m_groupSounds);
			AddSoundVector(s, m_groupSounds);
			s->AddContainer(m_responseSounds);
			AddSoundVector(s, m_responseSounds);
		}
	};

	typedef std::map<string, ReadabilitySoundGroup> SoundReadabilityPack;
	typedef std::map<string, SoundReadabilityPack> SoundPacksMap;

	struct ReadabilityTestIter
	{
		ReadabilityTestIter() : idx(-1) {}
		void	Reset()
		{
			idx = -1;
			sounds.clear();
		}
		std::vector<ReadabilitySoundEntry*>	sounds;
		int	idx;
	};

	CAIReadabilityManager();
	~CAIReadabilityManager();

	void	Reload();
	bool	PlayReadabilitySound(CAIHandler* pActor, const char* text, int readabilityType, bool stopPreviousSound);
	// Returns true if the readability group is specified signal has response sound.
	bool	HasResponseSound(CAIHandler* pActor, const char* text);
	void	GetReadabilityBlockingParams(CAIHandler* pActor, const char* text, float& time, int& id);
	SoundReadabilityPack*	FindSoundPack(const char* name);
	const char* GetSoundPackName(SoundReadabilityPack* pPack);
	void GetMemoryStatistics(ICrySizer * s);

	enum ETestResult
	{
		TEST_DONE,
		TEST_FAILED,
		TEST_SUCCEED,
	};

	bool	PrepareReadabilityPackTest(CAIHandler* pActor, ReadabilityTestIter& iter);
	bool	PrepareReadabilityPackTest(CAIHandler* pActor, ReadabilityTestIter& iter, const char* szReadability);

	ETestResult	TestReadabilityPack(CAIHandler* pActor, ReadabilityTestIter& iter);
	ETestResult	TestReadabilityPack(CAIHandler* pActor, ReadabilityTestIter& iter, const char* szReadability);

protected:

	/// Play sound entry at actor position or actor target (AI attention target) position. Returns true if successful.
	bool	PlaySound(CAIHandler* pActor, ReadabilitySoundEntry* sound, bool playSoundAtActorTarget, bool stopPreviousSound);

	bool	Load(const char* szPackName);

	SoundPacksMap	m_SoundPacks;
	int						m_reloadID;
};

#endif
