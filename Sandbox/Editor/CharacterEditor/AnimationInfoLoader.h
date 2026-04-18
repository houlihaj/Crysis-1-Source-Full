////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   AnimationInfoLoader.h
//  Version:     v1.00
//  Created:     22/6/2006 by Alexey Medvedev.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _ANIMATION_INFO_LOADER
#define _ANIMATION_INFO_LOADER

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//#include "NameCRCHelper.h"


enum EMoveDirection
{
	EMD_Ignore,
	eMD_Forward,
	eMD_Back,
	eMD_Right,
	eMD_Left
};

enum EDirection 
{
	ED_Ignore,
	ED_Identity,
	ED_L90,
	ED_R90
};

inline void FixString(string& str)
{
	std::transform(str.begin(),str.end(),str.begin(),tolower);	
	std::replace(str.begin(),str.end(), '\\','/' );	
}

// Bone information
struct BonePreset
{
	//		uint32 m_BoneId;

	float m_fRotError;
	float m_fPosError;
	uint8 m_RotCompressionFormat;
	uint8 m_PosCompressionFormat;

	XmlNodeRef m_BoneRef;



	BonePreset() : m_PosCompressionFormat(0), m_RotCompressionFormat(9), m_BoneRef(0) {};
};

typedef std::map<uint32,BonePreset> TBoneInfoMap;

struct CompressionPreset
{
	string m_Name;
	XmlNodeRef m_pNodeRef;


//	static BonePreset DefaultPreset;

	void ClearNodeRefs()
	{

		m_pNodeRef = 0;

		TBoneInfoMap::iterator it = m_BoneInfoMap.begin();
		TBoneInfoMap::iterator end = m_BoneInfoMap.end();
		for(; it != end; ++it)
		{
			it->second.m_BoneRef = 0;
		}
	}

	TBoneInfoMap m_BoneInfoMap;

	/*const */BonePreset * FindBonePreset(uint32  bone)
	{
		TBoneInfoMap::iterator it = m_BoneInfoMap.find(bone);
		if (it != m_BoneInfoMap.end())
		{
			return &(it->second);
		}

		return 0;
	}
	CompressionPreset() : m_Name("Default")
	{

	}
};


struct SAnimationDesc //: public CNameCRCHelper
{
	int	m_CompressionQuality;
	int m_FootPlant;
	int m_Human;
	int m_Hunter;
	int m_FixedToFoots;
	int m_DeleteRotController;
	int m_DeletePosController;
	EMoveDirection m_MoveDirection;
	EDirection m_Direction;
	CompressionPreset m_Preset;

	XmlNodeRef m_pNodeRef;

	float GetMinPosError() const
	{
		if (m_Hunter > 0)
			return 0.000000001f;
		else
		  return 0.00000001f;
	}

	float GetMaxPosError() const
	{
		if (m_Hunter > 0)
			return 0.000001f;
		else
			return 0.000001f;
	}

	float GetMinRotError() const
	{
		if (m_Hunter > 0)
			return 0.000000001f;
		else
			return 0.00000001f;
	}
		
	float GetMaxRotError() const
	{
		if (m_Hunter > 0)
			return 0.000001f;
		else
			return 0.000001f;
	}


	void SetAnimationName(string name)
	{
		m_AnimationName = name;
		FixString(m_AnimationName);
	}


	SAnimationDesc() :  m_CompressionQuality(0), m_FootPlant(0), m_MoveDirection(EMD_Ignore), m_Direction(ED_Ignore), m_Human(0), m_FixedToFoots(0), m_Hunter(0)
	{

	};

	const string& GetAnimationName() const { return m_AnimationName;};
private:
	string m_AnimationName;

};

struct SAnimationDefinition 
{
	string m_Model;
	XmlNodeRef m_pNodeRef;
	

	SAnimationDesc m_MainDesc;

	std::vector<SAnimationDesc> m_OverrideAnimations;
	SAnimationDesc * GetAnimationDesc(const string& name );

	void SetAnimationPath(string& name)
	{
		m_AnimationPath = name;
		FixString(m_AnimationPath);
	}

	int GetAnimationNumber(const string& name );
	void GetAnimationNumbers(const string& name, std::vector<int>& anims );
	bool FindIdentical(const string& name );
	const string& GetAnimationPath() const { return m_AnimationPath; };

	void AddAnimationPreset(CompressionPreset * pPreset, const char *animName);


private:

	string m_AnimationPath;

};

struct  PresetFinder
{
	bool FillPreset(const char * name, CompressionPreset& newpreset)
	{
		uint32 end = m_Presets.size();
		for (uint32 i = 0; i < end; ++i)
		{
			if (stricmp(m_Presets[i].m_Name.c_str(), name) == 0)
			{
				newpreset = m_Presets[i];
				return true;
			}
		}

		return false;
	}

	int FindPreset(const char * name)
	{
		uint32 end = m_Presets.size();
		for (uint32 i = 0; i < end; ++i)
		{
			if ((stricmp(m_Presets[i].m_Name.c_str(), name) == 0) && strlen(name) == m_Presets[i].m_Name.size())
			{
				return i;
			}
		}

		return -1;
	}


	std::vector<CompressionPreset> m_Presets;
};


class CAnimationInfoLoader
{
public:
	CAnimationInfoLoader();
	~CAnimationInfoLoader(void);

	bool LoadDescription(const string& name);
	bool SaveDescription(const string& name);

	void AddPreset(CompressionPreset& preset);

	const SAnimationDefinition * GetAnimationDefinition(const string& name ) const;
	SAnimationDefinition * GetAnimationDefinitionFromModel(const string& name );

	std::vector<SAnimationDefinition> m_Definitions;

protected:
	void InsertPresetToXML(int num);//CompressionPreset& preset);

	XmlNodeRef m_pRoot;
public:
	PresetFinder m_PresetFinder;

};


//void ReportWarning(IAnimationLoaderListener* pListener, const char* szFormat, ...);
//void ReportInfo(IAnimationLoaderListener* pListener, const char* szFormat, ...);
//void ReportError(IAnimationLoaderListener* pListener, const char* szFormat, ...);

#endif