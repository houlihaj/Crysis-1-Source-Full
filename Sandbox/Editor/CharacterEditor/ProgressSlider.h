#pragma once

#include <set>

// CProgressSlider

class IDragger;

class ISliderListener
{
public:
	virtual void SliderDraggingFinished() = 0;
};

class CProgressSlider : public CSliderCtrl
{
	DECLARE_DYNAMIC(CProgressSlider)

	friend class ThumbDragger;
public:
	CProgressSlider();
	virtual ~CProgressSlider();
	void AddListener(ISliderListener* pListener);

	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);

	static const int NUM_INCREMENTS;

private:
	void InitialisePainting();
	void CreateMemoryBitmap(CDC& compatibleDC, CBitmap& compatibleBitmap, int nWidth, int nHeight);
	void CProgressSlider::ConvertToCompatibleBitmap(CDC& compatibleDC, CBitmap& compatibleBitmap, CBitmap& DIB);
	int GetBitmapWidth(const CBitmap& bitmap);
	int GetBitmapHeight(const CBitmap& bitmap);
	float GetThumbPositionX();
	float SetThumbPositionX(float nPositionX);

	CBitmap m_trackImage;
	CDC m_trackImageDC;

	CBitmap m_thumbImage;
	CDC m_thumbImageDC;

	CBitmap m_startImage;
	CDC m_startImageDC;

	CBitmap m_endImage;
	CDC m_endImageDC;

	CBitmap m_memoryBitmap;
	CDC m_memoryDC;

	CBitmap m_disabledBitmap;
	CDC m_disabledBitmapDC;

	bool m_bPaintingInitialised;
	
	IDragger* m_pDragger;

	std::set<ISliderListener*> m_listeners;

protected:
	DECLARE_MESSAGE_MAP()
};


