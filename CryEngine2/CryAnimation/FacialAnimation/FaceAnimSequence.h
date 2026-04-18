////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   FaceAnimSequence.h
//  Version:     v1.00
//  Created:     15/11/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FaceAnimSequence_h__
#define __FaceAnimSequence_h__
#pragma once

#include "FaceEffector.h"
#include "LipSync.h"

class CFacialInstance;
class CFacialAnimationContext;
class CFacialAnimSequence;
class CFacialAnimation;

//////////////////////////////////////////////////////////////////////////
class CFacialAnimChannelInterpolator : public spline::CBaseSplineInterpolator<float,spline::HermitSplineEx<float> >
{
public:
	//////////////////////////////////////////////////////////////////////////
	virtual int GetNumDimensions() { return 1; };
	virtual ESplineType GetSplineType() { return ESPLINE_HERMIT; }
	virtual void SerializeSpline( XmlNodeRef &node,bool bLoading );
	virtual void Interpolate( float time,ValueType &value )
	{
		value_type v = 0.0f;
		interpolate( time,v );
		ToValueType(v,value);
	}
	//////////////////////////////////////////////////////////////////////////

	void CleanupKeys(float errorMax);
	void SmoothKeys(float sigma);
	void RemoveNoise(float sigma, float threshold);
};

class CFacialCameraPathPositionInterpolator : public spline::CBaseSplineInterpolator<Vec3, spline::TCBSpline<Vec3> >
{
public:
	//////////////////////////////////////////////////////////////////////////
	virtual int GetNumDimensions() { return 3; };
	virtual ESplineType GetSplineType() { return ESPLINE_TCB; }
	virtual void SerializeSpline( XmlNodeRef &node,bool bLoading );
	virtual void Interpolate( float time,ValueType &value )
	{
		value_type v(0.0f, 0.0f, 0.0f);
		interpolate( time,v );
		ToValueType(v,value);
	}
	//////////////////////////////////////////////////////////////////////////
};

class CFacialCameraPathOrientationInterpolator : public spline::CBaseSplineInterpolator<Quat, spline::HermitSplineEx<Quat> >
{
public:
	//////////////////////////////////////////////////////////////////////////
	virtual int GetNumDimensions() { return 4; };
	virtual ESplineType GetSplineType() { return ESPLINE_TCB; }
	virtual void SerializeSpline( XmlNodeRef &node,bool bLoading );
	virtual void Interpolate( float time,ValueType &value )
	{
		value_type v(0.0f, 0.0f, 0.0f, 1.0f);
		interpolate( time,v );
		ToValueType(v,value);
	}
	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
class CFacialAnimChannel : public IFacialAnimChannel, public _i_reference_target_t
{
public:
	CFacialAnimChannel(int index);
	~CFacialAnimChannel();

	//////////////////////////////////////////////////////////////////////////
	// IFacialAnimChannel
	//////////////////////////////////////////////////////////////////////////
	virtual void SetName( const char *sNewName );
	virtual const char* GetName();

	virtual void SetEffectorName( const char *sNewEffectorName ) {m_effectorName = sNewEffectorName;}
	virtual const char* GetEffectorName() {return m_effectorName.c_str();}

	// Associate animation channel with the group.
	virtual void SetParent( IFacialAnimChannel *pParent ) { m_pParent = (CFacialAnimChannel*)pParent; };
	// Get group of this animation channel.
	virtual IFacialAnimChannel* GetParent() { return m_pParent; };

	virtual void SetEffector( IFacialEffector *pEffector );
	virtual IFacialEffector* GetEffector() { return m_pEffector; };

	virtual void SetFlags( uint32 nFlags ) { m_nFlags = nFlags; };
	virtual uint32 GetFlags() { return m_nFlags; };

	// Retrieve interpolator spline used to animated channel value.
	virtual ISplineInterpolator* GetInterpolator(int i) { return m_splines[i]; }
	virtual ISplineInterpolator* GetLastInterpolator() { return (!m_splines.empty() ? m_splines.back() : 0); }
	virtual void AddInterpolator();
	virtual void DeleteInterpolator(int i);
	virtual int GetInterpolatorCount() const {return m_splines.size();}

	virtual void CleanupKeys(float fErrorMax);
	virtual void SmoothKeys(float sigma);
	virtual void RemoveNoise(float sigma, float threshold);
	//////////////////////////////////////////////////////////////////////////

	uint32 GetInstanceChannelId() const { return m_nInstanceChannelId; }
	void  SetInstanceChannelId( uint32 nChanelId ) { m_nInstanceChannelId = nChanelId; }

	float Evaluate( float t );
	bool HaveEffector() const { return m_pEffector != 0; }

	void CreateInterpolator();
	_smart_ptr<CFacialEffector> GetEffectorPtr() { return m_pEffector; }

private:
	uint32 m_nFlags;
	uint32 m_nInstanceChannelId;
	string m_name;
	string m_effectorName;
	_smart_ptr<CFacialAnimChannel> m_pParent;
	_smart_ptr<CFacialEffector> m_pEffector;
	std::vector<CFacialAnimChannelInterpolator*> m_splines;
};

//////////////////////////////////////////////////////////////////////////
class CFacialAnimSequenceInstance : public _i_reference_target_t
{
public:
	struct ChannelInfo
	{
		int nChannelId;
		_smart_ptr<CFacialEffector> pEffector;
		bool bUse;
		int nBalanceChannelListIndex; // Index into m_balanceChannelIndices
	};
	typedef std::vector<ChannelInfo> Channels;

	Channels m_channels;
	Channels m_proceduralChannels;

	struct BalanceChannelEntry
	{
		int nChannelIndex;
		float fEvaluatedBalance; // Place to store temporary evaluation of spline - could be on stack, but would require dynamic allocation per frame/instance.
		int nMorphIndexStartIndex;
		int nMorphIndexCount;
	};
	typedef std::vector<BalanceChannelEntry> BalanceEntries;
	BalanceEntries m_balanceChannelEntries;

	std::vector<int> m_balanceChannelStateIndices;

	int m_nValidateID;
	CFacialAnimationContext *m_pAnimContext;

	// Phonemes related.
	int m_nCurrentPhoneme;
	int m_nCurrentPhonemeChannelId;

	//////////////////////////////////////////////////////////////////////////
	CFacialAnimSequenceInstance()
	{
		m_nValidateID = 0;
		m_pAnimContext = 0;
		m_nCurrentPhoneme = -1;
		m_nCurrentPhonemeChannelId = -1;
	}
	~CFacialAnimSequenceInstance()
	{
		if (m_pAnimContext)
			UnbindChannels();
	}

	//////////////////////////////////////////////////////////////////////////
	void BindChannels( CFacialAnimationContext *pContext,CFacialAnimSequence *pSequence );
	void UnbindChannels();

private:
	void BindProceduralChannels(CFacialAnimationContext *pContext,CFacialAnimSequence *pSequence);
	void UnbindProceduralChannels();
};

class CProceduralChannel
{
public:
	CFacialAnimChannelInterpolator* GetInterpolator() {return &m_interpolator;}

	void SetEffectorName(const char* szEffectorName) {m_effectorName = szEffectorName;}
	const char* GetEffectorName() const {return m_effectorName.c_str();}

private:
	CFacialAnimChannelInterpolator m_interpolator;
	string m_effectorName;
};

class CProceduralChannelSet
{
public:
	enum ChannelType
	{
		ChannelTypeHeadUpDown,
		ChannelTypeHeadRightLeft,

		ChannelType_count
	};

	CProceduralChannelSet();
	CProceduralChannel* GetChannel(ChannelType type) {return &m_channels[type];}

private:
	CProceduralChannel m_channels[ChannelType_count];
};

//////////////////////////////////////////////////////////////////////////
class CFacialAnimSoundEntry : public IFacialAnimSoundEntry
{
public:
	CFacialAnimSoundEntry();

	virtual void SetSoundFile( const char *sSoundFile );
	virtual const char* GetSoundFile();

	virtual IFacialSentence* GetSentence() { return m_pSentence; };

	virtual float GetStartTime();
	virtual void SetStartTime(float time);

	void ValidateSentence();
	bool IsSentenceInvalid();

	_smart_ptr<CFacialSentence> m_pSentence;
	string m_sound;
	int m_nSentenceValidateID;
	float m_startTime;
};

//////////////////////////////////////////////////////////////////////////
class CFacialAnimSkeletalAnimationEntry : public IFacialAnimSkeletonAnimationEntry
{
public:
	CFacialAnimSkeletalAnimationEntry();

	virtual void SetName(const char* skeletonAnimationFile);
	virtual const char* GetName() const;

	virtual void SetStartTime(float time);
	virtual float GetStartTime() const;

	string m_animationName;
	float m_startTime;
};

//////////////////////////////////////////////////////////////////////////
class CFacialAnimSequence : public IFacialAnimSequence
{
public:
	CFacialAnimSequence( CFacialAnimation *pFaceAnim );

	virtual void AddRef() { ++m_nRefCount; };
	virtual void Release();

	//////////////////////////////////////////////////////////////////////////
	// IFacialAnimSequence
	//////////////////////////////////////////////////////////////////////////
	virtual void SetName( const char *sNewName );
	virtual const char* GetName() { return m_name; }

	virtual void SetFlags( int nFlags ) { m_nFlags = nFlags; };
	virtual int  GetFlags() { return m_nFlags; };

	virtual Range GetTimeRange() { return m_timeRange; };
	virtual void SetTimeRange( Range range ) { m_timeRange = range; };

	virtual int GetChannelCount() { return m_channels.size(); };
	virtual IFacialAnimChannel* GetChannel( int nIndex );
	virtual IFacialAnimChannel* CreateChannel();
	virtual IFacialAnimChannel* CreateChannelGroup();
	virtual void RemoveChannel( IFacialAnimChannel *pChannel );

	virtual int GetSoundEntryCount();
	virtual void InsertSoundEntry(int index);
	virtual void DeleteSoundEntry(int index);
	virtual IFacialAnimSoundEntry* GetSoundEntry(int index);

	virtual void Animate( const QuatTS& rAnimLocationNext, CFacialAnimSequenceInstance *pInstance,float fTime );

	virtual int GetSkeletonAnimationEntryCount();
	virtual void InsertSkeletonAnimationEntry(int index);
	virtual void DeleteSkeletonAnimationEntry(int index);
	virtual IFacialAnimSkeletonAnimationEntry* GetSkeletonAnimationEntry(int index);

	virtual void SetJoystickFile(const char* joystickFile);
	virtual const char* GetJoystickFile() const;

	virtual void Serialize( XmlNodeRef &xmlNode,bool bLoading,ESerializationFlags flags );

	virtual void MergeSequence(IFacialAnimSequence* pMergeSequence, const Functor1wRet<const char*, MergeCollisionAction>& collisionStrategy);

	virtual ISplineInterpolator* GetCameraPathPosition() {return &m_cameraPathPosition;}
	virtual ISplineInterpolator* GetCameraPathOrientation() {return &m_cameraPathOrientation;}
	virtual ISplineInterpolator* GetCameraPathFOV() {return &m_cameraPathFOV;}

	//////////////////////////////////////////////////////////////////////////

	void SerializeChannel( IFacialAnimChannel *pChannel,XmlNodeRef &node,bool bLoading );

	int GetValidateId() const { return m_nValidateID; };

	CProceduralChannelSet& GetProceduralChannelSet() {return m_proceduralChannels;}

private:
	friend class CFacialAnimation;

	void UpdateProceduralChannels();
	void GenerateProceduralChannels();
	void UpdateSoundEntriesValidateID();

	int m_nRefCount;

	// Validate id insure that sequence instances are valid.
	int m_nValidateID;
	int m_nFlags;
	string m_name;
	string m_joystickFile;
	std::vector<CFacialAnimSkeletalAnimationEntry> m_skeletonAnimationEntries;

	typedef std::vector<_smart_ptr<CFacialAnimChannel> > Channels;
	Channels m_channels;
	Range m_timeRange;

	std::vector<CFacialAnimSoundEntry> m_soundEntries;

	int m_nProceduralChannelsValidateID;
	int m_nSoundEntriesValidateID;

	CFacialAnimation* m_pFaceAnim;

	CProceduralChannelSet m_proceduralChannels;

	CFacialCameraPathPositionInterpolator m_cameraPathPosition;
	CFacialCameraPathOrientationInterpolator m_cameraPathOrientation;
	CFacialAnimChannelInterpolator m_cameraPathFOV;
};

#endif // __IFacialAnimation_h__
