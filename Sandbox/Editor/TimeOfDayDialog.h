////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   TimeOfDayDialog.h
//  Version:     v1.00
//  Created:     12/3/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __TimeOfDayDialog_h__
#define __TimeOfDayDialog_h__

#include "ToolbarDialog.h"
#include "Controls/SliderCtrlEx.h"
#include "Controls/PropertyCtrl.h"
#include "Controls/TimelineCtrl.h"

#define TIMEOFDAYN_CHANGE  0x0800

struct ISplineInterpolator;

//////////////////////////////////////////////////////////////////////////
class CTimeOfDayEdit : public CXTTimeEdit
{
public:
	CTimeOfDayEdit()
	{
		m_bMilitary = true;
	}
	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		if ( pMsg->message == WM_KEYDOWN || pMsg->message==WM_KEYUP )
		{
			switch ( pMsg->wParam )
			{
			case VK_RETURN:
				::SendMessage( GetOwner()->GetSafeHwnd(),WM_COMMAND,MAKEWPARAM( GetDlgCtrlID(),TIMEOFDAYN_CHANGE ),(LPARAM)GetSafeHwnd() );
				break;
			}
		}
		return __super::PreTranslateMessage(pMsg);
	}
	virtual bool ProcessMask(UINT& nChar,int nEndPos)
	{
		// check the key against the mask
		switch ( m_strMask.GetAt( nEndPos ) )
		{
		case '0':		// digit only //completely changed this
			{
				if ( _istdigit( (TCHAR)nChar ) )
				{
					switch (nEndPos)
					{
					case 0:
						if (m_bMilitary)
						{
							if (nChar < 48 || nChar > 50)
							{
								MessageBeep((UINT)-1);
								return false;
							}
						}
						else
						{
							if (nChar < 48 || nChar > 49)
							{
								MessageBeep((UINT)-1);
								return false;
							}
						}
						break;

					case 1:
						if (m_bMilitary)
						{
							if (nChar < 48 || nChar > 57)
							{
								MessageBeep((UINT)-1);
								return false;
							}
						}
						else
						{
							if (nChar < 48 || nChar > 50)
							{
								MessageBeep((UINT)-1);
								return false;
							}
						}
						break;

					case 3:
						if (nChar < 48 || nChar > 53)
						{
							MessageBeep((UINT)-1);
							return false;
						}
						break;

					case 4:
						if (nChar < 48 || nChar > 57)
						{
							MessageBeep((UINT)-1);
							return false;
						}
						break;
					}

					return true;
				}
				break;
			}
		}

		MessageBeep((UINT)-1);
		return false;
	}

};

class CSplineCtrl;
class CColorGradientCtrl;
//////////////////////////////////////////////////////////////////////////
// Window that holds effector info.
//////////////////////////////////////////////////////////////////////////
class CTimeOfDayDialog : public CToolbarDialog, public IEditorNotifyListener
{
	DECLARE_DYNCREATE(CTimeOfDayDialog)
public:
	CTimeOfDayDialog();
	~CTimeOfDayDialog();
	
	static void RegisterViewClass();

	static void UpdateValues();

	enum { IDD = IDD_DATABASE };

protected:
	DECLARE_MESSAGE_MAP()

	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void PostNcDestroy();
	afx_msg BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll( UINT nSBCode,UINT nPos,CScrollBar* pScrollBar );
	afx_msg void OnSplineRClick( UINT nID, NMHDR* pNMHDR, LRESULT* lpResult );
	afx_msg void OnBeforeSplineChange( UINT nID, NMHDR* pNMHDR, LRESULT* lpResult );
	afx_msg void OnSplineChange( UINT nID, NMHDR* pNMHDR, LRESULT* lpResult );
	afx_msg LRESULT OnTaskPanelNotify( WPARAM wParam, LPARAM lParam );
	afx_msg void OnClose();

	afx_msg void OnPlayAnim();
	afx_msg void OnUpdatePlayAnim( CCmdUI* pCmdUI );
	afx_msg void OnRecord();
	afx_msg void OnUpdateRecord( CCmdUI* pCmdUI );
	afx_msg void OnPlayAnimFrom0();
	afx_msg void OnUpdatePlayAnimFrom0( CCmdUI* pCmdUI );
	afx_msg void OnGotoMinus1();
	afx_msg void OnGoto0();
	afx_msg void OnGoto1();
	afx_msg void OnChangeCurrentTime( UINT nID );
	afx_msg void OnChangeTimeAnimSpeed();

	afx_msg void OnImport();
	afx_msg void OnExport();
	afx_msg void OnExpandAll();
	afx_msg void OnResetToDefaultValues();
	afx_msg void OnCollapseAll();

	afx_msg void OnHold();
	afx_msg void OnFetch();
	afx_msg void OnUndo();
	afx_msg void OnRedo();

	afx_msg void OnPropertySelected(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTimelineCtrlChange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSplineCtrlScrollZoom(NMHDR* pNMHDR, LRESULT* pResult);

	void OnUpdateProperties( IVariable *var );

	void CreateProperties();

	void ReloadCtrls();
	void SetTime( float fTime,bool bOnlyUpdateUI=false,bool bNoPropertiesUpdate=false );
	void SetTimeRange( float fTimeStart,float fTimeEnd,float fSpeed );
	float GetTime() const;
	void ClearCtrls();
	void RefreshPropertiesValues( bool bSetTime=true );

	void RecalcLayout();

	//////////////////////////////////////////////////////////////////////////
	// IEditorNotifyListener
	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );
	//////////////////////////////////////////////////////////////////////////

private:
	CXTPToolBar m_wndToolBar;
	CXTPTaskPanel m_wndTaskPanel;
	CImageList m_imageList;

	CPropertyCtrl m_propsCtrl;
	CVarBlockPtr m_pVars;

	CTimelineCtrl m_timelineCtrl;
	CTimelineCtrl m_timelineCtrl2;
	//CSliderCtrlCustomDraw m_weightSlider;
	CSliderCtrlEx m_weightSlider;
	CStatic m_textMinus100,m_textPlus100,m_text0;
	bool m_bAnimation;
	bool m_bPlayFrom0;
	bool m_bIgnoreScroll;
	bool m_bRecording;

	int m_splineCtrlHeight;

	CTimeOfDayEdit m_timeEdit;
	CButton m_forceUpdateCheck;

	CTimeOfDayEdit m_timeEditStart;
	CTimeOfDayEdit m_timeEditEnd;

	CNumberCtrl m_timeAnimSpeedCtrl;

	struct ControllerInfo
	{
		_smart_ptr<IVariable> pVariable;
		CColorGradientCtrl* pColorCtrl;
		CSplineCtrlEx* pSplineCtrl;
		int m_nVarId;

		ControllerInfo() : pColorCtrl(0),pSplineCtrl(0) {};
	};
	std::vector<ControllerInfo> m_controllers;
	static CTimeOfDayDialog* m_pInstance;

	CSplineCtrlEx *m_pSplineCtrl;
	CColorGradientCtrl *m_pColorGradientCtrl;
};

#endif // __TimeOfDayDialog_h__