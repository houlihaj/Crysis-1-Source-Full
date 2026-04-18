// ColorCheckBox.cpp : implementation file
//

#include "stdafx.h"
#include "ColorCheckBox.h"

/////////////////////////////////////////////////////////////////////////////
// CColorCheckBox
IMPLEMENT_DYNCREATE( CColorCheckBox,CColoredPushButton )

CColorCheckBox::CColorCheckBox()
{
	m_nChecked = 0;
}

CColorCheckBox::~CColorCheckBox()
{
}

//BEGIN_MESSAGE_MAP(CColorCheckBox, CColoredPushButton)
//	//{{AFX_MSG_MAP(CColorCheckBox)
//		// NOTE - the ClassWizard will add and remove mapping macros here.
//	//}}AFX_MSG_MAP
//END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CColorCheckBox message handlers

void CColorCheckBox::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) 
{
	if (m_nChecked == 1)
	{
		lpDrawItemStruct->itemState |= ODS_SELECTED;
	}
	CColoredPushButton::DrawItem( lpDrawItemStruct );
}

//////////////////////////////////////////////////////////////////////////
void CColorCheckBox::SetCheck(int nCheck)
{
	__super::SetCheck(nCheck);
	if (m_nChecked != nCheck)
	{
		m_nChecked = nCheck;
		if(::IsWindow(m_hWnd))
			Invalidate();
	}
};

//////////////////////////////////////////////////////////////////////////
void CColorCheckBox::ChangeStyle()
{
	if ((gSettings.gui.bWindowsVista || gSettings.gui.bSkining) && !m_hIcon)
	{
		if (m_bOwnerDraw)
		{
			m_bOwnerDraw = false;
			ModifyStyle(0xf, BS_CHECKBOX|BS_PUSHLIKE);
		}
	}
	else
	{
		if (!m_bOwnerDraw)
		{
			m_bOwnerDraw = true;
			ModifyStyle(0xf, BS_OWNERDRAW);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CColorCheckBox::PreSubclassWindow() 
{
	CColoredPushButton::PreSubclassWindow();

	ChangeStyle();
}

//////////////////////////////////////////////////////////////////////////
BOOL CColorCheckBox::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_PAINT)
	{
		if (gSettings.gui.bWindowsVista || gSettings.gui.bSkining)
		{
			if ((m_bOwnerDraw && !m_hIcon) || (!m_bOwnerDraw && m_hIcon))
			{
				ChangeStyle();
				Invalidate(TRUE);
			}
		}
		else
		{
			if (!m_bOwnerDraw)
			{
				ChangeStyle();
				Invalidate(TRUE);
			}
		}
	}
	return __super::PreTranslateMessage(pMsg);
}
