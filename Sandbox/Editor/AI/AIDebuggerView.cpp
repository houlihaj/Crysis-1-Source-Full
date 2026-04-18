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

#include "StdAfx.h"
#include "Resource.h"
#include "AIDebuggerView.h"

#include "Controls/MemDC.h"
#include <math.h>
#include <stdlib.h>
#include <IAIAction.h>
#include <IAISystem.h>
#include <IAgent.h>

#include "Resource.h"
#include "IViewPane.h"
#include "Objects\Entity.h"
#include "Objects\SelectionGroup.h"
#include "Clipboard.h"
#include "ViewManager.h"

#include "ItemDescriptionDlg.h"

#include "AIManager.h"


#define ID_AIDEBUGGER_COPY_LABEL					100
#define ID_AIDEBUGGER_FIND								101
#define ID_AIDEBUGGER_GOTO_START					102
#define ID_AIDEBUGGER_GOTO_END						103
#define ID_AIDEBUGGER_COPY_POS						104
#define ID_AIDEBUGGER_GOTO_POS						105

#define ID_AIDEBUGGER_SIGNAL_RECEIVED			1
#define ID_AIDEBUGGER_SIGNAL_RECEIVED_AUX	2
#define ID_AIDEBUGGER_BEHAVIOR_SELECTED		3
#define ID_AIDEBUGGER_ATTENTION_TARGET		4
#define ID_AIDEBUGGER_GOALPIPE_SELECTED		5
#define ID_AIDEBUGGER_GOALPIPE_INSERTED		6
#define ID_AIDEBUGGER_LUA_COMMENT					7
#define ID_AIDEBUGGER_HEALTH							8
#define ID_AIDEBUGGER_HIT_DAMAGE					9
#define ID_AIDEBUGGER_DEATH								10


//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CAIDebuggerView,CWnd)

BEGIN_MESSAGE_MAP (CAIDebuggerView, CWnd)
	ON_WM_MOUSEMOVE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_SIZE()
	//ON_WM_MOUSEWHEEL() // Why doesn't this work? Pass from parent.
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MBUTTONDBLCLK()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_VSCROLL( )
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_WM_SETCURSOR()
	
	ON_REGISTERED_MESSAGE(WM_FINDREPLACE, OnFindReplace)

	//ON_COMMAND(ID_MINIMIZE,OnToggleMinimize)
END_MESSAGE_MAP ()


//////////////////////////////////////////////////////////////////////////
CAIDebuggerView::CAIDebuggerView() :
m_dragType(DRAG_NONE),
m_timeRulerStart(0),
m_timeRulerEnd(0),
m_timeCursor(0),
m_pFindDialog(0),
m_itemsOffset(0),
m_listWidth(LIST_WIDTH),
m_listFullHeight(0),
m_listItemHeight(1),
m_detailsWidth(DETAILS_WIDTH),
m_streamSignalReceived(false),
m_streamSignalReceivedAux(true),
m_streamBehaviorSelected(false),
m_streamAttenionTarget(false),
m_streamGoalPipeSelected(false),
m_streamGoalPipeInserted(false),
m_streamLuaComment(false),
m_streamHealth(false),
m_streamHitDamage(false),
m_streamDeath(false)
{
}


//////////////////////////////////////////////////////////////////////////
CAIDebuggerView::~CAIDebuggerView() 
{
}

//////////////////////////////////////////////////////////////////////////
BOOL CAIDebuggerView::Create( DWORD dwStyle,const RECT &rect,CWnd *pParentWnd,UINT nID )
{
	if (!m_hWnd)
	{
		// Create window.
		CRect rcDefault(0,0,100,100);
		LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW,	AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
		VERIFY( CreateEx( NULL,lpClassName,"AIDebuggerView",dwStyle,rcDefault, pParentWnd, nID));

		if (!m_hWnd)
			return FALSE;

		// Set up scroll bar
//		SetScrollRange(SB_VERT, 0, 10000);

		// Create a custom font
		LOGFONT	smallFont;
		GetObject(gSettings.gui.hSystemFont, sizeof(LOGFONT), &smallFont);
		smallFont.lfHeight = (int)(smallFont.lfHeight * 0.85f);
		if(!m_smallFont.CreateFontIndirect(&smallFont))
			return false;

		// Data initialisation
		m_timeStart = 0.0f;
		m_timeScale = 5.0f / 100.0f;

		m_listWidth = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_ListWidth", LIST_WIDTH);
		m_detailsWidth = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_DetailsWidth", DETAILS_WIDTH);

		m_streamSignalReceived = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamSignalReceived", FALSE) == TRUE;
		m_streamSignalReceivedAux = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamSignalReceivedAux", TRUE) == TRUE;
		m_streamBehaviorSelected = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamBehaviorSelected", FALSE) == TRUE;
		m_streamAttenionTarget = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamAttentionTarget", FALSE) == TRUE;
		m_streamGoalPipeSelected = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamGoalPipeSelected", FALSE) == TRUE;
		m_streamGoalPipeInserted = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamGoalPipeInserted", FALSE) == TRUE;
		m_streamLuaComment = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamLuaComment", FALSE) == TRUE;
		m_streamHealth = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamHealth", FALSE) == TRUE;
		m_streamHitDamage = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamHitDamage", FALSE) == TRUE;
		m_streamDeath = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamDeath", FALSE) == TRUE;

		UpdateStreamDesc();

		m_timeStart = GetRecordStartTime();

		// Update views
		UpdateViewRects();
		UpdateView();

	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
BOOL CAIDebuggerView::PreTranslateMessage(MSG* pMsg)
{
	BOOL bResult = __super::PreTranslateMessage( pMsg );
	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::DoDataExchange(CDataExchange* pDX)
{
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::PostNcDestroy()
{
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnClose()
{
}

//////////////////////////////////////////////////////////////////////////
BOOL CAIDebuggerView::OnEraseBkgnd( CDC* pDC )
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
// Why can't we catch these messages? Called from parent.
BOOL CAIDebuggerView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	ScreenToClient(&pt);

	if(m_timelineRect.PtInRect(pt) || m_rulerRect.PtInRect(pt))
	{
		float	val = ((float)zDelta / (float)WHEEL_DELTA) * 0.75f;
		int	x = pt.x - m_timelineRect.left;
		float	mouseTime = ClientToTime(x);

		if(val > 0)
			m_timeScale *= val;
		else if(val < 0)
			m_timeScale /= -val;

		// Make sure the numbers stay sensible
		m_timeScale = CLAMP(m_timeScale, 0.000001f, 10000.0f);

		m_timeStart = mouseTime - x * m_timeScale;

//		UpdateView();
		Invalidate();

		return TRUE;
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnMouseMove(UINT nFlags, CPoint point) 
{
	if(m_dragType != DRAG_NONE)
	{
		int	dx = point.x - m_dragStartPt.x;
		int	dy = point.y - m_dragStartPt.y;
		float	dt = dx * m_timeScale;
		if(m_dragType == DRAG_PAN)
		{
			m_timeStart = m_dragStartVal - dt;

			if (!m_items.empty())
			{
				m_itemsOffset = (int)m_dragStartVal2 + dy;
				int maxItemsOffset = max(0, m_listFullHeight - m_listRect.Height());
				if (m_itemsOffset > 0) 
					m_itemsOffset = 0;
				if (m_itemsOffset < -maxItemsOffset)
					m_itemsOffset = -maxItemsOffset;
			}
			SetScrollPos(SB_VERT, -m_itemsOffset, TRUE);
			UpdateViewRects();
		}
		else if(m_dragType == DRAG_RULER_START)
		{
			m_timeRulerStart = m_dragStartVal + dt;
		}
		else if(m_dragType == DRAG_RULER_END)
		{
			m_timeRulerEnd = m_dragStartVal + dt;
		}
		else if(m_dragType == DRAG_RULER_SEGMENT)
		{
			m_timeRulerStart = m_dragStartVal + dt;
			m_timeRulerEnd = m_dragStartVal2 + dt;
		}
		else if(m_dragType == DRAG_TIME_CURSOR)
		{
			m_timeCursor = m_dragStartVal + dt;
		}
		else if(m_dragType == DRAG_LIST_WIDTH)
		{
			m_listWidth = (int)m_dragStartVal + dx;
			if(m_listWidth < 10) m_listWidth = 10;
			UpdateViewRects();
		}
		else if(m_dragType == DRAG_DETAILS_WIDTH)
		{
			m_detailsWidth = (int)m_dragStartVal - dx;
			if(m_detailsWidth < 10) m_detailsWidth = 10;
			UpdateViewRects();
		}
		else if(m_dragType == DRAG_ITEMS)
		{
			if (!m_items.empty())
			{
				m_itemsOffset = (int)m_dragStartVal + dy;
				int maxItemsOffset = max(0, m_listFullHeight - m_listRect.Height());
				if (m_itemsOffset > 0) 
					m_itemsOffset = 0;
				if (m_itemsOffset < -maxItemsOffset)
					m_itemsOffset = -maxItemsOffset;
			}
			SetScrollPos(SB_VERT, -m_itemsOffset, TRUE);
			UpdateViewRects();
		}

//		UpdateView();
		Invalidate();
	}

	CWnd::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnLButtonUp(UINT nFlags, CPoint point) 
{
	CWnd::OnLButtonUp(nFlags, point);

	if(m_timeRulerStart > m_timeRulerEnd)
		std::swap(m_timeRulerStart, m_timeRulerEnd);

	if(m_dragType != DRAG_NONE)
	{
		m_dragType = DRAG_NONE;
//		UpdateView();
		Invalidate();

		ReleaseCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	CWnd::OnLButtonDblClk(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnMButtonDown(UINT nFlags, CPoint point) 
{
	CWnd::OnMButtonDown(nFlags, point);

	if(m_dragType == DRAG_NONE)
	{
		if(m_timelineRect.PtInRect(point) || m_rulerRect.PtInRect(point))
		{
			m_dragStartVal = m_timeStart;
			m_dragStartVal2 = m_itemsOffset;
			m_dragStartPt = point;
			m_dragType = DRAG_PAN;
//			UpdateView();
			Invalidate();
			SetCapture();
		} 
		else if (m_listRect.PtInRect(point) || m_listRect.PtInRect(point))
		{
			m_dragStartVal = m_itemsOffset;
			m_dragStartPt = point;
			m_dragType = DRAG_ITEMS;
//			UpdateView();
			Invalidate();
			SetCapture();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnMButtonUp(UINT nFlags, CPoint point) 
{
	CWnd::OnMButtonUp(nFlags, point);

	if(m_dragType != DRAG_NONE)
	{
		m_dragType = DRAG_NONE;
//		UpdateView();
		Invalidate();

		ReleaseCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CWnd::OnLButtonDown(nFlags, point);

	int	rulerHit = HitTestRuler(point);
	int	cursorHit = HitTestTimeCursor(point);
	int	separatorHit = HitTestSeparators(point);

	if(rulerHit != RULERHIT_NONE)
	{
		if(rulerHit == RULERHIT_EMPTY)
		{
			// Create ruler
			m_timeRulerStart = ClientToTime(point.x - m_rulerRect.left);
			m_timeRulerEnd = m_timeRulerStart;
			m_dragStartVal = m_timeRulerStart;
			m_dragType = DRAG_RULER_END;
		}
		else if(rulerHit == RULERHIT_START)
		{
			m_dragStartVal = m_timeRulerStart;
			m_dragType = DRAG_RULER_START;
		}
		else if(rulerHit == RULERHIT_END)
		{
			m_dragStartVal = m_timeRulerEnd;
			m_dragType = DRAG_RULER_END;
		}
		else if(rulerHit == RULERHIT_SEGMENT)
		{
			m_dragStartVal = m_timeRulerStart;
			m_dragStartVal2 = m_timeRulerEnd;
			m_dragType = DRAG_RULER_SEGMENT;
		}
		m_dragStartPt = point;
//		UpdateView();
		Invalidate();
		SetCapture();
	}
	else if(separatorHit != SEPARATORHIT_NONE)
	{
		if(separatorHit == SEPARATORHIT_LIST)
		{
			m_dragStartVal = m_listWidth;
			m_dragType = DRAG_LIST_WIDTH;
		}
		else
		{
			m_dragStartVal = m_detailsWidth;
			m_dragType = DRAG_DETAILS_WIDTH;
		}
		m_dragStartPt = point;
//		UpdateView();
		Invalidate();
		SetCapture();
	}
	else if(cursorHit != CURSORHIT_NONE)
	{
		if(cursorHit == CURSORHIT_EMPTY)
			m_timeCursor = ClientToTime(point.x - m_timelineRect.left);
		m_dragStartVal = m_timeCursor;
		m_dragType = DRAG_TIME_CURSOR;
		m_dragStartPt = point;
//		UpdateView();
		Invalidate();
		SetCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnMButtonDblClk(UINT nFlags, CPoint point)
{
	CWnd::OnMButtonDblClk(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnRButtonUp(UINT nFlags, CPoint point) 
{
	CWnd::OnRButtonUp(nFlags, point);

	if(m_timelineRect.PtInRect(point))
	{
		const	char*	curLabel = GetLabelAtPoint(point);
		CString	copyLabel("Copy Label ");
		bool	hasLabel(false);
		if(curLabel && strlen(curLabel) > 0)
		{
			copyLabel += curLabel;
			hasLabel = true;
		}

		Vec3	pos = GetPosAtPoint(point);
		CString	gotoPos("Goto agent location ");
		CString	copyPos("Copy agent location ");
		bool	hasPos(false);
		char	posLabel[64] = "\0";
		if(!pos.IsZero(0.0001f))
		{
			_snprintf(posLabel, 64, "%.2f, %.2f, %.2f", pos.x, pos.y, pos.z);
			copyPos += posLabel;
			gotoPos += posLabel;
			hasPos = true;
		}

		CPoint screenPoint = point;
		ClientToScreen(&screenPoint);

		CMenu	menu;
		menu.CreatePopupMenu();

		menu.AppendMenu(MF_STRING|(!hasLabel ? MF_DISABLED : 0), ID_AIDEBUGGER_COPY_LABEL, copyLabel);
		menu.AppendMenu(MF_STRING, ID_AIDEBUGGER_FIND, "Find...");
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_AIDEBUGGER_GOTO_START, "Goto Start");
		menu.AppendMenu(MF_STRING, ID_AIDEBUGGER_GOTO_END, "Goto End");
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING|(!hasPos ? MF_DISABLED : 0), ID_AIDEBUGGER_GOTO_POS, gotoPos);
		menu.AppendMenu(MF_STRING|(!hasPos ? MF_DISABLED : 0), ID_AIDEBUGGER_COPY_POS, copyPos);
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING|(m_streamSignalReceived ? MF_CHECKED : 0), ID_AIDEBUGGER_SIGNAL_RECEIVED, "Signal Received");
		menu.AppendMenu(MF_STRING|(m_streamSignalReceivedAux ? MF_CHECKED : 0), ID_AIDEBUGGER_SIGNAL_RECEIVED_AUX, "Signal Received Aux");
		menu.AppendMenu(MF_STRING|(m_streamBehaviorSelected ? MF_CHECKED : 0), ID_AIDEBUGGER_BEHAVIOR_SELECTED, "Behaviour Selected");
		menu.AppendMenu(MF_STRING|(m_streamAttenionTarget ? MF_CHECKED : 0), ID_AIDEBUGGER_ATTENTION_TARGET, "Attention Target");
		menu.AppendMenu(MF_STRING|(m_streamGoalPipeSelected ? MF_CHECKED : 0), ID_AIDEBUGGER_GOALPIPE_SELECTED, "Selected Goal Pipe");
		menu.AppendMenu(MF_STRING|(m_streamGoalPipeInserted ? MF_CHECKED : 0), ID_AIDEBUGGER_GOALPIPE_INSERTED, "Inserted Goal Pipe");
		menu.AppendMenu(MF_STRING|(m_streamLuaComment ? MF_CHECKED : 0), ID_AIDEBUGGER_LUA_COMMENT, "Lua Comment");
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING|(m_streamHealth ? MF_CHECKED : 0), ID_AIDEBUGGER_HEALTH, "Health");
		menu.AppendMenu(MF_STRING|(m_streamHitDamage ? MF_CHECKED : 0), ID_AIDEBUGGER_HIT_DAMAGE, "HitDamage");
		menu.AppendMenu(MF_STRING|(m_streamDeath ? MF_CHECKED : 0), ID_AIDEBUGGER_DEATH, "Death");

		int cmd = menu.TrackPopupMenu(TPM_RETURNCMD|TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY, screenPoint.x, screenPoint.y, this);

		if(cmd == ID_AIDEBUGGER_COPY_LABEL)
		{
			if(curLabel)
			{
				CClipboard clipboard;
				clipboard.PutString(curLabel);
			}
		}
		else if(cmd == ID_AIDEBUGGER_FIND)
		{
			if(m_pFindDialog == NULL)
			{
				CString	find = AfxGetApp()->GetProfileString(AfxGetApp()->m_pszAppName, "AIDebugger_FindString");
				m_pFindDialog = new CFindReplaceDialog();
				m_pFindDialog->Create(TRUE, find, NULL, FR_DOWN|FR_HIDEUPDOWN|FR_HIDEMATCHCASE|FR_HIDEWHOLEWORD, this);
			}
		}
		else if(cmd == ID_AIDEBUGGER_GOTO_START)
		{
			m_timeCursor = GetRecordStartTime();
			AdjustViewToTimeCursor();
		}
		else if(cmd == ID_AIDEBUGGER_GOTO_END)
		{
			m_timeCursor = GetRecordEndTime();
			AdjustViewToTimeCursor();
		}
		else if(cmd == ID_AIDEBUGGER_COPY_POS)
		{
			CClipboard clipboard;
			clipboard.PutString(posLabel);
		}
		else if(cmd == ID_AIDEBUGGER_GOTO_POS)
		{
			CViewport *pRenderViewport = GetIEditor()->GetViewManager()->GetGameViewport();
			if (pRenderViewport)
			{
				Matrix34 tm = pRenderViewport->GetViewTM();
				tm.SetTranslation(pos);
				pRenderViewport->SetViewTM(tm);
			}
		}
		else if(cmd == ID_AIDEBUGGER_SIGNAL_RECEIVED)
			m_streamSignalReceived = !m_streamSignalReceived;
		else if(cmd == ID_AIDEBUGGER_SIGNAL_RECEIVED_AUX)
			m_streamSignalReceivedAux = !m_streamSignalReceivedAux;
		else if(cmd == ID_AIDEBUGGER_BEHAVIOR_SELECTED)
			m_streamBehaviorSelected = !m_streamBehaviorSelected;
		else if(cmd == ID_AIDEBUGGER_ATTENTION_TARGET)
			m_streamAttenionTarget = !m_streamAttenionTarget;
		else if(cmd == ID_AIDEBUGGER_GOALPIPE_SELECTED)
			m_streamGoalPipeSelected = !m_streamGoalPipeSelected;
		else if(cmd == ID_AIDEBUGGER_GOALPIPE_INSERTED)
			m_streamGoalPipeInserted = !m_streamGoalPipeInserted;
		else if(cmd == ID_AIDEBUGGER_LUA_COMMENT)
			m_streamLuaComment = !m_streamLuaComment;
		else if(cmd == ID_AIDEBUGGER_HEALTH)
			m_streamHealth = !m_streamHealth;
		else if(cmd == ID_AIDEBUGGER_HIT_DAMAGE)
			m_streamHitDamage = !m_streamHitDamage;
		else if(cmd == ID_AIDEBUGGER_DEATH)
			m_streamDeath = !m_streamDeath;

		UpdateStreamDesc();

		UpdateView();
		Invalidate();
	}
}


///////////////////////////////
// Internal functions
///////////////////////////////

//////////////////////////////////////////////////////////////////////////
int	CAIDebuggerView::HitTestRuler(CPoint pt)
{
	if(!m_rulerRect.PtInRect(pt))
		return RULERHIT_NONE;

	bool	hasRuler(false);
	int		rulerStartX, rulerEndX;

	if(fabsf(m_timeRulerEnd - m_timeRulerStart) > 0.001f)
	{
		rulerStartX = m_rulerRect.left + TimeToClient(m_timeRulerStart);
		rulerEndX = m_rulerRect.left + TimeToClient(m_timeRulerEnd);
		if(rulerStartX > rulerEndX)
			std::swap(rulerStartX, rulerEndX);
		hasRuler = true;
	}

	if(hasRuler)
	{
		if(abs(pt.x - rulerStartX) < 2)
			return RULERHIT_START;
		else if(abs(pt.x - rulerEndX) < 2)
			return RULERHIT_END;
		else if(pt.x > rulerStartX && pt.x < rulerEndX)
			return RULERHIT_SEGMENT;
	}

	return RULERHIT_EMPTY;
}

//////////////////////////////////////////////////////////////////////////
int	CAIDebuggerView::HitTestTimeCursor(CPoint pt)
{
	if(!m_timelineRect.PtInRect(pt))
		return CURSORHIT_NONE;

	int	cursorX = m_timelineRect.left + TimeToClient(m_timeCursor);

	if(abs(pt.x - cursorX) < 3)
		return CURSORHIT_CURSOR;

	return CURSORHIT_EMPTY;
}

//////////////////////////////////////////////////////////////////////////
int	CAIDebuggerView::HitTestSeparators(CPoint pt)
{
	if(abs(pt.x - m_listRect.right) < 3)
		return SEPARATORHIT_LIST;
	else if(abs(pt.x - m_detailsRect.left) < 3)
		return SEPARATORHIT_DETAILS;

	return SEPARATORHIT_NONE;
}

//////////////////////////////////////////////////////////////////////////
const char* CAIDebuggerView::GetLabelAtPoint(CPoint point)
{
	point.x -= m_timelineRect.left;
	point.y -= m_timelineRect.top;

	IAISystem* pAISystem(GetIEditor()->GetAI()->GetAISystem());

	float	t = ClientToTime(point.x);

	for(TItemList::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		const SItem&	item (*it);
		int	h = point.y - item.y;
		if(h < 0 && h >= item.h)
			continue;

//		int	n = h / ITEM_HEIGHT;
		int	n = 0;
		int	y = 0;
		for(; n < (int)m_streams.size(); ++n)
		{
			y += m_streams[n].height;
			if(h <= y)
				break;
		}

		IAIObject* pAI = pAISystem->GetAIObjectByName(item.type, item.name.c_str());
		if(!pAI)
			continue;

		IAIDebugRecord* pRecord(pAI->GetAIDebugRecord());
		if(!pRecord)
			continue;

		if(n >= 0 && n < (int)m_streams.size() && m_streams[n].track == TRACK_TEXT)
		{
			IAIDebugStream*	pStream = pRecord->GetStream((IAIRecordable::e_AIDbgEvent)m_streams[n].type);
			if(!pStream)
				return 0;
			float	curTime;
			pStream->Seek(t);
			return (char*)(pStream->GetCurrent(curTime));
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CAIDebuggerView::GetPosAtPoint(CPoint point)
{
	point.x -= m_timelineRect.left;
	point.y -= m_timelineRect.top;

	IAISystem* pAISystem(GetIEditor()->GetAI()->GetAISystem());

	float	t = ClientToTime(point.x);

	for(TItemList::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		const SItem&	item (*it);
		int	h = point.y - item.y;
		if(h < 0 && h >= item.h)
			continue;

		IAIObject* pAI = pAISystem->GetAIObjectByName(item.type, item.name.c_str());
		if(!pAI)
			continue;

		IAIDebugRecord* pRecord(pAI->GetAIDebugRecord());
		if(!pRecord)
			continue;

		IAIDebugStream*	pStream = pRecord->GetStream(IAIRecordable::E_AGENTPOS);
		if(!pStream)
			return Vec3(ZERO);
		float	curTime;
		pStream->Seek(t);
		Vec3*	pos = (Vec3*)pStream->GetCurrent(curTime);
		if(pos)
			return *pos;
		return Vec3(ZERO);
	}

	return Vec3(ZERO);
}

//////////////////////////////////////////////////////////////////////////
bool CAIDebuggerView::FindNext(const char* what, float start, float& t)
{
	float	bestTime = GetRecordEndTime();
	bool	found = false;

	IAISystem* pAISystem(GetIEditor()->GetAI()->GetAISystem());

	for(TItemList::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		const SItem&	item (*it);

		IAIObject* pAI = pAISystem->GetAIObjectByName(item.type, item.name.c_str());
		if(!pAI)
			continue;

		IAIDebugRecord* pRecord(pAI->GetAIDebugRecord());
		if(!pRecord)
			continue;

		for(size_t i = 0; i < m_streams.size(); i++)
		{
			IAIDebugStream*	pStream = pRecord->GetStream((IAIRecordable::e_AIDbgEvent)m_streams[i].type);
			if(pStream)
			{
				float	t;
				if(FindInStream(what, start, pStream, t))
				{
					if(t > start && t < bestTime)
						bestTime = t;
					found = true;
				}
			}
		}
	}

	t = bestTime;

	return found;
}

//////////////////////////////////////////////////////////////////////////
bool CAIDebuggerView::FindInStream(const char* what, float start, IAIDebugStream* pStream, float& t)
{
	float	curTime;
	pStream->Seek(start);
	char*	pString = (char*)(pStream->GetCurrent(curTime));

	while(pStream->GetCurrentIdx() < pStream->GetSize())
	{
		float nextTime;
		char* pNextString = (char*)(pStream->GetNext(nextTime));

		if(pString && curTime > start && strstri(pString, what) != 0)
		{
			t = curTime;
			return true;
		}

		curTime = nextTime;
		pString = pNextString;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::AdjustViewToTimeCursor()
{
	float	timeStart = m_timeStart;
	float	timeEnd = m_timeStart + m_timelineRect.Width() * m_timeScale;
	float	timeGap = 10 * m_timeScale;

	timeStart += timeGap;
	timeEnd -= timeGap;

	if(timeEnd < timeStart) timeEnd = timeStart;

	if(m_timeCursor < timeStart)
	{
		m_timeStart = m_timeCursor - timeGap;
	}
	else if(m_timeCursor > timeEnd)
	{
		float	dt = timeEnd - timeStart;
		m_timeStart = m_timeCursor - dt;
	}
}

//////////////////////////////////////////////////////////////////////////
afx_msg LRESULT CAIDebuggerView::OnFindReplace(WPARAM wParam, LPARAM lParam)
{
	if(!m_pFindDialog)
		return 0;

	if(m_pFindDialog->FindNext())
	{
		CString   find = m_pFindDialog->GetFindString();

		float	t;
		bool	found = FindNext(find, m_timeCursor, t);
		if(!found && fabsf(t - GetRecordEndTime()) < 0.001f )
		{
			float	start = GetRecordStartTime();
			found = FindNext(find, start, t);
			if(!found)
			{
				CString	msg("The following specified text was not found:\n");
				msg += find;
				AfxMessageBox(msg, MB_OK);
			}
		}
		if(found)
		{
			m_timeCursor = t;
			AdjustViewToTimeCursor();
		}

//		UpdateView();
		Invalidate();
	}
	else if(m_pFindDialog->IsTerminating())
	{
		CString   find = m_pFindDialog->GetFindString();
		AfxGetApp()->WriteProfileString(AfxGetApp()->m_pszAppName, "AIDebugger_FindString", find);
		m_pFindDialog = 0;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnDestroy()
{
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_ListWidth", m_listWidth);
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_DetailsWidth", m_detailsWidth);

	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamSignalReceived", m_streamSignalReceived ? TRUE : FALSE);
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamSignalReceivedAux", m_streamSignalReceivedAux ? TRUE : FALSE);
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamBehaviorSelected", m_streamBehaviorSelected ? TRUE : FALSE);
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamAttentionTarget", m_streamAttenionTarget ? TRUE : FALSE);
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamGoalPipeSelected", m_streamGoalPipeSelected ? TRUE : FALSE);
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamGoalPipeInserted", m_streamGoalPipeInserted ? TRUE : FALSE);
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamLuaComment", m_streamLuaComment ? TRUE : FALSE);
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamHealth", m_streamHealth ? TRUE : FALSE);
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamHitDamage", m_streamHitDamage ? TRUE : FALSE);
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "AIDebugger_StreamDeath", m_streamDeath ? TRUE : FALSE);

	if(m_pFindDialog)
	{
		m_pFindDialog->DestroyWindow();
		m_pFindDialog = 0;
	}

	m_smallFont.DeleteObject();

	IAISystem* pAISystem(GetIEditor()->GetAI()->GetAISystem());
	if(pAISystem)
		pAISystem->SetDrawRecorderRange(0, 0, 0);

	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::UpdateViewRects()
{
	CRect	clientRc;
	GetClientRect(clientRc);

	m_rulerRect.left = clientRc.left + m_listWidth;
	m_rulerRect.right = clientRc.right - m_detailsWidth;
	m_rulerRect.top = clientRc.top;
	m_rulerRect.bottom = clientRc.top + RULER_HEIGHT;

	m_listRect.left = clientRc.left;
	m_listRect.right = clientRc.left + m_listWidth;
	m_listRect.top = clientRc.top + RULER_HEIGHT;
	m_listRect.bottom = clientRc.bottom;

	m_timelineRect.left = clientRc.left + m_listWidth;
	m_timelineRect.right = clientRc.right - m_detailsWidth;
	m_timelineRect.top = clientRc.top + RULER_HEIGHT;
	m_timelineRect.bottom = clientRc.bottom;

	m_detailsRect.left = clientRc.right - m_detailsWidth;
	m_detailsRect.right = clientRc.right;
	m_detailsRect.top = clientRc.top + RULER_HEIGHT;
	m_detailsRect.bottom = clientRc.bottom;
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::UpdateView()
{
	m_items.clear();

	IAISystem *pAISystem( GetIEditor()->GetAI()->GetAISystem() );

	int	y = 0;

	int	rows = (int)m_streams.size();
	if(rows == 0)
		rows = 1;

	int	h = 0;
	for(size_t i = 0; i < m_streams.size(); ++i)
		h += m_streams[i].height;
	if(h < ITEM_HEIGHT)
		h = ITEM_HEIGHT;

	AutoAIObjectIter	it = pAISystem->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIOBJECT_PUPPET);
	for(; it->GetObject(); it->Next())
		m_items.push_front(SItem(it->GetObject()->GetName(), AIOBJECT_PUPPET));
	AutoAIObjectIter	itv = pAISystem->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIOBJECT_VEHICLE);
	for(; itv->GetObject(); itv->Next())
		m_items.push_front(SItem(itv->GetObject()->GetName(), AIOBJECT_VEHICLE));

	m_items.sort();

	AutoAIObjectIter	itp = pAISystem->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIOBJECT_PLAYER);
	for(; itp->GetObject(); itp->Next())
		m_items.push_front(SItem(itp->GetObject()->GetName(), AIOBJECT_PLAYER));

	for(TItemList::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		it->y = y;
		it->h = h;
		y += it->h + 2;	// small padding to separate sections.
	}

	m_listFullHeight = y;
	m_listItemHeight = h;

	if(pAISystem)
		pAISystem->SetDrawRecorderRange(m_timeRulerStart, m_timeRulerEnd, m_timeCursor);

	UpdateVertScrollInfo();
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::UpdateVertScrollInfo()
{
	int rectHeight = m_listRect.Height();
	int maxItemsOffset = m_listFullHeight;
	SCROLLINFO si;
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_PAGE|SIF_RANGE|SIF_DISABLENOSCROLL;
	si.nMin = 0;
	si.nMax = maxItemsOffset;
	si.nPage = rectHeight;

	SetScrollInfo(SB_VERT, &si);

	int iPos = GetScrollPos(SB_VERT);
	if (iPos < 0)
		SetScrollPos(SB_VERT, 0, TRUE);
	else if (iPos > maxItemsOffset)
		SetScrollPos(SB_VERT, maxItemsOffset, TRUE);

}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);  

	m_offscreenBitmap.DeleteObject();
	if (!m_offscreenBitmap.GetSafeHandle())
	{
		CDC *pDC = GetDC();
		m_offscreenBitmap.CreateCompatibleBitmap( pDC,cx,cy );
		ReleaseDC(pDC);
	}

	int rectHeight = m_listRect.Height();
	int maxItemsOffset = max(0, m_listFullHeight - rectHeight);
	if (m_itemsOffset < -maxItemsOffset)
		m_itemsOffset = -maxItemsOffset;

//	UpdateView();
	UpdateVertScrollInfo();
	UpdateViewRects();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::OnPaint()
{
	CPaintDC paintDC(this); // device context for painting

	CRect	clientRc;
	GetClientRect(clientRc);

	if(!m_offscreenBitmap.GetSafeHandle())
		m_offscreenBitmap.CreateCompatibleBitmap(&paintDC, clientRc.Width(), clientRc.Height());

	CMemDC dc(paintDC, &m_offscreenBitmap);

	dc.SetTextAlign(TA_LEFT|TA_TOP);
	dc.SetTextColor(GetSysColor(COLOR_BTNTEXT));
	dc.SetBkMode(TRANSPARENT);
	dc.SelectObject(gSettings.gui.hSystemFont);

	// Clear background
	dc.FillSolidRect(clientRc, GetSysColor(COLOR_3DFACE));

	DrawList(&dc);
	DrawTimeline(&dc);
	DrawDetails(&dc);
	DrawRuler(&dc);
}

//////////////////////////////////////////////////////////////////////////
inline int Blend(int a, int b, int c)
{
	return a + ((b - a) * c) / 255;
}

//////////////////////////////////////////////////////////////////////////
inline COLORREF Blend(COLORREF a, COLORREF b, int c)
{
	return RGB(Blend(GetRValue(a), GetRValue(b), c),
		Blend(GetGValue(a), GetGValue(b), c),
		Blend(GetBValue(a), GetBValue(b), c));
}

// From:
// Nice Numbers for Graph Labels
// by Paul Heckbert
// from "Graphics Gems", Academic Press, 1990
static float nicenum(float x, bool round)
{
	int exp;
	float f;
	float nf;

	exp = floorf(log10f(x));
	f = x / powf(10.0f, exp);		/* between 1 and 10 */
	if(round)
	{
		if(f < 1.5f) nf = 1.0f;
		else if(f < 3.0f) nf = 2.0f;
		else if(f < 7.0f) nf = 5.0f;
		else nf = 10.0f;
	}
	else
	{
		if(f <= 1.0f) nf = 1.0f;
		else if(f <= 2.0f) nf = 2.0f;
		else if(f <= 5.0f) nf = 5.0f;
		else nf = 10.0f;
	}
	return nf * powf(10.0f, exp);
}

//////////////////////////////////////////////////////////////////////////
int	CAIDebuggerView::TimeToClient(float t)
{
	return (int)((t - m_timeStart) / m_timeScale);
}

//////////////////////////////////////////////////////////////////////////
float	CAIDebuggerView::ClientToTime(int x)
{
	return (float)x * m_timeScale + m_timeStart;
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::DrawRuler(CDC* dc)
{
	bool	hasRuler(false);
	int		rulerStartX, rulerEndX;
	float	timeRulerLen(fabsf(m_timeRulerEnd - m_timeRulerStart));

	if(timeRulerLen > 0.001f)
	{
		rulerStartX = CLAMP(m_rulerRect.left + TimeToClient(m_timeRulerStart), m_rulerRect.left, m_rulerRect.right);
		rulerEndX = CLAMP(m_rulerRect.left + TimeToClient(m_timeRulerEnd), m_rulerRect.left, m_rulerRect.right);
		hasRuler = true;
	}

	dc->FillSolidRect(m_rulerRect, GetSysColor(COLOR_3DFACE));

	if(hasRuler)
		dc->FillSolidRect(rulerStartX, m_rulerRect.top, rulerEndX - rulerStartX, m_rulerRect.Height(), Blend(GetSysColor(COLOR_3DFACE), GetSysColor(COLOR_HIGHLIGHT), 100));

	int	ntick = (int)ceilf(m_timelineRect.Width() / (float)TIME_TICK_SPACING);
	int	w = ntick * TIME_TICK_SPACING;

	float	timeMin = m_timeStart;
	float	timeMax = m_timeStart + m_timeScale * w;

	float	range = nicenum(timeMax - timeMin, 0.0f);
	float	d = nicenum(range/(ntick - 1), false);
	float	graphMin = floorf(timeMin / d) * d;
	float	graphMax = ceilf(timeMax / d) * d;
	int	nfrac = max(-floorf(log10f(d)), 0.0f);
	int	nsub = (int)(d / powf(10.0f, floorf(log10f(d))));
	if(nsub < 2)
		nsub *= 10;
	// # of fractional digits to show
	char format[6];
	_snprintf(format, 6, "%%.%df", nfrac);	// simplest axis labels

	CFont*	oldFont = dc->SelectObject(&m_smallFont);
	TEXTMETRIC	textMetric;
	dc->GetTextMetrics(&textMetric);

	CString	label;

	int	texth = textMetric.tmHeight;
	int	y = m_rulerRect.bottom - 2 - texth;

	dc->SetTextColor(GetSysColor(COLOR_BTNTEXT));

	CPen	greyPen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
	CPen*	oldPen = dc->SelectObject(&greyPen);

	// Make this loop rock solid
	int steps = (graphMax - graphMin) / d + 1;

	float t = graphMin;
	for(int i = 0; i < steps; i++, t = graphMin + i * d)
	{
		label.Format(format, t);
		int	x = m_timelineRect.left + TimeToClient(t);

		if(x >= m_rulerRect.left && x < m_rulerRect.right)
		{
			dc->MoveTo(x, m_rulerRect.top + RULER_HEIGHT/2);
			dc->LineTo(x, m_rulerRect.top + RULER_HEIGHT);
		}

		for(int i = 1; i < nsub; i++)
		{
			float	t2 = t + ((float)i / (float)nsub) * d;
			int	x2 = m_timelineRect.left + TimeToClient(t2);
			if(x2 >= m_rulerRect.left && x2 < m_rulerRect.right)
			{
				dc->MoveTo(x2, m_rulerRect.top + RULER_HEIGHT*3/4);
				dc->LineTo(x2, m_rulerRect.top + RULER_HEIGHT);
			}
		}

		dc->ExtTextOut(x + 3, y, ETO_CLIPPED, m_rulerRect, label, NULL);
	}

	if(hasRuler)
	{
		CPen	highlightPen(PS_SOLID, 1, GetSysColor(COLOR_HIGHLIGHT));
		dc->SelectObject(&highlightPen);

		dc->MoveTo(rulerStartX, m_rulerRect.top);
		dc->LineTo(rulerStartX, m_timelineRect.bottom);
		dc->MoveTo(rulerEndX, m_rulerRect.top);
		dc->LineTo(rulerEndX, m_timelineRect.bottom);

		CString	label;
		label.Format("%.2fs", timeRulerLen);
		CSize	extend = dc->GetTextExtent(label);
		extend.cx += 4;

		if(extend.cx < rulerEndX - rulerStartX)
		{
			dc->MoveTo(rulerStartX, m_rulerRect.top + 2 + texth/2);
			dc->LineTo((rulerStartX + rulerEndX)/2 - extend.cx/2, m_rulerRect.top + 2 + texth/2);
			dc->MoveTo(rulerEndX, m_rulerRect.top + 2 + texth/2);
			dc->LineTo((rulerStartX + rulerEndX)/2 + extend.cx/2, m_rulerRect.top + 2 + texth/2);
		}

		dc->SetTextColor(GetSysColor(COLOR_HIGHLIGHT));
		dc->ExtTextOut((rulerStartX + rulerEndX)/2 - extend.cx/2 + 2, m_rulerRect.top + 2, ETO_CLIPPED, m_rulerRect, label, NULL);
	}

	CRect	tmp = m_rulerRect;
	tmp.bottom += 1;
	dc->Draw3dRect(tmp, GetSysColor(COLOR_3DHILIGHT), GetSysColor(COLOR_3DSHADOW));

	{
		COLORREF	cursorCol(RGB(240,16,6));
		CPen	cursorPen(PS_SOLID, 1, cursorCol);
		dc->SelectObject(&cursorPen);
		int	cursorX = m_rulerRect.left + TimeToClient(m_timeCursor);
		if(cursorX >= m_rulerRect.left && cursorX < m_rulerRect.right)
		{
			dc->MoveTo(cursorX, m_rulerRect.top);
			dc->LineTo(cursorX, m_timelineRect.bottom);
		}

		CString	label;
		label.Format("%.2fs", m_timeCursor);

		dc->SetTextColor(cursorCol);
		dc->ExtTextOut(cursorX + 2, m_rulerRect.top + 2, ETO_CLIPPED, m_rulerRect, label, NULL);
	}

	dc->SelectObject(oldPen);
	dc->SelectObject(oldFont);
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::DrawList(CDC* dc)
{
	CAdjustDC dcHelper(dc, m_listRect);

	dc->FillSolidRect(0,0,dcHelper.GetWidth(), dcHelper.GetHeight(), GetSysColor(COLOR_WINDOW));

	COLORREF	altBG = Blend(GetSysColor(COLOR_WINDOW), RGB(0,0,0), 8);

	dc->SelectObject(gSettings.gui.hSystemFont);
	TEXTMETRIC	textMetric;
	dc->GetTextMetrics(&textMetric);

	CFont*	oldFont = dc->SelectObject(&m_smallFont);

	for(size_t i = 0; i < m_streams.size(); i++)
	{
		CSize	size = dc->GetTextExtent(CString(m_streams[i].name.c_str()));
		m_streams[i].nameWidth = size.cx;
	}

	CRect clipRect;
	clipRect.left = 0;
	clipRect.top = 0;
	clipRect.right = m_listRect.Width();
	clipRect.bottom = m_listRect.Height();

	int	texth = textMetric.tmHeight;
	int	i = 0;
	if(m_items.empty())
	{
		dc->SetTextColor(GetSysColor(COLOR_3DSHADOW));
		dc->TextOut(ITEM_HEIGHT/3, ITEM_HEIGHT/2 - texth/2, CString("No AI Objects."));
	}
	else
	{
		for(TItemList::iterator it = m_items.begin(); it != m_items.end(); ++it, ++i)
		{
			const SItem&	item (*it);
			int	y = m_itemsOffset + item.y;
			int y2 = y + item.h;
			if (y2 >= 0 && y < dcHelper.GetHeight())
			{
				if((i & 1) == 0)
				{
					int yy = y;
					int hh = item.h;
					if (yy < 0)
					{
						hh += yy;
						yy = 0;
					}
					if (hh > 0)
						dc->FillSolidRect(0, yy, dcHelper.GetWidth(), hh, altBG);
				}

				dc->SetTextColor(GetSysColor(COLOR_BTNTEXT));
				dc->SelectObject(gSettings.gui.hSystemFont);
				dc->ExtTextOut(ITEM_HEIGHT/3, y + item.h/2 - texth/2, ETO_CLIPPED, clipRect, CString(item.name.c_str()), NULL);

				dc->SetTextColor(GetSysColor(COLOR_3DSHADOW));
				dc->SelectObject(&m_smallFont);
				int	yy = 0;
				for(size_t i = 0; i < m_streams.size(); i++)
				{
					dc->ExtTextOut(m_listRect.right - ITEM_HEIGHT/3 - m_streams[i].nameWidth, y + yy + m_streams[i].height/2 - texth/2,
						ETO_CLIPPED, clipRect, CString(m_streams[i].name.c_str()), NULL);

					yy += m_streams[i].height;
				}
			}
		}
	}

	dc->SelectObject(oldFont);
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::DrawDetails(CDC* dc)
{
	CAdjustDC dcHelper(dc, m_detailsRect);

	dc->FillSolidRect(0,0,dcHelper.GetWidth(), dcHelper.GetHeight(), GetSysColor(COLOR_3DFACE));

	if(m_items.empty())
		return;

	COLORREF	altBG = Blend(GetSysColor(COLOR_3DFACE), RGB(0,0,0), 8);

	dc->SelectObject(gSettings.gui.hSystemFont);

	TEXTMETRIC	textMetric;
	dc->GetTextMetrics(&textMetric);

	int	texth = textMetric.tmHeight;
	int	i = 0;

	IAISystem* pAISystem(GetIEditor()->GetAI()->GetAISystem());

	dc->SetTextColor(GetSysColor(COLOR_BTNTEXT));

	for(TItemList::iterator it = m_items.begin(); it != m_items.end(); ++it, ++i)
	{
		const SItem&	item (*it);
		int	y = m_itemsOffset + item.y;
		int y2 = y + item.h;
		if (y2 >= 0 && y < dcHelper.GetHeight())
		{
			IAIObject* pAI = pAISystem->GetAIObjectByName(item.type, item.name.c_str());
			if(!pAI)
				continue;

			IAIDebugRecord* pRecord(pAI->GetAIDebugRecord());
			if(!pRecord)
				continue;

			if((i & 1) == 0)
			{
				int yy = y;
				int hh = item.h;
				if (yy < 0)
				{
					hh += yy;
					yy = 0;
				}
				if (hh > 0)
					dc->FillSolidRect(0, yy, dcHelper.GetWidth(), hh, altBG);
			}

			IAIDebugStream* pStream(0);
			for(size_t i = 0; i < m_streams.size(); i++)
			{
				pStream = pRecord->GetStream((IAIRecordable::e_AIDbgEvent)m_streams[i].type);
				if(pStream)
				{
					if(m_streams[i].track == TRACK_TEXT)
						DrawStreamDetail(dc, pStream, y, m_streams[i].height, texth, m_streams[i].color);
					else if(m_streams[i].track == TRACK_FLOAT)
						DrawStreamFloatDetail(dc, pStream, y, m_streams[i].height, texth, m_streams[i].color);
				}
				y += m_streams[i].height;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::DrawStreamDetail(CDC* dc, IAIDebugStream* pStream, int iy, int ih, int texth, COLORREF color)
{
	float	curTime;
	pStream->Seek(m_timeCursor);
	char*	pString = (char*)(pStream->GetCurrent(curTime));

	int	y = iy;

	if(pString)
	{
		CRect clipRect;
		clipRect.left = 0;
		clipRect.top = 0;
		clipRect.right = m_detailsRect.Width();
		clipRect.bottom = m_detailsRect.Height();

		dc->ExtTextOut(ITEM_HEIGHT/3, y + ih/2 - texth/2, ETO_CLIPPED, clipRect, CString(pString), NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::DrawStreamFloatDetail(CDC* dc, IAIDebugStream* pStream, int iy, int ih, int texth, COLORREF color)
{
	float	curTime;
	pStream->Seek(m_timeCursor);
	float*	pVal = (float*)(pStream->GetCurrent(curTime));

	int	y = iy;

	if(pVal)
	{
		char	msg[32];
		_snprintf(msg, 32, "%.3f", *pVal);
		dc->TextOut(ITEM_HEIGHT/3, y + ih/2 - texth/2, CString(msg));
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::DrawTimeline(CDC* dc)
{
	CAdjustDC dcHelper(dc, m_timelineRect);

	dc->FillSolidRect(0,0,dcHelper.GetWidth(), dcHelper.GetHeight(), GetSysColor(COLOR_WINDOW));

	if(m_items.empty())
	{
		dc->Draw3dRect(0, 0, dcHelper.GetWidth(), dcHelper.GetHeight(), GetSysColor(COLOR_3DSHADOW), GetSysColor(COLOR_3DHILIGHT));
		return;
	}

	IAISystem* pAISystem(GetIEditor()->GetAI()->GetAISystem());

	COLORREF	altBG = Blend(GetSysColor(COLOR_WINDOW), RGB(0,0,0), 8);

	dc->SetTextColor(GetSysColor(COLOR_BTNTEXT));
	CFont*	oldFont = dc->SelectObject(&m_smallFont);
	TEXTMETRIC	textMetric;
	dc->GetTextMetrics(&textMetric);

	int	texth = textMetric.tmHeight;

	int	n = 0;
	for(TItemList::iterator it = m_items.begin(); it != m_items.end(); ++it, ++n)
	{
		const SItem&	item (*it);

		IAIObject* pAI = pAISystem->GetAIObjectByName(item.type, item.name.c_str());
		if(!pAI)
			continue;

		IAIDebugRecord* pRecord(pAI->GetAIDebugRecord());
		if(!pRecord)
			continue;

		IAIDebugStream* pStream(0);
		int	y = m_itemsOffset + item.y;
		int y2 = y + item.h;
		if (y2 >= 0 && y < dcHelper.GetHeight())
		{
			for(size_t i = 0; i < m_streams.size(); i++)
			{
				pStream = pRecord->GetStream((IAIRecordable::e_AIDbgEvent)m_streams[i].type);
				if(pStream)
				{
					if(m_streams[i].track == TRACK_TEXT)
						DrawStream(dc, pStream, y, m_streams[i].height, texth, m_streams[i].color, n, dcHelper.GetWidth());
					else if(m_streams[i].track == TRACK_FLOAT)
						DrawStreamFloat(dc, pStream, y, m_streams[i].height, texth, m_streams[i].color, n, dcHelper.GetWidth(), m_streams[i].vmin, m_streams[i].vmax);
				}
				y += m_streams[i].height;

				dc->FillSolidRect(0, y, dcHelper.GetWidth(), 1, Blend(GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_BTNSHADOW), 128));
			}
			dc->FillSolidRect(0, y, dcHelper.GetWidth(), 1, GetSysColor(COLOR_BTNSHADOW));
		}
	}

	dc->SelectObject(oldFont);

	//dc->Draw3dRect(m_timelineRect, GetSysColor(COLOR_3DSHADOW), GetSysColor(COLOR_3DHILIGHT));
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::DrawStream(CDC *dc, IAIDebugStream* pStream, int iy, int ih, int texth, COLORREF color, int i, int width)
{
	CPen	hilightPen(PS_SOLID, 2, color);
	CPen*	oldPen = dc->SelectObject(&hilightPen);

	dc->SetTextColor(color);

	float	curTime;
	pStream->Seek(m_timeStart);
	char*	pString = (char*)(pStream->GetCurrent(curTime));

	int	y = iy;
	int	leftSide, rightSide;

	leftSide = TimeToClient(curTime);
	rightSide = 0;

	CRect	textRc;
	textRc.top = y;
	textRc.bottom = y + ih;

	int	streamStartX = CLAMP(TimeToClient(pStream->GetStartTime()),0, width);
	int	streamEndX = CLAMP(TimeToClient(pStream->GetEndTime()), 0, width);
	if(streamEndX > streamStartX)
	{
		COLORREF	bg = Blend(GetSysColor(COLOR_WINDOW), color, ((i & 1) == 0) ? 32 : 16);
		dc->FillSolidRect(streamStartX, y, streamEndX - streamStartX, ih, bg);
	}

	while(leftSide < width && pStream->GetCurrentIdx() < pStream->GetSize())
	{
		float nextTime;
		char* pNextString = (char*)(pStream->GetNext(nextTime));

		rightSide = TimeToClient(nextTime);
		textRc.left = max(leftSide, 0);
		textRc.right = min(rightSide, width);

		if(!pNextString)
			textRc.right = width;

		if(leftSide >= 0 && leftSide < width - 1)
		{
			dc->MoveTo(leftSide + 1, y + 2);
			dc->LineTo(leftSide + 1, y + ih - 2);
		}
		if(textRc.Width() > 1)
			dc->ExtTextOut(leftSide + 4, y + ih/2 - texth/2, ETO_CLIPPED, textRc, CString(pString), NULL);

		curTime = nextTime;
		leftSide = rightSide;
		pString = pNextString;
	}

	dc->SelectObject(oldPen);
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::DrawStreamFloat(CDC *dc, IAIDebugStream* pStream, int iy, int ih, int texth, COLORREF color, int i, int width, float vmin, float vmax)
{
	CPen	hilightPen(PS_SOLID, 1, color);
	CPen*	oldPen = dc->SelectObject(&hilightPen);

	dc->SetTextColor(color);

	float	curTime;
	pStream->Seek(m_timeStart);
	float*	pVal = (float*)(pStream->GetCurrent(curTime));

	int	y = iy;
	int	leftSide, rightSide;

	leftSide = CLAMP(TimeToClient(curTime), 0, m_timelineRect.Width());
	rightSide = 0;

	CRect	textRc;
	textRc.top = y;
	textRc.bottom = y + ih;

	int	streamStartX = CLAMP(TimeToClient(pStream->GetStartTime()),0, width);
	int	streamEndX = CLAMP(TimeToClient(pStream->GetEndTime()), 0, width);
	if(streamEndX > streamStartX)
	{
		COLORREF	bg = Blend(GetSysColor(COLOR_WINDOW), color, ((i & 1) == 0) ? 32 : 16);
		dc->FillSolidRect(streamStartX, y, streamEndX - streamStartX, ih, bg);
	}

	bool	first = true;

	if(pVal)
	{
		int	x = leftSide;
		float	v = CLAMP((*pVal - vmin) / (vmax - vmin), 0.0f, 1.0f);
		int y = iy + ih - 1 - (int)(v * (ih-3));
		dc->MoveTo(x, y);
		first = false;
	}

	while(leftSide < width && pStream->GetCurrentIdx() < pStream->GetSize())
	{
		float nextTime;
		float* pNextVal = (float*)(pStream->GetNext(nextTime));

		rightSide = CLAMP(TimeToClient(nextTime), 0, m_timelineRect.Width());
//		textRc.left = max(leftSide, 0);
//		textRc.right = min(rightSide, width);

		if(pNextVal)
		{
			int	x = leftSide;
			float	v = CLAMP((*pNextVal - vmin) / (vmax - vmin), 0.0f, 1.0f);
			int y = iy + ih - 1 - (int)(v * (ih-3));
			if(first)
			{
				dc->MoveTo(x, y);
				first = false;
			}
			else
				dc->LineTo(x, y);
		}

		curTime = nextTime;
		leftSide = rightSide;
		pVal = pNextVal;
	}

	dc->SelectObject(oldPen);
}

//////////////////////////////////////////////////////////////////////////
BOOL CAIDebuggerView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);

	HCURSOR hCursor = 0;

	int	rulerHit = HitTestRuler(point);
	int	cursorHit = HitTestTimeCursor(point);
	int	separatorHit = HitTestSeparators(point);

	if((m_dragType == DRAG_NONE && (rulerHit == RULERHIT_START || rulerHit == RULERHIT_END)) ||
		m_dragType == DRAG_RULER_START || m_dragType == DRAG_RULER_END)
		hCursor = AfxGetApp()->LoadCursor(IDC_LEFTRIGHT);
	else if((m_dragType == DRAG_NONE && rulerHit == RULERHIT_SEGMENT) || m_dragType == DRAG_RULER_SEGMENT)
		hCursor = AfxGetApp()->LoadCursor(IDC_ARRWHITE);
	else if((m_dragType == DRAG_NONE && separatorHit != SEPARATORHIT_NONE) || m_dragType == DRAG_LIST_WIDTH || m_dragType == DRAG_DETAILS_WIDTH)
		hCursor = AfxGetApp()->LoadCursor(IDC_LEFTRIGHT);
	else if((m_dragType == DRAG_NONE && cursorHit == CURSORHIT_CURSOR) || m_dragType == DRAG_TIME_CURSOR)
		hCursor = AfxGetApp()->LoadCursor(IDC_LEFTRIGHT);
	else if(rulerHit == RULERHIT_EMPTY)
		hCursor = AfxGetApp()->LoadCursor(IDC_HIT_CURSOR);
	else if(m_dragType == DRAG_PAN)
		hCursor = AfxGetApp()->LoadCursor(IDC_HAND_INTERNAL);

	if(hCursor)
	{
		SetCursor(hCursor);
		return TRUE;
	}

	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

//////////////////////////////////////////////////////////////////////////
float	CAIDebuggerView::GetRecordStartTime()
{
	IAISystem* pAISystem(GetIEditor()->GetAI()->GetAISystem());

	float	minTime = 0.0f;
	int	i = 0;
	for(TItemList::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		const SItem&	item (*it);

		IAIObject* pAI = pAISystem->GetAIObjectByName(item.type, item.name.c_str());
		if(!pAI)
			continue;

		IAIDebugRecord* pRecord(pAI->GetAIDebugRecord());
		if(!pRecord)
			continue;

		IAIDebugStream* pStream(0);
		for(size_t i = 0; i < m_streams.size(); i++)
		{
			pStream = pRecord->GetStream((IAIRecordable::e_AIDbgEvent)m_streams[i].type);
			if(pStream)
			{
				float	t = pStream->GetStartTime();
				if(i == 0)
					minTime = t;
				else
					minTime = min(minTime, t);
				++i;
			}
		}
	}

	return minTime;
}

//////////////////////////////////////////////////////////////////////////
float	CAIDebuggerView::GetRecordEndTime()
{
	IAISystem* pAISystem(GetIEditor()->GetAI()->GetAISystem());

	float	maxTime = 0.0f;
	int	i = 0;
	for(TItemList::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		const SItem&	item (*it);

		IAIObject* pAI = pAISystem->GetAIObjectByName(item.type, item.name.c_str());
		if(!pAI)
			continue;

		IAIDebugRecord* pRecord(pAI->GetAIDebugRecord());
		if(!pRecord)
			continue;

		IAIDebugStream* pStream(0);
		for(size_t i = 0; i < m_streams.size(); i++)
		{
			pStream = pRecord->GetStream((IAIRecordable::e_AIDbgEvent)m_streams[i].type);
			if(pStream)
			{
				float	t = pStream->GetEndTime();
				if(i == 0)
					maxTime = t;
				else
					maxTime = max(maxTime, t);
				++i;
			}
		}

	}

	return maxTime;
}

//////////////////////////////////////////////////////////////////////////
void CAIDebuggerView::UpdateStreamDesc()
{
	m_streams.clear();

	if(m_streamSignalReceived)
		m_streams.push_back(SStreamDesc("Signal", IAIRecordable::E_SIGNALRECIEVED, TRACK_TEXT, RGB(13,41,122), ITEM_HEIGHT)); 
	if(m_streamSignalReceivedAux)
		m_streams.push_back(SStreamDesc("AuxSignal", IAIRecordable::E_SIGNALRECIEVEDAUX, TRACK_TEXT, RGB(49,122,13), ITEM_HEIGHT)); 
	if(m_streamBehaviorSelected)
		m_streams.push_back(SStreamDesc("Behavior", IAIRecordable::E_BEHAVIORSELECTED, TRACK_TEXT, RGB(218,157,10), ITEM_HEIGHT)); 
	if(m_streamAttenionTarget)
		m_streams.push_back(SStreamDesc("AttTarget", IAIRecordable::E_ATTENTIONTARGET, TRACK_TEXT, RGB(167,0,0), ITEM_HEIGHT)); 
	if(m_streamGoalPipeSelected)
		m_streams.push_back(SStreamDesc("GoalPipe", IAIRecordable::E_GOALPIPESELECTED, TRACK_TEXT, RGB(171,60,184), ITEM_HEIGHT)); 
	if(m_streamGoalPipeInserted)
		m_streams.push_back(SStreamDesc("GoalPipe Ins.", IAIRecordable::E_GOALPIPEINSERTED, TRACK_TEXT, RGB(110,60,184), ITEM_HEIGHT)); 
	if(m_streamLuaComment)
		m_streams.push_back(SStreamDesc("Lua Comment", IAIRecordable::E_LUACOMMENT, TRACK_TEXT, RGB(133,126,108), ITEM_HEIGHT)); 

	if(m_streamHealth)
		m_streams.push_back(SStreamDesc("Health", IAIRecordable::E_HEALTH, TRACK_FLOAT, RGB(23,154,229), ITEM_HEIGHT*2, 0.0f, 100.0f)); 
	if(m_streamHitDamage)
		m_streams.push_back(SStreamDesc("Hit Damage", IAIRecordable::E_HIT_DAMAGE, TRACK_TEXT, RGB(229,96,23), ITEM_HEIGHT)); 
	if(m_streamDeath)
		m_streams.push_back(SStreamDesc("Death", IAIRecordable::E_DEATH, TRACK_TEXT, RGB(32,19,6), ITEM_HEIGHT)); 
}

//////////////////////////////////////////////////////////////////////////

void CAIDebuggerView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// Scrollbar data
	pScrollBar = NULL; // Should be passed as null, so don't use it!
//	int max = GetScrollLimit(SB_VERT);
	
	int rectHeight = m_listRect.Height();
	int maxItemsOffset = max(0, m_listFullHeight - rectHeight);
	int pageSize = rectHeight;
	int iPosition = -m_itemsOffset;

	switch (nSBCode) {
		case SB_BOTTOM:   // Scroll to bottom.
			iPosition = 0;
			break;
		case SB_TOP:			// Scroll to top.
			iPosition = maxItemsOffset;
			break;
		case SB_LINEDOWN: // Scroll one line down.
			iPosition += m_listItemHeight;
			break;
		case SB_LINEUP:   // Scroll one line up.
			iPosition -= m_listItemHeight;
			break;
		case SB_PAGEDOWN: // Scroll one page down.
			iPosition += pageSize;
			break;
		case SB_PAGEUP:   // Scroll one page up.
			iPosition -= pageSize;
			break;
		case SB_THUMBPOSITION:	// Scroll to the absolute position. The current position is provided in nPos.
		case SB_THUMBTRACK:			//   Drag scroll box to specified position. The current position is provided in nPos.
			{
			iPosition = nPos;
			break;
			}
		case SB_ENDSCROLL:   // End scroll.
		default:
			;// Ignore
	};

	if (iPosition < 0) 
		iPosition = 0;
	if (iPosition > maxItemsOffset)
		iPosition = maxItemsOffset;

	m_itemsOffset = -iPosition;

	UpdateViewRects();
//	UpdateView();
	Invalidate();

	// Adjust the scroll block
	SetScrollPos(SB_VERT, iPosition, TRUE);
}
