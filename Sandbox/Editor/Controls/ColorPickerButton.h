//---------------------------------------------------------------------------
// Copyright 2006 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __COLORPICKERBUTTON_H__
#define __COLORPICKERBUTTON_H__

#include "ColorButton.h"

class CColorPickerButton : public CColorButton
{
	DECLARE_DYNAMIC(CColorPickerButton)
public:
	CColorPickerButton();

private:
	afx_msg void OnButtonClicked();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

	DECLARE_MESSAGE_MAP()
};

#endif //__COLORPICKERBUTTON_H__
