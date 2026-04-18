
#ifndef LINE_SEGMENT_2D_H
#define LINE_SEGMENT_2D_H

#include "Utils.h"

#include <vector>

/**
* @brief A simple oriented 2D line segment implementation.
*/
class LineSeg
{
	Vector2d m_start;
	Vector2d m_end;
public:
	LineSeg (const Vector2d & start, const Vector2d & end);
	const Vector2d & Start () const;
	const Vector2d & End () const;
	/// Inverts this line segment's direction.
	void Invert ();
	Vector2d Dir () const;
	/// Takes parameter and returns point corresponding to that parameter.
	Vector2d Pt (float t) const;
	/// Returns (unnormalized) normal vector to this lineseg.
	Vector2d Normal () const;
};

inline LineSeg::LineSeg (const Vector2d & start, const Vector2d & end) :
m_start (start), m_end (end)
{
}

inline const Vector2d & LineSeg::Start () const
{
	return m_start;
}

inline const Vector2d & LineSeg::End () const
{
	return m_end;
}

inline void LineSeg::Invert ()
{
	std::swap (m_start, m_end);
}

inline Vector2d LineSeg::Dir () const
{
	return m_end - m_start;
}

inline Vector2d LineSeg::Pt (float t) const
{
	return m_start + Dir() * t;
}

inline Vector2d LineSeg::Normal () const
{
	return Dir ().Perp ();
}

typedef std::vector <LineSeg> LineSegVec;

#endif
