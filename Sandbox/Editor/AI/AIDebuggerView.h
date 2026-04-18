////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   AIDebuggerView.h
//  Version:     v1.00
//  Created:     28-06-2006 by Matthew Jack
//  Compilers:   Visual Studio.NET 2003
//  Description: View that draws main debugging output
// -------------------------------------------------------------------------
//  History: Repackaged from Mikko's AI Signal View
//
//
////////////////////////////////////////////////////////////////////////////


#ifndef __AIDebuggerView_h__
#define __AIDebuggerView_h__
#pragma once

#include <list>
#include <IAISystem.h>


struct IAIDebugRecord;
struct IAIDebugStream;


//////////////////////////////////////////////////////////////////////////
//
//Drawing helper
//
//////////////////////////////////////////////////////////////////////////
class CAdjustDC
{
public:
	CAdjustDC(CDC* dc, int left, int top, int right, int bottom);
	CAdjustDC(CDC* dc, CRect &rect);
	~CAdjustDC();
	int GetWidth(void) { return m_width; }
	int GetHeight(void) { return m_height; };

protected:
	CDC *m_dc;
	int m_dcState;
	int m_width, m_height;
};


static UINT WM_FINDREPLACE = ::RegisterWindowMessage(FINDMSGSTRING);

//////////////////////////////////////////////////////////////////////////
//
// Main Dialog for AI Debugger
//
//////////////////////////////////////////////////////////////////////////
class CAIDebuggerView : public CView
{
	DECLARE_DYNAMIC(CAIDebuggerView)
public:
	CAIDebuggerView();
	~CAIDebuggerView();

	// Creates view window.
	BOOL Create( DWORD dwStyle,const RECT &rect,CWnd *pParentWnd,UINT nID );

	void	UpdateView();

	// Why can't we catch these messages? Called from parent.
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

protected:

	///////////////////////////////
	// Window handling functions
	///////////////////////////////

	DECLARE_MESSAGE_MAP()

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy();  

	afx_msg void OnDestroy();
	afx_msg void OnClose();  
	afx_msg void OnPaint();
	virtual void OnDraw(CDC* pDC) {};
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);


	afx_msg LRESULT OnFindReplace(WPARAM wParam, LPARAM lParam);

	///////////////////////////////
	// Internal members and functions
	///////////////////////////////

	static const int RULER_HEIGHT = 30;
	static const int LIST_WIDTH = 200;
	static const int DETAILS_WIDTH = 150;
	static const int ITEM_HEIGHT = 20;
	static const int TIME_TICK_SPACING = 40;

	void	UpdateViewRects();
	void  DrawRuler(CDC* dc);
	void	DrawList(CDC* dc);
	void	DrawTimeline(CDC* dc);
	void	DrawDetails(CDC* dc);
	void	DrawStream(CDC* dc, IAIDebugStream* pStream, int iy, int ih, int texth, COLORREF color, int i, int width);
	void	DrawStreamDetail(CDC* dc, IAIDebugStream* pStream, int iy, int ih, int texth, COLORREF color);
	void	DrawStreamFloat(CDC* dc, IAIDebugStream* pStream, int iy, int ih, int texth, COLORREF color, int i, int width, float vmin, float vmax);
	void	DrawStreamFloatDetail(CDC* dc, IAIDebugStream* pStream, int iy, int ih, int texth, COLORREF color);

	enum ERulerHit
	{
		RULERHIT_NONE,
		RULERHIT_EMPTY,
		RULERHIT_SEGMENT,
		RULERHIT_START,
		RULERHIT_END,
	};

	enum ECursorHit
	{
		CURSORHIT_NONE,
		CURSORHIT_EMPTY,
		CURSORHIT_CURSOR,
	};

	enum ESeparatorHit
	{
		SEPARATORHIT_NONE,
		SEPARATORHIT_LIST,
		SEPARATORHIT_DETAILS,
	};

	enum EDragType
	{
		DRAG_NONE,
		DRAG_PAN,
		DRAG_RULER_START,
		DRAG_RULER_END,
		DRAG_RULER_SEGMENT,
		DRAG_TIME_CURSOR,
		DRAG_LIST_WIDTH,
		DRAG_DETAILS_WIDTH,
		DRAG_ITEMS,
	};

	int		HitTestRuler(CPoint pt);
	int		HitTestTimeCursor(CPoint pt);
	int		HitTestSeparators(CPoint pt);

	int		TimeToClient(float t);
	float	ClientToTime(int x);

	bool	FindNext(const char* what, float start, float& t);
	bool	FindInStream(const char* what, float start, IAIDebugStream* pStream, float& t);

	float	GetRecordStartTime();
	float	GetRecordEndTime();

	void UpdateVertScrollInfo();

	const char*	GetLabelAtPoint(CPoint point);
	Vec3				GetPosAtPoint(CPoint point);

	void	AdjustViewToTimeCursor();

	void UpdateVertScroll();

	struct	SItem
	{
		inline SItem(const char* szName, int type) : name(szName), type(type), y(0), h(0) {}
		inline bool operator<(const SItem& rhs) const { return name.compare(rhs.name) < 0; }
		inline bool operator>(const SItem& rhs) const { return name.compare(rhs.name) > 0; }
		string			name;
		int					y;
		int					h;
		int					type;
	};

	typedef std::list<SItem>	TItemList;

	TItemList	m_items;

	int		m_listFullHeight;
	int		m_listItemHeight;

	CRect	m_rulerRect;
	CRect	m_listRect;
	CRect	m_timelineRect;
	CRect	m_detailsRect;

	CFont		m_smallFont;

	float		m_timeStart;
	float		m_timeScale;

	CBitmap	m_offscreenBitmap;

	EDragType	m_dragType;
	CPoint		m_dragStartPt;
	float			m_dragStartVal;
	float			m_dragStartVal2;

	float			m_timeRulerStart;
	float			m_timeRulerEnd;

	float			m_timeCursor;

	int				m_listWidth;
	int				m_detailsWidth;
	int				m_itemsOffset;

	CFindReplaceDialog*	m_pFindDialog;

	bool			m_streamSignalReceived;
	bool			m_streamSignalReceivedAux;
	bool			m_streamBehaviorSelected;
	bool			m_streamAttenionTarget;
	bool			m_streamGoalPipeSelected;
	bool			m_streamGoalPipeInserted;
	bool			m_streamLuaComment;
	bool			m_streamHealth;
	bool			m_streamHitDamage;
	bool			m_streamDeath;

	enum ETrackType
	{
		TRACK_TEXT,
		TRACK_FLOAT,
	};

	struct SStreamDesc
	{
		SStreamDesc(const char* na, int t, ETrackType tr, COLORREF c, int h, float vmin = 0.0f, float vmax = 0.0f) :
			name(na), type(t), track(tr), color(c), height(h), vmin(vmin), vmax(vmax) {}
		string			name;
		int					type;
		int					height;
		int					nameWidth;
		float				vmin, vmax;
		ETrackType	track;
		COLORREF		color;
	};

	void	UpdateStreamDesc();

	std::vector<SStreamDesc>	m_streams;

};


#endif __AIDebuggerView_h__
