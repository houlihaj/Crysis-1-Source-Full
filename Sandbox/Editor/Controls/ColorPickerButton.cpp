//---------------------------------------------------------------------------
// Copyright 2006 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#include "stdafx.h"
#include "ColorPickerButton.h"
#include "CustomColorDialog.h"

CColorPickerButton::CColorPickerButton()
{
}

void CColorPickerButton::OnButtonClicked()
{
	CCustomColorDialog dlg(this->GetColor(), CC_FULLOPEN, this);
	//dlg.SetColorChangeCallback( functor(*this,&CPropertyItem::OnColorChange) );
	if (dlg.DoModal() == IDOK)
	{
		this->SetColor(dlg.GetColor());
		//this->GetParent()->SendMessage(CPN_XT_SELENDOK, this->GetDlgCtrlID(), 0);
		this->GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(this->GetDlgCtrlID(), CPN_XT_SELENDOK), (LPARAM)this->GetSafeHwnd());
	}
}

int CColorPickerButton::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CColorButton::OnCreate(lpCreateStruct) == -1) 
		return -1;
	this->SetWindowText("");
	return 0;
}

IMPLEMENT_DYNAMIC(CColorPickerButton, CColorButton)
BEGIN_MESSAGE_MAP(CColorPickerButton, CColorButton)
	ON_CONTROL_REFLECT(BN_CLICKED, OnButtonClicked)
	ON_WM_CREATE()
END_MESSAGE_MAP()
