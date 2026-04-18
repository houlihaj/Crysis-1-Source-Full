// CharacterEditor\ProgressSlider.cpp : implementation file
//

#include "stdafx.h"
#include "CharacterEditor.h"
#include "CharacterEditor\ProgressSlider.h"


const int CProgressSlider::NUM_INCREMENTS = 100000;

// CProgressSlider

IMPLEMENT_DYNAMIC(CProgressSlider, CSliderCtrl)
CProgressSlider::CProgressSlider()
:	m_bPaintingInitialised(false),
	m_pDragger(0)
{
}

CProgressSlider::~CProgressSlider()
{
}

void CProgressSlider::AddListener(ISliderListener* pListener)
{
	m_listeners.insert(pListener);
}

BEGIN_MESSAGE_MAP(CProgressSlider, CSliderCtrl)
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

class IDragger
{
public:
	virtual void OnStartDragging(const CPoint& point, bool shift, bool control) = 0;
	virtual void OnUpdateDragging(const CPoint& point, bool shift, bool control) = 0;
	virtual void OnFinishDragging(const CPoint& point, bool shift, bool control) = 0;
};

class ThumbDragger : public IDragger
{
public:
	ThumbDragger(CProgressSlider& owner);
	virtual void OnStartDragging(const CPoint& point, bool shift, bool control);
	virtual void OnUpdateDragging(const CPoint& point, bool shift, bool control);
	virtual void OnFinishDragging(const CPoint& point, bool shift, bool control);
	bool ShouldStartDragging(const CPoint& point);

private:
	CPoint GetOffsetFromThumb(const CPoint& point);
	float GetNewThumbPos(const CPoint& point, bool shift, bool control);

	CPoint m_startPoint;
	Vec2 m_startThumb;
	CProgressSlider& m_owner;
};

// CProgressSlider message handlers

void CProgressSlider::OnPaint()
{
	// We have to load resources here, since OnCreate doesn't get called (DDX_Control is
	// called after the window is created, so WM_CREATE is handled by the default slider proc).
	InitialisePainting();

	// If there is no update rect, do nothing. Why do we get called when there is nothing to
	// do? Who can say?
	CRect rect;
	if (!GetUpdateRect(&rect))
		return;
	GetClientRect(&rect);

	// Get ready to paint.
	Invalidate(FALSE);
	PAINTSTRUCT paintStruct;
	CDC* pDC = BeginPaint(&paintStruct);
	//pDC->SelectClipRgn(0);

	// Clear the back buffer.
	CBrush backgroundBrush;
	backgroundBrush.Attach(GetSysColorBrush(COLOR_BTNFACE));
	m_memoryDC.FillRect(rect, &backgroundBrush);
	backgroundBrush.Detach();

	// Draw the track. The track is represented by a single bitmap - this is tiled across the background
	// of the slider.
	int nVerticalOffset = ((rect.bottom - rect.top) - GetBitmapHeight(m_trackImage)) / 2;
	for (int nX = rect.left; nX < rect.right; nX += GetBitmapWidth(m_trackImage))
	{
		// Blit the image to the back buffer.
		m_memoryDC.BitBlt(nX, nVerticalOffset, GetBitmapWidth(m_trackImage), GetBitmapHeight(m_trackImage), &m_trackImageDC, 0, 0, SRCCOPY);
	}

	// Draw the start and end bitmaps.
	COLORREF transparentColour = m_startImageDC.GetPixel(0, 0);
	nVerticalOffset = ((rect.bottom - rect.top) - GetBitmapHeight(m_startImage)) / 2;
	m_memoryDC.TransparentBlt(0, nVerticalOffset, GetBitmapWidth(m_startImage), GetBitmapHeight(m_startImage), &m_startImageDC, 0, 0, GetBitmapWidth(m_startImage), GetBitmapHeight(m_startImage), transparentColour);
	transparentColour = m_endImageDC.GetPixel(0, 0);
	nVerticalOffset = ((rect.bottom - rect.top) - GetBitmapHeight(m_endImage)) / 2;
	m_memoryDC.TransparentBlt(rect.right - GetBitmapWidth(m_endImage), nVerticalOffset, GetBitmapWidth(m_endImage), GetBitmapHeight(m_endImage), &m_endImageDC, 0, 0, GetBitmapWidth(m_endImage), GetBitmapHeight(m_endImage), transparentColour);

	// Work out the x coordinate at which to draw the thumb.
	int nPositionX = GetThumbPositionX() - GetBitmapWidth(m_thumbImage) / 2;

	// Draw the thumb
	transparentColour = m_thumbImageDC.GetPixel(0, 0);
	nVerticalOffset = ((rect.bottom - rect.top) - GetBitmapHeight(m_thumbImage)) / 2;
	m_memoryDC.TransparentBlt(nPositionX, nVerticalOffset, GetBitmapWidth(m_thumbImage), GetBitmapHeight(m_thumbImage), &m_thumbImageDC, 0, 0, GetBitmapWidth(m_thumbImage), GetBitmapHeight(m_thumbImage), transparentColour);

	// If we are disabled, we need to show it. Apply a dither pattern to the back buffer.
	if (!IsWindowEnabled())
	{
		BLENDFUNCTION blend;
		blend.BlendOp = AC_SRC_OVER;
		blend.BlendFlags = 0;
		blend.SourceConstantAlpha = 128;
		blend.AlphaFormat = 0;
		m_memoryDC.AlphaBlend(0, 0, rect.right - rect.left, rect.bottom - rect.top, &m_disabledBitmapDC, 0, 0, rect.right - rect.left, rect.bottom - rect.top, blend);
	}

	// Blit the back buffer to the screen.
	pDC->BitBlt(0, 0, rect.right - rect.left, rect.bottom - rect.top, &m_memoryDC, 0, 0, SRCCOPY);

	// Wind up the painting.
	EndPaint(&paintStruct);
}

void CProgressSlider::CreateMemoryBitmap(CDC& compatibleDC, CBitmap& compatibleBitmap, int nWidth, int nHeight)
{
	// Get the screen DC.
	CDC screenDC;
	screenDC.Attach(::GetWindowDC(0));

	// Create a bitmap compatible with the screen.
	compatibleBitmap.CreateCompatibleBitmap(&screenDC, nWidth, nHeight);

	// Create a dc compatible with the screen.
	compatibleDC.CreateCompatibleDC(&screenDC);
	compatibleDC.SelectObject(&compatibleBitmap);

	// Release the screen DC.
	::ReleaseDC(0, screenDC.Detach());
}

void CProgressSlider::ConvertToCompatibleBitmap(CDC& compatibleDC, CBitmap& compatibleBitmap, CBitmap& DIB)
{
	// Get the screen DC.
	CDC screenDC;
	screenDC.Attach(::GetWindowDC(0));

	// Create a compatible bitmap.
	compatibleBitmap.CreateCompatibleBitmap(&screenDC, GetBitmapWidth(DIB), GetBitmapHeight(DIB));

	// Create a compatible DC.
	compatibleDC.CreateCompatibleDC(&screenDC);
	compatibleDC.SelectObject(&compatibleBitmap);

	// Create a temporary DC to select the DIB.
	CDC tempDC;
	tempDC.CreateCompatibleDC(&screenDC);
	tempDC.SelectObject(DIB);

	// Blit to the compatible bitmap.
	compatibleDC.BitBlt(
		0,
		0,
		GetBitmapWidth(DIB),
		GetBitmapHeight(DIB),
		&tempDC,
		0,
		0,
		SRCCOPY);

	// Release the screen DC.
	::ReleaseDC(0, screenDC.Detach());
}

void CProgressSlider::InitialisePainting()
{
	if (m_bPaintingInitialised)
		return;

	// Load the bitmaps.
	{
		CBitmap dib;

		// Load the track image.
		dib.Attach(::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_PROGSLIDER_TRACK), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR | LR_SHARED));
		ConvertToCompatibleBitmap(m_trackImageDC, m_trackImage, dib);
		::DeleteObject(dib.Detach());

		// Load the thumb image.
		dib.Attach(::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_PROGSLIDER_THUMB), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR | LR_SHARED));
		ConvertToCompatibleBitmap(m_thumbImageDC, m_thumbImage, dib);
		::DeleteObject(dib.Detach());

		// Load the start image.
		dib.Attach(::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_PROGSLIDER_START), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR | LR_SHARED));
		ConvertToCompatibleBitmap(m_startImageDC, m_startImage, dib);
		::DeleteObject(dib.Detach());

		// Load the end image.
		dib.Attach(::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_PROGSLIDER_END), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR | LR_SHARED));
		ConvertToCompatibleBitmap(m_endImageDC, m_endImage, dib);
		::DeleteObject(dib.Detach());
	}

	// Create a memory dc for the slider.
	CRect rect;
	GetClientRect(&rect);
	CreateMemoryBitmap(m_memoryDC, m_memoryBitmap, rect.right - rect.left, rect.bottom - rect.top);

	// Create a bitmap to show when the control is disabled.
	{
		CreateMemoryBitmap(m_disabledBitmapDC, m_disabledBitmap, rect.right - rect.left, rect.bottom - rect.top);
		CBrush backgroundBrush;
		backgroundBrush.Attach(GetSysColorBrush(COLOR_BTNFACE));
		m_disabledBitmapDC.FillRect(rect, &backgroundBrush);
		backgroundBrush.Detach();
	}

	m_bPaintingInitialised = true;
}

int CProgressSlider::GetBitmapWidth(const CBitmap& bitmap)
{
	BITMAP bm;
	const_cast<CBitmap&>(bitmap).GetBitmap(&bm);
	return bm.bmWidth;
}

int CProgressSlider::GetBitmapHeight(const CBitmap& bitmap)
{
	BITMAP bm;
	const_cast<CBitmap&>(bitmap).GetBitmap(&bm);
	return bm.bmHeight;
}

void CProgressSlider::OnLButtonDown(UINT nFlags, CPoint point)
{
	// Grab the focus.
	SetFocus();

	// Check whether we should start dragging.
	if (!ThumbDragger(*this).ShouldStartDragging(point))
	{
		// Since we are far from the thumb, have the thumb snap to us.
		int nValue = SetThumbPositionX(point.x);
		GetParent()->SendMessage(WM_HSCROLL, MAKEWPARAM(TB_THUMBTRACK, nValue), (LPARAM)GetSafeHwnd());
	}

	// Capture the mouse.
	SetCapture();

	// Create a dragger object on the heap.
	m_pDragger = new ThumbDragger(*this);
	m_pDragger->OnStartDragging(point, (nFlags & MK_SHIFT) != 0, (nFlags & MK_CONTROL) != 0);
}

void CProgressSlider::OnLButtonUp(UINT nFlags, CPoint point)
{
	// Check whether we are dragging.
	if (m_pDragger != 0)
	{
		// Finalise dragging.
		m_pDragger->OnFinishDragging(point, (nFlags & MK_SHIFT) != 0, (nFlags & MK_CONTROL) != 0);
		delete m_pDragger;
		m_pDragger = 0;
		ReleaseCapture();
	}
}

void CProgressSlider::OnMouseMove(UINT nFlags, CPoint point)
{
	// Check whether we are dragging.
	if (m_pDragger != 0)
	{
		// Update dragging.
		m_pDragger->OnUpdateDragging(point, (nFlags & MK_SHIFT) != 0, (nFlags & MK_CONTROL) != 0);
	}
}

float CProgressSlider::GetThumbPositionX()
{
	RECT rect;
	GetClientRect(&rect);
	float fProgress = float(GetPos()) / NUM_INCREMENTS;
	int nThumbEndPadding = (GetBitmapWidth(m_thumbImage) / 2) + 2;
	float fVisualRange = float((rect.right - rect.left) - nThumbEndPadding * 2);
	float fPositionX = fProgress * fVisualRange + nThumbEndPadding + rect.left;
	return fPositionX;
}

float CProgressSlider::SetThumbPositionX(float fPositionX)
{
	RECT rect;
	GetClientRect(&rect);
	int nThumbEndPadding = (GetBitmapWidth(m_thumbImage) / 2) + 2;
	float fVisualRange = float((rect.right - rect.left) - nThumbEndPadding * 2);
	float fProgress = float(fPositionX - rect.left - nThumbEndPadding) / fVisualRange;
	float fValue = fProgress * NUM_INCREMENTS;
	SetPos(int(fValue));
	Invalidate(FALSE);
	return fValue;
}

ThumbDragger::ThumbDragger(CProgressSlider& owner)
:	m_owner(owner)
{
}

void ThumbDragger::OnStartDragging(const CPoint& point, bool shift, bool control)
{
	// Remember how far we are from the thumb.
	float fThumbPositionX = m_owner.GetThumbPositionX();
	RECT rect;
	m_owner.GetClientRect(&rect);
	float fThumbPositionY = float((rect.bottom + rect.top) / 2);

	m_startThumb = Vec2(fThumbPositionX, fThumbPositionY);
	m_startPoint = point;
}

void ThumbDragger::OnUpdateDragging(const CPoint& point, bool shift, bool control)
{
	// Maintain the offset from the thumb.
	float fNewThumbPosX = GetNewThumbPos(point, shift, control);
	float fValue = m_owner.SetThumbPositionX(fNewThumbPosX);
	m_owner.GetParent()->SendMessage(WM_HSCROLL, MAKEWPARAM(TB_THUMBTRACK, int(fValue)), (LPARAM)m_owner.GetSafeHwnd());
}

void ThumbDragger::OnFinishDragging(const CPoint& point, bool shift, bool control)
{
	// Maintain the offset from the thumb.
	float fNewThumbPosX = GetNewThumbPos(point, shift, control);
	float fValue = m_owner.SetThumbPositionX(fNewThumbPosX);
	m_owner.GetParent()->SendMessage(WM_HSCROLL, MAKEWPARAM(TB_THUMBTRACK, fValue), (LPARAM)m_owner.GetSafeHwnd());
	m_owner.GetParent()->SendMessage(WM_HSCROLL, MAKEWPARAM(TB_THUMBPOSITION, fValue), (LPARAM)m_owner.GetSafeHwnd());

	// Inform listeners of the event.
	std::for_each(m_owner.m_listeners.begin(), m_owner.m_listeners.end(), std::mem_fun<void, ISliderListener>(&ISliderListener::SliderDraggingFinished));
}

bool ThumbDragger::ShouldStartDragging(const CPoint& point)
{
	// Work out how far from the thumb the point is.
	CPoint distances = GetOffsetFromThumb(point);

	// Find the size of the thumb, by examining the thumb bitmap.
	int nExtentX = m_owner.GetBitmapWidth(m_owner.m_thumbImage) / 2;
	int nExtentY = m_owner.GetBitmapHeight(m_owner.m_thumbImage) / 2;

	// Check whether the point is close enough.
	return (abs(distances.x) < nExtentX && abs(distances.y) < nExtentY);
}

CPoint ThumbDragger::GetOffsetFromThumb(const CPoint& point)
{
	float nThumbPositionX = m_owner.GetThumbPositionX();
	RECT rect;
	m_owner.GetClientRect(&rect);
	int nThumbPositionY = (rect.bottom + rect.top) / 2;
	int nDistanceX = point.x - nThumbPositionX;
	int nDistanceY = point.y - nThumbPositionY;
	return CPoint(nDistanceX, nDistanceY);
}

float ThumbDragger::GetNewThumbPos(const CPoint& point, bool shift, bool control)
{
	float fOldThumbPosX = m_startThumb.x;
	float fDelta = point.x - m_startPoint.x;
	if (shift)
		fDelta *= 0.01f;
	if (control)
		fDelta *= 0.1f;
	float fNewThumbPosX = fOldThumbPosX + fDelta;
	return fNewThumbPosX;
}
