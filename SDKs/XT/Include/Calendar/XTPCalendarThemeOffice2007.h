// XTPCalendarThemeOffice2007.h: interface for the CXTPCalendarControlPaintManager class.
//
// This file is a part of the XTREME CALENDAR MFC class library.
// (c)1998-2007 Codejock Software, All Rights Reserved.
//
// THIS SOURCE FILE IS THE PROPERTY OF CODEJOCK SOFTWARE AND IS NOT TO BE
// RE-DISTRIBUTED BY ANY MEANS WHATSOEVER WITHOUT THE EXPRESSED WRITTEN
// CONSENT OF CODEJOCK SOFTWARE.
//
// THIS SOURCE CODE CAN ONLY BE USED UNDER THE TERMS AND CONDITIONS OUTLINED
// IN THE XTREME TOOLKIT PRO LICENSE AGREEMENT. CODEJOCK SOFTWARE GRANTS TO
// YOU (ONE SOFTWARE DEVELOPER) THE LIMITED RIGHT TO USE THIS SOFTWARE ON A
// SINGLE COMPUTER.
//
// CONTACT INFORMATION:
// support@codejock.com
// http://www.codejock.com
//
/////////////////////////////////////////////////////////////////////////////

//{{AFX_CODEJOCK_PRIVATE
#if !defined(_XTP_CALENDAR_THEME_OFFICE_2007_H__)
#define _XTP_CALENDAR_THEME_OFFICE_2007_H__
//}}AFX_CODEJOCK_PRIVATE

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "XTPCalendarTheme.h"

#if (_MSC_VER > 1100)
#pragma warning(push)
#endif

#pragma warning(disable: 4250)
//#pragma warning(disable: 4097)

#pragma warning(disable : 4100)// TODO: remove when themes will be finished

class CXTPOffice2007Images;
class CXTPImageManager;

/////////////////////////////////////////////////////////////////////////////
// Initial version. Will be expanded in the feature.
//{{AFX_CODEJOCK_PRIVATE
class _XTP_EXT_CLASS CXTPCalendarThemeOffice2007 : public CXTPCalendarTheme
{
	//{{ AFX_CODEJOCK_PRIVATE
	DECLARE_DYNCREATE(CXTPCalendarThemeOffice2007)
	//}} AFX_CODEJOCK_PRIVATE

	typedef CXTPCalendarTheme TBase;
public:

	DECLARE_THEMEPART(CTOHeader, CXTPCalendarTheme::CTOHeader)
		struct CHeaderColorsSet
		{
			CXTPPaintManagerColor         clrBorder;

			CXTPPaintManagerColorGradient grclrBkTop;
			CXTPPaintManagerColorGradient grclrBkBottom;

			CXTPPaintManagerColor         clrRBorderTop;
			CXTPPaintManagerColorGradient grclrRBorderTop;
			CXTPPaintManagerColorGradient grclrRBorderBottom;

			void CopySettings(const CHeaderColorsSet& rSrc);
		};

		//===========================================
		CXTPPaintManagerColor m_clrBaseColor;
		CXTPPaintManagerColor m_clrTodayBaseColor;

		CHeaderColorsSet m_clrsetNormal;
		CHeaderColorsSet m_clrsetSelected;
		CHeaderColorsSet m_clrsetToday;

		CHeaderText  m_TextLeftRight;
		CHeaderText  m_TextCenter;

		DECLARE_THEMEPART_MEMBER(0, CTOFormula_MulDivC, HeightFormula)
		//===========================================
		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual void RefreshFromParent(CTOHeader* pParentSrc);

		virtual void DoPropExchange(CXTPPropExchange* pPX);
		virtual void Serialize(CArchive& ar);

		virtual void Draw(CCmdTarget* pObject, CDC* pDC) {ASSERT(FALSE);};

		virtual int CalcHeight(CDC* pDC, int nCellWidth);

		virtual void Draw_Header(const CRect& rcRect, CDC* pDC,
			CHeaderColorsSet& setColors, int nFlags,
			LPCTSTR pcszLeftText,
			LPCTSTR pcszCenterText = NULL,
			LPCTSTR pcszRightText = NULL);

		virtual void Draw_Background(CDC* pDC, const CRect& rcRect, CHeaderColorsSet& setColors, int nFlags);
		virtual void Draw_Borders(CDC* pDC, const CRect& rcRect, CHeaderColorsSet& setColors);
		virtual void Draw_TextLR(CDC* pDC, LPCTSTR pcszText, const CRect& rcRect, int nFlags,
			UINT uFormat = DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER, int* pnWidth = NULL);

		virtual void Draw_TextCenter(CDC* pDC, LPCTSTR pcszCenterText, const CRect& rcRect, int nFlags, int* pnWidth = NULL);

	protected:
		virtual void _RefreshColors();
		virtual void _RefreshToodayColors();
	};
	friend class CTOHeader;

	//=======================================================================
	DECLARE_THEMEPART(CTOEvent, CXTPCalendarTheme::CTOEvent)
		struct CEventFontsColorsSet
		{
			CXTPPaintManagerColor           clrBorder;
			CXTPPaintManagerColorGradient   grclrBackground;

			CThemeFontColorSetValue         fcsetSubject;
			CThemeFontColorSetValue         fcsetLocation;
			CThemeFontColorSetValue         fcsetBody;

			CThemeFontColorSetValue         fcsetStartEnd; // for a month view single day event times
			                                               // or DayView multiday event From/To
			virtual void CopySettings(const CEventFontsColorsSet& rSrc);

			virtual void doPX(CXTPPropExchange* pPX, LPCTSTR pcszPropName, CXTPCalendarTheme* pTheme);
			virtual void Serialize(CArchive& ar);
		};

		CEventFontsColorsSet    m_fcsetNormal;
		CEventFontsColorsSet    m_fcsetSelected;

		CXTPPaintManagerColor   m_clrGripperBorder;
		CXTPPaintManagerColor   m_clrGripperBackground;

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual void RefreshFromParent(CTOEvent* pParentSrc);

		virtual void DoPropExchange(CXTPPropExchange* pPX);
		virtual void Serialize(CArchive& ar);

		virtual int CalcMinEventHeight(CDC* pDC);

		virtual void GetEventColors(CXTPCalendarViewEvent* pViewEvent, COLORREF& rClrBorder,
									COLORREF& rClrBorderNotSel, CXTPPaintManagerColorGradient& rGrclrBk,
									BOOL bSelected = -1);

		virtual void FillEventBackgroundEx(CDC* pDC, CXTPCalendarViewEvent* pViewEvent, const CRect& rcRect);
	protected:
		virtual BOOL Draw_ArrowL(CXTPCalendarViewEvent* pViewEvent, CDC* pDC, CRect& rrcRect);
		virtual BOOL Draw_ArrowR(CXTPCalendarViewEvent* pViewEvent, CDC* pDC, CRect& rrcRect);

		virtual CSize Draw_Icons(CXTPCalendarViewEvent* pViewEvent, CDC* pDC, const CRect& rcIconsMax, BOOL bCalculate = FALSE);
	};
	friend class CTOEvent;

	/////////////////////////////////////////////////////////////////////////
	// ******** Day View *********

	//=======================================================================
	DECLARE_THEMEPART2(CTODayViewEvent, CTOEvent, CXTPCalendarTheme::CTODayViewEvent)

		DECLARE_THEMEPART_MEMBER(0, CTOEventIconsToDraw, EventIconsToDraw)
		DECLARE_THEMEPART_MEMBER(1, CTOFormula_MulDivC,  HeightFormula)

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		//  void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect, int nEventPlaceNumber);
	protected:
	};

	DECLARE_THEMEPART2(CTODayViewEvent_MultiDay, CTODayViewEvent, CXTPCalendarTheme::CTODayViewEvent_MultiDay)

		CXTPCalendarThemeStringValue m_strDateFormatFrom;
		CXTPCalendarThemeStringValue m_strDateFormatTo;

		//CThemeFontColorSetValue        m_fcsetFromToDates;

		virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect, int nEventPlaceNumber);
		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);

		virtual void DoPropExchange(CXTPPropExchange* pPX);
		virtual void Serialize(CArchive& ar);

		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

		virtual void CalcEventYs(CRect& rcRect, int nEventPlaceNumber);

	protected:
		virtual BOOL Draw_ArrowLtext(CXTPCalendarViewEvent* pViewEvent, CDC* pDC, CThemeFontColorSetValue* pfcsetText, CRect& rrcRect, int nLeft_x);
		virtual BOOL Draw_ArrowRtext(CXTPCalendarViewEvent* pViewEvent, CDC* pDC, CThemeFontColorSetValue* pfcsetText, CRect& rrcRect, int nRight_x);

		virtual CString Format_FromToDate(CXTPCalendarViewEvent* pViewEvent, int nStart1End2);
	};
	friend class CTODayViewEvent_MultiDay;

	DECLARE_THEMEPART2(CTODayViewEvent_SingleDay, CTODayViewEvent, CXTPCalendarTheme::CTODayViewEvent_SingleDay)
		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect, int nEventPlaceNumber);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);
	protected:
		virtual void InitBusyStatusDefaultColors();
	};
	friend class CTODayViewEvent_SingleDay;

	//=======================================================================
	DECLARE_THEMEPART(CTODayViewTimeScale, CXTPCalendarTheme::CTODayViewTimeScale)
		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);

		DECLARE_THEMEPART_MEMBER(0, CTOFormula_MulDivC, HeightFormula)
	};

	//=======================================================================
	DECLARE_THEMEPART2(CTODayViewDayGroupHeader, CTOHeader, CXTPCalendarTheme::CTODayViewDayGroupHeader)
		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);
	};

	DECLARE_THEMEPART(CTODayViewDayGroupAllDayEvents, CXTPCalendarTheme::CTODayViewDayGroupAllDayEvents)

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

		//virtual int CalcHeight(CDC* pDC, int nCellWidth) {return 19;} // pDayView->CalculateHeaderFormatAndHeight(pDC, nCellWidth);
	};

	DECLARE_THEMEPART(CTODayViewDayGroupCell, CXTPCalendarTheme::CTODayViewDayGroupCell)
		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual int CalcHeight(CDC* pDC, int nCellWidth) {return 23;}
	};

	//=======================================================================
	DECLARE_THEMEPART(CTODayViewDayGroup, CXTPCalendarTheme::CTODayViewDayGroup)

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);

		//virtual void Draw(CCmdTarget* pObject, CDC* pDC);

		virtual BOOL IsSelected(CXTPCalendarViewGroup* pViewGroup);

		DECLARE_THEMEPART_MEMBER(0, CTODayViewDayGroupHeader,       Header)
		DECLARE_THEMEPART_MEMBER(1, CTODayViewDayGroupAllDayEvents, AllDayEvents)
		DECLARE_THEMEPART_MEMBER(2, CTODayViewDayGroupCell,         Cell)
		DECLARE_THEMEPART_MEMBER(3, CTODayViewEvent_MultiDay,       MultiDayEvent)
		DECLARE_THEMEPART_MEMBER(4, CTODayViewEvent_SingleDay,      SingleDayEvent)

	protected:
		virtual void AdjustDayEvents(CXTPCalendarDayViewGroup* pDayViewGroup, CDC* pDC);
	};

	DECLARE_THEMEPART2(CTODayViewDayHeader, CTOHeader, CXTPCalendarTheme::CTODayViewDayHeader)

		CTODayViewDayHeader() {
			m_nWeekDayFormat = 0;
		}

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

	private:
		int m_nWeekDayFormat; // 0 - no Week Day, 1 - short, 2 - long
	};

	//=======================================================================
	DECLARE_THEMEPART(CTODayViewDay, CXTPCalendarTheme::CTODayViewDay)

		CXTPPaintManagerColor         m_clrBorder;
		CXTPPaintManagerColor         m_clrTodayBorder;

		DECLARE_THEMEPART_MEMBER(0, CTODayViewDayHeader,            Header)
		DECLARE_THEMEPART_MEMBER(1, CTODayViewDayGroup,             Group)

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);

		virtual void DoPropExchange(CXTPPropExchange* pPX);
		virtual void Serialize(CArchive& ar);

		virtual void Draw_DayBorder(CXTPCalendarDayViewDay* pDayViewDay, CDC* pDC);
		virtual CRect ExcludeDayBorder(CXTPCalendarDayViewDay* pDayViewDay, const CRect& rcDay);
	};

	DECLARE_THEMEPART(CTODayViewHeader, CTOHeader)
		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
	};


	//=======================================================================
	DECLARE_THEMEPART(CTODayView, CXTPCalendarTheme::CTODayView)

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);

		DECLARE_THEMEPART_MEMBER_(0, CTODayViewHeader,   Header, CXTPCalendarTheme::CTOHeader)
		DECLARE_THEMEPART_MEMBER(1, CTODayViewEvent,     Event)
		DECLARE_THEMEPART_MEMBER(2, CTODayViewTimeScale, TimeScale)

		DECLARE_THEMEPART_MEMBER(3, CTODayViewDay, Day)

		// theme specific control options
		virtual BOOL IsUseCellAlignedDraggingInTimeArea() {return TRUE;};
	protected:
	};

	/////////////////////////////////////////////////////////////////////////
	// ******** Month View *********

	// ======= MonthViewEvent ======
	DECLARE_THEMEPART2(CTOMonthViewEvent, CTOEvent, CXTPCalendarTheme::CTOMonthViewEvent)
		DECLARE_THEMEPART_MEMBER(0, CTOEventIconsToDraw, EventIconsToDraw)
		DECLARE_THEMEPART_MEMBER(1, CTOFormula_MulDivC,  HeightFormula)

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect, int nEventPlaceNumber);
		//virtual void Draw(CCmdTarget* pObject, CDC* pDC);
	};
	friend class CTOMonthViewEvent;

	// ---- MonthViewEvent_SingleDay ----
	DECLARE_THEMEPART2(CTOMonthViewEvent_SingleDay, CTOMonthViewEvent, CXTPCalendarTheme::CTOMonthViewEvent_SingleDay)

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		//virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect, int nEventPlaceNumber);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

	protected:
		virtual void Draw_Background(CDC* pDC, const CRect& rcEventRect, CXTPCalendarMonthViewEvent* pViewEvent);
		virtual CSize Draw_Time(CDC* pDC, const CRect& rcEventRect, CXTPCalendarMonthViewEvent* pViewEvent);
		virtual void Draw_Caption(CDC* pDC, const CRect& rcTextRect, CXTPCalendarMonthViewEvent* pViewEvent);

	protected:
	};
	friend class CTOMonthViewEvent_SingleDay;

	// ---- MonthViewEvent_MultiDay ----
	DECLARE_THEMEPART2(CTOMonthViewEvent_MultiDay, CTOMonthViewEvent, CXTPCalendarTheme::CTOMonthViewEvent_MultiDay)

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		//virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect, int nEventPlaceNumber);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

	protected:
		virtual void Draw_Time(CDC* pDC, const CRect& rcEventRect, CXTPCalendarMonthViewEvent* pViewEvent);
	};
	friend class CTOMonthViewEvent_MultiDay;

	// ===== MonthViewDayHeader ====
	DECLARE_THEMEPART2(CTOMonthViewDayHeader, CTOHeader, CXTPCalendarTheme::CTOMonthViewDayHeader)
		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		//virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

	};
	friend class CTOMonthViewDayHeader;

	// ======= MonthViewDay =======
	DECLARE_THEMEPART(CTOMonthViewDay, CXTPCalendarTheme::CTOMonthViewDay)

		CXTPPaintManagerColor m_clrBorder;
		CXTPPaintManagerColor m_clrTodayBorder;

		CXTPPaintManagerColor m_clrBackgroundLight;
		CXTPPaintManagerColor m_clrBackgroundDark;
		CXTPPaintManagerColor m_clrBackgroundSelected;

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		//virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

		virtual void DoPropExchange(CXTPPropExchange* pPX);
		virtual void Serialize(CArchive& ar);

		DECLARE_THEMEPART_MEMBER(0, CTOMonthViewDayHeader,       Header);
		DECLARE_THEMEPART_MEMBER(1, CTOMonthViewEvent_MultiDay,  MultiDayEvent);
		DECLARE_THEMEPART_MEMBER(2, CTOMonthViewEvent_SingleDay, SingleDayEvent);

	};
	friend class CTOMonthViewDay;

	// ======= MonthViewWeekDayHeader =======
	DECLARE_THEMEPART2(CTOMonthViewWeekDayHeader, CTOHeader, CXTPCalendarTheme::CTOMonthViewWeekDayHeader)

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

	protected:
		virtual void Draw_Borders2(CDC* pDC, const CRect& rcRect, CHeaderColorsSet& setColors,
									BOOL bDrawRightBorder);

		virtual void _RefreshColors();
	protected:
		BOOL m_bWeekDayNamesLong;
		BOOL m_bWeekDayNameSaSuLong;

		CStringArray m_arWeekDayNamesLong;  // 0 - Sunday, 1 - Monday, ...; 7 - Sat/Sun
		CStringArray m_arWeekDayNamesShort; // 0 - Sunday, 1 - Monday, ...; 7 - Sat/Sun

	public:
		CTOMonthViewWeekDayHeader()
		{
			m_bWeekDayNamesLong = FALSE;
			m_bWeekDayNameSaSuLong = FALSE;
		}
	};
	friend class CTOMonthViewWeekDayHeader;

	// ======= MonthViewWeekHeader =======
	DECLARE_THEMEPART(CTOMonthViewWeekHeader, CTOHeader)
		CXTPPaintManagerColor   m_clrFreeSpaceBk;
		// CTOHeader::m_TextCenter.fcsetNormal is used to draw header caption;

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

		virtual void DoPropExchange(CXTPPropExchange* pPX);
		virtual void Serialize(CArchive& ar);

		virtual int CalcWidth(CDC* pDC);

		CTOMonthViewWeekHeader()
		{
			m_rcHeader.SetRect(0,0,0,0);
			m_bDateFormatShort = TRUE;
		}
	protected:
		CRect m_rcHeader;
		BOOL  m_bDateFormatShort;

		virtual void _RefreshColors();

		virtual void FormatWeekCaption(COleDateTime dtWDay1, CString& rstrShort, CString& rstrLong, int nShort1Long2 = 0);

		virtual CString _FormatWCaption(LPCTSTR pcszDay1, LPCTSTR pcszMonth1, LPCTSTR pcszDay7,
										LPCTSTR pcszMonth7, LPCTSTR pcszDayMonthSeparator, int nDateOrdering);

		virtual void Draw_Background(CDC* pDC, const CRect& rcRect, CHeaderColorsSet& setColors, int nFlags);
		virtual void Draw_Borders(CDC* pDC, const CRect& rcRect, CHeaderColorsSet& setColors);
		virtual void Draw_TextCenter(CDC* pDC, LPCTSTR pcszCenterText, const CRect& rcRect, int nFlags);
	};
	friend class CTOMonthViewWeekHeader;

	// ******* MonthView theme part object *******
	DECLARE_THEMEPART(CTOMonthView, CXTPCalendarTheme::CTOMonthView)

		// TODO:
		// BOOL m_bShowWeekNumbers;
		//
		//virtual void DoPropExchange(CXTPPropExchange* pPX);
		//virtual void Serialize(CArchive& ar);

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual void AdjustLayout(CDC* pDC, const CRect& rcRect, BOOL bCallPostAdjustLayout);
		virtual void Draw(CDC* pDC);

		DECLARE_THEMEPART_MEMBER( 0, CTOHeader,                 Header);
		DECLARE_THEMEPART_MEMBER( 1, CTOMonthViewEvent,         Event);
		DECLARE_THEMEPART_MEMBER( 2, CTOMonthViewWeekDayHeader, WeekDayHeader);
		DECLARE_THEMEPART_MEMBER_(3, CTOMonthViewWeekHeader,    WeekHeader, CTOHeader);
		DECLARE_THEMEPART_MEMBER( 4, CTOMonthViewDay,           Day);
	};
	friend class CTOMonthView;


	/////////////////////////////////////////////////////////////////////////
	// ******** WeekView *********

	// ======= WeekViewEvent =======
	DECLARE_THEMEPART2(CTOWeekViewEvent, CTOEvent, CXTPCalendarTheme::CTOWeekViewEvent)
		DECLARE_THEMEPART_MEMBER(0, CTOEventIconsToDraw, EventIconsToDraw)
		DECLARE_THEMEPART_MEMBER(1, CTOFormula_MulDivC,  HeightFormula)

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect, int nEventPlaceNumber);
		//virtual void Draw(CCmdTarget* pObject, CDC* pDC);
	};
	friend class CTOWeekViewEvent;

	// ==== WeekViewEvent_SingleDay ====
	DECLARE_THEMEPART2(CTOWeekViewEvent_SingleDay, CTOWeekViewEvent, CXTPCalendarTheme::CTOWeekViewEvent_SingleDay)
		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		//virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect, int nEventPlaceNumber);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);
	protected:
		virtual void Draw_Background(CDC* pDC, const CRect& rcEventRect, CXTPCalendarWeekViewEvent* pViewEvent);
		virtual CSize Draw_Time(CDC* pDC, const CRect& rcEventRect, CXTPCalendarWeekViewEvent* pViewEvent);
		virtual void Draw_Caption(CDC* pDC, const CRect& rcTextRect, CXTPCalendarWeekViewEvent* pViewEvent);
	};
	friend class CTOWeekViewEvent_SingleDay;

	// ==== WeekViewEvent_MultiDay ====
	DECLARE_THEMEPART2(CTOWeekViewEvent_MultiDay, CTOWeekViewEvent, CXTPCalendarTheme::CTOWeekViewEvent_MultiDay)
		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		//virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect, int nEventPlaceNumber);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

	//protected:
	//  virtual void Draw_Time(CDC* pDC, const CRect& rcEventRect, CXTPCalendarWeekViewEvent* pViewEvent);
	};
	friend class CTOWeekViewEvent_MultiDay;

	// ==== WeekViewDayHeader ====
	DECLARE_THEMEPART2(CTOWeekViewDayHeader, CTOHeader, CXTPCalendarTheme::CTOWeekViewDayHeader)
		CXTPCalendarThemeStringValue m_strHeaderFormat;

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		//virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

		virtual void DoPropExchange(CXTPPropExchange* pPX);
		virtual void Serialize(CArchive& ar);
	};
	friend class CTOWeekViewDayHeader;

	// ======= WeekViewDay =======
	DECLARE_THEMEPART(CTOWeekViewDay, CXTPCalendarTheme::CTOWeekViewDay)

		CXTPPaintManagerColor m_clrBorder;
		CXTPPaintManagerColor m_clrTodayBorder;

		CXTPPaintManagerColor m_clrBackgroundLight;
		CXTPPaintManagerColor m_clrBackgroundDark;
		CXTPPaintManagerColor m_clrBackgroundSelected;

		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		//virtual void AdjustLayout(CCmdTarget* pObject, CDC* pDC, const CRect& rcRect);
		virtual void Draw(CCmdTarget* pObject, CDC* pDC);

		virtual void DoPropExchange(CXTPPropExchange* pPX);
		virtual void Serialize(CArchive& ar);

		DECLARE_THEMEPART_MEMBER(0, CTOWeekViewDayHeader,       Header);
		DECLARE_THEMEPART_MEMBER(1, CTOWeekViewEvent_MultiDay,  MultiDayEvent);
		DECLARE_THEMEPART_MEMBER(2, CTOWeekViewEvent_SingleDay, SingleDayEvent);
	};
	friend class CTOWeekViewDay;

	// ==== WeekView theme part object ====
	DECLARE_THEMEPART(CTOWeekView, CXTPCalendarTheme::CTOWeekView)
		virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);
		//virtual void AdjustLayout(CDC* pDC, const CRect& rcRect, BOOL bCallPostAdjustLayout);
		//virtual void Draw(CDC* pDC);

		DECLARE_THEMEPART_MEMBER(0, CTOWeekViewEvent, Event);
		DECLARE_THEMEPART_MEMBER(1, CTOWeekViewDay,   Day);
	};

	//=======================================================================
	DECLARE_THEMEPART_MEMBER(0, CTOColorsSet,   ColorsSet)

	DECLARE_THEMEPART_MEMBER(1, CTOHeader,      Header)
	DECLARE_THEMEPART_MEMBER(2, CTOEvent,       Event)

	DECLARE_THEMEPART_MEMBER(3, CTODayView,     DayView)
	DECLARE_THEMEPART_MEMBER(4, CTOMonthView,   MonthView)
	DECLARE_THEMEPART_MEMBER(5, CTOWeekView,    WeekView)

	// DECLARE_THEMEPART_MEMBER(1, CTOFormula_MulDivC,  HeightFormula)
	//=======================================================================
	CXTPCalendarThemeOffice2007();
	virtual ~CXTPCalendarThemeOffice2007();

	// If enough space on the rect - draw 3 strings one under other
	// as multi-line text.
	// If rect height allow draw only 1 line - string 1 an 2 are drawn
	// as single line with a separator.
	// Separate font settings are used to draw each text.
	virtual CSize DrawText_Auto2SL3ML(CDC* pDC,
					LPCTSTR pcszText1, LPCTSTR pcszText2, LPCTSTR pcszText3,
					CThemeFontColorSet* pFontColor1, CThemeFontColorSet* pFontColor2,
					CThemeFontColorSet* pFontColor3,
					CRect& rcRect, LPCTSTR pcszText1Separator);


	// Common    colors id's: {0   - 99}
	// DayViev   colors id's: {100 - 499}
	// MonthViev colors id's: {500 - 999}
	// WeekViev  colors id's: {1000 - 1499}
	//
	// For darken colors: id + 5000

	enum XTPEnumThemeColorsSet
	{
		xtpCLR_DarkenOffset = 5000,

//      xtpCLR_base         = 1,
		xtpCLR_SelectedBk   = 2 + xtpCLR_DarkenOffset,

		xtpCLR_HeaderBorder = 3,
		xtpCLR_DayBorder    = 4,

		xtpCLR_HeaderTopGRfrom      = 10,
		xtpCLR_HeaderTopGRto        = 11,
		xtpCLR_HeaderBottomGRfrom   = 12,
		xtpCLR_HeaderBottomGRto     = 13,

		xtpCLR_MultiDayEventBorder          = 15,
		xtpCLR_MultiDayEventSelectedBorder  = 16 + xtpCLR_DarkenOffset,
		xtpCLR_MultiDayEventBkGRfrom        = 17,
		xtpCLR_MultiDayEventBkGRto          = 18,

		xtpCLR_MultiDayEventFromToDates     = 20,

		xtpCLR_DayViewHeaderRBorderTop          = 101,
		xtpCLR_DayViewHeaderRBorderTopGRfrom    = 102,
		xtpCLR_DayViewHeaderRBorderTopGRto      = 103,
		xtpCLR_DayViewHeaderRBorderBottomGRfrom = 104,
		xtpCLR_DayViewHeaderRBorderBottomGRto   = 105,

		xtpCLR_DayViewCellWorkBk                = 130,
		xtpCLR_DayViewCellNonWorkBk             = 131,

		xtpCLR_DayViewCellWorkBorderBottomInHour    = 132,
		xtpCLR_DayViewCellWorkBorderBottomHour      = 133,
		xtpCLR_DayViewCellNonWorkBorderBottomInHour = 134,
		xtpCLR_DayViewCellNonWorkBorderBottomHour   = 135,

		xtpCLR_DayViewAllDayEventsBk            = 140,
		xtpCLR_DayViewAllDayEventsBorderBottom  = 141,

		xtpCLR_DayViewSingleDayEventBorder      = 150,
		xtpCLR_DayViewSingleDayEventSelectedBorder= 151 + xtpCLR_DarkenOffset,
		xtpCLR_DayViewSingleDayEventBkGRfrom    = 152,
		xtpCLR_DayViewSingleDayEventBkGRto      = 153,

		//xtpCLR_MonthView_ = 500,
		xtpCLR_MonthViewWHeaderBBorderLeftGRfrom    = 501,
		xtpCLR_MonthViewWHeaderBBorderLeftGRto      = 502,
		xtpCLR_MonthViewWHeaderBBorderRightGRfrom   = 503,
		xtpCLR_MonthViewWHeaderBBorderRightGRto     = 504,

		xtpCLR_MonthViewDayBkLight                  = 510,
		xtpCLR_MonthViewDayBkDark                   = 511,
		xtpCLR_MonthViewDayBkSelected               = 512,

		xtpCLR_MonthViewEventTime                   = 515 + xtpCLR_DarkenOffset,

		xtpCLR_MonthViewSingleDayEventBorder        = 520,
		xtpCLR_MonthViewSingleDayEventSelectedBorder= 521 + xtpCLR_DarkenOffset,
		xtpCLR_MonthViewSingleDayEventBkGRfrom      = 522,
		xtpCLR_MonthViewSingleDayEventBkGRto        = 523,

		//xtpCLR_WeekView_ = 1000,
		xtpCLR_WeekViewWHeaderBBorderLeftGRfrom    = 1001,
		xtpCLR_WeekViewWHeaderBBorderLeftGRto      = 1002,
		xtpCLR_WeekViewWHeaderBBorderRightGRfrom   = 1003,
		xtpCLR_WeekViewWHeaderBBorderRightGRto     = 1004,

		xtpCLR_WeekViewDayBkLight                  = 1010,
		xtpCLR_WeekViewDayBkDark                   = 1011,
		xtpCLR_WeekViewDayBkSelected               = 1012,

		xtpCLR_WeekViewEventTime                   = 1015 + xtpCLR_DarkenOffset,

		xtpCLR_WeekViewSingleDayEventBorder         = 1020,
		xtpCLR_WeekViewSingleDayEventSelectedBorder = 1021 + xtpCLR_DarkenOffset,
		xtpCLR_WeekViewSingleDayEventBkGRfrom       = 1022,
		xtpCLR_WeekViewSingleDayEventBkGRto         = 1023,

	};

public:
	virtual CXTPCalendarViewEventSubjectEditor* StartEditSubject(CXTPCalendarViewEvent* pViewEvent);

	//-----------------------------------------------------------------------
	// Summary:
	//     Thin method determine event view type (day, week, month) and
	//     return corresponding drawing part.
	// Returns:
	//     Theme drawing part for editing event.
	//-----------------------------------------------------------------------
	virtual CTOEvent* GetThemePartForEvent(CXTPCalendarViewEvent* pViewEvent);

	virtual void GetItemTextIfNeed(int nItem, CString* pstrText, CXTPCalendarViewDay* pViewDay);

protected:
	virtual void RefreshMetrics(BOOL bRefreshChildren = TRUE);

protected:
	CXTPOffice2007Images*   m_pImages;

};

//////////////////////////////////////////////////////////////////////
class _XTP_EXT_CLASS  CXTPCalendarViewEventSubjectEditor2007 : public CXTPCalendarViewEventSubjectEditor
{
public:
	//-----------------------------------------------------------------------
	// Summary:
	//     Default object constructor.
	// Parameters:
	//     pOwner     - A pointer to an owner calendar control object.
	//     pViewEvent - A pointer to an editing event view object.
	//     pTheme2007 - A pointer to an owner theme.
	// See Also: ~CXTPCalendarViewEventSubjectEditor2007()
	//-----------------------------------------------------------------------
	CXTPCalendarViewEventSubjectEditor2007(CXTPCalendarControl* pOwner, CXTPCalendarViewEvent* pViewEvent, CXTPCalendarThemeOffice2007* pTheme2007);

	//-----------------------------------------------------------------------
	// Summary:
	//     Default class destructor.
	//-----------------------------------------------------------------------
	virtual ~CXTPCalendarViewEventSubjectEditor2007();

protected:
	CXTPCalendarViewEvent* m_pViewEvent;        // Stored pointer to the editing event object.
	CXTPCalendarThemeOffice2007* m_pTheme2007;  // Stored pointer to the owner theme.
	CXTPPaintManagerColorGradient m_grclrBk;    // Subject Editor background color (or gradient colors).

	// {{AFX_CODEJOCK_PRIVATE
	DECLARE_MESSAGE_MAP()

	afx_msg virtual BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg virtual HBRUSH CtlColor(CDC* pDC, UINT nCtlColor);
	afx_msg virtual void OnChange();
	// }}AFX_CODEJOCK_PRIVATE
};

//}}AFX_CODEJOCK_PRIVATE
/////////////////////////////////////////////////////////////////////////////

#if (_MSC_VER > 1100)
#pragma warning(pop)
#endif

#endif // !defined(_XTP_CALENDAR_THEME_OFFICE_2007_H__)
