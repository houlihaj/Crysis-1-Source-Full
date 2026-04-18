/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Loads levels, gathers level info.
  
 -------------------------------------------------------------------------
  History:
  - 17:8:2004   18:00 : Created by Márcio Martins

*************************************************************************/
#ifndef __LEVELSYSTEM_H__
#define __LEVELSYSTEM_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "ILevelSystem.h"


class CLevelInfo :
	public ILevelInfo
{
	friend class CLevelSystem;
public:
	CLevelInfo(): m_heightmapSize(0), m_levelChecksum(0) {};

	// ILevelInfo
	virtual const char *GetName() const { return m_levelName.c_str(); };
	virtual const char *GetPath() const { return m_levelPath.c_str(); };
	virtual const char *GetPaks() const { return m_levelPaks.c_str(); };
  virtual const char* GetDisplayName() const;
	virtual const TStringVec& GetMusicLibs() const { return m_musicLibs; };
	virtual int GetHeightmapSize() const { return m_heightmapSize; };

	virtual int GetGameTypeCount() const { return m_gameTypes.size(); };
	virtual const ILevelInfo::TGameTypeInfo *GetGameType(int gameType) const { return &m_gameTypes[gameType]; };
	virtual bool SupportsGameType(const char *gameTypeName) const;
	virtual const ILevelInfo::TGameTypeInfo *GetDefaultGameType() const;

	virtual uint64 GetLevelChecksum() const { return m_levelChecksum; }
	virtual void SetLevelChecksum(uint64 cs) { m_levelChecksum = cs; }
	// ~ILevelInfo

	bool operator < (const CLevelInfo &rhs) const
	{
		if (m_levelName<rhs.m_levelName)
			return true;
		else if (m_levelName==rhs.m_levelName)
		{
			if (m_filetime>rhs.m_filetime)
				return true;
		}
		return false;
	}

	void GetMemoryStatistics(ICrySizer * );

private:
  void        ReadMetaData();
	string			m_levelName;
	string			m_levelPath;
	string			m_levelPaks;
  string      m_levelDisplayName;
	TStringVec	m_musicLibs;
  TStringVec  m_gamerules;
	int					m_heightmapSize;
	uint64			m_filetime;
	uint64			m_levelChecksum;		// 0 unless previously calculated: used to cache to prevent repeated calculation
	std::vector<ILevelInfo::TGameTypeInfo> m_gameTypes;
};


class CLevelRotation : public ILevelRotation
{
public:
	CLevelRotation();
	virtual ~CLevelRotation();

	typedef struct SLevelRotationEntry
	{
		string levelName;
		string gameRulesName;
    std::vector<string> settings;
	} SLevelRotationEntry;

	typedef std::vector<SLevelRotationEntry> TLevelRotationVector;

	// ILevelRotation
  virtual bool Load(ILevelRotationFile* file);
	virtual bool Save(ILevelRotationFile* file);
	
	virtual void Reset();
	virtual int  AddLevel(const char *level, const char *gamerules);
  virtual void AddSetting(int level, const char* setting);

	virtual bool First();
	virtual bool Advance();
	virtual const char *GetNextLevel() const;
	virtual const char *GetNextGameRules() const;

  virtual int  GetNextSettingsNum() const;
  virtual const char *GetNextSetting(int idx); 

	virtual int GetLength() const;

  virtual void SetRandom(bool rnd);
  virtual bool IsRandom()const;

  virtual void ChangeLevel(IConsoleCmdArgs* pArgs = NULL);
	//~ILevelRotation
  void    Shuffle();
protected:
	TLevelRotationVector m_rotation;
  bool                 m_random;
	int                  m_next;
  std::vector<int>     m_shuffle;
};


class CLevelSystem :
	public ILevelSystem,
	public ISystem::ILoadingProgressListener
{
public:
	CLevelSystem(ISystem *pSystem, const char *levelsFolder);
	virtual ~CLevelSystem();

	void Release() { delete this; };

	// ILevelSystem
	virtual void Rescan(const char *levelsFolder);
  virtual void LoadRotation();
	virtual int GetLevelCount();
	virtual ILevelInfo *GetLevelInfo(int level);
	virtual ILevelInfo *GetLevelInfo(const char *levelName);
	virtual uint64 CalcLevelChecksum(const char *levelName);

	virtual void AddListener(ILevelSystemListener *pListener);
	virtual void RemoveListener(ILevelSystemListener *pListener);

	virtual ILevel *GetCurrentLevel() const { return m_pCurrentLevel;	}
	virtual ILevel *LoadLevel(const char *levelName);
	virtual void UnloadLevel();
	virtual void PrepareNextLevel(const char *levelName);
	virtual float GetLastLevelLoadTime() { return m_fLastLevelLoadTime; };
	virtual bool IsLevelLoaded() { return m_bLevelLoaded; }

	virtual ILevelRotation *GetLevelRotation() { return &m_levelRotation; };
	// ~ILevelSystem

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char *levelName);
	virtual void OnLoadingStart(ILevelInfo *pLevel);
	virtual void OnLoadingComplete(ILevel *pLevel);
	virtual void OnLoadingError(ILevelInfo *pLevel, const char *error);
	virtual void OnLoadingProgress(ILevelInfo *pLevel, int progressAmount);
	// ~ILevelSystemListener

	// ILoadingProgessListener
	virtual void OnLoadingProgress(int steps);
	// ~ILoadingProgessListener

	virtual void PrecacheLevelRenderData();
	virtual int GetPrecacheCameraNumber();

	void GetMemoryStatistics(ICrySizer * s);

private:
	// lowercase string and replace backslashes with forward slashes
	// TODO: move this to a more general place in CryEngine
	string&	UnifyName(string&	name);
	void ScanFolder(const char *subfolder);
	void LogLoadingTime();

	// internal get functions for the level infos ... they preserve the type and don't
	// directly cast to the interface
	CLevelInfo *GetLevelInfoInternal(int level);
	CLevelInfo *GetLevelInfoInternal(const char *levelName);

	ISystem									*m_pSystem;
	std::vector<CLevelInfo> m_levelInfos;
	string									m_levelsFolder;
	ILevel									*m_pCurrentLevel;
	ILevelInfo							*m_pLoadingLevelInfo;

	CLevelRotation					m_levelRotation;

	string                  m_lastLevelName;
	float                   m_fLastLevelLoadTime;
	float										m_fFilteredProgress;
	float										m_fLastTime;

	bool                    m_bLevelLoaded;

	int                     m_nLoadedLevelsCount;

	CTimeValue              m_levelLoadStartTime;

	int											m_precachedCameraNumber;

	std::vector<ILevelSystemListener *> m_listeners;
};

#endif //__LEVELSYSTEM_H__