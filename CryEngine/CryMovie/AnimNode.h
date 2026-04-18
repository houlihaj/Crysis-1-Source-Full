////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   animnode.h
//  Version:     v1.00
//  Created:     23/4/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description: Base of all Animation Nodes
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __animnode_h__
#define __animnode_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "IMovieSystem.h"
#include "Movie.h"

/*
//////////////////////////////////////////////////////////////////////////
// Description of the animation track.
//////////////////////////////////////////////////////////////////////////
struct SAnimParamInfo
{
	SAnimParamInfo() : name(""),paramId(0),valueType(AVALUE_FLOAT),flags(0) {};
	SAnimParamInfo( const char *_name,int _paramId,EAnimValue _valueType,int _flags ) : name(_name),paramId(_paramId),valueType(_valueType),flags(_flags) {};
	//////////////////////////////////////////////////////////////////////////
	const char *name;         // parameter name.
	const char *typeName;     // parameter type name.
	int paramId;              // parameter id.
	EAnimValue valueType;     // value type, defines type of track to use for animating this parameter.
};
/*
//////////////////////////////////////////////////////////////////////////
// Interface that describes animation node.
//////////////////////////////////////////////////////////////////////////
struct IAnimNodeClass
{
	// Creates a new animation node from this description.
	virtual IAnimNode* CreateNode() = 0;

	// Return name of the class.
	virtual const char* GetClass();

	//////////////////////////////////////////////////////////////////////////
	// Supported parameters.
	//////////////////////////////////////////////////////////////////////////

	// Description:
	//		Returns number of supported parameters by this animation node (position,rotation,scale,etc..).
	// Returns:
	//		Number of supported parameters.
	virtual int GetParamCount() const = 0;

	// Description:
	//		Returns decription of supported parameter of this animation node (position,rotation,scale,etc..).
	// Arguments:
	//		nIndex - parameter index in range 0 <= nIndex < GetSupportedParamCount()
	virtual bool GetParamInfo( int nIndex, SParamInfo &info ) const = 0;
};

//////////////////////////////////////////////////////////////////////////
// CAnimNodeClass implements IAnimNodeClass interface.
//////////////////////////////////////////////////////////////////////////
class CAnimNodeClass : public IAnimNodeClass
{
public:
private:
};
*/
/*
// Common parameter-bits of animation node.
enum AnimParamTypeBits
{
	APARAMBIT_POS =			0x00000001,	//!< Position parameter id.
	APARAMBIT_ROT =			0x00000002,	//!< Rotation parameter id.
	APARAMBIT_SCL =			0x00000004,	//!< Scale parameter id.
	APARAMBIT_ENTITY =	0x00000008,	//!< Entity parameter id.
	APARAMBIT_VISIBLE =	0x00000010,	//!< Visibilty parameter id.
	APARAMBIT_CAMERA =	0x00000020,	//!< Camera parameter id.
	APARAMBIT_FOV =			0x00000040,	//!< FOV parameter id.
};*/

#define PARAM_BIT(param) (1<<(param))

class CAnimBlock : public IAnimBlock
{
public:
	//////////////////////////////////////////////////////////////////////////
	virtual void Release() { if (--m_nRefCounter <= 0) delete this; }
	//////////////////////////////////////////////////////////////////////////

	void SetId( int id ) { m_id = id; };
	int	GetId() const { return m_id; };

	int GetTrackCount() const;
	bool GetTrackInfo( int index,int &paramId,IAnimTrack **pTrack ) const;

	const char* GetParamName( AnimParamType param ) const;

	IAnimTrack* GetTrack( int paramId ) const;
	void SetTrack( int paramId,IAnimTrack *track );
	IAnimTrack* CreateTrack( int paramId,EAnimValue valueType );

	void SetTimeRange( Range timeRange );

	bool RemoveTrack( IAnimTrack *track );

	void Serialize( IAnimNode *pNode,XmlNodeRef &xmlNode,bool bLoading, bool bLoadEmptyTracks=true );

private:
	void AddTrack( int param,IAnimTrack *track );

	struct TrackDesc
	{
		int paramId;           // Track parameter id.
		_smart_ptr<IAnimTrack> track;  // Track pointer.
	};
	std::vector<TrackDesc> m_tracks;
	int m_id;
};

/*!
		Base class for all Animation nodes,
		can host multiple animation tracks, and execute them other time.
		Animation node is reference counted.
 */
class CAnimNode : public IAnimNode
{
public:
	CAnimNode( IMovieSystem *sys );
	~CAnimNode();

	//////////////////////////////////////////////////////////////////////////
	virtual void Release() { if (--m_nRefCounter <= 0) delete this; }
	//////////////////////////////////////////////////////////////////////////

	void SetName( const char *name ) { m_name = name; };
	const char* GetName() { return m_name; };

	void SetId( int id ) { m_id = id; };
	int GetId() const { return m_id; };

	virtual void SetEntityGuid( const EntityGUID &guid ) {};
	virtual EntityGUID* GetEntityGuid() { return NULL; };
	virtual IEntity* GetEntity() { return 0; };

	void SetFlags( int flags );
	int GetFlags() const;

	virtual void CreateDefaultTracks() {};
	IAnimTrack* CreateTrack( int paramId );
	virtual bool RemoveTrack( IAnimTrack *pAnimTrack );
	int FindTrack(IAnimTrack *pTrack);

	IMovieSystem*	GetMovieSystem() { return m_pMovieSystem; };

	virtual void Reset() {}
	virtual void Pause() {}
	virtual void Resume() {}

	//////////////////////////////////////////////////////////////////////////
	// Space position/orientation scale.
	//////////////////////////////////////////////////////////////////////////
	void SetPos( float time,const Vec3 &pos ) {};
	void SetRotate( float time,const Quat &quat ) {};
	void SetScale( float time,const Vec3 &scale ) {};

	Vec3 GetPos() { return Vec3(0,0,0); };
	Quat GetRotate() { return Quat(0,0,0,0); };
	Vec3 GetScale() { return Vec3(0,0,0); };
	float GetTime() { return 0.0f; };

	//////////////////////////////////////////////////////////////////////////
	bool SetParamValue( float time,AnimParamType param,float val );
	bool SetParamValue( float time,AnimParamType param,const Vec3 &val );
	bool SetParamValue( float time,AnimParamType param,const Vec4 &val );
	bool GetParamValue( float time,AnimParamType param,float &val );
	bool GetParamValue( float time,AnimParamType param,Vec3 &val );
	bool GetParamValue( float time,AnimParamType param,Vec4 &val );

	void SetTarget( IAnimNode *node ) {};
	IAnimNode* GetTarget() const { return 0; };

	void SyncUpdate( ESyncUpdateType type,SAnimContext &ac ) {}
	void Animate( SAnimContext &ec );

	IAnimBlock* GetAnimBlock() const { return m_animBlock; };
	void SetAnimBlock( IAnimBlock *block );
	IAnimBlock* CreateAnimBlock() { return new CAnimBlock; };

	IAnimTrack* GetTrack( int nParamId ) const;
	void SetTrack( int nParamId,IAnimTrack *track );

	virtual void Serialize( XmlNodeRef &xmlNode,bool bLoading );

	virtual void SetNodeOwner( IAnimNodeOwner *pOwner ) { m_pOwner = pOwner; };
	virtual IAnimNodeOwner* GetNodeOwner() { return m_pOwner; };

	// Called by sequence when needs to activate a node.
	virtual void Activate( bool bActivate ) {};

	bool IsParamValid( int paramId ) const;

private:
	//! Id of animation node.
	int m_id;

	//! Name of animation node.
	string m_name;

	_smart_ptr<IAnimBlock> m_animBlock;

protected:
	IAnimNodeOwner* m_pOwner;
	CMovieSystem* m_pMovieSystem;
	unsigned int m_dwSupportedTracks;
	int m_flags;
	unsigned int m_bIgnoreSetParam : 1; // Internal flags.
};

#endif // __animnode_h__
