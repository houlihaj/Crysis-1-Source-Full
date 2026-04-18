/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements debris manager interface.

-------------------------------------------------------------------------
History:
- 24:1:2007: Created by Pavel Mores

*************************************************************************/

#ifndef __CDEBRIS_MGR_H__
#define __CDEBRIS_MGR_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IDebrisMgr.h"

class CDebrisMgr : public IDebrisMgr {
public:

	CDebrisMgr ();

	/// Generates expiration time according to a system-wide policy.
	virtual void AddPiece (IEntity* );

	/// Lets caller set expiration for this piece.
	virtual void AddPiece (IEntity* , Time expiration_time);

  /// Removes a piece from the list. Use for cleanup.
  virtual void RemovePiece(EntityId piece);
	
	virtual void Update();
	
	/// Call SetMinExpirationTime (Time::NEVER) for never-expiring debris.
	virtual void SetMinExpirationTime (Time );
	virtual void SetMaxExpirationTime (Time );

	/// Use -1 if no limit on debris list length should be placed.
	virtual void SetMaxDebrisListLen (int );

	// Feel free to add
	virtual void GetMemoryStatistics(ICrySizer * s) {}

private:

	/// Generates expiration time wrt policy if caller hasn't supplied one.
	float GenerateExpirationTime () const;

	void AssureExpirationTimeConsistency ();

	/// Returns 'true' if "never expire" policy is in force.
	bool DontExpirePieces () const;

	/// Returns 'true' if max number of debris pieces is limited.
	bool DebrisListLenLimited () const;

	// NOTE Jan 24, 2007: <pvl> doesn't in fact need to be a member
	/**
	 * Encapsulates whatever needs to be done to an Entity before it dies.
	 * To be called once a piece expires.
	 */
	void ExpirePiece (EntityId ) const;

	// Resource usage policy - min/max expiration times, max debris list len.

	/// Given in seconds, counting starts at the moment debris is added.
	Time m_expirationTimeMin;
	Time m_expirationTimeMax;
	int m_maxDebrisPieces;

	/**
	 * To avoid big number pitfalls, all "absolute" time values are in fact
	 * relative to this value that's fixed when an instance is created.
	 */
	const CTimeValue m_baseTime;

	typedef std::multimap <Time,EntityId> DebrisPiecesList;
	
	DebrisPiecesList m_debrisPieces;
};

#endif // __CDEBRIS_MGR_H__

