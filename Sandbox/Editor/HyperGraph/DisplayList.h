#ifndef __DISPLAYLIST_H__
#define __DISPLAYLIST_H__

#pragma once

#include <Gdiplus.h>

class CDisplayElem : public _reference_target_t
{
public:
	CDisplayElem() : m_event(-1), m_bBoundsCacheValid(false) {}

	virtual void Draw( Gdiplus::Graphics * pGr ) const = 0;
	const Gdiplus::RectF& GetBounds( Gdiplus::Graphics * pGr ) const
	{
		if (!m_bBoundsCacheValid)
		{
			m_boundsCache = DoGetBounds(pGr);
			m_bBoundsCacheValid = true;
		}
		return m_boundsCache;
	}

	CDisplayElem * SetHitEvent( int event ) { m_event = event; return this; }
	int GetHitEvent() const { return m_event; }

protected:
	void InvalidateBoundsCache() { m_bBoundsCacheValid = false; }

private:
	virtual Gdiplus::RectF DoGetBounds( Gdiplus::Graphics * pGr ) const = 0;

	int m_event;
	mutable bool m_bBoundsCacheValid;
	mutable Gdiplus::RectF m_boundsCache;
};
typedef _smart_ptr<CDisplayElem> CDisplayElemPtr;

class CDisplayRectangle : public CDisplayElem
{
public:
	CDisplayRectangle() : m_pPen(0), m_pBrush(0) {}

	virtual void Draw( Gdiplus::Graphics * pGr ) const
	{
		if (m_pBrush)
			pGr->FillRectangle( m_pBrush, m_rect );
		if (m_pPen)
			pGr->DrawRectangle( m_pPen, m_rect );
	}
	virtual Gdiplus::RectF DoGetBounds( Gdiplus::Graphics * pGr ) const
	{
		return m_rect;
	}

	CDisplayRectangle * SetHitEvent( int event ) { CDisplayElem::SetHitEvent(event); return this; }

	CDisplayRectangle * SetRect( const Gdiplus::RectF& rect ) { m_rect = rect; InvalidateBoundsCache(); return this; }
	CDisplayRectangle * SetRect( float x, float y, float w, float h ) { m_rect.X = x; m_rect.Y = y; m_rect.Width = w; m_rect.Height = h; InvalidateBoundsCache(); return this; }
	CDisplayRectangle * SetFilled( Gdiplus::Brush * pBrush ) { m_pBrush = pBrush; return this; }
	CDisplayRectangle * SetStroked( Gdiplus::Pen * pPen ) { m_pPen = pPen; return this; }

private:
	Gdiplus::Pen * m_pPen;
	Gdiplus::Brush * m_pBrush;
	Gdiplus::RectF m_rect;
};

class CDisplayGradientRectangle : public CDisplayElem
{
public:
	CDisplayGradientRectangle() : m_pPen(0) {}

	virtual void Draw( Gdiplus::Graphics * pGr ) const
	{
		Gdiplus::LinearGradientBrush brush(m_rect, m_a, m_b, Gdiplus::LinearGradientModeVertical);
		pGr->FillRectangle( &brush, m_rect );
		if (m_pPen)
			pGr->DrawRectangle( m_pPen, m_rect );
	}
	virtual Gdiplus::RectF DoGetBounds( Gdiplus::Graphics * pGr ) const
	{
		return m_rect;
	}

	CDisplayGradientRectangle * SetHitEvent( int event ) { CDisplayElem::SetHitEvent(event); return this; }

	CDisplayGradientRectangle * SetRect( const Gdiplus::RectF& rect ) { m_rect = rect; InvalidateBoundsCache(); return this; }
	CDisplayGradientRectangle * SetRect( float x, float y, float w, float h ) { m_rect.X = x; m_rect.Y = y; m_rect.Width = w; m_rect.Height = h; InvalidateBoundsCache(); return this; }
	CDisplayGradientRectangle * SetStroked( Gdiplus::Pen * pPen ) { m_pPen = pPen; return this; }
	CDisplayGradientRectangle * SetColors( Gdiplus::Color a, Gdiplus::Color b ) { m_a = a; m_b = b; return this; }

private:
	Gdiplus::Pen * m_pPen;
	Gdiplus::RectF m_rect;
	Gdiplus::Color m_a, m_b;
};

class CDisplayEllipse : public CDisplayElem
{
public:
	CDisplayEllipse() : m_pPen(0), m_pBrush(0) {}

	virtual void Draw( Gdiplus::Graphics * pGr ) const
	{
		if (m_pBrush)
			pGr->FillEllipse( m_pBrush, m_rect );
		if (m_pPen)
			pGr->DrawEllipse( m_pPen, m_rect );
	}
	virtual Gdiplus::RectF DoGetBounds( Gdiplus::Graphics * pGr ) const
	{
		return m_rect;
	}

	CDisplayEllipse * SetHitEvent( int event ) { CDisplayElem::SetHitEvent(event); return this; }

	CDisplayEllipse * SetRect( const Gdiplus::RectF& rect ) { m_rect = rect; InvalidateBoundsCache(); return this; }
	CDisplayEllipse * SetRect( float x, float y, float w, float h ) { m_rect.X = x; m_rect.Y = y; m_rect.Width = w; m_rect.Height = h; InvalidateBoundsCache(); return this; }
	CDisplayEllipse * SetFilled( Gdiplus::Brush * pBrush ) { m_pBrush = pBrush; return this; }
	CDisplayEllipse * SetStroked( Gdiplus::Pen * pPen ) { m_pPen = pPen; return this; }

private:
	Gdiplus::Pen * m_pPen;
	Gdiplus::Brush * m_pBrush;
	Gdiplus::RectF m_rect;
};

//////////////////////////////////////////////////////////////////////////
class CDisplayPortArrow : public CDisplayElem
{
public:
	CDisplayPortArrow() : m_pPen(0), m_pBrush(0) {}

	virtual void Draw( Gdiplus::Graphics * pGr ) const
	{
		float l = 5;
		float h = 5;
		float h1 = 3;
		float x = m_rect.GetLeft();
		float y = m_rect.GetTop() + m_rect.Height/2 - h/2;
		Gdiplus::Point point0(x,y);
		Gdiplus::Point point1(x+l,y);
		Gdiplus::Point point2(x+l,y-h1);
		Gdiplus::Point point3(x+l+l,y+h/2);
		Gdiplus::Point point4(x+l,y+h+h1);
		Gdiplus::Point point5(x+l,y+h);
		Gdiplus::Point point6(x,y+h);
		Gdiplus::Point points[7] = {point0,point1, point2, point3, point4, point5,point6};

		if (m_pBrush)
		{
			pGr->FillPolygon( m_pBrush,points,7,Gdiplus::FillModeAlternate);
		}
		if (m_pPen)
			pGr->DrawPolygon( m_pPen,points,7 );
	}
	virtual Gdiplus::RectF DoGetBounds( Gdiplus::Graphics * pGr ) const
	{
		return m_rect;
	}

	CDisplayPortArrow * SetHitEvent( int event ) { CDisplayElem::SetHitEvent(event); return this; }

	CDisplayPortArrow * SetRect( const Gdiplus::RectF& rect ) { m_rect = rect; InvalidateBoundsCache(); return this; }
	CDisplayPortArrow * SetRect( float x, float y, float w, float h ) { m_rect.X = x; m_rect.Y = y; m_rect.Width = w; m_rect.Height = h; InvalidateBoundsCache(); return this; }
	CDisplayPortArrow * SetFilled( Gdiplus::Brush * pBrush ) { m_pBrush = pBrush; return this; }
	CDisplayPortArrow * SetStroked( Gdiplus::Pen * pPen ) { m_pPen = pPen; return this; }

private:
	Gdiplus::Pen * m_pPen;
	Gdiplus::Brush * m_pBrush;
	Gdiplus::RectF m_rect;
};

//////////////////////////////////////////////////////////////////////////
class CDisplayMinimizeArrow : public CDisplayElem
{
public:
	CDisplayMinimizeArrow() : m_pPen(0), m_pBrush(0) {}

	virtual void Draw( Gdiplus::Graphics * pGr ) const
	{
		float s = 3;
		float x = m_rect.GetLeft() + s;
		float y = m_rect.GetTop() + s;
		Gdiplus::Point point0(x,y);
		Gdiplus::Point point1(x+m_rect.Width-s*2,y);
		Gdiplus::Point point2(x+(m_rect.Width-s*2)/2,y+m_rect.Height-s*2);
		Gdiplus::Point points[3] = {point0,point1,point2};

		if (m_pBrush)
		{
			pGr->FillPolygon( m_pBrush,points,3,Gdiplus::FillModeAlternate);
		}
		if (m_pPen)
			pGr->DrawRectangle( m_pPen,m_rect );
		if (m_pPen)
			pGr->DrawPolygon( m_pPen,points,3 );
	}
	virtual Gdiplus::RectF DoGetBounds( Gdiplus::Graphics * pGr ) const
	{
		return m_rect;
	}

	CDisplayMinimizeArrow * SetHitEvent( int event ) { CDisplayElem::SetHitEvent(event); return this; }

	CDisplayMinimizeArrow * SetRect( const Gdiplus::RectF& rect ) { m_rect = rect; InvalidateBoundsCache(); return this; }
	CDisplayMinimizeArrow * SetRect( float x, float y, float w, float h ) { m_rect.X = x; m_rect.Y = y; m_rect.Width = w; m_rect.Height = h; InvalidateBoundsCache(); return this; }
	CDisplayMinimizeArrow * SetFilled( Gdiplus::Brush * pBrush ) { m_pBrush = pBrush; return this; }
	CDisplayMinimizeArrow * SetStroked( Gdiplus::Pen * pPen ) { m_pPen = pPen; return this; }

private:
	Gdiplus::Pen * m_pPen;
	Gdiplus::Brush * m_pBrush;
	Gdiplus::RectF m_rect;
};

//////////////////////////////////////////////////////////////////////////
class CDisplayString : public CDisplayElem
{
public:
	CDisplayString() : m_pFont(0), m_pBrush(0), m_pFormat(0) {}

	virtual void Draw( Gdiplus::Graphics * pGr ) const
	{
		if (!m_pBrush || !m_pFont)
			return;
		if (m_rect.IsEmptyArea())
		{
			if (m_pFormat)
				pGr->DrawString( m_string, m_string.GetLength(), m_pFont, m_location, m_pFormat, m_pBrush );
			else
				pGr->DrawString( m_string, m_string.GetLength(), m_pFont, m_location, m_pBrush );
		}
		else
		{
			if (m_pFormat)
			{
				m_pFormat->SetTrimming( Gdiplus::StringTrimmingEllipsisCharacter );
				pGr->DrawString( m_string, m_string.GetLength(), m_pFont, m_rect, m_pFormat, m_pBrush );
			}
			else
			{
				Gdiplus::StringFormat format;
				format.SetTrimming( Gdiplus::StringTrimmingEllipsisCharacter );
				format.SetAlignment( Gdiplus::StringAlignmentNear );
				pGr->DrawString( m_string, m_string.GetLength(), m_pFont, m_rect, &format, m_pBrush );
			}
		}
	}

	virtual Gdiplus::RectF DoGetBounds( Gdiplus::Graphics * pGr ) const
	{
		if (!m_rect.IsEmptyArea())
			return m_rect;
		return GetTextBounds(pGr);
	}
	virtual Gdiplus::RectF GetTextBounds( Gdiplus::Graphics * pGr ) const
	{
		if (!m_pBrush || !m_pFont)
			return Gdiplus::RectF(0,0,0,0);
		Gdiplus::RectF rect;
		if (m_pFormat)
			pGr->MeasureString( m_string, m_string.GetLength(), m_pFont, m_location, m_pFormat, &rect );
		else
			pGr->MeasureString( m_string, m_string.GetLength(), m_pFont, m_location, &rect );
		return rect;
	}

	CDisplayString * SetHitEvent( int event ) { CDisplayElem::SetHitEvent(event); return this; }

	CDisplayString * SetRect( const Gdiplus::RectF& rect ) { m_rect = rect; InvalidateBoundsCache(); return this; }
	CDisplayString * SetRect( float x, float y, float w, float h ) { m_rect.X = x; m_rect.Y = y; m_rect.Width = w; m_rect.Height = h; InvalidateBoundsCache(); return this; }
	CDisplayString * SetLocation( const Gdiplus::PointF& location ) { m_location = location; InvalidateBoundsCache(); return this; }
	CDisplayString * SetFont( Gdiplus::Font * pFont ) { m_pFont = pFont; InvalidateBoundsCache(); return this; }
	CDisplayString * SetBrush( Gdiplus::Brush * pBrush ) { m_pBrush = pBrush; InvalidateBoundsCache(); return this; }
	CDisplayString * SetText( const CStringW& text ) { m_string = text; InvalidateBoundsCache(); return this; }
	CDisplayString * SetText( const CString& text ) { m_string = text; InvalidateBoundsCache(); return this; }
	CDisplayString * SetStringFormat( Gdiplus::StringFormat * pFormat ) { m_pFormat = pFormat; InvalidateBoundsCache(); return this; }
	
private:
	CStringW m_string;
	Gdiplus::Font * m_pFont;
	Gdiplus::Brush * m_pBrush;
	Gdiplus::StringFormat * m_pFormat;
	Gdiplus::PointF m_location;
	Gdiplus::RectF m_rect;
};

//////////////////////////////////////////////////////////////////////////
class CDisplayImage : public CDisplayElem
{
public:
	CDisplayImage() : m_pImage(0), m_position(0.0f, 0.0f) {}

	CDisplayImage* SetImage(Gdiplus::Image* pImage) {if (m_pImage) delete m_pImage; m_pImage = pImage; return this;}
	CDisplayImage* SetLocation(const Gdiplus::PointF& position) {m_position = position; return this;}

	virtual void Draw( Gdiplus::Graphics * pGr ) const
	{
		if (!m_pImage)
			return;

		pGr->DrawImage(this->m_pImage, m_position);
	}

	virtual Gdiplus::RectF DoGetBounds( Gdiplus::Graphics * pGr ) const
	{
		return Gdiplus::RectF(m_position, Gdiplus::SizeF(m_pImage->GetWidth(), m_pImage->GetHeight()));
	}

private:
	Gdiplus::Image* m_pImage;
	Gdiplus::PointF m_position;
};

class CDisplayLine : public CDisplayElem
{
public:
	CDisplayLine() : m_pPen(0) {}

	virtual void Draw( Gdiplus::Graphics * pGr ) const
	{
		if (!m_pPen)
			return;
		pGr->DrawLine( m_pPen, m_start, m_end );
	}
	virtual Gdiplus::RectF DoGetBounds( Gdiplus::Graphics * pGr ) const
	{
		Gdiplus::RectF rect;
		if (m_start.X > m_end.X)
		{
			rect.X = m_end.X;
			rect.Width = m_start.X - m_end.X;
		}
		else
		{
			rect.X = m_start.X;
			rect.Width = m_end.X - m_start.X;
		}
		if (m_start.Y > m_end.Y)
		{
			rect.Y = m_end.Y;
			rect.Height = m_start.Y - m_end.Y;
		}
		else
		{
			rect.Y = m_start.Y;
			rect.Height = m_end.Y - m_start.Y;
		}
		return rect;
	}

	CDisplayLine * SetHitEvent( int event ) { CDisplayElem::SetHitEvent(event); return this; }

	CDisplayLine * SetPen( Gdiplus::Pen * pPen ) { m_pPen = pPen; InvalidateBoundsCache(); return this; }
	CDisplayLine * SetStart( Gdiplus::PointF start ) { m_start = start; InvalidateBoundsCache(); return this; }
	CDisplayLine * SetEnd( Gdiplus::PointF end ) { m_end = end; InvalidateBoundsCache(); return this; }
	CDisplayLine * SetStartEnd( const Gdiplus::PointF& start, const Gdiplus::PointF& end ) { m_start = start; m_end = end; InvalidateBoundsCache(); return this; }

private:
	Gdiplus::Pen * m_pPen;
	Gdiplus::PointF m_start;
	Gdiplus::PointF m_end;
};

class CDisplayList
{
public:
	CDisplayList() : m_pGraphics(0), m_bBoundsOk(true) {}

	template <class T>
	T * Add()
	{
		m_bBoundsOk = false;
		T * pT = new T();
		m_elems.push_back(pT);
		return pT;
	}
	void SetAttachRect( int id, Gdiplus::RectF attach )
	{
		m_attachRects[id] = attach;
	}
	bool GetAttachRect( int id, Gdiplus::RectF * pAttach ) const
	{
		std::map<int, Gdiplus::RectF>::const_iterator iter = m_attachRects.find(id);
		if (iter == m_attachRects.end())
			return false;
		*pAttach = iter->second;
		return true;
	}
	CDisplayLine* AddLine( const Gdiplus::PointF &p1,const Gdiplus::PointF &p2,Gdiplus::Pen *pPen )
	{
		CDisplayLine *pLine = Add<CDisplayLine>();
		pLine->SetPen( pPen );
		pLine->SetStartEnd( p1,p2 );
		return pLine;
	}

	void Clear()
	{
		m_bBoundsOk = false;
		m_elems.resize(0);
		m_attachRects.clear();
	}

	bool Empty()
	{
		return m_elems.empty();
	}

	Gdiplus::Graphics * GetGraphics()
	{
		return m_pGraphics;
	}
	void SetGraphics( Gdiplus::Graphics * pGr )
	{
		m_pGraphics = pGr;
	}

	void Draw()
	{
		for (std::vector<CDisplayElemPtr>::const_iterator iter = m_elems.begin(); iter != m_elems.end(); ++iter)
		{
			(*iter)->Draw( m_pGraphics );
		}
	}
	const Gdiplus::RectF& GetBounds() const
	{
		if (!m_bBoundsOk)
		{
			if (m_elems.empty())
				m_bounds = Gdiplus::RectF(0,0,0,0);
			else
			{
				m_bounds = m_elems[0]->GetBounds(m_pGraphics);
				for (size_t i=1; i<m_elems.size(); ++i)
					Gdiplus::RectF::Union(m_bounds, m_bounds, m_elems[i]->GetBounds(m_pGraphics));
			}
			m_bBoundsOk = true;
		}
		return m_bounds;
	}
	int GetObjectAt( Gdiplus::PointF point ) const
	{
		if (!GetBounds().Contains(point))
			return -1;
		for (std::vector<CDisplayElemPtr>::const_reverse_iterator iter = m_elems.rbegin(); iter != m_elems.rend(); ++iter)
		{
			int event = (*iter)->GetHitEvent();
			if (event != -1 && (*iter)->GetBounds(m_pGraphics).Contains(point))
				return event;
		}
		return -1;
	}

private:
	Gdiplus::Graphics * m_pGraphics;
	std::vector<CDisplayElemPtr> m_elems;
	std::map<int, Gdiplus::RectF> m_attachRects;
	mutable bool m_bBoundsOk;
	mutable Gdiplus::RectF m_bounds;
};

#endif
