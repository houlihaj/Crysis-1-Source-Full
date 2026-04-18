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
#include "StdAfx.h"
#include "DebrisMgr.h"

#include <cassert>

const float IDebrisMgr::Time::NEVER = 1000000.0f;

CDebrisMgr::CDebrisMgr () :
		m_expirationTimeMin (10.0f), m_expirationTimeMax (20.0f),
		m_maxDebrisPieces (0), m_baseTime (gEnv->pTimer->GetFrameStartTime ())
{
}

bool CDebrisMgr::DontExpirePieces () const
{
	return m_expirationTimeMin == IDebrisMgr::Time::NEVER;
}

bool CDebrisMgr::DebrisListLenLimited () const
{
	return m_maxDebrisPieces > 0;
}

/**
 * If pieces expire, this function returns time in seconds that a piece should
 * sit in the DebrisPiecesList before it expires.  I.e. in this case the
 * returned value is relative to the time the piece was added to the list.
 *
 * If pieces DON'T expire, the returned value is >="infinity" and it's absolute.
 */
float CDebrisMgr::GenerateExpirationTime () const
{
	if (DontExpirePieces ()) {

		if (m_debrisPieces.empty ()) {
			return IDebrisMgr::Time::NEVER;
		} else {
			DebrisPiecesList::const_iterator last = --m_debrisPieces.end ();
			return (*last).first;
		}
	} else {

		float range = m_expirationTimeMax - m_expirationTimeMin;
		return m_expirationTimeMin + (cry_rand () / (float )RAND_MAX) * range;
	}
}

void CDebrisMgr::AssureExpirationTimeConsistency ()
{
	if (m_expirationTimeMin > m_expirationTimeMax)
		m_expirationTimeMax = m_expirationTimeMin;
}

void CDebrisMgr::SetMinExpirationTime (Time time)
{
	m_expirationTimeMin = time;
	AssureExpirationTimeConsistency ();
}

void CDebrisMgr::SetMaxExpirationTime (Time time)
{
	m_expirationTimeMax = time;
	AssureExpirationTimeConsistency ();
}

void CDebrisMgr::SetMaxDebrisListLen (int new_max_list_len)
{
	m_maxDebrisPieces = new_max_list_len;
}

/**
 * 'expiration_time' should be given in seconds.
 */
void CDebrisMgr::AddPiece (IEntity* entity, Time expiration_time)
{
	// if the list is too long make space by removing a piece from the front
	if (DebrisListLenLimited () && (m_debrisPieces.size () >= m_maxDebrisPieces))
	{
		assert (m_debrisPieces.size () == m_maxDebrisPieces);

		DebrisPiecesList::iterator it = m_debrisPieces.begin ();
		assert (it != m_debrisPieces.end ());

		m_debrisPieces.erase (it);
	}

	// use absolute expiration time as a key in DebrisPiecesList
	Time abs_expiration_time = expiration_time;

	if (expiration_time < IDebrisMgr::Time::NEVER) {
		CTimeValue now = gEnv->pTimer->GetFrameStartTime () - m_baseTime;
		abs_expiration_time += now.GetSeconds ();
	}	// if expiration_time is infinity or higher, then it's already absolute

	m_debrisPieces.insert (std::make_pair (abs_expiration_time, entity->GetId ()));
}

void CDebrisMgr::AddPiece (IEntity* entity)
{
	AddPiece (entity, GenerateExpirationTime ());
}


void CDebrisMgr::RemovePiece(EntityId piece)
{ 
  // ugly, but necessary
  for (DebrisPiecesList::iterator it=m_debrisPieces.begin(),end=m_debrisPieces.end(); it!=end; ++it)
  {
    if (it->second == piece)
    { 
      m_debrisPieces.erase(it);
      break;
    }
  }

  gEnv->pEntitySystem->RemoveEntity(piece, true);
}

// TODO Jan 24, 2007: <pvl> hmmm ... if this stays a one-liner it *may* be more
// readable to inline it to Update()
void CDebrisMgr::ExpirePiece (EntityId entity_id) const
{
	gEnv->pEntitySystem->RemoveEntity (entity_id);
}

void CDebrisMgr::Update ()
{
	if (DontExpirePieces ())
		return;

	float now = (gEnv->pTimer->GetFrameStartTime () - m_baseTime).GetSeconds ();

	// iterator to the first item that will not expire now
	DebrisPiecesList::iterator first_nonexp = m_debrisPieces.lower_bound (now);
	DebrisPiecesList::iterator it = m_debrisPieces.begin ();

	for ( ; it != first_nonexp; ++it)
		ExpirePiece (it->second);

	m_debrisPieces.erase (m_debrisPieces.begin (), first_nonexp);
}
